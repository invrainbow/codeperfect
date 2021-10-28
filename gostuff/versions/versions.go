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
	"darwin": "6979b473302eb1b3e4bfc6b99eb4f8afd536b3af8a3df298c735a8388e200890",
	"darwin_arm": "544a34b99e016b8a3a31bdedfe4b01d73749383de4bfde0b9af4758caffebee0",
}

var VersionUpdateHashes = map[string]string{
	"darwin": "8161181f29967bd00d47873fc43cef866473e6b44367f23eadc3fb160019611b",
	"darwin_arm": "c2957f076b339ed151ab157f8772e744710ae8d958e43d30002ab1be227d94f4",
}
