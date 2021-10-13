package helper

import (
	"bytes"
	"fmt"
	"go/format"
	"log"
	"os"
	"os/exec"
	"unsafe"

	"github.com/denormal/go-gitignore"
	"github.com/google/shlex"
	"github.com/invrainbow/codeperfect/gostuff/versions"
	"github.com/reviewdog/errorformat"
	"golang.org/x/tools/imports"
)

/*
#include <stdint.h>
#include <stdlib.h>

typedef struct _GH_Build_Error {
    char* text;
    int32_t is_valid;
    char *filename;
    int32_t line;
    int32_t col;
    int32_t is_vcol;
} GH_Build_Error;

typedef struct _GH_Message {
	char* text;
	char* title;
	int32_t is_panic;
} GH_Message;
*/
import "C"

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

var currentBuild *GoBuild = nil

func stopBuild() {
	if currentBuild == nil {
		return
	}

	proc := currentBuild.cmd.Process
	currentBuild = nil

	if proc != nil {
		proc.Kill()
	}
}

var LastError error

//export GHStartBuild
func GHStartBuild(cmdstr *C.char) bool {
	stopBuild()

	parts, err := shlex.Split(C.GoString(cmdstr))
	if err != nil {
		LastError = err
		return false
	}

	if len(parts) == 0 {
		LastError = fmt.Errorf("Build command was empty.")
		return false
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

	return true
}

//export GHStopBuild
func GHStopBuild() {
	stopBuild()
}

const (
	GHBuildInactive = iota
	GHBuildDone
	GHBuildRunning
)

//export GHFreeBuildStatus
func GHFreeBuildStatus(p unsafe.Pointer, n int) {
	errors := (*[1 << 30]C.GH_Build_Error)(p)
	for i := 0; i < n; i++ {
		it := &errors[i]

		C.free(unsafe.Pointer(it.text))
		if it.is_valid != 0 {
			C.free(unsafe.Pointer(it.filename))
		}
	}
	C.free(p)
}

//export GHGetBuildStatus
func GHGetBuildStatus(pstatus *int, plines *int) *C.GH_Build_Error {
	if currentBuild == nil {
		*pstatus = GHBuildInactive
		*plines = 0
		return nil
	}

	if !currentBuild.done {
		*pstatus = GHBuildRunning
		*plines = 0
		return nil
	}

	*pstatus = GHBuildDone
	*plines = len(currentBuild.errors)

	bufsize := C.size_t(*plines) * C.size_t(unsafe.Sizeof(C.GH_Build_Error{}))
	errorsptr := C.malloc(bufsize)

	errors := (*[1 << 30]C.GH_Build_Error)(errorsptr)
	for i, ent := range currentBuild.errors {
		out := &errors[i]
		out.text = C.CString(ent.Text)
		out.is_valid = C.int32_t(BoolToInt(ent.Valid))
		if out.is_valid != 0 {
			out.filename = C.CString(ent.Filename)
			out.line = C.int32_t(ent.Lnum)
			out.col = C.int32_t(ent.Col)
			out.is_vcol = C.int32_t(BoolToInt(ent.Vcol))
		}
	}

	return (*C.GH_Build_Error)(errorsptr)
}

//export GHGetGoEnv
func GHGetGoEnv(name *C.char) *C.char {
	cmd := fmt.Sprintf("go env %s", C.GoString(name))
	return C.CString(GetShellOutput(cmd))
}

//export GHFree
func GHFree(p unsafe.Pointer) {
	C.free(p)
}

var autofmtBuffer []byte = nil

//export GHFmtStart
func GHFmtStart() {
	autofmtBuffer = []byte{}
}

//export GHFmtAddLine
func GHFmtAddLine(line *C.char) {
	autofmtBuffer = append(autofmtBuffer, []byte(C.GoString(line))...)
	autofmtBuffer = append(autofmtBuffer, '\n')
}

const (
	FmtGoFmt                   = 0
	FmtGoImports               = 1
	FmtGoImportsWithAutoImport = 2
)

//export GHFmtFinish
func GHFmtFinish(fmtType int) *C.char {
	buflen := len(autofmtBuffer)
	if buflen > 0 {
		autofmtBuffer = autofmtBuffer[:buflen-1]
	}

	var newSource []byte
	var err error

	if fmtType == FmtGoFmt {
		newSource, err = format.Source(autofmtBuffer)
	} else if fmtType == FmtGoImports {
		newSource, err = imports.Process("<standard input>", autofmtBuffer, &imports.Options{
			TabWidth:   8,
			TabIndent:  true,
			Comments:   true,
			Fragment:   true,
			FormatOnly: true,
		})
	} else if fmtType == FmtGoImportsWithAutoImport {
		newSource, err = imports.Process("<standard input>", autofmtBuffer, &imports.Options{
			TabWidth:   8,
			TabIndent:  true,
			Comments:   true,
			Fragment:   true,
			FormatOnly: false,
		})
	} else {
		LastError = fmt.Errorf("Invalid format type.")
		return nil
	}

	if err != nil {
		LastError = fmt.Errorf("unable to format")
		return nil
	}

	return C.CString(string(newSource))
}

//export GHAuthAndUpdate
func GHAuthAndUpdate() {
	go AuthAndUpdate()
}

type GitignoreChecker struct {
	ignore gitignore.GitIgnore
}

var gitignoreChecker *GitignoreChecker

//export GHGitIgnoreInit
func GHGitIgnoreInit(repo *C.char) bool {
	ignore, err := gitignore.NewRepository(C.GoString(repo))
	if err != nil {
		LastError = err
		return false
	}

	gitignoreChecker = &GitignoreChecker{
		ignore: ignore,
	}

	return true
}

//export GHGitIgnoreCheckFile
func GHGitIgnoreCheckFile(file *C.char) bool {
	match := gitignoreChecker.ignore.Match(C.GoString(file))
	return match != nil && match.Ignore()
}

//export GHRenameFileOrDirectory
func GHRenameFileOrDirectory(oldpath, newpath *C.char) bool {
	return os.Rename(C.GoString(oldpath), C.GoString(newpath)) == nil
}

//export GHEnableDebugMode
func GHEnableDebugMode() {
	DebugModeFlag = true
}

//export GHGetVersion
func GHGetVersion() int {
	return versions.CurrentVersion
}

//export GHGetGoBinaryPath
func GHGetGoBinaryPath() *C.char { return C.CString(config.GoBinaryPath) }

//export GHGetDelvePath
func GHGetDelvePath() *C.char { return C.CString(config.DelvePath) }

//export GHGetGopath
func GHGetGopath() *C.char { return C.CString(config.Gopath) }

//export GHGetGoroot
func GHGetGoroot() *C.char { return C.CString(config.Goroot) }

//export GHGetGomodcache
func GHGetGomodcache() *C.char { return C.CString(config.Gomodcache) }

//export GHGetMessage
func GHGetMessage(p unsafe.Pointer) bool {
	select {
	case msg := <-MessageChan:
        log.Println("msg received")
        log.Println(msg)
		out := (*C.GH_Message)(p)
		out.text = C.CString(msg.Text)
		out.title = C.CString(msg.Title)
		out.is_panic = C.int32_t(BoolToInt(msg.IsPanic))
		return true
	default:
		return false
	}
}

//export GHFreeMessage
func GHFreeMessage(p unsafe.Pointer) {
	out := (*C.GH_Message)(p)
	C.free(unsafe.Pointer(out.text))
}

//export GHInitConfig
func GHInitConfig() bool {
	if err := InitConfig(); err != nil {
		log.Printf("error: %s\n", err)
		return false
	}
	return true
}
