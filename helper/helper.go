package main

import (
	"bufio"
	"fmt"
	"go/build"
	"os"
    "os/exec"
	"path/filepath"
	"strconv"
	"strings"
    "github.com/google/shlex"
    "github.com/reviewdog/errorformat"
    "bytes"
)

const (
	OpInvalid = iota
	OpCheckIncludedInBuild
	OpResolveImportPath
	OpTest
    OpStartBuild
    OpGetBuildStatus
    OpStopBuild
)

func write(x interface{}) {
    fmt.Println(x)
}

func writeError(x error) {
    write("error")
    write(strings.ReplaceAll(x.Error(), "\n", "\\n"))
}

func boolToInt(b bool) int {
    if b {
        return 1
    }
    return 0
}

type GoBuild struct {
    done bool
    errors []*errorformat.Entry
    cmd *exec.Cmd
}

func main() {
    // os.Chdir("c:/users/brandon/cryptopals")

	in := bufio.NewScanner(os.Stdin)
	ctx := build.Default
    var currentBuild *GoBuild = nil

    readLine := func() string {
        if !in.Scan() {
            panic("unable to read line")
        }
        return in.Text()
    }

    stopBuild := func() {
        if currentBuild == nil {
            return
        }

        currentBuild.cmd.Process.Kill()
        currentBuild = nil
    }

loop:
	for in.Scan() {
		op, err := strconv.Atoi(in.Text())
		if err != nil {
			writeError(err)
			break loop
		}

		switch op {
		case OpCheckIncludedInBuild:
            path := readLine()
			match, err := ctx.MatchFile(filepath.Dir(path), filepath.Base(path))
			if err != nil {
				writeError(err)
			} else if match {
				write("true")
			} else {
				write("false")
			}

		case OpResolveImportPath:
			path := readLine()
			pkg, err := ctx.Import(path, "", build.FindOnly)
			if err != nil {
				writeError(err)
			} else {
				write(pkg.Dir)
			}

        case OpTest:
			path := readLine()
            pkg, err := ctx.Import(path, "", 0)
            if err != nil {
                writeError(err)
                break
            }
            write(pkg.ImportPath)

        case OpStartBuild:
            stopBuild()

            parts, err := shlex.Split(readLine())
            if err != nil {
                writeError(err)
            }
            cmd := exec.Command(parts[0], parts[1:]...)

            currentBuild = &GoBuild{}
            currentBuild.done = false
            currentBuild.cmd = cmd

            go func(b *GoBuild) {
                out, err := b.cmd.CombinedOutput()
                if err != nil {
                    if _, ok := err.(*exec.ExitError); ok {
                        efm, _ := errorformat.NewErrorformat([]string{`%f:%l:%c: %m`})
                        s := efm.NewScanner(bytes.NewReader(out))
                        for s.Scan() {
                            b.errors = append(b.errors, s.Entry())
                        }
                    }
                }
                b.done = true
            }(currentBuild)

            write("true")

        case OpGetBuildStatus:
            if currentBuild == nil {
                write("inactive")
                break
            }

            if currentBuild.done {
                write("done")
                write(len(currentBuild.errors))
                for _, ent := range currentBuild.errors {
                    write(ent.Text)
                    write(boolToInt(ent.Valid))
                    if ent.Valid {
                        write(ent.Filename)
                        write(ent.Lnum)
                        write(ent.Col)
                        write(boolToInt(ent.Vcol))
                    }
                }
            } else {
                write("running")
            }

        case OpStopBuild:
            stopBuild()
            write("true")

		default:
			break loop
		}
	}
}
