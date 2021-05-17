package helper

import (
	"bufio"
	"fmt"
	"os"
	"os/exec"
	"strconv"
	"strings"

	"github.com/google/shlex"
)

const (
	OpInvalid = iota
	OpSetDirectory
	OpCheckIncludedInBuild
	OpStartBuild
	OpGetBuildStatus
	OpStopBuild
	OpGetGoEnvVars
)

var scanner *bufio.Scanner

func Write(x interface{}) {
	fmt.Println(x)
}

func WriteError(x error) {
	Write("error")
	Write(strings.ReplaceAll(x.Error(), "\n", "\\n"))
}

func BoolToInt(b bool) int {
	if b {
		return 1
	}
	return 0
}

func GetShellOutput(cmd string) string {
	parts, err := shlex.Split(cmd)
	if err != nil {
		return ""
	}
	out, err := exec.Command(parts[0], parts[1:]...).Output()
	if err != nil {
		return ""
	}
	return strings.TrimSpace(string(out))
}

func ReadLine() string {
	if !scanner.Scan() {
		panic("unable to read line")
	}
	return scanner.Text()
}

func InitScanner() {
	scanner = bufio.NewScanner(os.Stdin)
}

func HandleOpSetDirectory() {
	path := ReadLine()

	if err := os.Chdir(path); err != nil {
		WriteError(err)
		return
	}

	Write(true)
}

func MainLoop(f func(op int)) {
	InitScanner()

	for {
		op, err := strconv.Atoi(ReadLine())
		if err != nil {
			WriteError(err)
			break
		}
		f(op)
	}
}
