package models

import (
	"time"

	"gorm.io/gorm"
)

const (
	LicenseInvalid = iota
	LicenseOnly
	LicenseAndSub
	SubOnly
)

type User struct {
	gorm.Model
	Email      string
	Name       string
	LicenseKey string

	HasPerpetual    bool
	HasSubscription bool

	StripeCustomerID string

	// Active               bool
	StripeSubscriptionID string
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

type CurrentVersion struct {
	gorm.Model
	Version int
}

// users email license_key stripe_cus_id stripe_cus_status
