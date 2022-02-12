package db

import (
	"log"
	"os"

	"github.com/invrainbow/codeperfect/gostuff/models"
	"gorm.io/driver/postgres"
	"gorm.io/gorm"
)

var DB *gorm.DB

func init() {
	res, err := gorm.Open(postgres.Open(os.Getenv("POSTGRES_URL")), &gorm.Config{})
	if err != nil {
		log.Fatal(err)
	}
	DB = res
	DB.AutoMigrate(&models.User{})
	DB.AutoMigrate(&models.Session{})
	DB.AutoMigrate(&models.Version{})
	DB.AutoMigrate(&models.CurrentVersion{})
}
