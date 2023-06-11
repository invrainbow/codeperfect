package main

import (
	"archive/zip"
	"bufio"
	"encoding/json"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"syscall"
	"time"

	"github.com/codeperfect95/codeperfect/go/models"
	"github.com/codeperfect95/codeperfect/go/utils"
	"github.com/codeperfect95/codeperfect/go/versions"
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

	osSlug, err := versions.GetOSSlug(runtime.GOOS, runtime.GOARCH)
	if err != nil {
		panic("invalid os/arch")
	}

	downloadUrl := makeS3Url(fmt.Sprintf("update/%s-%s.zip", osSlug, versions.VersionToString(ver)))
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
		dynamic_gohelper.go
		...
	newbin/
*/

type MainAppInfo struct {
	shouldUpdate     bool
	email            string
	licenseKey       string
	sendCrashReports bool
}

func readInfoFromMainApp() (*MainAppInfo, error) {
	pipeFile, err := utils.GetAppToLauncherPipeFile()
	if err != nil {
		return nil, err
	}

	os.Remove(pipeFile)
	err = syscall.Mkfifo(pipeFile, 0666)
	if err != nil {
		return nil, err
	}

	file, err := os.OpenFile(pipeFile, os.O_RDONLY, os.ModeNamedPipe)
	if err != nil {
		return nil, err
	}
	defer file.Close()

	info := &MainAppInfo{}

	s := bufio.NewScanner(file)
	if s.Scan() {
		info.shouldUpdate = s.Text() == "autoupdate"
	}
	if s.Scan() {
		info.email = s.Text()
	}
	if s.Scan() {
		info.licenseKey = s.Text()
	}
	if s.Scan() {
		info.sendCrashReports = s.Text() == "sendcrash"
	}

	return info, nil
}

const (
	FlockSh = 1
	FlockEx = 2
	FlockNb = 4
	FlockUn = 8
)

func SendCrashReports(license *utils.License) {
	configdir, err := utils.GetConfigDir()
	if err != nil {
		log.Print(err)
		return
	}

	lockfilepath := filepath.Join(configdir, "codeperfect_crash_reports_mutex")
	lockfile, err := os.OpenFile(lockfilepath, os.O_WRONLY|os.O_CREATE, 0o0644)
	if err != nil {
		log.Print(err)
		return
	}
	defer lockfile.Close()

	if err := syscall.Flock(int(lockfile.Fd()), syscall.LOCK_EX|syscall.LOCK_NB); err != nil {
		log.Print("couldn't lock: %v", err)
		return
	}
	defer syscall.Flock(int(lockfile.Fd()), syscall.LOCK_UN)

	homedir, err := os.UserHomeDir()
	if err != nil {
		log.Print(err)
		return
	}

	reportsdir := filepath.Join(homedir, "Library/Logs/DiagnosticReports")
	files, err := ioutil.ReadDir(reportsdir)
	if err != nil {
		log.Print(err)
		return
	}

	desiredProcName := "ide"
	desiredProcPathSuffix := "/CodePerfect.app/Contents/MacOS/bin/ide"
	if utils.IsDebugMode() {
		desiredProcPathSuffix = "/ide"
	}

	isOurCrashFileIps := func(data []byte) bool {
		lines := strings.Split(string(data), "\n")
		blob := strings.Join(lines[1:], "\n")

		type Info struct {
			ProcName string `json:"procName"`
			ProcPath string `json:"procPath"`
		}

		var info Info
		if err := json.Unmarshal([]byte(blob), &info); err != nil {
			log.Print(err)
			return false
		}

		return (strings.HasSuffix(info.ProcPath, desiredProcPathSuffix) && info.ProcName == desiredProcName)
	}

	isOurCrashFileCrash := func(data []byte) bool {
		lines := strings.Split(string(data), "\n")

		pathmatch := false
		idmatch := false

		for _, line := range lines {
			parts := strings.Fields(line)
			if len(parts) != 2 {
				continue
			}
			if parts[0] == "Path:" {
				if !strings.HasSuffix(line, desiredProcPathSuffix) {
					return false
				}
				pathmatch = true
			} else if parts[0] == "Identifier:" {
				if len(parts) != 2 || parts[1] != desiredProcName {
					return false
				}
				idmatch = true
			} else {
				continue
			}
			if pathmatch && idmatch {
				return true
			}
		}
		return false
	}

	isOurCrashFile := func(fullpath string, data []byte) bool {
		if strings.HasSuffix(fullpath, ".ips") {
			return isOurCrashFileIps(data)
		}
		if strings.HasSuffix(fullpath, ".crash") {
			return isOurCrashFileCrash(data)
		}
		return false
	}

	processFile := func(fullpath string, data []byte) {
		defer os.Remove(fullpath)

		// big crash report, skip
		if len(data) > 64000 {
			log.Printf("data is big, len = %d", len(data))
			return
		}

		osSlug, _ := versions.GetOSSlug(runtime.GOOS, runtime.GOARCH)
		req := models.CrashReportRequest{
			OS:      osSlug,
			Content: string(data),
			Version: versions.CurrentVersion,
		}

		if err := utils.CallServer("v2/crash-report", license, req, nil); err != nil {
			log.Print(err)
			return
		}
	}

	for _, f := range files {
		if f.IsDir() {
			continue
		}
		fullpath := filepath.Join(reportsdir, f.Name())
		data, err := os.ReadFile(fullpath)
		if err != nil {
			log.Print(fullpath, err)
			continue
		}
		if isOurCrashFile(fullpath, data) {
			processFile(fullpath, data)
		}
	}
}

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

	info, err := readInfoFromMainApp()
	if err != nil {
		log.Printf("error reading from pipe: %v", err)
		return
	}

	if info.shouldUpdate {
		doUpdate()
	}

	if info.sendCrashReports {
		cmd.Wait()
		time.Sleep(time.Second * 10)

		var license *utils.License
		if info.email != "" && info.licenseKey != "" {
			license = &utils.License{
				Email:      info.email,
				LicenseKey: info.licenseKey,
			}
		}
		SendCrashReports(license)
	}
}
