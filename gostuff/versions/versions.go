package versions

const CurrentVersion = 2

type Version struct {
	URL  string
	Hash string
}

var ValidOSes = map[string]bool{
	"darwin":  true,
	"windows": true,
}

var VersionHashes = map[int]string{
	1: "1b62520341b1d00839a9ed921add4f144b6cb7e32d08dd903e5daca2fa9bdcce",
	2: "47abc6a57e4aae58895f7dbc9a989f1a0a0fff6a9dcbe7fac6cc9e2e585423d9",
}
