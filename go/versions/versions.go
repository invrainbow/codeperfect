package versions

import (
	"fmt"
)

const CurrentVersion = 220905

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
