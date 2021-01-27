package models

import (
	"time"
	"gorm.io/gorm"
	_ "gorm.io/drivers/sqlite"
)

type Item struct {
	gorm.Model

	ItemType int
	Content string
	ExternalID string
	CreatedAt time.Time
	UpdatedAt
	ExternallyCreatedAt time.Time
	SourceID int
}

type Source struct {
	SourceType int
}

var DB *gorm.DB

func Connect() {
	db, err := gorm.Open("sqlite3", "test.db")
	if err != nil {
		panic("Failed to connect to db!")
	}

	db.AutoMigrate(&Item, &Source)
	DB = db
}