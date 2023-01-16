package main

import (
	"fmt"

	"github.com/codeperfect95/codeperfect/go/versions"
)

func main() {
	fmt.Printf("%d %s\n", versions.CurrentVersion, versions.CurrentVersionAsString())
}
