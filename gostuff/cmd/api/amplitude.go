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
	"time"
)

var AmplitudeAPIKey = os.Getenv("AMPLITUDE_API_KEY")
var SlackWebhookURL = os.Getenv("SLACK_WEBHOOK_URL")

// https://developers.amplitude.com/docs/http-api-v2#keys-for-the-event-argument
type AmplitudeEvent struct {
	UserID          string      `json:"user_id,omitempty"`
	EventType       string      `json:"event_type,omitempty"`
	Time            int64       `json:"time,omitempty"`
	EventProperties interface{} `json:"event_properties,omitempty"`
	UserProperties  interface{} `json:"event_properties,omitempty"`
}

// Read is a function that still needs to be documented.
func (ae *AmplitudeEvent) Read(p []byte) (int, error) {
	panic("not implemented")
}

type AmplitudeRequest struct {
	APIKey string        `json:"api_key"`
	Events []interface{} `json:"events"`
}

func LogEvent(userID int, event *AmplitudeEvent) error {
	event.Time = time.Now().UnixMilli()
	event.UserID = fmt.Sprintf("%05d", userID)
	err := ActuallyLogEvent(event)
	if err != nil {
		log.Println(err)
	}
	return err
}

func ActuallyLogEvent(event *AmplitudeEvent) error {
	req := &AmplitudeRequest{
		APIKey: AmplitudeAPIKey,
		Events: []interface{}{event},
	}

	data, err := json.Marshal(req)
	if err != nil {
		return err
	}

	resp, err := http.Post("https://api.amplitude.com/2/httpapi", "application/json", bytes.NewReader(data))
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

	// all good!
	return nil
}

func SendSlackMessage(format string, args ...interface{}) {
	if err := ActuallySendSlackMessage(format, args...); err != nil {
		log.Printf("error sending slack message: %s", err)
	}
}

func ActuallySendSlackMessage(format string, args ...interface{}) error {
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
