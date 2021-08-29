package api

import (
	"log"
	"os"

	"github.com/invrainbow/codeperfect/gostuff/models"
	"gorm.io/driver/postgres"
	"gorm.io/gorm"
)

var db *gorm.DB

func init() {
	res, err := gorm.Open(postgres.Open(os.Getenv("POSTGRES_URL")), &gorm.Config{})
	if err != nil {
		log.Fatal(err)
	}
	db = res

	db.AutoMigrate(&models.User{})
}
