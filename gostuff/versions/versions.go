package versions

const CurrentVersion = 3

type Version struct {
	URL  string
	Hash string
}

var ValidOSes = map[string]bool{
	"darwin":     true,
	"darwin_arm": true,
}

var AppHashes = map[string]string{
	"darwin":     "1ffd228a76fc28889861b525bed4a542ffbc09027a76f69bf87023189cc10676",
	"darwin_arm": "6d16e197fac87be91497228f0f10e556f4bd547b30e476751317e3804e6e6657",
}

var VersionUpdateHashes = map[string]string{
	"darwin":     "97c6fd3924197c1cc963f68639d0901dd26ac40f0fe68acf77bc0c212a0fa2e5",
	"darwin_arm": "7966bd81057c5e8b66345b8801e2e9b1e1d82c681d55911e3a9d4296db778c30",
}
