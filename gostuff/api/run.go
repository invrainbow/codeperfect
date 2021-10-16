package api

import (
	"bytes"
	"errors"
	"fmt"
	"net/http"
	"os"
	"text/template"

	"github.com/gin-contrib/cors"
	"github.com/gin-gonic/gin"
	"github.com/invrainbow/codeperfect/gostuff/db"
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

var installScript = `#!/bin/bash
set -e

if [[ -z "$(which go)" ]]; then
	echo "Please make sure Go 1.13+ is installed."
	exit 1
fi

OS_NAME=darwin
if [ "$(sysctl -n machdep.cpu.brand_string)" = "Apple M1" ]; then
    OS_NAME=darwin_arm
fi

CODEPERFECT_CODE="{{.Code}}"
API_BASE="{{.APIBase}}"

download_codeperfect() {
    TMPDIR=$(mktemp -d)
    pushd "${TMPDIR}"

    DOWNLOAD_URL="${API_BASE}/download?code=${CODEPERFECT_CODE}&os=${OS_NAME}&noredirect=1"
    BINARY_URL="$(curl -s "${DOWNLOAD_URL}")"

    curl -s -o codeperfect.zip "${BINARY_URL}"
    unzip codeperfect.zip
    rm -rf /Applications/CodePerfect.app
    mv CodePerfect.app /Applications

    popd
    rm -rf $TMPDIR
}

download_license() {
    curl -s -o ~/.cplicense "${API_BASE}/license?code=${CODEPERFECT_CODE}"
}

create_config() {
    echo "{" > ~/.cpconfig
    echo "  \\"go_binary_path\\": \\"$(which go)\\"," >> ~/.cpconfig
    echo "  \\"goroot\\": \\"$(go env GOROOT)\\"," >> ~/.cpconfig
    echo "  \\"gomodcache\\": \\"$(go env GOMODCACHE)\\"" >> ~/.cpconfig
    echo "}" >> ~/.cpconfig
}

echo -n "Downloading CodePerfect..."
download_codeperfect > /dev/null 2>&1
echo " done!"

echo -n "Downloading license..."
download_license > /dev/null 2>&1
echo " done!"

echo -n "Configuring..."
create_config > /dev/null 2>&1
echo " done!"

echo ""
echo "CodePerfect.app is available to use in your Applications folder."
`

func Run() {
	r := gin.Default()
	r.Use(cors.Default())

	r.GET("/install", func(c *gin.Context) {
		user := authUserByCode(c, c.Query("code"))
		if user == nil {
			return
		}

		SendSlackMessage("%s accessed install script.", user.Email)

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
	})

	r.GET("/download", func(c *gin.Context) {
		user := authUserByCode(c, c.Query("code"))
		if user == nil {
			return
		}

		SendSlackMessage("%s downloaded version `%s`.", user.Email, c.Query("os"))

		LogEvent(int(user.ID), &AmplitudeEvent{
			EventType:      "user_download",
			UserProperties: user,
		})

		presignedUrl := MustGetDownloadLink(c, c.Query("os"))
		if presignedUrl == "" {
			return
		}

		c.Data(http.StatusOK, "text/plain", []byte(presignedUrl))
	})

	r.POST("/auth-web", func(c *gin.Context) {
		var req models.AuthWebRequest
		if c.ShouldBindJSON(&req) != nil {
			sendError(c, models.ErrorInvalidData)
			return
		}

		user := authUserByCode(c, req.Code)
		if user == nil {
			return
		}

		SendSlackMessage("%s opened the download page.", user.Email)

		LogEvent(int(user.ID), &AmplitudeEvent{
			EventType:       "user_web_auth",
			EventProperties: req,
			UserProperties:  user,
		})

		c.JSON(http.StatusOK, &models.AuthWebResponse{
			Email:      user.Email,
			LicenseKey: user.LicenseKey,
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

		SendSlackMessage("%s authed.", user.Email)

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
			resp.DownloadHash = versions.VersionUpdateHashes[req.OS]
		}

		c.JSON(http.StatusOK, resp)
	})

	r.Run()
}
