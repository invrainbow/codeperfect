package main

import (
	"log"
	"os"
	"path/filepath"

	"github.com/codeperfect95/codeperfect/go/utils"
)

func main() {
	exepath, err := os.Executable()
	if err != nil {
		panic(err)
	}

	exedir := filepath.Dir(exepath)

	err = utils.ReplaceFolder(
		filepath.Join(exedir, "newbin"),
		filepath.Join(exedir, "bin"),
	)
	if err != nil {
		panic(err)
	}

	cmd := utils.MakeExecCommand(filepath.Join(exedir, idePath), os.Args[1:]...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	cmd.Env = os.Environ()
	cmd.Dir = filepath.Join(exedir, "bin")

	if err := cmd.Start(); err != nil {
		log.Printf("%v", err)
	}
}
