package utils

import (
	"os"
	"path/filepath"
	"strings"
)

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

func GetAppToLauncherPipeFile() (string, error) {
	configdir, err := GetConfigDir()
	if err != nil {
		return "", err
	}
	return filepath.Join(configdir, ".launcher_pipe"), nil
}

type License struct {
	Email      string `json:"email"`
	LicenseKey string `json:"key"`
}
