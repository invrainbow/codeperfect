package main

import (
	"bytes"
	"fmt"
	"os/exec"

	"github.com/google/shlex"
	"github.com/invrainbow/ide/helper"
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

	helper.MainLoop(func(op int) {
		switch op {
		case helper.OpSetDirectory:
			helper.HandleOpSetDirectory()

		case helper.OpStartBuild:
			stopBuild()

			parts, err := shlex.Split(helper.ReadLine())
			if err != nil {
				helper.WriteError(err)
				break
			}

			if len(parts) == 0 {
				helper.WriteError(fmt.Errorf("Build command was empty."))
				break
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

			helper.Write(true)

		case helper.OpGetBuildStatus:
			if currentBuild == nil {
				helper.Write("inactive")
				break
			}

			if currentBuild.done {
				helper.Write("done")
				helper.Write(len(currentBuild.errors))
				for _, ent := range currentBuild.errors {
					helper.Write(ent.Text)
					helper.Write(helper.BoolToInt(ent.Valid))
					if ent.Valid {
						helper.Write(ent.Filename)
						helper.Write(ent.Lnum)
						helper.Write(ent.Col)
						helper.Write(helper.BoolToInt(ent.Vcol))
					}
				}
			} else {
				helper.Write("running")
			}

		case helper.OpStopBuild:
			stopBuild()
			helper.Write(true)

		default:
			return
		}
	})
}
