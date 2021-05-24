package main

import (
	"fmt"
	"os"

	"github.com/invrainbow/ide/helpers/lib"
)

func fail(s string) {
	fmt.Printf("%s", s)
	os.Exit(1)
}

func main() {
	licenseKey, err := lib.GetLicenseKey()
	if err != nil {
		fail("Unable to read license key.")
	}

	// make a basic call to the server and see if license key authenticates
	if _, err := lib.GetLatestVer(licenseKey); err != nil {
		fail("Invalid license key.")
	}
}
