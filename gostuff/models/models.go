package models

import (
	"gorm.io/gorm"
)

type User struct {
	gorm.Model
	Email           string
	LicenseKey      string
	IsActive        bool
	StripeCusId     string
	StripeCusStatus string
	DownloadCode    string
}

// users email license_key stripe_cus_id stripe_cus_status
