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
		1: "271607e388c63709c983ddad95d8d6d22dc29478197be06c3eedbad9ec8fe7c6",
	},
	"darwin_arm": {
		1: "9e948230bc5e66870e1b98dce23366fe73bc5e2c1846b13faa34600bc8eb1c10",
	},
}

var VersionUpdateHashes = map[string]map[int]string{
	"darwin": {
		1: "a075f99c0399a08254f3b87be434e22bfbdc7a64c1397a65aea2a1806629b4fc",
	},
	"darwin_arm": {
		1: "052bfd9ccca68b3727ad7b76a7a6077c512a4a977df40a006462d25110f2c77b",
	},
}
