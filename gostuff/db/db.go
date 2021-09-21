package db

import (
	"log"
	"os"

	"github.com/invrainbow/codeperfect/gostuff/models"
	"gorm.io/driver/postgres"
	"gorm.io/gorm"
)

var Db *gorm.DB

func init() {
	res, err := gorm.Open(postgres.Open(os.Getenv("POSTGRES_URL")), &gorm.Config{})
	if err != nil {
		log.Fatal(err)
	}
	Db = res
	Db.AutoMigrate(&models.User{})
}
