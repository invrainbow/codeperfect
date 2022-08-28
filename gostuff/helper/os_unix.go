//go:build !windows

package main

import (
	"fmt"
	"os/exec"
)

func makeShellCommand(s string) *exec.Cmd {
	return exec.Command("/bin/bash", "-lc", s)
}

func makeFindBinaryPathCommand(bin string) *exec.Cmd {
	return makeShellCommand(fmt.Sprintf("which %s", bin))
}
