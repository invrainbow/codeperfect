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
	"darwin": "7668e2b9e3affccfb4034241bbf4fb0d25fd137916df20eb43d0b8f1801311e2",
	"darwin_arm": "df8af20d90ca9cf5f1b0e1b32499eb7370d3065fcb9b7c9c449ee2ce22e82c9d",
}

var VersionUpdateHashes = map[string]string{
	"darwin": "30936d93ae0f1b62d69e987d7f47c706ecca362078253f6ca1877b1b63228330",
	"darwin_arm": "4e9559ed724b1c418ce2d14ed4dc2dd7bcb4bd222ab55be97e42a0f789ddeb2f",
}
