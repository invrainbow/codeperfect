package main

import (
	"fmt"
	"os"

	"github.com/invrainbow/codeperfect/gostuff/launcher"
)

func main() {
	if err := launcher.Run(); err != nil {
		fmt.Fprintf(os.Stderr, "error: %v\n", err)
		os.Exit(1)
		return
	}
}
