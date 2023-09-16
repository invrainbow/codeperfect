package main

import (
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
)

func PrepareConfigDir() (string, error) {
	appdir, err := GetConfigDir()
	if err != nil {
		return "", err
	}

	if err := os.MkdirAll(appdir, os.ModePerm); err != nil {
		return "", err
	}
	return appdir, nil
}

func ReadFile(fp string) (string, error) {
	f, err := os.Open(fp)
	if err != nil {
		return "", err
	}

	buf, err := io.ReadAll(f)
	if err != nil {
		return "", err
	}

	return string(buf), nil
}

func MakeExecCommand(name string, args ...string) *exec.Cmd {
	return exec.Command(name, args...)
}

func IsTestMode() bool {
	return strings.HasSuffix(os.Args[0], ".test")
}

var DebugModeFlag = false

func IsDebugMode() bool {
	return DebugModeFlag || os.Getenv("DEBUG") == "1"
}

func GetConfigDir() (string, error) {
	configdir, err := os.UserConfigDir()
	if err != nil {
		return "", err
	}
	return filepath.Join(configdir, "CodePerfect"), nil
}

func makeShellCommand(s string) *exec.Cmd {
	return MakeExecCommand("/bin/bash", "-lc", s)
}

func makeFindBinaryPathCommand(bin string) *exec.Cmd {
	return makeShellCommand(fmt.Sprintf("which %s", bin))
}
