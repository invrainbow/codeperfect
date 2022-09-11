package main

import (
	"log"
	"os/exec"

	"github.com/buildkite/shellwords"
	"github.com/invrainbow/codeperfect/go/utils"
)

func makeShellCommand(s string) *exec.Cmd {
	parts, err := shellwords.Split(s)
	if err != nil {
		log.Printf("shellwords.split error: %v", err)
		return nil
	}

	if len(parts) == 0 {
		log.Printf("parts is empty")
		return nil
	}

	return utils.MakeExecCommand(parts[0], parts[1:]...)
}

func makeFindBinaryPathCommand(bin string) *exec.Cmd {
	return utils.MakeExecCommand("where", bin)
}
