package helper

import (
	"archive/zip"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net"
	"net/http"
	"os"
	"path"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"syscall"
	"time"

	"github.com/invrainbow/codeperfect/gostuff/models"
	"github.com/invrainbow/codeperfect/gostuff/versions"
)

type License struct {
	Email      string `json:"email"`
	LicenseKey string `json:"key"`
}

func DownloadFile(url string, f *os.File) error {
	resp, err := http.Get(url)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	n, err := io.Copy(f, resp.Body)
	if err != nil {
		return err
	}
	if n == 0 {
		return fmt.Errorf("failed to copy anything")
	}
	return nil
}

func ReadLicense() *License {
	homedir, err := os.UserHomeDir()
	if err != nil {
		return nil
	}

	fmt.Printf("homedir: %s\n", homedir)

	licensefile := path.Join(homedir, ".cplicense")
	fmt.Printf("licensefile: %s\n", licensefile)
	buf, err := os.ReadFile(licensefile)
	if err != nil {
		return nil
	}

	var license License
	if err := json.Unmarshal(buf, &license); err != nil {
		return nil
	}

	return &license
}

/*
on startup:
    read license file
    if doesn't exist
        die("pls put license file in ~/.cplicense")

    auth_fail = false
    if offline
        auth_fail = true
    else
        auth_fail = auth against server

    if auth_fail
        check time of last auth
        if too long ago
            die("you exceeded the grace period")
        else
            if offline
                msg("unable to auth, you have 3 day grace period")
            else
                msg("invalid license key") // should we have grace period?

    look at needs_autoupdate from server auth
    if needs_autoupdate
        if not (.zip file already exists locally and hash matches)
            download again
        if not (tmpfolder already exists)
            unzip into ${tmpfolder}_tmp
            mv ${tmpfolder}_tmp ${tmpfolder}
*/

func GetFileSHAHash(filename string) (string, error) {
	f, err := os.Open(filename)
	if err != nil {
		return "", err
	}
	defer f.Close()

	h := sha256.New()
	if _, err := io.Copy(h, f); err != nil {
		log.Fatal(err)
	}

	return hex.EncodeToString(h.Sum(nil)), nil
}

// Unzip will decompress a zip archive, moving all files and folders
// within the zip file (parameter 1) to an output directory (parameter 2).
//
// Taken from here: https://golangcode.com/unzip-files-in-go/
func Unzip(src string, dest string) error {
	r, err := zip.OpenReader(src)
	if err != nil {
		return err
	}
	defer r.Close()

	copyFile := func(f *zip.File) error {
		fpath := filepath.Join(dest, f.Name)

		// check for ZipSlip: http://bit.ly/2MsjAWE
		if !strings.HasPrefix(fpath, filepath.Clean(dest)+string(os.PathSeparator)) {
			return fmt.Errorf("%s: illegal file path", fpath)
		}

		if f.FileInfo().IsDir() {
			return os.MkdirAll(fpath, os.ModePerm)
		}

		if err := os.MkdirAll(filepath.Dir(fpath), os.ModePerm); err != nil {
			return err
		}

		outfile, err := os.OpenFile(fpath, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, f.Mode())
		if err != nil {
			return err
		}
		defer outfile.Close()

		infile, err := f.Open()
		if err != nil {
			return err
		}
		defer infile.Close()

		if _, err = io.Copy(outfile, infile); err != nil {
			return err
		}

		return nil
	}

	for _, f := range r.File {
		if err := copyFile(f); err != nil {
			return err
		}
	}
	return nil
}

type AuthUpdateError struct {
	RequiresExit bool
	Message      string
}

func NewAuthUpdateError(exit bool, format string, args ...interface{}) *AuthUpdateError {
	return &AuthUpdateError{
		RequiresExit: exit,
		Message:      fmt.Sprintf(format, args...),
	}
}

const MessageInternetOffline = `
It appears your internet is offline, or for some reason we're not able to connect to the server to authenticate your license key.

The program will continue to run for a grace period of a week -- please just rerun CodePerfect at some point with an internet connection. Thanks!
`

const MessageTrialEnded = `
Your trial has ended. Please contact support@codeperfect95.com to activate your subscription.

The program will continue to run for a grace period of 3 days.
`

const MessageInvalidCreds = `
We were unable to authenticate your credentials. Please contact support@codeperfect95.com if you believe this was in error.

The program will continue to run for a grace period of 24 hours.
`

func pushPanic(msg string) {
    PushMessage(msg, "Authentication Error", true)
}

func pushWarning(msg string) {
	PushMessage(msg, "Authentication", false)
}

func pushUnknownError(desc string, err error) {
	if DebugModeFlag {
		fmt.Printf("%v\n", err)
	}
	pushWarning(
		fmt.Sprintf(
			"%s: An unexpected error has occurred:\n\n%v\n\nThe program will continue to run, but please report this error to us if possible. Thanks!",
			desc,
			err,
		),
	)
}

// tolsa = "time of last successful auth"
func getTolsaPath() (string, error) {
	configdir, err := os.UserConfigDir()
	if err != nil {
		return "", err
	}

	appdir := path.Join(configdir, "CodePerfect")
	if err := os.MkdirAll(appdir, os.ModePerm); err != nil {
		return "", err
	}

	return path.Join(appdir, ".tolsa"), nil
}

// don't return error, we don't care
func writeTime(filepath string, t time.Time) {
	timestr := strconv.FormatInt(t.Unix(), 10)
	os.WriteFile(filepath, []byte(timestr), os.ModePerm)
}

func handleGracePeriod(message string, days int) {
	pushWarning(message)

	getLastSuccessfulAuthTime := func() (time.Time, error) {
		zero := time.Time{}

		timepath, err := getTolsaPath()
		if err != nil {
			return zero, err
		}

		content, err := os.ReadFile(timepath)
		if err != nil {
			if os.IsNotExist(err) {
				now := time.Now()
				writeTime(timepath, now)
				return now, nil
			}
			return zero, err
		}

		n, err := strconv.ParseInt(string(content), 10, 64)
		if err != nil {
			return zero, err
		}

		return time.Unix(n, 0), nil
	}

	isGracePeriodStillGood := func() bool {
		lastTime, err := getLastSuccessfulAuthTime()
		if err != nil {
			// surface err?
			return false
		}
		if time.Since(lastTime) > time.Hour*time.Duration(24*days) {
			return false
		}
		return true
	}

	if !isGracePeriodStillGood() {
		pushPanic("The grace period has ended. Please contact support@codeperfect95.com to renew your subscription.")
	}
}

func AuthAndUpdate() {
	license := ReadLicense()
	if license == nil {
		pushPanic("Unable to read license. Please place your license file at ~/.cplicense.")
		return
	}

	req := &models.AuthRequest{
		OS:             runtime.GOOS,
		CurrentVersion: versions.CurrentVersion,
	}

	var resp models.AuthResponse
	if err := CallServer("auth", license, req, &resp); err != nil {
		switch e := err.(type) {
		case net.Error, *net.OpError, syscall.Errno:
			handleGracePeriod(MessageInternetOffline, 7)
			return
		case *ServerError:
			switch e.Code {
			case models.ErrorUserNoLongerActive:
				handleGracePeriod(MessageTrialEnded, 3)
				return
			case models.ErrorEmailNotFound, models.ErrorInvalidLicenseKey:
				handleGracePeriod(MessageInvalidCreds, 1)
				return
			}
		}
		pushUnknownError("auth", err)
		return
	}

	// after a successful auth call, update tolsa
	timepath, err := getTolsaPath()
	if err != nil {
		pushUnknownError("time", err)
	}
	writeTime(timepath, time.Now())

	if resp.NeedAutoupdate {
		tmpfile, err := os.CreateTemp("", "update.zip")
		if err != nil {
			pushUnknownError("CreateTemp", err)
			return
		}
		defer os.Remove(tmpfile.Name())

		if err := DownloadFile(resp.DownloadURL, tmpfile); err != nil {
			pushUnknownError("DownloadFile", err)
			return
		}

		hash, err := GetFileSHAHash(tmpfile.Name())
		if err != nil {
			pushUnknownError("SHA", err)
			return
		}
		if hash != resp.DownloadHash {
			pushUnknownError("hash mismatch", fmt.Errorf("got %s, expected %s", hash, resp.DownloadHash))
			return
		}

		var exepath string
		if IsTestMode() {
			exepath = "/Users/bh/ide/gostuff/helper/autoupdate.go"
		} else {
			path, err := os.Executable()
			if err != nil {
				pushUnknownError("executable", err)
				return
			}
			exepath = path
		}

		// TODO:
		// destroy newbintmp if it exists
		// unzip to newbintmp instead
		// move newbintmp to newbin

		if err := Unzip(tmpfile.Name(), path.Join(path.Dir(path.Dir(exepath)), "newbin")); err != nil {
			pushUnknownError("unzip", err)
			return
		}
	}

	return
}

/*
folder structure:

<root>/
	launcher*
	bin/
		ide*
		nvim*
		gohelper.dylib       # includes autoupdate code
		dynamic_gohelper.go
		...
	newbin/
*/
