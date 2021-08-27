package helper

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"

	"github.com/invrainbow/ide/gostuff/models"
)

var ServerBase = "https://api.codeperfect95.com"

func init() {
	if IsTestMode() {
		ServerBase = "http://localhost:8080"
	}

	fmt.Printf("serverbase: %v\n", ServerBase)
}

func CallServer(endpoint string, license *License, params interface{}, out interface{}) error {
	buf, err := json.Marshal(params)
	if err != nil {
		return err
	}

	url := fmt.Sprintf("%s/%s", ServerBase, endpoint)
	req, err := http.NewRequest("POST", url, bytes.NewBuffer(buf))
	req.Header.Set("X-Email", license.Email)
	req.Header.Set("X-License-Key", license.LicenseKey)
	req.Header.Set("Content-Type", "application/json")

	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		log.Printf("io.ReadAll error: %v", err)
		return err
	}

	if resp.StatusCode != http.StatusOK {
		var errResp models.ErrorResponse
		if err := json.Unmarshal(body, &errResp); err != nil {
			return err
		}
		return fmt.Errorf("%s", errResp.Error)
	}

	if err := json.Unmarshal(body, out); err != nil {
		log.Printf("json.Unmarshal error: %v", err)
		return err
	}

	return nil
}
