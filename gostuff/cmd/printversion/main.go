package main

import (
	"fmt"

	"github.com/invrainbow/codeperfect/gostuff/versions"
)

func main() {
	fmt.Printf("%d %s\n", versions.CurrentVersion, versions.CurrentVersionAsString())
}
