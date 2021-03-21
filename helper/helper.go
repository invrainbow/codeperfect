package main

import (
	"bufio"
	"fmt"
	"go/build"
	"os"
	"path/filepath"
	"strconv"
	"strings"
)

const (
	OpInvalid = iota
	OpCheckIncludedInBuild
	OpResolveImportPath
	OpTest
)

func write(x interface{}) {
    fmt.Println(x)
}

func writeError(x error) {
    write("error")
    write(strings.ReplaceAll(x.Error(), "\n", "\\n"))
}

func main() {
	in := bufio.NewScanner(os.Stdin)
	// out := bufio.NewWriter(os.Stdout)
	ctx := build.Default

    /*
    write("running...")
    pkg, err := ctx.Import("c:/users/brandon/go/pkg/mod/k8s.io/api@v0.20.1/core/v1", "", build.FindOnly)
    if err != nil {
        writeError(err)
    } else {
        write(pkg.ImportPath)
    }
    return
    */

loop:
	for in.Scan() {
		op, err := strconv.Atoi(in.Text())
		if err != nil {
			writeError(err)
			break loop
		}

		switch op {
		case OpCheckIncludedInBuild:
			if !in.Scan() {
				break loop
			}
			path := in.Text()
			match, err := ctx.MatchFile(filepath.Dir(path), filepath.Base(path))
			if err != nil {
				writeError(err)
			} else if match {
				write("true")
			} else {
				write("false")
			}

		case OpResolveImportPath:
			if !in.Scan() {
				break loop
			}
			path := in.Text()
			pkg, err := ctx.Import(path, "", build.FindOnly)
			if err != nil {
				writeError(err)
			} else {
				write(pkg.Dir)
			}

        case OpTest:
			if !in.Scan() {
				break loop
			}
			path := in.Text()
            pkg, err := ctx.Import(path, "", 0)
            if err != nil {
                writeError(err)
                break
            }
            write(pkg.ImportPath)

		default:
			break loop
		}
	}
}
