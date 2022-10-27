package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
)

var ConvertKitAPIKey = os.Getenv("CONVERTKIT_API_KEY")
var ConvertKitAPISecret = os.Getenv("CONVERTKIT_API_SECRET")
var ConvertKitFormID = os.Getenv("CONVERTKIT_FORM_ID")

func AddEmailToMailingList(email string) error {
	body := map[string]string{
		"api_key": ConvertKitAPIKey,
		"email":   email,
	}
	data, err := json.Marshal(body)
	if err != nil {
		return err
	}

	url := fmt.Sprintf("https://api.convertkit.com/v3/forms/%v/subscribe", ConvertKitFormID)

	resp, err := http.Post(url, "application/json", bytes.NewBuffer(data))
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	buf, err := io.ReadAll(resp.Body)
	if err != nil {
		return err
	}

	responseBody := map[string]any{}

	if err := json.Unmarshal(buf, &responseBody); err != nil {
		return err
	}

	if _, ok := responseBody["subscription"]; !ok {
		return fmt.Errorf("unable to create subscription")
	}

	return nil
}
