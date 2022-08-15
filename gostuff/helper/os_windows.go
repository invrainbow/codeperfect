package main

import (
	"os/exec"
	"fmt"
)

func makeShellCommand(s string) *exec.Cmd {
	return exec.Command("cmd.exe", "/C", s)
}

func makeFindBinaryPathCommand(bin string) *exec.Cmd {
	return makeShellCommand(fmt.Sprintf("where %s", bin))
}
