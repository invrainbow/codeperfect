package main

import (
	"io"
	"os"
	"path/filepath"

	"github.com/codeperfect95/codeperfect/go/utils"
)

// read file located in same folder as executable
func ReadFileFromExeFolder(filename string) ([]byte, error) {
	exepath, err := os.Executable()
	if err != nil {
		return nil, err
	}
	return os.ReadFile(filepath.Join(filepath.Dir(exepath), filename))
}

func PrepareConfigDir() (string, error) {
	appdir, err := utils.GetConfigDir()
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
