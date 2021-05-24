package main

import (
	"fmt"
	"go/build"
	"os"
	"path/filepath"
	"runtime"
)

func main() {
	_, fpath, _, ok := runtime.Caller(0)
	if !ok {
		fmt.Printf("unable to get current file path")
		os.Exit(1)
	}

	dirpath := filepath.Dir(fpath)

	match, err := build.Default.MatchFile(dirpath, "dummy.go")
	if err != nil {
		fmt.Printf("%v", err)
		os.Exit(1)
	}

	if !match {
		fmt.Printf("bad version, version is %s", runtime.Version())
		os.Exit(1)
	}

	fmt.Printf("true")
}
