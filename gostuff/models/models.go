package models

import (
	"time"

	"gorm.io/gorm"
)

type User struct {
	gorm.Model
	Email          string
	LicenseKey     string
	Active         bool
	TrialStartedAt time.Time
}

type Session struct {
	gorm.Model
	UserID          uint
	StartedAt       time.Time
	LastHeartbeatAt time.Time
	Heartbeats      int
}

type Version struct {
	gorm.Model
	Version    int
	OS         string
	AppHash    string
	UpdateHash string
}

// users email license_key stripe_cus_id stripe_cus_status
