package main

import (
	"crypto/rand"
	"fmt"
	"os"

	"github.com/btcsuite/btcutil/base58"
	"github.com/invrainbow/codeperfect/gostuff/db"
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

	user := &models.User{
		Email:        email,
		LicenseKey:   licenseKey,
		DownloadCode: downloadCode,
	}
	db.Db.Create(&user)

	fmt.Printf("ID = %d\n", user.ID)
	fmt.Printf("License key: %s\n", user.LicenseKey)
	fmt.Printf("Download code: %s\n", user.DownloadCode)
}
