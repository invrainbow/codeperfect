package main

import (
	"crypto/rand"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"os"

	"github.com/btcsuite/btcutil/base58"
	"github.com/invrainbow/codeperfect/gostuff/db"
	"github.com/invrainbow/codeperfect/gostuff/helper"
	"github.com/invrainbow/codeperfect/gostuff/models"
)

func generateKey(nbytes int) string {
	b := make([]byte, nbytes)
	n, err := rand.Read(b)
	if err != nil || n != cap(b) {
		panic("shit done fucked up")
	}
	return base58.Encode(b)
}

func main() {
	if len(os.Args) <= 1 {
		panic("expected email")
	}

	email := os.Args[1]
	licenseKey := generateKey(32)
	downloadCode := generateKey(16)

	license := &helper.License{
		Email:      email,
		LicenseKey: licenseKey,
	}
	data, err := json.MarshalIndent(license, "", "  ")
	if err != nil {
		panic(err)
	}
	if err := ioutil.WriteFile(".cplicense", data, 0644); err != nil {
		panic(err)
	}

	user := &models.User{
		Email:        email,
		LicenseKey:   licenseKey,
		DownloadCode: downloadCode,
		IsActive:     true,
	}
	db.Db.Create(&user)

	fmt.Printf("ID = %d\n", user.ID)
	fmt.Printf("License key: %s\n", user.LicenseKey)
	fmt.Printf("Download code: %s\n", user.DownloadCode)
	fmt.Printf("Download link: https://codeperfect95.com/download?code=%s\n", user.DownloadCode)
}
