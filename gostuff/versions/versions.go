package versions

const CurrentVersion = 4

type Version struct {
	URL  string
	Hash string
}

var ValidOSes = map[string]bool{
	"darwin":     true,
	"darwin_arm": true,
}

var AppHashes = map[string]string{
	"darwin":     "d02b979989fa5251bfc8c32b6991690470127a4609e0de05b239904e9435a68c",
	"darwin_arm": "6d16e197fac87be91497228f0f10e556f4bd547b30e476751317e3804e6e6657",
}

var VersionUpdateHashes = map[string]string{
	"darwin":     "f7d4b208f7a2808484aab3791bad8f2dc94d440cc08395d6eb0c13ee97507994",
	"darwin_arm": "7966bd81057c5e8b66345b8801e2e9b1e1d82c681d55911e3a9d4296db778c30",
}
