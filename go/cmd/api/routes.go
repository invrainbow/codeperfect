package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log"
	"net"
	"net/http"
	"os"
	"strconv"
	"strings"
	"time"

	"github.com/codeperfect95/codeperfect/go/cmd/api/emails"
	"github.com/codeperfect95/codeperfect/go/cmd/lib"
	"github.com/codeperfect95/codeperfect/go/db"
	"github.com/codeperfect95/codeperfect/go/models"
	"github.com/codeperfect95/codeperfect/go/versions"
	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
	"github.com/stripe/stripe-go/v72"
	"github.com/stripe/stripe-go/v72/customer"
	"github.com/stripe/stripe-go/v72/webhook"
	"gorm.io/gorm"
)

var (
	IsDevelMode         = os.Getenv("DEVELOPMENT_MODE") == "1"
	StripeAPIKey        = os.Getenv("STRIPE_API_KEY")
	StripeWebhookSecret = os.Getenv("STRIPE_WEBHOOK_SECRET")
)

var eventQueue chan stripe.Event

func init() {
	if StripeAPIKey == "" {
		panic("invalid stripe api key")
	}

	if StripeWebhookSecret == "" {
		panic("invalid stripe webhook secret")
	}

	stripe.Key = StripeAPIKey
	eventQueue = make(chan stripe.Event)
}

// https://github.com/gin-gonic/gin/issues/2697#issuecomment-957244574
func isPrivateIP(ip string) bool {
	if ip == "127.0.0.1" || ip == "::1" {
		return true
	}

	ranges := [][]net.IP{
		{net.ParseIP("10.0.0.0"), net.ParseIP("10.255.255.255")},
		{net.ParseIP("127.0.0.0"), net.ParseIP("127.255.255.255")},
		{net.ParseIP("169.254.0.0"), net.ParseIP("169.254.255.255")},
		{net.ParseIP("172.16.0.0"), net.ParseIP("172.31.255.255")},
		{net.ParseIP("192.168.0.0"), net.ParseIP("192.168.255.255")},
	}

	trial := net.ParseIP(ip)
	for _, pair := range ranges {
		if bytes.Compare(trial, pair[0]) >= 0 && bytes.Compare(trial, pair[1]) <= 0 {
			return true
		}
	}
	return false
}

func WorkaroundGinRetardationAndGetClientIP(c *gin.Context) string {
	ip := c.ClientIP()
	if ip != "127.0.0.1" && ip != "::1" {
		return ip
	}
	realip := c.GetHeader("X-Forwarded-For")
	if realip == "" || isPrivateIP(realip) {
		return ip
	}
	return realip
}

func GetAPIBase() string {
	if IsDevelMode {
		return "http://localhost:8080"
	}
	return "https://api.codeperfect95.com"
}

func GetFrontendBase() string {
	if IsDevelMode {
		return "http://localhost:3000"
	}
	return "https://codeperfect95.com"
}

func sendError(c *gin.Context, errmsg string) {
	// i'm not using http semantics
	// 500 just means error, any error
	// 200 means ok
	c.JSON(500, &models.ErrorResponse{Error: errmsg})
}

func sendServerError(c *gin.Context, format string, args ...interface{}) {
	log.Printf("server error: %s", fmt.Sprintf(format, args...))
	sendError(c, "An unknown server error occurred.")
}

type authUserResult struct {
	user    *models.User
	result  models.AuthResult
	version int
}

func readVersionHeader(c *gin.Context) int {
	n, err := strconv.Atoi(c.GetHeader("X-Version"))
	if err != nil {
		return 0
	}
	return n
}

func authUser(c *gin.Context, version int) (*authUserResult, error) {
	email := c.GetHeader("X-Email")
	licenseKey := c.GetHeader("X-License-Key")

	if version == 0 {
		version = readVersionHeader(c)
		if version == 0 {
			return &authUserResult{result: models.AuthFailInvalidVersion}, nil
		}
	}

	makeResult := func(user *models.User, result models.AuthResult) (*authUserResult, error) {
		return &authUserResult{
			user:    user,
			version: version,
			result:  result,
		}, nil
	}

	makeErr := func(result models.AuthResult) (*authUserResult, error) {
		return makeResult(nil, result)
	}

	var user models.User
	if res := db.DB.First(&user, "email = ? and license_key = ?", email, licenseKey); res.Error != nil {
		if errors.Is(res.Error, gorm.ErrRecordNotFound) {
			return makeErr(models.AuthFailInvalidCredentials)
		}
		return nil, res.Error
	}

	if !user.Active {
		return makeErr(models.AuthFailUserInactive)
	}

	if user.LockedVersion != 0 {
		if version > user.LockedVersion {
			return makeErr(models.AuthFailVersionLocked)
		}
		return makeResult(&user, models.AuthSuccessLocked)
	}
	return makeResult(&user, models.AuthSuccessUnlocked)
}

var SendAlertsForSelf = (os.Getenv("SEND_ALERTS_FOR_SELF") == "1")

func SendSlackForUser(user *models.User, format string, args ...interface{}) {
	if !SendAlertsForSelf && user.Email == "bh@codeperfect95.com" {
		return
	}
	go SendSlack(format, args...)
}

func PostTrialV2(c *gin.Context) {
	var req models.AuthRequestV2
	if err := c.ShouldBindJSON(&req); err != nil {
		sendError(c, "Invalid data.")
		return
	}

	version := readVersionHeader(c)
	if version == 0 {
		sendError(c, "Invalid version.")
		return
	}

	go SendSlack(
		"Trial user `%s` opened on `%s-%s`",
		WorkaroundGinRetardationAndGetClientIP(c),
		req.OS,
		versions.VersionToString(version),
	)

	c.JSON(http.StatusOK, &models.AuthResponseV2{Result: models.AuthSuccessUnlocked})
}

func PostTrial(c *gin.Context) {
	var req models.AuthRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		sendError(c, "Invalid data.")
		return
	}

	ip := WorkaroundGinRetardationAndGetClientIP(c)

	go SendSlack("trial user `%s` opened on `%s-%s`", ip, req.OS, versions.VersionToString(req.CurrentVersion))

	c.JSON(http.StatusOK, &models.AuthResponse{
		Success: true,
	})
}

func PostAuth(c *gin.Context) {
	var req models.AuthRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		sendError(c, "Invalid data.")
		return
	}

	res, err := authUser(c, req.CurrentVersion)
	if err != nil {
		sendServerError(c, "authUser error: %v", err)
		return
	}

	if !res.result.IsSuccess() {
		c.JSON(http.StatusOK, &models.AuthResponse{Success: false})
		return
	}

	SendSlackForUser(res.user, "%s authed on `%s-%s`.", res.user.Email, req.OS, versions.VersionToString(req.CurrentVersion))

	c.JSON(http.StatusOK, &models.AuthResponse{
		Success:   true,
		SessionID: 0,
	})
}

func PostAuthV2(c *gin.Context) {
	var req models.AuthRequestV2
	if err := c.ShouldBindJSON(&req); err != nil {
		sendError(c, "Invalid data.")
		return
	}

	res, err := authUser(c, 0)
	if err != nil {
		sendServerError(c, "authUser error: %v", err)
		return
	}

	SendSlackForUser(
		res.user,
		"%s authed on `%s-%s` (result = `%s`)",
		res.user.Email,
		req.OS,
		versions.VersionToString(res.version),
		res.result.String(),
	)

	if !res.result.IsSuccess() {
		c.JSON(http.StatusOK, &models.AuthResponseV2{
			Result: res.result,
		})
	} else {
		c.JSON(http.StatusOK, &models.AuthResponseV2{
			Result:        res.result,
			LockedVersion: &res.version,
		})
	}
}

func PostHeartbeat(c *gin.Context) {
	c.JSON(200, &gin.H{"ok": true})
}

func handleCrashReport(c *gin.Context, user *models.User, osslug string, version int, report string) {
	if len(osslug) > 16 {
		c.JSON(http.StatusBadRequest, gin.H{})
		log.Printf("user sent bad os: %s", osslug[:128])
		return
	}

	if user == nil {
		log.Print("user is nil")
	} else {
		log.Printf("user not nil, email = %s license = %s", user.Email, user.LicenseKey)
	}

	if len(report) > 64000 {
		log.Printf("user sent large content with len %d", len(report))
		report = report[:64000]
	}

	newlines := 0
	for i, ch := range report {
		if ch == '\n' {
			newlines++
			if newlines > 1024 {
				log.Printf("user sent large content with lots of lines")
				report = report[:i]
				break
			}
		}
	}

	infoParts := []string{
		osslug + "-" + versions.VersionToString(version),
		WorkaroundGinRetardationAndGetClientIP(c),
	}

	if user != nil {
		infoParts = append(infoParts, user.Email)
	}

	for i, val := range infoParts {
		infoParts[i] = fmt.Sprintf("`%s`", val)
	}

	ext := ".crash"
	if strings.HasPrefix(report, `{"app_name":`) {
		ext = ".ips"
	}

	filename := fmt.Sprintf("crash-%s%s", uuid.New().String(), ext)

	if err := lib.UploadFileToS3("codeperfect-crashes", filename, report); err != nil {
		log.Print(err)
		return
	}

	url, err := lib.GetPresignedURL("codeperfect-crashes", filename, time.Hour*24*7)
	if err != nil {
		log.Print(err)
		return
	}

	go SendSlack("New crash report | %s | <%s|View report>", strings.Join(infoParts, " "), url)
}

func PostCrashReport(c *gin.Context) {
	var req models.CrashReportRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{})
		log.Printf("json.unmarshal", err)
		return
	}

	auth, err := authUser(c, req.Version)
	if err != nil {
		log.Print(err) // don't fail
	}

	handleCrashReport(c, auth.user, req.OS, req.Version, req.Content)
	c.JSON(http.StatusOK, true)
}

func PostCrashReportV2(c *gin.Context) {
	var req models.CrashReportRequestV2
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{})
		log.Printf("json.unmarshal", err)
		return
	}

	auth, err := authUser(c, 0)
	if err != nil {
		log.Print(err) // don't fail
	}

	handleCrashReport(c, auth.user, req.OS, auth.version, req.Content)
	c.JSON(http.StatusOK, true)
}

func PostStripeWebhook(c *gin.Context) {
	body, err := io.ReadAll(c.Request.Body)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{})
		log.Printf("ioutil.Readall: %v", err)
		return
	}

	event, err := webhook.ConstructEvent(body, c.Request.Header.Get("Stripe-Signature"), StripeWebhookSecret)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{})
		log.Printf("webhook.ConstructEvent: %v", err)
		return
	}

	eventQueue <- event
}

func stripeEventWorker() {
	for event := range eventQueue {
		processStripeEvent(event)
	}
}

var paymentLinkTypesDev = map[string]string{
	"plink_1N3vNpBpL0Zd3zdOuDCwTfaR": "license_and_sub",
	"plink_1N3vNBBpL0Zd3zdO412WVvjR": "license_and_sub",
	"plink_1N3vMWBpL0Zd3zdOho8Q0oPU": "sub_only",
	"plink_1N3vMOBpL0Zd3zdOHtyvGnlE": "sub_only",
	"plink_1N3v85BpL0Zd3zdOtCHOLlYl": "license_only",
	"plink_1N3v7uBpL0Zd3zdOOaU2UVIq": "sub_only",
	"plink_1N3v7ZBpL0Zd3zdO0JbYZunV": "sub_only",
	"plink_1N3v7DBpL0Zd3zdOWUOCeXKf": "license_only",
	"plink_1N3v1gBpL0Zd3zdOrmBJGL8T": "license_and_sub",
	"plink_1N3v0RBpL0Zd3zdOSifnvyCs": "license_and_sub",
}

var paymentLinkTypesProd = map[string]string{
	"plink_1N3vNpBpL0Zd3zdOuDCwTfaR": "license_and_sub",
	"plink_1N3vNBBpL0Zd3zdO412WVvjR": "license_and_sub",
	"plink_1N3vMWBpL0Zd3zdOho8Q0oPU": "sub_only",
	"plink_1N3vMOBpL0Zd3zdOHtyvGnlE": "sub_only",
	"plink_1N3v85BpL0Zd3zdOtCHOLlYl": "license_only",
	"plink_1N3v7uBpL0Zd3zdOOaU2UVIq": "sub_only",
	"plink_1N3v7ZBpL0Zd3zdO0JbYZunV": "sub_only",
	"plink_1N3v7DBpL0Zd3zdOWUOCeXKf": "license_only",
	"plink_1N3v1gBpL0Zd3zdOrmBJGL8T": "license_and_sub",
	"plink_1N3v0RBpL0Zd3zdOSifnvyCs": "license_and_sub",
}

func getPaymentLinkType(id string) string {
	if IsDevelMode {
		return paymentLinkTypesDev[id]
	}
	return paymentLinkTypesDev[id]
}

func paymentLinkHasLicense(id string) bool {
	return getPaymentLinkType(id) != "sub_only"
}

func sendNewUserEmail(user *models.User) {
	paymentLinkType := getPaymentLinkType(user.StripePaymentLinkID)

	type EmailParams struct {
		Email             string
		LicenseKey        string
		Greeting          string
		BillingPortalLink string
		PaymentLinkType   string
	}

	makeGreeting := func() string {
		if user.Name != "" {
			fields := strings.Fields(user.Name)
			if len(fields) > 0 {
				return fmt.Sprintf("Hi %s,", fields[0])
			}
		}
		return "Hi,"
	}

	params := &EmailParams{
		Email:           user.Email,
		LicenseKey:      user.LicenseKey,
		Greeting:        makeGreeting(),
		PaymentLinkType: paymentLinkType,
	}

	if IsDevelMode {
		params.BillingPortalLink = "https://billing.stripe.com/p/login/test_5kAcNMdzp6encGk4gg"
	} else {
		params.BillingPortalLink = "https://billing.stripe.com/p/login/9AQ5o60VDdYC2Zy144"
	}

	txt, err := lib.ExecuteTemplate(emails.EmailNewUserText, params)
	if err != nil {
		return
	}

	html, err := lib.ExecuteTemplate(emails.EmailNewUserHtml, params)
	if err != nil {
		return
	}

	if err := lib.SendEmail(user.Email, "Your new CodePerfect license", string(txt), string(html)); err != nil {
		log.Printf("failed to send email to %s: %v", user.Email, err)
	}
}

func ensureUser(cus *stripe.Customer) (*models.User, bool, error) {
	var user models.User
	isNew := false

	res := db.DB.First(&user, "stripe_customer_id = ?", cus.ID)
	if res.Error != nil {
		if !errors.Is(res.Error, gorm.ErrRecordNotFound) {
			return nil, false, fmt.Errorf("error looking up user: %v", res.Error)
		}

		isNew = true
		user.LicenseKey = lib.GenerateLicenseKey()
		user.StripeCustomerID = cus.ID
		db.DB.Save(&user)
	}

	changed := false

	if user.Email != cus.Email {
		changed = true
		user.Email = cus.Email
	}

	if user.Name != cus.Name {
		changed = true
		user.Name = cus.Name
	}

	if changed {
		db.DB.Save(&user)
	}

	return &user, isNew, nil
}

func processStripeEvent(event stripe.Event) {
	log.Printf("[EVENT] %s", event.Type)

	switch event.Type {
	case "checkout.session.completed":
		var sess stripe.CheckoutSession
		if err := json.Unmarshal(event.Data.Raw, &sess); err != nil {
			log.Printf("json.Unmarshal: %v", err)
			return
		}

		cus, err := customer.Get(sess.Customer.ID, nil)
		if err != nil {
			log.Printf("customer.Get: %v", err)
			return
		}

		user, _, err := ensureUser(cus)
		if err != nil {
			log.Printf("ensureUser: %v", err)
			return
		}

		plid := sess.PaymentLink.ID

		if user.StripePaymentLinkID != "" && user.StripePaymentLinkID != plid {
			msg := fmt.Sprintf(
				"new payment link on existing user: user id = %v, cus_id = %v, old = %v, new = %v",
				user.ID,
				user.StripeCustomerID,
				user.StripePaymentLinkID,
				plid,
			)
			log.Printf("%s", msg)
			SendSlack("%s", msg)
		}

		user.StripePaymentLinkID = plid
		user.Active = true
		db.DB.Save(&user)

		sendNewUserEmail(user)

	case "customer.subscription.created", "customer.subscription.updated", "customer.subscription.deleted":
		var sub stripe.Subscription
		if err := json.Unmarshal(event.Data.Raw, &sub); err != nil {
			log.Printf("json.Unmarshal: %v", err)
			return
		}

		cus, err := customer.Get(sub.Customer.ID, nil)
		if err != nil {
			log.Printf("customer.Get: %v", err)
			return
		}
		// spew.Dump(cus)

		user, _, err := ensureUser(cus)
		if err != nil {
			log.Printf("ensureUser: %v", err)
			return
		}

		// user should only be able to have one sub at a time
		if user.StripeSubscriptionID != "" && user.StripeSubscriptionID != sub.ID {
			msg := fmt.Sprintf(
				"user subscription id changed: user id = %v, cus_id = %v, old sub = %v, new = %v",
				user.ID,
				user.StripeCustomerID,
				user.StripeSubscriptionID,
				sub.ID,
			)
			log.Printf("%s", msg)
			SendSlack("%s", msg)
		}

		user.StripeSubscriptionID = sub.ID
		user.StripeSubscriptionActive = (sub.Status == stripe.SubscriptionStatusActive)
		db.DB.Save(&user)
	}
}
