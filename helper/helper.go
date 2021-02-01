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

func x() {
	testFoo := 1
	if testFoo == 1 {
		testBar := 2
		if testBar == 2 {
			testBaz := 3

		}
	}
}

type Oper uint8

const (
	OperBuild Oper = iota
	OperCheckIsFileBuilt
)

type Reader struct {
	rdr *bufio.Reader
}

func (r *Reader) ReadInto(n int, out interface{}) error {
	return binary.Read(bytes.NewReader(r.ReadN(n)), binary.LittleEndian, out)
}

func NewReader() *Reader {
	return &Reader{
		rdr: bufio.NewReader(os.Stdin),
	}
}

func (r *Reader) ReadN(n int) ([]uint8, error) {
	arr := make([]uint8, n)
	num, err := r.rdr.Read(arr)
	if err != nil {
		return nil, err
	}
	if num != n {
		return nil, fmt.Errorf("We were unable to read %d bytes, read %d instead.", n, num)
	}
	return arr, nil
}

func (r *Reader) Read1() (uint8, error) {
	return r.rdr.ReadByte()
}

func (r *Reader) Read2() (uint16, error) {
	var ret uint16
	if err := r.ReadInto(2, ret); err != nil {
		return 0, err
	}
	return ret, nil
}

func (r *Reader) Read4() (uint16, error) {
	var ret uint16
	if err := r.ReadInto(4, ret); err != nil {
		return 0, err
	}
	return ret, nil
}

func (r *Reader) ReadString() (string, error) {
	len, err := r.Read2()
	if err != nil {
		return "", err
	}
	
	arr, err := r.ReadN(len)
	if err != nil {
		return "", err
	}
	
	return string(arr), nil
}

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

func main() {
	in := NewReader()
	out := bufio.NewWriter(os.Stdout)
	
	for {
		op, err := in.Read1()
		if err != nil {
			panic(err)
		}

		switch op {
		case OperBuild:
			numParts, err := r.Read2()
			if err != nil {
				panic(err)
			}
			var arr string[]
			for i := 0; i < numParts; i++ {
				s, err := r.ReadString():
				if err != nil {
					panic(err)
				}
				arr = append(arr, s)
			}
			
			cmd := exec.Command(arr[0], arr[1:]...)
			out, err := cmd.CombinedOutput()
			if err != nil {
				panic(err)
			}
			
		case OperCheckIsFileBuilt:
			path, err := r.ReadString()
			if err != nil {
				panic(err)
			}
			match, err := build.Default.MatchFile(filepath.Dir(path), filepath.Base(path))
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
}
