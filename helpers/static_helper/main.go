package main

import (
	"bytes"
	"fmt"
	"os/exec"

	"github.com/google/shlex"
	"github.com/invrainbow/ide/helpers"
	"github.com/reviewdog/errorformat"
)

type GoBuild struct {
	done   bool
	errors []*errorformat.Entry
	cmd    *exec.Cmd
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

	helpers.MainLoop(func(op int) {
		switch op {
		case helpers.OpSetDirectory:
			helpers.HandleOpSetDirectory()

		case helpers.OpStartBuild:
			stopBuild()

			parts, err := shlex.Split(helpers.ReadLine())
			if err != nil {
				helpers.WriteError(err)
				break
			}

			if len(parts) == 0 {
				helpers.WriteError(fmt.Errorf("Build command was empty."))
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

			helpers.Write(true)

		case helpers.OpGetBuildStatus:
			if currentBuild == nil {
				helpers.Write("inactive")
				break
			}

			if currentBuild.done {
				helpers.Write("done")
				helpers.Write(len(currentBuild.errors))
				for _, ent := range currentBuild.errors {
					helpers.Write(ent.Text)
					helpers.Write(helpers.BoolToInt(ent.Valid))
					if ent.Valid {
						helpers.Write(ent.Filename)
						helpers.Write(ent.Lnum)
						helpers.Write(ent.Col)
						helpers.Write(helpers.BoolToInt(ent.Vcol))
					}
				}
			} else {
				helpers.Write("running")
			}

		case helpers.OpStopBuild:
			stopBuild()
			helpers.Write(true)

		default:
			return
		}
	})
}
