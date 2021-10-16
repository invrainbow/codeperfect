package main

import (
	"crypto/rand"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"regexp"
	"strings"

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

func filterAlnum(s string) string {
	reg, err := regexp.Compile("[^a-zA-Z0-9]+")
	if err != nil {
		log.Fatal(err)
	}
	return reg.ReplaceAllString(s, "")
}

func main() {
	if len(os.Args) <= 1 {
		panic("expected email")
	}

	for _, email := range os.Args[1:] {
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

		filename := fmt.Sprintf("license_%s.json", filterAlnum(strings.Split(email, "@")[0]))
		if err := ioutil.WriteFile(filename, data, 0644); err != nil {
			panic(err)
		}

		user := &models.User{
			Email:        email,
			LicenseKey:   licenseKey,
			DownloadCode: downloadCode,
			IsActive:     true,
		}
		db.Db.Create(&user)

		fmt.Printf("%s\n", email)
		fmt.Printf("Download link: https://codeperfect95.com/download?code=%s\n", user.DownloadCode)
		fmt.Printf("\n")
	}
}
