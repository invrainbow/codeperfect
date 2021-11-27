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
	"darwin":     "cf54b4f207de85f1e217c9eac1d43ad48888dfc4f89141c5b933cc87f688c01d",
	"darwin_arm": "efcb7b2ba1eb7466ea2685510882c706eae7e1f5ef368c19ad3a53c131e70f6c",
}

var VersionUpdateHashes = map[string]string{
	"darwin":     "fce925373ddd58415f76bb74e01dc89aff2766a083e1c4549009b43d4deeacce",
	"darwin_arm": "c38bb1ff006dc38867b8d39c386c33dc984bcb3dbffd94e07413f02ebe219125",
}
