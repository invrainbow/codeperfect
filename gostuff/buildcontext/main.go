// prints out the go/build.Default context for consumption by helper
package main

import (
	"bytes"
	"encoding/gob"
	"fmt"
	"go/build"
	"os"
)

func main() {
	var buf bytes.Buffer
	enc := gob.NewEncoder(&buf)
	if err := enc.Encode(build.Default); err != nil {
		fmt.Printf("couldn't encode default build context: %v\n", err)
		os.Exit(1)
	}
	os.Stdout.Write(buf.Bytes())
}
