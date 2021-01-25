package main

import (
	"bufio"
	"go/build"
	"os"
	"path/filepath"
)

func main() {
	in := bufio.NewScanner(os.Stdin)
	out := bufio.NewWriter(os.Stdout)
	ctx := build.Default

	for in.Scan() {
		path := in.Text()
		match, err := ctx.MatchFile(filepath.Dir(path), filepath.Base(path))
		if err != nil {
			out.WriteByte(2)
		} else if match {
			out.WriteByte(1)
		} else {
			out.WriteByte(0)
		}
		out.Flush()
	}
}
