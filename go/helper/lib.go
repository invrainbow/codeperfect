package main

import (
	"os"
	"path/filepath"
	"strings"

	"github.com/google/shlex"
	"github.com/invrainbow/codeperfect/go/utils"
)

func GetShellOutput(cmd string) string {
	parts, err := shlex.Split(cmd)
	if err != nil {
		return ""
	}
	out, err := utils.MakeExecCommand(parts[0], parts[1:]...).Output()
	if err != nil {
		return ""
	}
	return strings.TrimSpace(string(out))
}

// read file located in same folder as executable
func ReadFileFromExeFolder(filename string) ([]byte, error) {
	exepath, err := os.Executable()
	if err != nil {
		return nil, err
	}
	return os.ReadFile(filepath.Join(filepath.Dir(exepath), filename))
}

func IsTestMode() bool {
	return strings.HasSuffix(os.Args[0], ".test")
}

var DebugModeFlag = false

func IsDebugMode() bool {
	return DebugModeFlag || os.Getenv("DEBUG") == "1"
}

func PrepareConfigDir() (string, error) {
	configdir, err := os.UserConfigDir()
	if err != nil {
		return "", err
	}

	appdir := filepath.Join(configdir, "CodePerfect")
	if err := os.MkdirAll(appdir, os.ModePerm); err != nil {
		return "", err
	}

	return appdir, nil
}