package helper

import (
	"encoding/json"
	"fmt"
	"os"
	"path"
)

type Config struct {
	GoBinaryPath string `json:"go_binary_path"`
	DelvePath    string `json:"delve_path"`
	Goroot       string `json:"goroot"`
	Gopath       string `json:"gopath"`
	Gomodcache   string `json:"gomodcache"`
}

var config *Config

func InitConfig() error {
	homedir, err := os.UserHomeDir()
	if err != nil {
		return err
	}
	configfile := path.Join(homedir, ".cpconfig")

	buf, err := os.ReadFile(configfile)
	if err != nil {
		return err
	}

	conf := &Config{}
	if err := json.Unmarshal(buf, conf); err != nil {
		return err
	}

	config = conf
	if config.GoBinaryPath != "" {
		path := os.Getenv("PATH")
		if path == "" {
			path = config.GoBinaryPath
		} else {
			path = fmt.Sprintf("%s:%s", config.GoBinaryPath, path)
		}
		os.Setenv("PATH", path)
	}

	return nil
}
