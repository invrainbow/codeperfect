package main

import (
	"bytes"
	_ "embed"
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"os"
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

func sendError(c *gin.Context, code int) {
	err := &models.ErrorResponse{
		Code:  code,
		Error: models.ErrorMessages[code],
	}
	c.JSON(http.StatusBadRequest, err)
}

func sendServerError(c *gin.Context, format string, args ...interface{}) {
	sendError(c, models.ErrorInternal)
	fmt.Printf("%s\n", fmt.Sprintf(format, args...))
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

//go:embed install.sh
var installScript string

func GetInstall(c *gin.Context) {
	user := authUserByCode(c, c.Query("code"))
	if user == nil {
		return
	}

	SendSlackMessageForUser(user, "%s accessed install script.", user.Email)
	tpl, err := template.New("install").Parse(installScript)
	if err != nil {
		sendServerError(c, "unable to load install script")
		return
	}

	var data struct {
		APIBase string
		Code    string
	}

	data.Code = user.DownloadCode
	data.APIBase = "https://api.codeperfect95.com"
	if IsDevelMode {
		data.APIBase = "http://localhost:8080"
	}

	var buf bytes.Buffer
	if err := tpl.Execute(&buf, data); err != nil {
		sendServerError(c, "unable to generate install script")
		return
	}

	c.Data(http.StatusOK, "text/plain", buf.Bytes())
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
