package helperlib

import (
	"bufio"
	"fmt"
	"os"
	"strings"
)

var scanner *bufio.Scanner

func Write(x interface{}) {
	fmt.Println(x)
}

func WriteError(x error) {
	Write("error")
	Write(strings.ReplaceAll(x.Error(), "\n", "\\n"))
}

func InitScanner() {
	scanner = bufio.NewScanner(os.Stdin)
}

func ReadLine() string {
	if !scanner.Scan() {
		panic("unable to read line")
	}
	return scanner.Text()
}

func HandleSetDirectory() {
	path := ReadLine()

	if err := os.Chdir(path); err != nil {
		WriteError(err)
		return
	}

	Write(true)
}
