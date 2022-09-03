//go:build !windows

package main

import (
	"fmt"
	"os/exec"

	"github.com/invrainbow/codeperfect/gostuff/utils"
)

func makeShellCommand(s string) *exec.Cmd {
	return utils.MakeExecCommand("/bin/bash", "-lc", s)
}

func makeFindBinaryPathCommand(bin string) *exec.Cmd {
	return makeShellCommand(fmt.Sprintf("which %s", bin))
}
