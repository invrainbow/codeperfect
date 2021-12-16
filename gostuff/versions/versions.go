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
	"darwin":     "4f82a1918f772aede98c5c46ccccb208f0d9d5c893d664e7db199d1c32170433",
	"darwin_arm": "b5bb391be9bd8ebfd2c91e68a9408bf24286a3c6a445196b847950e0927864ec",
}
