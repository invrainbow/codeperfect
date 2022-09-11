package main

import (
	"archive/zip"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"runtime"
	"strings"

	"github.com/invrainbow/codeperfect/go/utils"
	"github.com/invrainbow/codeperfect/go/versions"
)

func isDir(fullpath string) bool {
	info, err := os.Stat(fullpath)
	return err == nil && info.IsDir()
}

/*
This checks if:

 - newbin exists
 - newbin is a directory
 - bin either doesn't exist, or is a directory

iff this is true, delete bin and mv newbin bin.
*/

func replaceBinFolder(basedir string) error {
	oldpath := filepath.Join(basedir, "newbin")
	newpath := filepath.Join(basedir, "bin")
	return replaceFolder(oldpath, newpath)
}

func replaceFolder(oldpath, newpath string) error {
	deletemePath := filepath.Join(filepath.Dir(newpath), "DELETEME")

	info, err := os.Stat(oldpath)
	if err != nil {
		if os.IsNotExist(err) {
			// oldpath doesn't exist, do nothing.
			return nil
		}
		return err
	} else if !info.IsDir() {
		// oldpath isn't a directory, fail condition.
		return nil
	}

	deleteme := false

	// check newpath
	//  - if it doesn't exist, keep going
	//  - if it's a directory, move it to ./DELETEME
	//  - if it's not a directory, fail condition
	info, err = os.Stat(newpath)
	if err != nil {
		if !os.IsNotExist(err) {
			return err
		}
	} else if !info.IsDir() {
		// newpath exists but is not a directory, fail condition.
		return nil
	} else {
		// newpath is a directory
		if err := os.Rename(newpath, deletemePath); err != nil {
			return err
		}
		deleteme = true
	}

	if err := os.Rename(oldpath, newpath); err != nil {
		if deleteme {
			// restore newpath
			os.Rename(deletemePath, newpath)
		}
		return err
	}

	if deleteme {
		// even if it fails, the move succeeded, we just have this extra directory here. let it pass
		os.RemoveAll(deletemePath)
	}

	return nil
}

var CodePerfectS3Url = "https://codeperfect95.s3.us-east-2.amazonaws.com"

func makeS3Url(endpoint string) string {
	return fmt.Sprintf("%s/%s", CodePerfectS3Url, endpoint)
}

func downloadFile(url string, f *os.File) error {
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

func getLatestVersion() (int, error) {
	resp, err := http.Get(makeS3Url("meta.json"))
	if err != nil {
		return 0, err
	}
	defer resp.Body.Close()

	data, err := io.ReadAll(resp.Body)
	if err != nil {
		return 0, err
	}

	var meta struct {
		CurrentVersion int `json:"current_version"`
	}

	if err := json.Unmarshal(data, &meta); err != nil {
		return 0, err
	}

	return meta.CurrentVersion, nil
}

func getOS() string {
	archvals := map[string]string{
		"amd64": "x64",
		"arm64": "arm",
	}

	osvals := map[string]string{
		"darwin":  "mac",
		"windows": "windows",
		"linux":   "linux",
	}

	if osval, ok := osvals[runtime.GOOS]; ok {
		if archval, ok := archvals[runtime.GOARCH]; ok {
			return fmt.Sprintf("%s-%s", osval, archval)
		}
	}

	panic("invalid os/arch")
}

func doUpdate() {
	log.Printf("current version is %s", versions.CurrentVersionAsString())

	ver, err := getLatestVersion()
	if err != nil {
		log.Print(err)
		return
	}

	log.Printf("latest version is %s", versions.VersionToString(ver))

	// we don't need to update
	if ver <= versions.CurrentVersion {
		return
	}

	downloadUrl := makeS3Url(fmt.Sprintf("update/%s-%s.zip", getOS(), versions.VersionToString(ver)))
	log.Printf("downloading %s", downloadUrl)

	tmpfile, err := os.CreateTemp("", "update.zip")
	if err != nil {
		log.Print("createtemp: ", err)
		return
	}
	defer os.Remove(tmpfile.Name())

	if err := downloadFile(downloadUrl, tmpfile); err != nil {
		log.Print("downloadFile: ", err)
		return
	}

	exepath, err := os.Executable()
	if err != nil {
		log.Print("executable: ", err)
		return
	}

	dest := filepath.Join(filepath.Dir(exepath), "newbintmp")

	log.Printf("deleting %s", dest)
	os.RemoveAll(dest)

	log.Printf("unzipping to %s", dest)
	if err := unzip(tmpfile.Name(), dest); err != nil {
		log.Print("unzip: ", err)
		return
	}

	realdest := filepath.Join(filepath.Dir(exepath), "newbin")
	log.Printf("moving unzipped folder to %s", realdest)

	if err := replaceFolder(dest, realdest); err != nil {
		log.Print("replaceFolder: ", err)
	}
}

// Unzip will decompress a zip archive, moving all files and folders
// within the zip file (parameter 1) to an output directory (parameter 2).
// Taken from here: https://golangcode.com/unzip-files-in-go/
func unzip(src string, dest string) error {
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

func main() {
	exepath, err := os.Executable()
	if err != nil {
		panic(err)
	}

	exedir := filepath.Dir(exepath)
	if err := replaceBinFolder(exedir); err != nil {
		panic(err)
	}

	cmd := utils.MakeExecCommand(filepath.Join(exedir, idePath), os.Args[1:]...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	cmd.Env = os.Environ()
	cmd.Dir = filepath.Join(exedir, "bin")

	if err := cmd.Start(); err != nil {
		log.Printf("%v", err)
	}

	doUpdate()
}
