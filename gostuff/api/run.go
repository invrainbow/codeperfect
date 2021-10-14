package api

import (
	"errors"
	"fmt"
	"net/http"
	"os"

	"github.com/gin-contrib/cors"
	"github.com/gin-gonic/gin"
	"github.com/invrainbow/codeperfect/gostuff/db"
	"github.com/invrainbow/codeperfect/gostuff/models"
	"github.com/invrainbow/codeperfect/gostuff/versions"
	"gorm.io/gorm"
)

var AdminPassword = os.Getenv("ADMIN_PASSWORD")

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

func authUser(c *gin.Context, email, licenseKey string) *models.User {
	var user models.User
	if res := db.Db.First(&user, "email = ?", email); res.Error != nil {
		if errors.Is(res.Error, gorm.ErrRecordNotFound) {
			sendError(c, models.ErrorEmailNotFound)
		} else {
			sendServerError(c, "error while grabbing user: %v", res.Error)
		}
		return nil
	}

	if !user.IsActive {
		sendError(c, models.ErrorUserNoLongerActive)
		return nil
	}

	if user.LicenseKey != licenseKey {
		sendError(c, models.ErrorInvalidLicenseKey)
		return nil
	}

	return &user
}

func authUserByCode(c *gin.Context, code string) *models.User {
	var user models.User
	if res := db.Db.First(&user, "download_code = ?", code); res.Error != nil {
		sendError(c, models.ErrorInvalidDownloadCode)
		return nil
	}

	if !user.IsActive {
		sendError(c, models.ErrorUserNoLongerActive)
		return nil
	}

	return &user
}

func MustGetDownloadLink(c *gin.Context, os string) string {
	if !versions.ValidOSes[os] {
		sendError(c, models.ErrorInvalidOS)
		return ""
	}

	filename := fmt.Sprintf("app/%v_v%v.zip", os, versions.CurrentVersion)
	presignedUrl, err := GetPresignedURL("codeperfect95", filename)
	if err != nil {
		sendServerError(c, "error while creating presigned url: %v", err)
		return ""
	}

	return presignedUrl
}

func Run() {
	r := gin.Default()
	r.Use(cors.Default())

	r.POST("/download", func(c *gin.Context) {
		var req models.DownloadRequest
		if c.ShouldBindJSON(&req) != nil {
			sendError(c, models.ErrorInvalidData)
			return
		}

		user := authUserByCode(c, req.Code)
		if user == nil {
			return
		}

		LogEvent(int(user.ID), &AmplitudeEvent{
			EventType:       "user_download",
			EventProperties: req,
			UserProperties:  user,
		})

		presignedUrl := MustGetDownloadLink(c, req.OS)
		if presignedUrl == "" {
			return
		}

		c.JSON(http.StatusOK, &models.DownloadResponse{
			URL: presignedUrl,
		})
	})

	r.POST("/auth", func(c *gin.Context) {
		var req models.AuthRequest
		if c.ShouldBindJSON(&req) != nil {
			sendError(c, models.ErrorInvalidData)
			return
		}

		user := authUser(c, c.GetHeader("X-Email"), c.GetHeader("X-License-Key"))
		if user == nil {
			return
		}

		LogEvent(int(user.ID), &AmplitudeEvent{
			EventType:       "user_auth",
			EventProperties: req,
			UserProperties:  user,
		})

		if req.CurrentVersion > versions.CurrentVersion {
			sendError(c, models.ErrorInvalidVersion)
			return
		}

		resp := &models.AuthResponse{
			Version:        versions.CurrentVersion,
			NeedAutoupdate: req.CurrentVersion < versions.CurrentVersion,
		}

		if resp.NeedAutoupdate {
			presignedUrl := MustGetDownloadLink(c, req.OS)
			if presignedUrl == "" {
				return
			}

			resp.DownloadURL = presignedUrl
			resp.DownloadHash = versions.VersionUpdateHashes[req.OS][versions.CurrentVersion]
		}

		c.JSON(http.StatusOK, resp)
	})

	r.Run()
}
