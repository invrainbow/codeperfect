package main

import (
	"fmt"
	"github.com/google/go-cmp/cmp"
	"github.com/invrainbow/whetstone/sync"
	"golang.org/x/text/language"
)

func main() {
	// start downloading
	fmt.Println("downloading")
	sync.Download()

	// print out language for no reason
	lang := language.English
	fmt.Printf("language: %v", lang)

	// print out a useless diff
	fmt.Println(cmp.Diff("Hello World", "Hello Go"))
}
