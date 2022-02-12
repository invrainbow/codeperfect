package main

import (
	"archive/zip"
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"path"
	"path/filepath"
	"runtime"
	"strings"

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

func pushUnknownError(desc string, err error) {
	if DebugModeFlag {
		log.Printf("%s: %v\n", desc, err)
	}

	msg := fmt.Sprintf(
		"An unexpected error has occurred:\n\n%s: %v\n\nThe program will continue to run, but please report this error to us if possible. Thanks!",
		desc,
		err,
	)
	PushMessage(msg, "Authentication", false)
}

func Update() {
	osSlug := runtime.GOOS
	req := &models.UpdateRequest{
		OS:             osSlug,
		CurrentVersion: versions.CurrentVersion,
	}

	var resp models.UpdateResponse
	if err := CallServer("update", nil, req, &resp); err != nil {
		log.Printf("%s", err)
		return
	}

	if !resp.NeedAutoupdate {
		return
	}

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

/*
folder structure:

<root>/
	launcher*
	bin/
		ide*
		nvim*
		dynamic_gohelper.go
		...
	newbin/
*/
