package main

import (
	"fmt"

	"github.com/invrainbow/ide/gostuff/launcher"
)

func main() {
	if err := launcher.Run(); err != nil {
		println(err)
	}
}
