package main

import (
	"fmt"
	"github.com/google/go-cmp/cmp"
	"github.com/invrainbow/whetstone/sync"
)

func main() {
	// start downloading
	fmt.Println("downloading")
	sync.Download()

	// print out a useless diff
	fmt.Println(cmp.Diff("Hello World", "Hello Go"))
}
