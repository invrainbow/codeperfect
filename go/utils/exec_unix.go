//go:build !windows

package utils

import (
	"os/exec"
)

func MakeExecCommand(name string, args ...string) *exec.Cmd {
	return exec.Command(name, args...)
}
