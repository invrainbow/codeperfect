package helper

import (
	"bytes"

	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"

	"github.com/invrainbow/codeperfect/gostuff/models"
)

func GetServerBase() string {
	if IsTestMode() || IsDebugMode() {
		return "http://localhost:8080"
	}
	return "https://api.codeperfect95.com"
}

type ServerError struct {
	Code    int
	Message string
}

func (se *ServerError) Error() string {
	return se.Message
}

func IsServerError(err error) bool {
	_, ok := err.(*ServerError)
	return ok
}

func CallServer(endpoint string, license *License, params interface{}, out interface{}) error {
	buf, err := json.Marshal(params)
	if err != nil {
		return err
	}

	fmt.Printf("email = %s, key = %s\n", license.Email, license.LicenseKey)

	url := fmt.Sprintf("%s/%s", GetServerBase(), endpoint)
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

	fmt.Printf("body: %s\n", body)

	if resp.StatusCode != http.StatusOK {
		var errResp models.ErrorResponse
		if err := json.Unmarshal(body, &errResp); err != nil {
			return err
		}
		return &ServerError{
			Code:    errResp.Code,
			Message: errResp.Error,
		}
	}

	if err := json.Unmarshal(body, out); err != nil {
		log.Printf("json.Unmarshal error: %v", err)
		return err
	}

	return nil
}
