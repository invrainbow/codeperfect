package main

import (
	"go/build"
	"path/filepath"

	"github.com/invrainbow/ide/helpers/helperlib"
)

func main() {
	helperlib.InitScanner()
	for {
		switch helperlib.ReadLine() {
		case "set_directory":
			helperlib.HandleSetDirectory()

		case "check_go_version":
			helperlib.Write(isCompatible)

		case "check_included_in_build":
			path := helperlib.ReadLine()
			match, err := build.Default.MatchFile(filepath.Dir(path), filepath.Base(path))
			if err != nil {
				helperlib.WriteError(err)
			} else {
				helperlib.Write(match)
			}
		}
	}
}
