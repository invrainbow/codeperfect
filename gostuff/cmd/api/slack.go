package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"io/ioutil"
	"log"
	"net/http"
	"os"
)

var SlackWebhookURL = os.Getenv("SLACK_WEBHOOK_URL")

func SendSlackMessage(format string, args ...interface{}) {
	run := func() error {
		req := map[string]string{"text": fmt.Sprintf(format, args...)}
		data, err := json.Marshal(req)
		if err != nil {
			return err
		}

		resp, err := http.Post(SlackWebhookURL, "application/json", bytes.NewReader(data))
		if err != nil {
			return err
		}
		defer resp.Body.Close()

		body, err := ioutil.ReadAll(resp.Body)
		if err != nil {
			return err
		}

		if resp.StatusCode != http.StatusOK {
			return errors.New(string(body))
		}

		return nil
	}

	if err := run(); err != nil {
		log.Printf("error sending slack message: %s", err)
	}
}
