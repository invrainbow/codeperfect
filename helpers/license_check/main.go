package main

import (
	"fmt"
	"os"
	"time"

	"github.com/invrainbow/ide/helpers/lib"
)

const GracePeriod = time.Hour * 24 * 7

func exit(s string) {
	fmt.Printf("%s", s)
	os.Exit(0)
}

func readLastSuccess() (time.Time, error) {
	unixTimestamp, err := foo() // TODO
	if err != nil {
		return time.Time{}, err
	}
	return time.Unix(unixTimestamp, 0), nil
}

func writeLastSuccess(t time.Time) error {
	// TODO: do something with t.Unix()
	return nil
}

func main() {
	licenseKey, err := lib.GetLicenseKey()
	if err != nil {
		exit("no_license_key_found")
	}

	if err := lib.AuthLicenseKey(licenseKey); err == nil {
		writeLastSuccess(time.Now())
		exit("ok")
	}

	lastSuccess, err := readLastSuccess()
	if err == nil {
		if time.Since(lastSuccess) < GracePeriod {
			exit("grace_period")
		}
	}

	exit("fail")
}
