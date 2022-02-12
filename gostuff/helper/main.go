package main

import (
	"bytes"
	"encoding/gob"
	"fmt"
	"go/build"
	"go/format"
	"log"
	"net"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"runtime"
	"strings"
	"syscall"
	"time"
	"unsafe"

	"github.com/denormal/go-gitignore"
	"github.com/invrainbow/codeperfect/gostuff/models"
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

typedef struct _GH_Goenv_Init {
	char *error;
	int32_t init_done;
	int32_t init_error;
} GH_Goenv_Init;
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

	s := strings.TrimSpace(C.GoString(cmdstr))
	if len(s) == 0 {
		LastError = fmt.Errorf("Build command was empty.")
		return false
	}

	currentBuild = &GoBuild{}
	currentBuild.done = false
	currentBuild.cmd = exec.Command("/bin/bash", "-i", "-c", s)

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

//export GHGetGoEnvVar
func GHGetGoEnvVar(name *C.char) *C.char {
	cmd := fmt.Sprintf("%s env %s", config.GoBinaryPath, C.GoString(name))
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

const (
	AuthWaiting = iota
	AuthOk
	AuthInternetError
	AuthUnknownError
	AuthBadCreds
)

var authStatus int = AuthWaiting

//export GHGetAuthStatus
func GHGetAuthStatus() int {
	return authStatus
}

//export GHAuth
func GHAuth(rawEmail *C.char, rawLicenseKey *C.char) {
	license := &License{}
	license.Email = C.GoString(rawEmail)
	license.LicenseKey = C.GoString(rawLicenseKey)

	osSlug := runtime.GOOS
	req := &models.AuthRequest{
		OS:             osSlug,
		CurrentVersion: versions.CurrentVersion,
	}

	var resp models.AuthResponse

	doAuth := func() int {
		if err := CallServer("auth", license, req, &resp); err != nil {
			log.Println(err)
			switch err.(type) {
			case net.Error, *net.OpError, syscall.Errno:
				return AuthInternetError
			}
			return AuthUnknownError
		}
		if !resp.Success {
			return AuthBadCreds
		}
		return AuthOk
	}

	run := func() {
		authStatus = doAuth()
		if authStatus == AuthOk {
			// heartbeat
			for {
				req := &models.HeartbeatRequest{SessionID: resp.SessionID}
				var resp models.HeartbeatResponse
				CallServer("heartbeat", license, req, &resp)

				time.Sleep(time.Minute)
			}
		}
	}

	go run()
}

//export GHUpdate
func GHUpdate() {
	go Update()
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

//export GHGetConfigDir
func GHGetConfigDir() *C.char {
	configDir, err := PrepareConfigDir()
	if err != nil {
		return nil
	}
	return C.CString(configDir)
}

var buildctx struct {
	Context  build.Context
	InitDone bool
	InitErr  bool
	Err      error
}

//export GHGoenvInit
func GHGoenvInit() {
	setBuildError := func(err error) {
		log.Print(err)
		buildstuff.Err = err
		buildstuff.InitDone = true
		buildstuff.InitErr = true
	}

	initBuildStuff := func() {
		dirpath, err := os.Executable()
		if err != nil {
			setBuildError(err)
			return
		}

		filepath := path.Join(path.Dir(filepath), "buildcontext.go")
		log.Printf("using buildcontext.go at %s", filepath)

		out, err := exec.Command("go", "run", filepath).Output()
		if err != nil {
			setBuildError(err)
			return
		}

		dec := gob.NewDecoder(bytes.NewBuffer(out))
		if err := dec.Decode(&buildstuff.Context); err != nil {
			setBuildError(err)
			return
		}

		buildstuff.InitDone = true
	}
	go initBuildStuff()
}

//export GHGoenvGetStatus
func GHGoenvGetStatus() *C.GH_Gobool {
	return buildstuff.IsReady
}

//export GHGoenvCheckFileIncluded
func GHGoenvCheckFileIncluded(cpath *C.char) bool {
	if !(buildstuff.InitDone && !buildstuff.InitErr) {
		return false
	}
	path := C.GoString(cpath)
	match, err := buildstuff.Context.MatchFile(filepath.Dir(path), filepath.Base(path))
	if err != nil {
		log.Printf("%v", err)
		return false
	}
	return match
}

//export GHGoenvCheckMinGoVersion
func GHGoenvCheckMinGoVersion() bool {
	for _, it := range build.Default.ReleaseTags {
		if it == "go1.13" {
			return true
		}
	}
	return false
}

func main() {}
