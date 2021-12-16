package main

import (
	"errors"
	"os"
	"strconv"

	"github.com/invrainbow/codeperfect/gostuff/db"
	"github.com/invrainbow/codeperfect/gostuff/models"
	"gorm.io/gorm"
)

func main() {
	versionStr := os.Args[1]
	versionNo, err := strconv.Atoi(versionStr)
	if err != nil {
		panic(err)
	}

	osSlug := os.Args[2]
	appHash := os.Args[3]
	updateHash := os.Args[4]

	var version models.Version
	res := db.DB.Where("version = ? AND os = ?", versionNo, osSlug).First(&version)
	if res.Error != nil {
		if !errors.Is(res.Error, gorm.ErrRecordNotFound) {
			panic(res.Error)
		}

		version.Version = versionNo
		version.OS = osSlug
		if err := db.DB.Create(&version).Error; err != nil {
			panic(err)
		}
	}

	version.AppHash = appHash
	version.UpdateHash = updateHash
	db.DB.Save(&version)
}
