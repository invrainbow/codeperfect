package main

import (
	"bytes"
	_ "embed"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"strings"
	"text/template"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/invrainbow/codeperfect/gostuff/db"
	"github.com/invrainbow/codeperfect/gostuff/helper"
	"github.com/invrainbow/codeperfect/gostuff/models"
	"github.com/invrainbow/codeperfect/gostuff/versions"
	"gorm.io/gorm"
)

var AdminPassword = os.Getenv("ADMIN_PASSWORD")
var IsDevelMode = os.Getenv("DEVELOPMENT_MODE") == "1"
var AirtableAPIKey = os.Getenv("AIRTABLE_API_KEY")
var BetaPaymentLink = "https://buy.stripe.com/28ofZb9Dl0RO9a05kr"
var ConvertKitAPIKey = os.Getenv("CONVERTKIT_API_KEY")

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

func sendError(c *gin.Context, code int) {
	err := &models.ErrorResponse{
		Code:  code,
		Error: models.ErrorMessages[code],
	}
	c.JSON(http.StatusBadRequest, err)
}

func sendServerError(c *gin.Context, format string, args ...interface{}) {
	sendError(c, models.ErrorInternal)
	log.Printf("%s\n", fmt.Sprintf(format, args...))
}

const TrialPeriod = time.Hour * 24 * 7

// can maybe refactor this
func authUser(c *gin.Context) *models.User {
	email := c.GetHeader("X-Email")
	licenseKey := c.GetHeader("X-License-Key")

	var user models.User
	if res := db.DB.First(&user, "email = ?", email); res.Error != nil {
		if errors.Is(res.Error, gorm.ErrRecordNotFound) {
			sendError(c, models.ErrorEmailNotFound)
		} else {
			sendServerError(c, "error while grabbing user: %v", res.Error)
		}
		return nil
	}

	if user.LicenseKey != licenseKey {
		sendError(c, models.ErrorInvalidLicenseKey)
		return nil
	}

	if !user.Active {
		sendError(c, models.ErrorUserNoLongerActive)
		return nil
	}

	return &user
}

var validOSes = map[string]bool{
	"darwin":     true,
	"darwin_arm": true,
}

func MustGetDownloadLink(c *gin.Context, os, folder string) string {
	if !validOSes[os] {
		sendError(c, models.ErrorInvalidOS)
		return ""
	}

	filename := fmt.Sprintf("%s/%v_v%v.zip", folder, os, versions.CurrentVersion)

	/*
		presignedUrl, err := GetPresignedURL("codeperfect95", filename)
		if err != nil {
			sendServerError(c, "error while creating presigned url: %v", err)
			return ""
		}
		return presignedUrl
	*/
	baseUrl := "https://codeperfect95.s3.us-east-2.amazonaws.com"
	return fmt.Sprintf("%s/%s", baseUrl, filename)
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

func executeTemplate(text string, params interface{}) ([]byte, error) {
	tpl, err := template.New("install").Parse(text)
	if err != nil {
		return nil, err
	}

	var buf bytes.Buffer
	if err := tpl.Execute(&buf, params); err != nil {
		return nil, err
	}

	return buf.Bytes(), nil
}

func PostUpdate(c *gin.Context) {
	var req models.UpdateRequest
	if c.ShouldBindJSON(&req) != nil {
		sendError(c, models.ErrorInvalidData)
		return
	}

	resp := &models.UpdateResponse{
		Version:        versions.CurrentVersion,
		NeedAutoupdate: req.CurrentVersion < versions.CurrentVersion,
	}

	if resp.NeedAutoupdate {
		url := MustGetDownloadLink(c, req.OS, "update")
		if url == "" {
			return
		}

		var versionObj models.Version
		res := db.DB.Where("version = ? AND os = ?", versions.CurrentVersion, req.OS).First(&versionObj)
		if res.Error != nil {
			sendServerError(c, "find version: %v", res.Error)
			return
		}

		resp.DownloadURL = url
		resp.DownloadHash = versionObj.UpdateHash
	}

}

func PostAuth(c *gin.Context) {
	var req models.AuthRequest
	if c.ShouldBindJSON(&req) != nil {
		sendError(c, models.ErrorInvalidData)
		return
	}

	user := authUser(c)
	if user == nil {
		return
	}

	SendSlackMessageForUser(user, "%s authed on version %s/%d.", user.Email, req.OS, req.CurrentVersion)

	LogEvent(int(user.ID), &AmplitudeEvent{
		EventType:       "user_auth",
		EventProperties: req,
		UserProperties:  user,
	})

	sess := models.Session{}
	sess.UserID = user.ID
	sess.StartedAt = time.Now()
	sess.LastHeartbeatAt = sess.StartedAt
	sess.Heartbeats = 1
	db.DB.Create(&sess)

	if req.CurrentVersion > versions.CurrentVersion {
		sendError(c, models.ErrorInvalidVersion)
		return
	}

	c.JSON(http.StatusOK, &gin.H{"ok": true})
}

func PostHeartbeat(c *gin.Context) {
	var req models.HeartbeatRequest
	if err := c.BindJSON(&req); err != nil {
		return
	}

	user := authUser(c)
	if user == nil {
		return
	}

	var sess models.Session
	res := db.DB.First(&sess, req.SessionID)
	if res.Error != nil {
		sendError(c, models.ErrorInvalidSession)
		return
	}

	sess.LastHeartbeatAt = time.Now()
	sess.Heartbeats++ // not safe, i don't care
	db.DB.Save(&sess)

	c.JSON(200, &models.HeartbeatResponse{Ok: true})
}

func NewPostJSONRequest(url string, body interface{}) (*http.Request, error) {
	dat, err := json.Marshal(body)
	if err != nil {
		return nil, err
	}
	req, err := http.NewRequest("POST", url, bytes.NewReader(dat))
	if err != nil {
		return nil, err
	}
	req.Header.Add("Content-Type", "application/json")
	return req, nil
}

func AddSignupToConvertKit(input *BetaSignupRequest) error {
	data := &gin.H{
		"api_key":    ConvertKitAPIKey,
		"email":      input.Email,
		"first_name": getFirstNameFromName(input.Name),
	}

	url := "https://api.convertkit.com/v3/tags/2785111/subscribe"
	req, err := NewPostJSONRequest(url, data)
	if err != nil {
		return err
	}

	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	if resp.StatusCode != 200 {
		buf, err := io.ReadAll(resp.Body)
		if err != nil {
			return err
		}
		return fmt.Errorf("bad response from convertkit: %s", string(buf))
	}
	return nil
}

//go:embed signup_email.txt
var SignupEmailText string

//go:embed signup_email.html
var SignupEmailHTML string

func getFirstNameFromName(name string) string {
	parts := strings.Fields(name)
	if len(parts) == 0 {
		return ""
	}
	return parts[0]
}

func getGreetingFromName(name string) string {
	firstName := getFirstNameFromName(name)
	if firstName == "" {
		return "Hi"
	}
	return fmt.Sprintf("Hi %s", firstName)
}

func SendBetaSignupEmail(name, email string) error {
	type Data struct {
		Greeting    string
		PaymentLink string
	}

	data := &Data{
		Greeting:    getGreetingFromName(name),
		PaymentLink: BetaPaymentLink,
	}

	emailText, err := executeTemplate(SignupEmailText, data)
	if err != nil {
		return err
	}

	emailHTML, err := executeTemplate(SignupEmailHTML, data)
	if err != nil {
		return err
	}

	return SendEmail(
		email,
		"CodePerfect Beta - Onboarding",
		string(emailText),
		string(emailHTML),
	)
}
