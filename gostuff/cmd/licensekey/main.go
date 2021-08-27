package main

import (
	"crypto/rand"
	"fmt"

	"github.com/btcsuite/btcutil/base58"
)

func main() {
	b := make([]byte, 32)
	n, err := rand.Read(b)
	if err != nil || n != cap(b) {
		panic("shit done fucked up")
	}

	fmt.Printf("%s\n", base58.Encode(b))
}
