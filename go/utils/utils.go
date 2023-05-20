package utils

import (
	"os"
	"path/filepath"
)

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
