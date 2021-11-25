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
	"darwin":     "86aa3fa6d6770cf7f5c41fa547a4704b9a1a26609312c8b6243a53ee4b53dd2e",
	"darwin_arm": "43c823a333d1c581ee17fb0c44121773a5d0ff5ad990feb8374a8a7ee09f72c4",
}

var VersionUpdateHashes = map[string]string{
	"darwin":     "4eb0e7d919e87529f25a1a607957b3c4826d3dacb950a8d6b7bafdd4674fd354",
	"darwin_arm": "ebdeb3e15e8583c91ece8f95a878a286273835c6e69b5a931202dbfa0a0b9467",
}
