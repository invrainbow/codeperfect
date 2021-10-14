package versions

const CurrentVersion = 2

type Version struct {
	URL  string
	Hash string
}

var ValidOSes = map[string]bool{
	"darwin":     true,
	"darwin_arm": true,
}

var AppHashes = map[string]string{
	"darwin": "0699914b53b68ec2736f496df8e2f8dc4fd2488b91ce42b73c54050dcc54a5e0",
	"darwin_arm": "1844d039b8b937b5c6eec6bc2631f748f394abdfa95e7f8cecd70543f38cead2",
}

var VersionUpdateHashes = map[string]string{
	"darwin": "1509a1b16149d9eb7f94b28cdf72928b3c2b5bf588b331e07103907584c35643",
	"darwin_arm": "eb684778795a625f04be0b5cbf5d6944bbe0401c00bfa27e95b206764bde20f1",
}
