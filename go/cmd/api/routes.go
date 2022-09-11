package main

import (
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/invrainbow/codeperfect/go/cmd/lib"
	"github.com/invrainbow/codeperfect/go/db"
	"github.com/invrainbow/codeperfect/go/models"
	"github.com/stripe/stripe-go/v72"
	"github.com/stripe/stripe-go/v72/billingportal/session"
	"github.com/stripe/stripe-go/v72/customer"
	"github.com/stripe/stripe-go/v72/webhook"
	"gorm.io/gorm"
)

var IsDevelMode = os.Getenv("DEVELOPMENT_MODE") == "1"
var StripeAPIKey = os.Getenv("STRIPE_API_KEY")
var StripeWebhookSecret = os.Getenv("STRIPE_WEBHOOK_SECRET")

func init() {
	if StripeAPIKey == "" {
		panic("invalid stripe api key")
	}

	if StripeWebhookSecret == "" {
		panic("invalid stripe webhook secret")
	}

	stripe.Key = StripeAPIKey
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

// can maybe refactor this
func authUser(c *gin.Context) (*models.User, error) {
	email := c.GetHeader("X-Email")
	licenseKey := c.GetHeader("X-License-Key")

	var user models.User
	if res := db.DB.First(&user, "email = ?", email); res.Error != nil {
		if errors.Is(res.Error, gorm.ErrRecordNotFound) {
			return nil, nil
		}
		return nil, res.Error
	}

	if user.LicenseKey != licenseKey {
		return nil, nil
	}
	if !user.Active {
		return nil, nil
	}
	return &user, nil
}

func isOSValid(os string) bool {
	// for now only darwin is valid
	return os == "darwin"
}

var SendAlertsForSelf = (os.Getenv("SEND_ALERTS_FOR_SELF") == "1")

func isMyself(user *models.User) bool {
	return user.Email == "bh@codeperfect95.com" || user.Email == "brhs.again@gmail.com"
}

func SendSlackMessageForUser(user *models.User, format string, args ...interface{}) {
	if !SendAlertsForSelf && isMyself(user) {
		return
	}
	SendSlackMessage(format, args...)
}

func PostAuth(c *gin.Context) {
	user, err := authUser(c)
	if err != nil {
		sendServerError(c, err.Error())
	}

	if user == nil {
		c.JSON(http.StatusOK, &models.AuthResponse{Success: false})
		return
	}

	var req models.AuthRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		sendError(c, "Invalid data.")
		return
	}

	SendSlackMessageForUser(user, "%s authed on version %s/%d.", user.Email, req.OS, req.CurrentVersion)

	PosthogCapture(user.ID, "user auth", PosthogProps{
		"os":              req.OS,
		"current_version": req.CurrentVersion,
	})

	sess := models.Session{}
	sess.UserID = user.ID
	sess.StartedAt = time.Now()
	sess.LastHeartbeatAt = sess.StartedAt
	sess.Heartbeats = 1
	db.DB.Create(&sess)

	c.JSON(http.StatusOK, &models.AuthResponse{
		Success:   true,
		SessionID: sess.ID,
	})
}

func PostHeartbeat(c *gin.Context) {
	var req models.HeartbeatRequest
	if err := c.BindJSON(&req); err != nil {
		return
	}

	user, err := authUser(c)
	if err != nil {
		sendServerError(c, err.Error())
		return
	}

	if user == nil {
		sendError(c, "Invalid credentials.")
		return
	}

	var sess models.Session
	res := db.DB.First(&sess, req.SessionID)
	if res.Error != nil {
		sendError(c, "Invalid session ID.")
		return
	}

	if sess.UserID != user.ID {
		sendError(c, "Invalid session ID.")
		return
	}

	PosthogCapture(user.ID, "user heartbeat", nil)

	sess.LastHeartbeatAt = time.Now()
	sess.Heartbeats++ // not safe, i don't care
	db.DB.Save(&sess)

	c.JSON(200, &gin.H{"ok": true})
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

	switch event.Type {
	case "customer.subscription.created":
	case "customer.subscription.updated":
	case "customer.subscription.deleted":
	default:
		return
	}

	log.Printf("[EVENT] %s", event.Type)

	var sub stripe.Subscription
	if err := json.Unmarshal(event.Data.Raw, &sub); err != nil {
		log.Printf("json.Unmarshal: %v", err)
		return
	}

	log.Printf("=== subscription change: %v, %s ===", sub.ID, sub.Status)

	cus, err := customer.Get(sub.Customer.ID, nil)
	if err != nil {
		log.Printf("customer.Get: %v", err)
		return
	}

	// spew.Dump(cus)

	newUser := false
	active := (sub.Status == stripe.SubscriptionStatusActive)

	var user models.User
	res := db.DB.First(&user, "stripe_subscription_id = ?", sub.ID)
	if res.Error != nil {
		if !errors.Is(res.Error, gorm.ErrRecordNotFound) {
			log.Printf("error looking up user: %v", res.Error)
			return
		}

		// new user. if not active, don't bother creating user yet.
		if !active {
			log.Printf("user doesn't exist for %s, status = %s, skipping...", cus.Email, sub.Status)
			return
		}

		newUser = true
		user.Email = cus.Email
		user.LicenseKey = lib.GenerateLicenseKey()
		user.StripeSubscriptionID = sub.ID
	}

	// if email changed, notify us via slack & let ops handle manually
	if user.Email != cus.Email {
		msg := fmt.Sprintf(
			"[slack webhook] customer email changed, old = %s, new = %s",
			user.Email,
			cus.Email,
		)
		log.Printf("%s", msg)
		SendSlackMessage("%s", msg)
		return
	}

	old := user.Active
	user.Active = active
	user.Name = cus.Name
	db.DB.Save(&user)

	if old == user.Active {
		return
	}

	PosthogCapture(user.ID, "user activation status changed", PosthogProps{
		"active": user.Active,
	})

	PosthogIdentify(user.ID, PosthogProps{"active": user.Active})

	type EmailParams struct {
		Email      string
		LicenseKey string
		Greeting   string
		PortalLink string
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

	makePortalLink := func() (string, error) {
		params := &stripe.BillingPortalSessionParams{
			Customer:  stripe.String(cus.ID),
			ReturnURL: stripe.String("https://codeperfect95.com/portal-done"),
		}
		s, err := session.New(params)
		if err != nil {
			return "", err
		}
		return s.URL, nil
	}

	portalLink, err := makePortalLink()
	if err != nil {
		log.Printf("unable to create portal link: %v", err)
		return
	}

	params := &EmailParams{
		Email:      user.Email,
		LicenseKey: user.LicenseKey,
		Greeting:   makeGreeting(),
		PortalLink: portalLink,
	}

	doSendEmail := func(subject, txtTmpl, htmlTmpl string) {
		txt, err := lib.ExecuteTemplate(txtTmpl, params)
		if err != nil {
			return
		}

		html, err := lib.ExecuteTemplate(htmlTmpl, params)
		if err != nil {
			return
		}

        log.Printf("%s", txt)
        log.Printf("%s", html)

        /*
		if err := lib.SendEmail(user.Email, subject, string(txt), string(html)); err != nil {
			log.Printf("failed to send email to %s: %v", user.Email, err)
		}
        */
	}

	if user.Active {
		if newUser {
			doSendEmail("CodePerfect 95: New License", emailUserCreatedTxt, emailUserCreatedHtml)
		} else {
			doSendEmail("CodePerfect 95: License Reactivated", emailUserEnabledTxt, emailUserEnabledHtml)
		}
	} else {
		doSendEmail("CodePerfect 95: License Deactivated", emailUserDisabledTxt, emailUserDisabledHtml)
	}
}
