package main

import (
	"fmt"
	"log"
	"os"
	"os/exec"
	"path"
)

func isDir(path string) bool {
	info, err := os.Stat("./newbin")
	return err == nil && info.IsDir()
}

/*
This checks if:

 - newbin exists
 - newbin is a directory
 - bin either doesn't exist, or is a directory

iff this is true, delete bin and mv newbin bin.
*/

func replaceBinFolder(basedir string) error {
	newbinPath := path.Join(basedir, "newbin")
	binPath := path.Join(basedir, "bin")
	deletemePath := path.Join(basedir, "DELETEME")

	info, err := os.Stat(newbinPath)
	if err != nil {
		if os.IsNotExist(err) {
			// newbin doesn't exist, fail condition.
			return nil
		}
		return err
	} else if !info.IsDir() {
		// newbin isn't a directory, fail condition.
		return nil
	}

	deleteme := false

	// check bin
	//  - if it doesn't exist, keep going
	//  - if it's a directory, move it to ./DELETEME
	//  - if it's not a directory, fail condition
	info, err = os.Stat(binPath)
	if err != nil {
		if !os.IsNotExist(err) {
			return err
		}
	} else if !info.IsDir() {
		// bin exists but is not a directory, fail condition.
		return nil
	} else {
		// bin is a directory
		if err := os.Rename(binPath, deletemePath); err != nil {
			return err
		}
		deleteme = true
	}

	if err := os.Rename(newbinPath, binPath); err != nil {
		if deleteme {
			// restore ./bin
			os.Rename(deletemePath, binPath)
		}
		return err
	}

	if deleteme {
		// even if it fails, the move succeeded, we just have this extra directory here. let it pass
		os.RemoveAll(deletemePath)
	}

	return nil
}

func Run() error {
	exepath, err := os.Executable()
	if err != nil {
		log.Printf("error: %v\n", err)
		return err
	}

	exedir := path.Dir(exepath)
	if err := replaceBinFolder(exedir); err != nil {
		log.Printf("error: %v\n", err)
		return err
	}

	cmd := exec.Command(path.Join(exedir, "bin/ide"), os.Args[1:]...)
	cmd.Env = os.Environ()
	cmd.Dir = path.Join(exedir, "bin")
	if err := cmd.Start(); err != nil {
		log.Printf("error: %v\n", err)
		return err
	}

	log.Printf("process created with pid %v\n", cmd.Process.Pid)
	return nil
}

func main() {
	if err := Run(); err != nil {
		fmt.Fprintf(os.Stderr, "error: %v\n", err)
		os.Exit(1)
		return
	}
}
