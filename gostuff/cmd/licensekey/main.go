package main

import (
	"crypto/rand"
	"fmt"
	"log"
	"os"
	"regexp"

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

	for _, email := range os.Args[1:] {
		user := &models.User{
			Email:        email,
			LicenseKey:   generateKey(32),
			DownloadCode: generateKey(16),
			IsActive:     true,
		}
		db.Db.Create(&user)
		fmt.Printf("%s: https://codeperfect95.com/install?code=%s\n", email, user.DownloadCode)
	}
}
