package launcher

import (
	"os"
	"path"
)

func Run() {
	exepath, err := os.Executable()
	if err != nil {
		return err
	}

	exedir := path.Dir(exepath)
	newpath := path.Join(exedir, "newbin")

	info, err := os.Stat("/path/to/whatever")
	if err == nil && info.IsDir() {
		os.Rename("./bin", "./DELETEME")
		os.Rename("./newbin", "./bin")
		os.RemoveAll("./DELETEME")
	}

	attr := &os.ProcAttr{
		Dir: "./bin/ide",
		Env: os.Environ(),
	}
	proc, err := os.StartProcess("./bin/ide", nil, attr)
	if err != nil {
		return err
	}

	if err := proc.Release(); err != nil {
		return err
	}
}
