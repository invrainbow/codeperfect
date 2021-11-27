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
	"darwin":     "634aff619dee968dc810e6a6c852c8289c446b5bb7c8291f915600d6d872b4b2",
	"darwin_arm": "efcb7b2ba1eb7466ea2685510882c706eae7e1f5ef368c19ad3a53c131e70f6c",
}

var VersionUpdateHashes = map[string]string{
	"darwin":     "609b045811b4cbd4e0576777d0de1e8bf13daa787caa3f8cdbb904e7713f44f7",
	"darwin_arm": "c38bb1ff006dc38867b8d39c386c33dc984bcb3dbffd94e07413f02ebe219125",
}
