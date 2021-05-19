package main

import (
	"gorm.io/driver/postgres"
	"gorm.io/gorm"
)

type User struct {
	gorm.Model

	Email                    string
	StripeCustomerID         string
	StripeSubscriptionID     string
	StripeSubscriptionStatus string
	LicenseKey               string
}

var db *gorm.DB

func initDB() {
	dsn := "host=localhost user=brandon password=brandon dbname=brandon port=5432 sslmode=disable TimeZone=US/Pacific"
	db, err := gorm.Open(postgres.Open(dsn), &gorm.Config{})
	if err != nil {
		panic(err)
	}

	// db.LogMode(true)
	db.AutoMigrate(&User{})
}
