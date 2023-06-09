package main

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strings"
)

const HastebinApiKey = "c7eef9b81f51a47ccfcbee5c25f9e336ef8307abacc7c1daf98cd95c6a77b535c2bdaec5ad1a78499b092cdddd238baaf788fc1cf005dc418344aee5b39931d7"

func uploadToHastebin(content string) (string, error) {
	url := "https://hastebin.com/documents"

	req, err := http.NewRequest("POST", url, strings.NewReader(content))
	if err != nil {
		return "", fmt.Errorf("unable to create request: %v", err)
	}

	req.Header.Set("Authorization", fmt.Sprintf("Bearer %s", HastebinApiKey))
	req.Header.Set("Content-Type", "text/plain")
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return "", fmt.Errorf("unable to post: %v", err)
	}

	data, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", fmt.Errorf("unable to read reply contents: %v", err)
	}

	type Out struct {
		Key string `json:"key"`
	}

	var out Out
	if err := json.Unmarshal(data, &out); err != nil {
		return "", err
	}

	return fmt.Sprintf("https://hastebin.com/share/%s", out.Key), nil
}
