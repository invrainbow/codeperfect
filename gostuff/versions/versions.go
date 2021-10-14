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
	"darwin_arm": "9e948230bc5e66870e1b98dce23366fe73bc5e2c1846b13faa34600bc8eb1c10",
}

var VersionUpdateHashes = map[string]string{
	"darwin": "30936d93ae0f1b62d69e987d7f47c706ecca362078253f6ca1877b1b63228330",
	"darwin_arm": "052bfd9ccca68b3727ad7b76a7a6077c512a4a977df40a006462d25110f2c77b",
}
