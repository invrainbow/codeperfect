package versions

import (
	"fmt"
)

const CurrentVersion = 230603

func VersionToString(v int) string {
	major := (v / 100) / 100
	minor := (v / 100) % 100
	patch := v % 100

	ret := fmt.Sprintf("%02d.%02d", major, minor)
	if patch > 0 {
		ret = fmt.Sprintf("%s.%d", ret, patch)
	}
	return ret
}

func CurrentVersionAsString() string {
	return VersionToString(CurrentVersion)
}

func GetOSSlug(goos, goarch string) (string, error) {
	archvals := map[string]string{
		"amd64": "x64",
		"arm64": "arm",
	}

	osvals := map[string]string{
		"darwin":  "mac",
		"windows": "windows",
		"linux":   "linux",
	}

	if osval, ok := osvals[goos]; ok {
		if archval, ok := archvals[goarch]; ok {
			return fmt.Sprintf("%s-%s", osval, archval), nil
		}
	}

	return fmt.Sprintf("invalid-%s-%s", goos, goarch), fmt.Errorf("invalid goos/goarch")
}
