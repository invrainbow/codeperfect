package versions

const CurrentVersion = 1

type Version struct {
	URL  string
	Hash string
}

var ValidOSes = map[string]bool{
	"darwin":     true,
	"darwin_arm": true,
	// "windows": true,
}

var VersionAppHashes = map[string]map[int]string{
	"darwin": {
		1: "9cf07d61d03c099c84c3a0aef53fdbd5e726891d9fd3c0db3581cbfd20e309e4",
	},
	"darwin_arm": {
		1: "",
	},
}

var VersionUpdateHashes = map[string]map[int]string{
	"darwin": {
		1: "dfe96ae93b5e3f5ace1985cee4c88e5817b99c6741d93825ee18a62ea551a613",
	},
	"darwin_arm": {
		1: "",
	},
}
