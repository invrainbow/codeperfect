package versions

const CurrentVersion = 1

type Version struct {
	URL  string
	Hash string
}

var ValidOSes = map[string]bool{
	"darwin":     true,
	"darwin_arm": true,
	"windows":    true,
}

var VersionHashes = map[int]string{}
