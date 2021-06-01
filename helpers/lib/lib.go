package lib

import (
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"
	"net/url"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"strings"

	"github.com/google/shlex"
)

func GetShellOutput(cmd string) string {
	parts, err := shlex.Split(cmd)
	if err != nil {
		return ""
	}
	out, err := exec.Command(parts[0], parts[1:]...).Output()
	if err != nil {
		return ""
	}
	return strings.TrimSpace(string(out))
}

// read file located in same folder as executable
func ReadFileFromExeFolder(filename string) ([]byte, error) {
	exepath, err := os.Executable()
	if err != nil {
		return nil, err
	}
	return os.ReadFile(path.Join(filepath.Dir(exepath), filename))
}

func GetLicenseKey() (string, error) {
	contents, err := ReadFileFromExeFolder(".idelicense")
	if err != nil {
		return "", err
	}
	return strings.TrimSpace(string(contents)), nil
}

func UnmarshalAndCloseHttpResp(resp *http.Response, out interface{}) error {
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		log.Printf("io.ReadAll error: %v", err)
		return err
	}

	if err := json.Unmarshal(body, out); err != nil {
		log.Printf("json.Unmarshal error: %v", err)
		return err
	}

	return nil
}

// const ServerBase = "http://localhost:8080"
const ServerBase = "https://api.codeperfect95.com"

func CallServer(endpoint string, licenseKey string, params url.Values, out interface{}) error {
	params["license_key"] = []string{licenseKey}

	resp, err := http.PostForm(fmt.Sprintf("%s/%s", ServerBase, endpoint), params)
	if err != nil {
		log.Printf("http.PostForm error: %v", err)
		return err
	}

	return UnmarshalAndCloseHttpResp(resp, out)
}

func GetLatestVer(licenseKey string) (int, error) {
	var data map[string]interface{}
	if err := CallServer("version", licenseKey, url.Values{}, &data); err != nil {
		return 0, err
	}

	ret, ok := data["version"].(float64)
	if !ok {
		log.Printf("unable to coerce version")
		return 0, fmt.Errorf("unable to coerce version")
	}

	return int(ret), nil
}

func AuthLicenseKey(licenseKey string) error {
	var data map[string]interface{}
	if err := CallServer("auth", licenseKey, url.Values{}, &data); err != nil {
		return err
	}
	if ret, ok := data["version"].(bool); ok && ret {
		return nil
	}
	return fmt.Errorf("Unable to authenticate license key.")
}
