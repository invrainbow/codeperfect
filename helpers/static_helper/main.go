package main

import (
	"bytes"
	"fmt"
	"os/exec"

	"github.com/google/shlex"
	"github.com/invrainbow/ide/helpers/helperlib"
	"github.com/invrainbow/ide/helpers/lib"
	"github.com/reviewdog/errorformat"
)

type GoBuild struct {
	done   bool
	errors []*errorformat.Entry
	cmd    *exec.Cmd
}

func BoolToInt(b bool) int {
	if b {
		return 1
	}
	return 0
}

func main() {
	var currentBuild *GoBuild = nil

	stopBuild := func() {
		if currentBuild == nil {
			return
		}
		currentBuild.cmd.Process.Kill()
		currentBuild = nil
	}

	helperlib.InitScanner()
	for {
		switch helperlib.ReadLine() {
		case "set_directory":
			helperlib.HandleSetDirectory()

		case "start_build":
			stopBuild()

			parts, err := shlex.Split(helperlib.ReadLine())
			if err != nil {
				helperlib.WriteError(err)
				break
			}

			if len(parts) == 0 {
				helperlib.WriteError(fmt.Errorf("Build command was empty."))
				break
			}

			cmd := exec.Command(parts[0], parts[1:]...)

			currentBuild = &GoBuild{}
			currentBuild.done = false
			currentBuild.cmd = cmd

			go func(b *GoBuild) {
				out, err := b.cmd.CombinedOutput()

				shouldReadErrors := func() bool {
					if err != nil {
						if _, ok := err.(*exec.ExitError); ok {
							return true
						}
					}
					return len(out) > 0 && out[0] == '?'
				}

				if shouldReadErrors() {
					efm, _ := errorformat.NewErrorformat([]string{`%f:%l:%c: %m`})
					s := efm.NewScanner(bytes.NewReader(out))
					for s.Scan() {
						b.errors = append(b.errors, s.Entry())
					}
				}

				b.done = true
			}(currentBuild)

			helperlib.Write(true)

		case "get_build_status":
			if currentBuild == nil {
				helperlib.Write("inactive")
				break
			}

			if currentBuild.done {
				helperlib.Write("done")
				helperlib.Write(len(currentBuild.errors))
				for _, ent := range currentBuild.errors {
					helperlib.Write(ent.Text)
					helperlib.Write(BoolToInt(ent.Valid))
					if ent.Valid {
						helperlib.Write(ent.Filename)
						helperlib.Write(ent.Lnum)
						helperlib.Write(ent.Col)
						helperlib.Write(BoolToInt(ent.Vcol))
					}
				}
			} else {
				helperlib.Write("running")
			}

		case "stop_build":
			stopBuild()
			helperlib.Write(true)

		case "go_init":
			helperlib.Write(true)
			helperlib.Write(lib.GetShellOutput("go env GOPATH"))
			helperlib.Write(lib.GetShellOutput("go env GOROOT"))
			helperlib.Write(lib.GetShellOutput("go env GOMODCACHE"))
		}
	}
}
