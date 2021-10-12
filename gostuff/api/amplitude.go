package api

import (
	"bytes"
	"encoding/json"
	"errors"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"time"
)

var AmplitudeAPIKey = os.Getenv("AMPLITUDE_API_KEY")

// add more as i need them i guess
// https://developers.amplitude.com/docs/http-api-v2#keys-for-the-event-argument
type AmplitudeEvent struct {
	UserID          string
	EventType       string
	Time            int64
	EventProperties interface{}
	UserProperties  interface{}
}

type AmplitudeRequest struct {
	APIKey string        `json:"api_key"`
	Events []interface{} `json:"events"`
}

func LogEvent(event *AmplitudeEvent) error {
	event.Time = time.Now().UnixMilli()
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

	if resp.StatusCode != http.StatusOK {
		body, err := ioutil.ReadAll(resp.Body)
		if err != nil {
			return err
		}
		return errors.New(string(body))
	}

	// all good!
	return nil
}
