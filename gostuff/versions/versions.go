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
	"darwin":     "6979b473302eb1b3e4bfc6b99eb4f8afd536b3af8a3df298c735a8388e200890",
	"darwin_arm": "43c823a333d1c581ee17fb0c44121773a5d0ff5ad990feb8374a8a7ee09f72c4",
}

var VersionUpdateHashes = map[string]string{
	"darwin":     "8161181f29967bd00d47873fc43cef866473e6b44367f23eadc3fb160019611b",
	"darwin_arm": "ebdeb3e15e8583c91ece8f95a878a286273835c6e69b5a931202dbfa0a0b9467",
}
