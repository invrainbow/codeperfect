package versions

import (
	"fmt"
)

const CurrentVersion = 230900

func CurrentVersionAsString() string {
	major := (CurrentVersion / 100) / 100
	minor := (CurrentVersion / 100) % 100
	patch := CurrentVersion % 100

	ret := fmt.Sprintf("%02d.%02d", major, minor)
	if patch > 0 {
		ret = fmt.Sprintf("%s.%d", ret, patch)
	}
	return ret
}
