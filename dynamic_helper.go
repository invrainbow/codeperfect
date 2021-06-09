package main

import (
	"bufio"
	"fmt"
	"go/build"
	"os"
	"path/filepath"
	"strings"
)

var scanner *bufio.Scanner = bufio.NewScanner(os.Stdin)

func Write(x interface{}) {
	fmt.Println(x)
}

func WriteError(x error) {
	Write("error")
	Write(strings.ReplaceAll(x.Error(), "\n", "\\n"))
}

func ReadLine() string {
	if !scanner.Scan() {
		panic("unable to read line")
	}
	return scanner.Text()
}

func CheckGoVersion(tag string) bool {
	for _, it := range build.Default.ReleaseTags {
		if it == tag {
			return true
		}
	}
	return false
}

func main() {
	for {
		switch ReadLine() {
		case "set_directory":
			path := ReadLine()
			if err := os.Chdir(path); err != nil {
				WriteError(err)
				break
			}
			Write(true)

		case "check_go_version":
			Write(CheckGoVersion("go1.16"))

		case "check_included_in_build":
			path := ReadLine()
			match, err := build.Default.MatchFile(filepath.Dir(path), filepath.Base(path))
			if err != nil {
				WriteError(err)
			} else {
				Write(match)
			}
		}
	}
}
