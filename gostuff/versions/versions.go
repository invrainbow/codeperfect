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
	"darwin_arm": "43c823a333d1c581ee17fb0c44121773a5d0ff5ad990feb8374a8a7ee09f72c4",
}

var VersionUpdateHashes = map[string]string{
	"darwin":     "fce925373ddd58415f76bb74e01dc89aff2766a083e1c4549009b43d4deeacce",
	"darwin_arm": "ebdeb3e15e8583c91ece8f95a878a286273835c6e69b5a931202dbfa0a0b9467",
}
