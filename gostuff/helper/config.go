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
    Goroot string `json:"goroot"`
    Gopath string `json:"gopath"`
    Gomodcache string `json:"gomodcache"`
}

var config *Config

func init() {
	conf, err := readConfig()
	if err != nil {
		panic(err)
	}
	config = conf

	// update path with gobinarypath
	if config.GoBinaryPath != "" {
		path := os.Getenv("PATH")
		if path == "" {
			path = config.GoBinaryPath
		} else {
			path = fmt.Sprintf("%s:%s", config.GoBinaryPath, path)
		}
		os.Setenv("PATH", path)
	}
}

func readConfig() (*Config, error) {
	homedir, err := os.UserHomeDir()
	if err != nil {
		return nil, err
	}
	configfile := path.Join(homedir, ".cpconfig")

	buf, err := os.ReadFile(configfile)
	if err != nil {
		return nil, err
	}

	config = &Config{}
	if err := json.Unmarshal(buf, config); err != nil {
		return nil, err
	}
	return config, nil
}
