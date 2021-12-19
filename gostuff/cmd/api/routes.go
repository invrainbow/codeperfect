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

func authUserByStatus(user *models.User) (bool, int) {
	switch user.Status {
	case models.UserStatusTrialWaiting:
		// start the user's trial
		user.Status = models.UserStatusTrial
		user.TrialStartedAt = time.Now()
		db.DB.Save(&user)

	case models.UserStatusTrial:
		if time.Since(user.TrialStartedAt) > TrialPeriod {
			return false, models.ErrorTrialExpired
		}

	case models.UserStatusInactive:
		return false, models.ErrorUserNoLongerActive
	}

	return true, 0
}

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

	ok, errCode := authUserByStatus(&user)
	if !ok {
		sendError(c, errCode)
		return nil
	}

	return &user
}

func authUserByCode(c *gin.Context, code string) *models.User {
	var user models.User
	if res := db.DB.First(&user, "download_code = ?", code); res.Error != nil {
		sendError(c, models.ErrorInvalidDownloadCode)
		return nil
	}

	ok, errCode := authUserByStatus(&user)
	if !ok {
		sendError(c, errCode)
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
	presignedUrl, err := GetPresignedURL("codeperfect95", filename)
	if err != nil {
		sendServerError(c, "error while creating presigned url: %v", err)
		return ""
	}

	return presignedUrl
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

//go:embed install.sh
var installScript string

func GetInstall(c *gin.Context) {
	user := authUserByCode(c, c.Query("code"))
	if user == nil {
		return
	}

	SendSlackMessageForUser(user, "%s accessed install script.", user.Email)

	type Data struct {
		APIBase string
		Code    string
	}

	tpl, err := executeTemplate(installScript, &Data{
		Code:    user.DownloadCode,
		APIBase: GetAPIBase(),
	})
	if err != nil {
		sendServerError(c, "unable to load install script")
		return
	}

	c.Data(http.StatusOK, "text/plain", tpl)
}

func GetLicense(c *gin.Context) {
	user := authUserByCode(c, c.Query("code"))
	if user == nil {
		return
	}

	SendSlackMessageForUser(user, "%s downloaded their license.", user.Email)
	license := &helper.License{
		Email:      user.Email,
		LicenseKey: user.LicenseKey,
	}
	data, err := json.MarshalIndent(license, "", "  ")
	if err != nil {
		sendServerError(c, "unable to return license")
	}
	c.Data(http.StatusOK, "application/json", data)
}

func GetDownload(c *gin.Context) {
	user := authUserByCode(c, c.Query("code"))
	if user == nil {
		return
	}

	SendSlackMessageForUser(user, "%s downloaded version `%s`.", user.Email, c.Query("os"))

	LogEvent(int(user.ID), &AmplitudeEvent{
		EventType:      "user_download",
		UserProperties: user,
	})

	presignedUrl := MustGetDownloadLink(c, c.Query("os"), "app")
	if presignedUrl == "" {
		return
	}

	c.Data(http.StatusOK, "text/plain", []byte(presignedUrl))
}

func PostAuthWeb(c *gin.Context) {
	var req struct {
		Code string `json:"code"`
	}

	if c.ShouldBindJSON(&req) != nil {
		sendError(c, models.ErrorInvalidData)
		return
	}

	user := authUserByCode(c, req.Code)
	if user == nil {
		return
	}

	SendSlackMessageForUser(user, "%s opened the download page.", user.Email)

	LogEvent(int(user.ID), &AmplitudeEvent{
		EventType:       "user_web_auth",
		EventProperties: req,
		UserProperties:  user,
	})

	type AuthWebResponse struct {
		Email      string `json:"email"`
		LicenseKey string `json:"license_key"`
	}

	c.JSON(http.StatusOK, &AuthWebResponse{
		Email:      user.Email,
		LicenseKey: user.LicenseKey,
	})
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

	if req.CurrentVersion > versions.CurrentVersion {
		sendError(c, models.ErrorInvalidVersion)
		return
	}

	sess := models.Session{}
	sess.UserID = user.ID
	sess.StartedAt = time.Now()
	sess.LastHeartbeatAt = sess.StartedAt
	sess.Heartbeats = 1
	db.DB.Create(&sess)

	resp := &models.AuthResponse{
		Version:        versions.CurrentVersion,
		NeedAutoupdate: req.CurrentVersion < versions.CurrentVersion,
		SessionID:      sess.ID,
	}

	if resp.NeedAutoupdate {
		presignedUrl := MustGetDownloadLink(c, req.OS, "update")
		if presignedUrl == "" {
			return
		}

		var versionObj models.Version
		res := db.DB.Where("version = ? AND os = ?", versions.CurrentVersion, req.OS).First(&versionObj)
		if res.Error != nil {
			sendServerError(c, "find version: %v", res.Error)
			return
		}

		resp.DownloadURL = presignedUrl
		resp.DownloadHash = versionObj.UpdateHash
	}

	c.JSON(http.StatusOK, resp)
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

type BetaSignupRequest struct {
	Name   string   `json:"name"`
	Email  string   `json:"email"`
	OS     []string `json:"os"`
	Editor []string `json:"editor"`
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

func AddSignupToAirtable(input *BetaSignupRequest) error {
	osLookup := map[string]string{
		"windows": "Windows",
		"mac":     "macOS",
		"linux":   "Linux",
	}

	editorLookup := map[string]string{
		"vscode":      "VSCode",
		"goland":      "GoLand",
		"other":       "Other",
		"text_editor": "A text editor (Vim, Sublime, etc.)",
	}

	newOS := []string{}
	for _, os := range input.OS {
		val, ok := osLookup[os]
		if !ok {
			return fmt.Errorf("invalid os")
		}
		newOS = append(newOS, val)
	}

	newEditor := []string{}
	for _, editor := range input.Editor {
		val, ok := editorLookup[editor]
		if !ok {
			return fmt.Errorf("invalid editor")
		}
		newEditor = append(newEditor, val)
	}

	data := gin.H{
		"records": []gin.H{
			gin.H{
				"fields": gin.H{
					"Name":             input.Name,
					"Email":            input.Email,
					"Operating System": newOS,
					"Currently Using":  newEditor,
				},
			},
		},
	}

	req, err := NewPostJSONRequest("https://api.airtable.com/v0/appjrtpjRjl0HV5EY/Signups", data)
	if err != nil {
		return err
	}

	req.Header.Add("Authorization", fmt.Sprintf("Bearer %s", AirtableAPIKey))

	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	buf, err := io.ReadAll(resp.Body)
	if err != nil {
		log.Printf("couldn't read response from airtable: %v", err)
		return err
	}
	log.Printf("%s", string(buf))
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

func SendSignupToSlack(req *BetaSignupRequest) {
	osList := []string{}
	editorList := []string{}

	for _, it := range req.OS {
		osList = append(osList, fmt.Sprintf("`%s`", it))
	}
	for _, it := range req.Editor {
		editorList = append(editorList, fmt.Sprintf("`%s`", it))
	}

	os := "none"
	if len(osList) > 0 {
		os = strings.Join(osList, ", ")
	}

	editor := "none"
	if len(editorList) > 0 {
		editor = strings.Join(editorList, ", ")
	}

	SendSlackMessage(
		"New signup:\n> *%s*\n> Email: %s\n> OS: %s\n> Editor: %s",
		req.Name,
		req.Email,
		os,
		editor,
	)
}

func PostBetaSignup(c *gin.Context) {
	var req BetaSignupRequest
	if err := c.BindJSON(&req); err != nil {
		return
	}

	SendSignupToSlack(&req)

	if err := AddSignupToAirtable(&req); err != nil {
		SendSlackMessage("couldn't add to airtable:\n```%v```", err)
		log.Println(err)
	}

	if err := AddSignupToConvertKit(&req); err != nil {
		SendSlackMessage("couldn't add to convertkit:\n```%v```", err)
		log.Println(err)
	}

	isMacOS := func() bool {
		for _, os := range req.OS {
			if os == "mac" {
				return true
			}
		}
		return false
	}

	nextStage := "not_supported"
	if isMacOS() {
		nextStage = "supported"
		if err := SendBetaSignupEmail(req.Name, req.Email); err != nil {
			log.Print(err)
		}
	}

	c.JSON(200, &gin.H{"next_stage": nextStage})
}
