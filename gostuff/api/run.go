package api

import (
	"errors"
	"fmt"
	"net/http"
	"os"

	"github.com/davecgh/go-spew/spew"
	"github.com/gin-gonic/gin"
	"github.com/invrainbow/ide/gostuff/models"
	"github.com/invrainbow/ide/gostuff/versions"
	"gorm.io/gorm"
)

var AdminPassword = os.Getenv("ADMIN_PASSWORD")

func sendError(c *gin.Context, format string, args ...interface{}) {
	err := &models.ErrorResponse{
		Error: fmt.Sprintf(format, args...),
	}
	c.JSON(http.StatusBadRequest, err)
}

func sendServerError(c *gin.Context, format string, args ...interface{}) {
	sendError(c, "Internal server error.")
	fmt.Printf("%s\n", fmt.Sprintf(format, args...))
}

func authUser(c *gin.Context, email, licenseKey string) *models.User {
	var user models.User
	if res := db.First(&user, "email = ?", email); res.Error != nil {
		if errors.Is(res.Error, gorm.ErrRecordNotFound) {
			sendError(c, "Email not found.")
		} else {
			sendServerError(c, "error while grabbing user: %v", res.Error)
		}
		return nil
	}

	if !user.IsActive {
		sendError(c, "User is no longer active.")
		return nil
	}

	if user.LicenseKey != licenseKey {
		sendError(c, "Invalid license key.")
		return nil
	}

	return &user
}

func Run() {
	r := gin.Default()

	r.POST("/auth", func(c *gin.Context) {
		var req models.AuthRequest
		if c.ShouldBindJSON(&req) != nil {
			sendError(c, "Invalid data.")
			return
		}

		user := authUser(c, c.GetHeader("X-Email"), c.GetHeader("X-License-Key"))
		if user == nil {
			return
		}

		if !versions.ValidOSes[req.OS] {
			sendError(c, "Invalid OS.")
			return
		}
		if req.CurrentVersion > versions.CurrentVersion {
			sendError(c, "Invalid version.")
			return
		}

		resp := &models.AuthResponse{
			Version:        versions.CurrentVersion,
			NeedAutoupdate: req.CurrentVersion < versions.CurrentVersion,
		}

		if resp.NeedAutoupdate {
			filename := fmt.Sprintf("%v_v%v.zip", req.OS, versions.CurrentVersion)
			spew.Dump(filename)
			presignedUrl, err := GetPresignedURL("codeperfect95", filename)
			spew.Dump(presignedUrl)
			if err != nil {
				sendServerError(c, "error while creating presigned url: %v", err)
				return
			}

			resp.DownloadURL = presignedUrl
			resp.DownloadHash = versions.VersionHashes[versions.CurrentVersion]
		}

		c.JSON(http.StatusOK, resp)
	})

	r.Run()
}
