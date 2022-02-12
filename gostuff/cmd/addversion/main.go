package main

import (
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"io"
	"log"
	"os"

	"github.com/invrainbow/codeperfect/gostuff/db"
	"github.com/invrainbow/codeperfect/gostuff/models"
	"github.com/invrainbow/codeperfect/gostuff/versions"
	"gorm.io/gorm"
)

func getFileSHA256(filepath string) (string, error) {
	f, err := os.Open(filepath)
	if err != nil {
		return "", err
	}
	defer f.Close()

	hasher := sha256.New()
	if _, err := io.Copy(hasher, f); err != nil {
		return "", nil
	}

	return hex.EncodeToString(hasher.Sum(nil)), nil
}

func main() {
	osSlug := os.Args[1]
	appFile := os.Args[2]
	updateFile := os.Args[3]

	appHash, err := getFileSHA256(appFile)
	if err != nil {
		panic(err)
	}

	updateHash, err := getFileSHA256(updateFile)
	if err != nil {
		panic(err)
	}

	versionNo := versions.CurrentVersion

	var version models.Version
	res := db.DB.Where("version = ? AND os = ?", versionNo, osSlug).First(&version)
	if res.Error != nil {
		if !errors.Is(res.Error, gorm.ErrRecordNotFound) {
			panic(res.Error)
		}

		version.Version = versionNo
		version.OS = osSlug
		if err := db.DB.Create(&version).Error; err != nil {
			log.Printf("id: %d", version.ID)
			panic(err)
		}
	}

	version.AppHash = appHash
	version.UpdateHash = updateHash
	db.DB.Save(&version)

	var row models.CurrentVersion
	if res := db.DB.First(&row, "os = ?", osSlug); res.Error != nil {
		row.OS = osSlug
		db.DB.Create(&row)
	}
	row.Version = versionNo
	db.DB.Save(&row)
}
