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
		1: "3b86a23f8e70f3769c71e39800f4f46f437c364d0ea16204d3a5252263a0edbc",
	},
	"darwin_arm": {
		1: "9e948230bc5e66870e1b98dce23366fe73bc5e2c1846b13faa34600bc8eb1c10",
	},
}

var VersionUpdateHashes = map[string]map[int]string{
	"darwin": {
		1: "75a615b27f95dc115a14de4a617a84655501c0c76ccba8894c80baefc9156f67",
	},
	"darwin_arm": {
		1: "052bfd9ccca68b3727ad7b76a7a6077c512a4a977df40a006462d25110f2c77b",
	},
}
