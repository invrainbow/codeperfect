package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
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
	Message string
}

func (se *ServerError) Error() string {
	return se.Message
}

func CallServer(endpoint string, license *License, params interface{}, out interface{}) error {
	buf, err := json.Marshal(params)
	if err != nil {
		return err
	}

	url := fmt.Sprintf("%s/%s", GetServerBase(), endpoint)
	req, err := http.NewRequest("POST", url, bytes.NewBuffer(buf))
	if license != nil {
		req.Header.Set("X-Email", license.Email)
		req.Header.Set("X-License-Key", license.LicenseKey)
	}
	req.Header.Set("Content-Type", "application/json")

	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return err
	}

	if resp.StatusCode != http.StatusOK {
		var errResp models.ErrorResponse
		if err := json.Unmarshal(body, &errResp); err != nil {
			return err
		}
		return &ServerError{
			Message: errResp.Error,
		}
	}

	if err := json.Unmarshal(body, out); err != nil {
		return err
	}

	return nil
}
