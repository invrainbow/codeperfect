package main_test

import (
	"encoding/hex"
	"fmt"
	"strings"
	"testing"

	"github.com/invrainbow/ide/payments"
)

func TestBlah(t *testing.T) {
	x := 1
	y := x + 2
	z := y * 3
	fmt.Printf("x = %d, y = %d, z = %d\n", x, y, z)
}

func TestGenerateLicenseKey(t *testing.T) {
	key, err := main.GenerateLicenseKey()
	if err != nil {
		t.Errorf("error while generating license key: %v", err)
	}

	validateKey := func() bool {
		if len(key) != 35 {
			return false
		}

		parts := strings.Split(key, "-")
		if len(parts) != 4 {
			return false
		}

		for _, part := range parts {
			if len(part) != 8 {
				return false
			}
			arr, err := hex.DecodeString(part)
			if err != nil {
				return false
			}
			if len(arr) != 4 {
				return false
			}
		}

		return true
	}

	if !validateKey() {
		t.Errorf("expected key to be xxxxxxxx-xxxxxxxx-xxxxxxxx-xxxxxxxx")
	}
}
