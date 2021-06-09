package main

/*
import (
	"fmt"
	"io"
	"net/http"
	"net/url"
	"os"
	"path"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"github.com/invrainbow/ide/helpers/lib"
)

var LicenseKey string

type DownloadInfo struct {
	Version      int    `json:"version"`
	Checksum     string `json:"checksum"`
	DownloadLink string `json:"download_link"`
}

func GetDownloadInfo(licenseKey string) (*DownloadInfo, error) {
	var data DownloadInfo
	if err := lib.CallServer("download", licenseKey, url.Values{}, &data); err != nil {
		return nil, err
	}
	return &data, nil
}

func ReadVersion() (int, error) {
	contents, err := lib.ReadFileFromExeFolder(".ideversion")
	if err != nil {
		// if the file just doesn't exist, don't count it as an error
		if os.IsNotExist(err) {
			return 0, nil
		}
		return 0, err
	}

	parts := strings.Split(string(contents), "\n")
	if len(parts) != 2 {
		return 0, fmt.Errorf("invalid .ideversion file")
	}

	return strconv.Atoi(strings.TrimSpace(parts[0]))
}

func DownloadFile(url string, outputFile string) error {
	out, err := os.Create(outputFile)
	defer out.Close()
	if err != nil {
		return err
	}

	resp, err := http.Get(url)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	n, err := io.Copy(out, resp.Body)
	if err != nil {
		return err
	}
	if n == 0 {
		return fmt.Errorf("failed to copy anything")
	}
	return nil
}

func WriteVersion(version int, checksum string) error {
	exepath, err := os.Executable()
	if err != nil {
		return err
	}

	lines := []string{strconv.Itoa(version), checksum}
	return os.WriteFile(
		path.Join(filepath.Dir(exepath), ".ideversion"),
		[]byte(strings.Join(lines, "\n")),
		0600,
	)
}

func TryDownloadingShit() {
	licenseKey, err := lib.GetLicenseKey()
	if err != nil {
		return
	}

	if licenseKey == "" {
		return
	}

	serverVersion, err := lib.GetLatestVer(licenseKey)
	if err != nil {
		return
	}

	currentVersion, err := ReadVersion()
	if err != nil {
		return
	}

	if serverVersion == currentVersion {
		return
	}

	dl, err := GetDownloadInfo(licenseKey)
	if err != nil {
		return
	}

	if dl.Version != serverVersion {
		return
	}

	DownloadFile(dl.DownloadLink, "ide.tmp.exe")
	WriteVersion(dl.Version, dl.Checksum)
}

func main() {
	for {
		TryDownloadingShit()
		time.Sleep(10 * time.Minute)
	}
}
*/
