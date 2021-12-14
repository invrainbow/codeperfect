package models

import (
	"time"

	"gorm.io/gorm"
)

const (
	UserStatusTrialWaiting = "trial_waiting"
	UserStatusTrial        = "trial"
	UserStatusPaid         = "paid"
	UserStatusInactive     = "inactive"
)

type User struct {
	gorm.Model
	Email          string
	LicenseKey     string
	DownloadCode   string
	Status         string
	TrialStartedAt time.Time
}

type Session struct {
	gorm.Model
	UserID          uint
	StartedAt       time.Time
	LastHeartbeatAt time.Time
	Heartbeats      int
}

// users email license_key stripe_cus_id stripe_cus_status
