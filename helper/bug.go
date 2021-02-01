package main

import (
	"bufio"
	"fmt"
	"go/build"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"encoding/binary"
	"bytes"

	"github.com/reviewdog/errorformat"
)

func parseErrors(buildOutput string) {
	efm, _ := errorformat.NewErrorformat([]string{
		"%-G# %.%#",
		"%-G%.%#panic: %m",
		"%f:%l:%c: %m",
		"%Ecan't load package: %m",
		"%C%*\\s%m",
		"%-G%.%#",
	})

	scan := efm.NewScanner(strings.NewReader(buildOutput))
	var arr []errorformat.Entry
	for scan.Scan() {
		arr = append(arr, scan
	}
}

