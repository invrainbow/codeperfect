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
			// out.WriteString(err.Error())
			// out.WriteString("\n")
			out.WriteByte(2)
		} else if match {
			// out.WriteString("match\n")
			out.WriteByte(1)
		} else {
			// out.WriteString("not match\n")
			out.WriteByte(0)
		}
		out.Flush()
	}
}
