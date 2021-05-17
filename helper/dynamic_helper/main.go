package main

import (
	"go/build"
	"path/filepath"

	"github.com/invrainbow/ide/helper"
)

func main() {
	ctx := build.Default

	helper.MainLoop(func(op int) {
		switch op {
		case helper.OpSetDirectory:
			helper.HandleOpSetDirectory()

		case helper.OpCheckIncludedInBuild:
			path := helper.ReadLine()
			match, err := ctx.MatchFile(filepath.Dir(path), filepath.Base(path))
			if err != nil {
				helper.WriteError(err)
			} else {
				helper.Write(match)
			}

		case helper.OpGetGoEnvVars:
			helper.Write(true)
			helper.Write(helper.GetShellOutput("go env GOPATH"))
			helper.Write(helper.GetShellOutput("go env GOROOT"))
			helper.Write(helper.GetShellOutput("go env GOMODCACHE"))

		default:
			return
		}
	})
}
