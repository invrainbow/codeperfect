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
	"darwin_arm": "88a3e5ade369dba55045e9a921c749216dd948ddede29ccd9ac78bf4a9d67a8e",
}

var VersionUpdateHashes = map[string]string{
	"darwin":     "fce925373ddd58415f76bb74e01dc89aff2766a083e1c4549009b43d4deeacce",
	"darwin_arm": "28041ed2f9352f503ce37945ecf4c61dbf33689d2bd7181c3078e753ffc86f54",
}
