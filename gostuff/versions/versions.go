package versions

const CurrentVersion = 1

type Version struct {
	URL  string
	Hash string
}

var ValidOSes = map[string]bool{
	"darwin":  true,
	"windows": true,
}

var VersionHashes = map[int]string{
	1: "hurrdurr", // set this at release
}
