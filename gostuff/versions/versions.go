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
	"darwin_arm": "3691e4058fab8fb98de8c20272aa1a3f699c5ebd13d949a15f2820fba84caf6a",
}

var VersionUpdateHashes = map[string]string{
	"darwin":     "97c6fd3924197c1cc963f68639d0901dd26ac40f0fe68acf77bc0c212a0fa2e5",
	"darwin_arm": "b025945ef93d3208a4fc9d643effd4c1d488af2733395318b40fb40d77b7b2db",
}
