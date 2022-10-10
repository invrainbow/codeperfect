package main

import (
	"os"
	"strconv"

	"github.com/posthog/posthog-go"
)

var client posthog.Client

func init() {
	theClient, err := posthog.NewWithConfig(
		os.Getenv("POSTHOG_API_KEY"),
		posthog.Config{
			Endpoint: "https://app.posthog.com",
			// PersonalApiKey: "your personal API key", // needed for feature flags
		},
	)
	if err != nil {
		panic(err)
	}

	client = theClient
}

type PosthogProps posthog.Properties

func PosthogCaptureStringId(id string, event string, properties PosthogProps) error {
	return client.Enqueue(posthog.Capture{
		DistinctId: id,
		Event:      event,
		Properties: posthog.Properties(properties),
	})
}

func PosthogCapture(userid uint, event string, properties PosthogProps) error {
	id := strconv.FormatUint(uint64(userid), 10)
	return PosthogCaptureStringId(id, event, properties)
}

func PosthogIdentify(userid uint, properties PosthogProps) error {
	return client.Enqueue(posthog.Identify{
		DistinctId: strconv.FormatUint(uint64(userid), 10),
		Properties: posthog.Properties(properties),
	})
}
