package main

import (
	"fmt"
	"os"

	"github.com/invrainbow/ide/helpers"
)

func main() {
    fail := func(s string) {
        fmt.Printf("%s", s)
        os.Exit(1)
    }

	licenseKey, err := helpers.GetLicenseKey()
	if err != nil {
		fail("Unable to read license key.")
	}

	// make a basic call to the server and see if license key authenticates
	_, err := helpers.GetLatestVer(licenseKey)
	if err != nil {
		fail("Invalid license key.")
	}
}
