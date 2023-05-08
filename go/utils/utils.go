package utils

import (
	"os"
	"path/filepath"
)

func ReplaceFolder(oldpath, newpath string) error {
	deletemePath := filepath.Join(filepath.Dir(newpath), "DELETEME")

	info, err := os.Stat(oldpath)
	if err != nil {
		if os.IsNotExist(err) {
			// oldpath doesn't exist, do nothing.
			return nil
		}
		return err
	} else if !info.IsDir() {
		// oldpath isn't a directory, fail condition.
		return nil
	}

	deleteme := false

	// check newpath
	//  - if it doesn't exist, keep going
	//  - if it's a directory, move it to ./DELETEME
	//  - if it's not a directory, fail condition
	info, err = os.Stat(newpath)
	if err != nil {
		if !os.IsNotExist(err) {
			return err
		}
	} else if !info.IsDir() {
		// newpath exists but is not a directory, fail condition.
		return nil
	} else {
		// newpath is a directory
		if err := os.Rename(newpath, deletemePath); err != nil {
			return err
		}
		deleteme = true
	}

	if err := os.Rename(oldpath, newpath); err != nil {
		if deleteme {
			// restore newpath
			os.Rename(deletemePath, newpath)
		}
		return err
	}

	if deleteme {
		// even if it fails, the move succeeded, we just have this extra directory here. let it pass
		os.RemoveAll(deletemePath)
	}

	return nil
}
