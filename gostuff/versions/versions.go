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
	"darwin_arm": "e3a137959e6b9077b95c1620cd5451d5847f7f62251ef31d2737e40b3b0be140",
}
