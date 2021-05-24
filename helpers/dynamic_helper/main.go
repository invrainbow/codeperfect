package main

import (
	"go/build"
	"path/filepath"

	"github.com/invrainbow/ide/helpers"
)

func main() {
	ctx := build.Default

	helpers.MainLoop(func(op int) {
		switch op {
		case helpers.OpSetDirectory:
			helpers.HandleOpSetDirectory()

		case helpers.OpCheckIncludedInBuild:
			path := helpers.ReadLine()
			match, err := ctx.MatchFile(filepath.Dir(path), filepath.Base(path))
			if err != nil {
				helpers.WriteError(err)
			} else {
				helpers.Write(match)
			}

		case helpers.OpGetGoEnvVars:
			helpers.Write(true)
			helpers.Write(helpers.GetShellOutput("go env GOPATH"))
			helpers.Write(helpers.GetShellOutput("go env GOROOT"))
			helpers.Write(helpers.GetShellOutput("go env GOMODCACHE"))

		default:
			return
		}
	})
}
