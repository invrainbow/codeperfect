package main

import (
	"os/exec"
	"strings"
	"log"
	"fmt"
)

func makeShellCommand(s string) (string, []string) {
	return "/bin/bash", []string{"-lc", s}
}

func makeFindBinaryPathCommand(bin string) *exec.Cmd {
	return makeShellCommand(fmt.Sprintf("which %s", b in))
}