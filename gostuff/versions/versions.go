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
	"darwin_arm": "efcb7b2ba1eb7466ea2685510882c706eae7e1f5ef368c19ad3a53c131e70f6c",
}

var VersionUpdateHashes = map[string]string{
	"darwin":     "97c6fd3924197c1cc963f68639d0901dd26ac40f0fe68acf77bc0c212a0fa2e5",
	"darwin_arm": "c38bb1ff006dc38867b8d39c386c33dc984bcb3dbffd94e07413f02ebe219125",
}
