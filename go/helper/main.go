package main

import (
	"bytes"
	"encoding/gob"
	"fmt"
	"go/build"
	"go/format"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"unsafe"

	"github.com/invrainbow/codeperfect/go/versions"
	"github.com/denormal/go-gitignore"
	"github.com/fatih/structtag"
	"github.com/pkg/browser"
	"github.com/reviewdog/errorformat"
	gofumpt "mvdan.cc/gofumpt/format"
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
	char *text;
	char *title;
	int32_t is_panic;
} GH_Message;

typedef struct _GH_Env_Vars {
	char *goroot;
	char *gomodcache;
} GH_Env_Vars;
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
	currentBuild.cmd = makeShellCommand(s)

	go func(b *GoBuild) {
		out, err := b.cmd.CombinedOutput()

		log.Print("build output & error")
		log.Println(string(out))
		log.Println(err)

		shouldReadErrors := func() bool {
			if _, ok := err.(*exec.ExitError); ok {
				return true
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

//export GHFmtFinish
func GHFmtFinish(useGofumpt bool) *C.char {
	buflen := len(autofmtBuffer)
	if buflen > 0 {
		autofmtBuffer = autofmtBuffer[:buflen-1]
	}

	var newSource []byte
	var err error

	if useGofumpt {
		newSource, err = gofumpt.Source(autofmtBuffer, gofumpt.Options{})
	} else {
		newSource, err = format.Source(autofmtBuffer)
	}

	if err != nil {
		LastError = fmt.Errorf("unable to format: %v", err)
		return nil
	}

	return C.CString(string(newSource))
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

//export GHGetLastError
func GHGetLastError() *C.char {
	if LastError == nil {
		return nil
	}
	return C.CString(LastError.Error())
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

//export GHGetVersionString
func GHGetVersionString() *C.char {
	return C.CString(versions.CurrentVersionAsString())
}

func GetBinaryPath(bin string) (string, error) {
	out, err := makeFindBinaryPathCommand(bin).Output()
	if err != nil {
		log.Print(err)
		if e, ok := err.(*exec.ExitError); ok {
			log.Printf("%s", string(e.Stderr))
		}
		return "", err
	}
	return strings.TrimSpace(string(out)), nil
}

func readCpgobin() string {
	homedir, err := os.UserHomeDir()
	if err != nil {
		return ""
	}

	s, err := ReadFile(filepath.Join(homedir, ".cpgobin"))
	if err != nil || s == "" {
		return ""
	}
	s = strings.TrimSpace(s)

	fi, err := os.Stat(s)
	if err != nil {
		return ""
	}

	if fi.IsDir() {
		return ""
	}

	return s
}

func actuallyGetGoBinaryPath() string {
	ret := readCpgobin()
	if ret != "" {
		return ret
	}

	ret, err := GetBinaryPath("go")
	if err != nil {
		return ""
	}
	return ret
}

var cachedGoBinPath string

func GetGoBinaryPath() string {
	if cachedGoBinPath != "" {
		return cachedGoBinPath
	}
	cachedGoBinPath = actuallyGetGoBinaryPath()
	return cachedGoBinPath
}

//export GHGetGoBinaryPath
func GHGetGoBinaryPath() *C.char {
	ret := GetGoBinaryPath()
	if ret == "" {
		return nil
	}
	return C.CString(ret)
}

//export GHGetDelvePath
func GHGetDelvePath() *C.char {
	ret, err := GetBinaryPath("dlv")
	if err != nil {
		log.Print(err)
		return nil
	}
	return C.CString(ret)
}

//export GHGetGoEnvVars
func GHGetGoEnvVars() *C.GH_Env_Vars {
	binpath := GetGoBinaryPath()
	if binpath == "" {
		log.Print("couldn't find go")
		return nil
	}
	out, err := MakeExecCommand(binpath, "env", "GOMODCACHE", "GOROOT").Output()
	if err != nil {
		log.Print(err)
		return nil
	}

	lines := strings.Split(strings.ReplaceAll(string(out), "\r\n", "\n"), "\n")
	if len(lines) < 3 {
		log.Printf("invalid output from go env: %s", out)
		return nil
	}

	vars := (*C.GH_Env_Vars)(C.malloc(C.size_t(unsafe.Sizeof(C.GH_Env_Vars{}))))
	vars.gomodcache = C.CString(lines[0])
	vars.goroot = C.CString(lines[1])
	return vars
}

//export GHFreeGoEnvVars
func GHFreeGoEnvVars(vars *C.GH_Env_Vars) {
	if vars.goroot != nil {
		C.free(unsafe.Pointer(vars.goroot))
	}
	if vars.gomodcache != nil {
		C.free(unsafe.Pointer(vars.gomodcache))
	}
	C.free(unsafe.Pointer(vars))
}

//export GHGetConfigDir
func GHGetConfigDir() *C.char {
	configDir, err := PrepareConfigDir()
	if err != nil {
		return nil
	}
	return C.CString(configDir)
}

type BuildEnvStatus int

const (
	BuildEnvWaiting = iota
	BuildEnvDone
	BuildEnvError
)

var buildenv struct {
	Context build.Context
	Ok      bool
}

func getBuildContextFile() (string, error) {
	dir, err := GetConfigDir()
	if err != nil {
		return "", err
	}
	return filepath.Join(dir, ".gobuildcontext"), nil
}

func readBuildContextFromDisk(out *build.Context) error {
	fullpath, err := getBuildContextFile()
	if err != nil {
		return err
	}
	f, err := os.Open(fullpath)
	if err != nil {
		return err
	}
	return gob.NewDecoder(f).Decode(out)
}

func saveBuildContextToDisk() error {
	fullpath, err := getBuildContextFile()
	if err != nil {
		return err
	}

	f, err := os.Create(fullpath)
	if err != nil {
		return err
	}

	enc := gob.NewEncoder(f)
	return enc.Encode(&buildenv.Context)
}

func getBuildContext() bool {
	if err := readBuildContextFromDisk(&buildenv.Context); err == nil {
		return true
	}

	exepath, err := os.Executable()
	if err != nil {
		log.Print(err)
		return false
	}

	dirpath := filepath.Dir(exepath)
	filepath := filepath.Join(dirpath, "buildcontext.go")

	log.Printf("pathsep = %v", os.PathSeparator)
	log.Printf("exepath = %s", exepath)
	log.Printf("dirpath = %s", dirpath)
	log.Printf("using buildcontext.go at %s", filepath)

	binpath := GetGoBinaryPath()
	if binpath == "" {
		return false
	}

	log.Printf("binpath: %s", binpath)

	cmd := MakeExecCommand(binpath, "run", filepath)
	cmd.Dir = dirpath
	out, err := cmd.Output()
	if err != nil {
		log.Print(err)
		log.Print(out)
		if err2, ok := err.(*exec.ExitError); ok {
			log.Print(string(err2.Stderr))
		}
		return false
	}

	dec := gob.NewDecoder(bytes.NewBuffer(out))
	if err := dec.Decode(&buildenv.Context); err != nil {
		log.Print(err)
		return false
	}

	if err := saveBuildContextToDisk(); err != nil {
		log.Print(err)
		// couldn't write to disk, but we do have the context, so don't fail
	}

	return true
}

//export GHBuildEnvInit
func GHBuildEnvInit() bool {
	buildenv.Ok = getBuildContext()
	return buildenv.Ok
}

//export GHBuildEnvIsFileIncluded
func GHBuildEnvIsFileIncluded(cpath *C.char) bool {
	if !buildenv.Ok {
		return false
	}
	path := C.GoString(cpath)
	match, err := buildenv.Context.MatchFile(filepath.Dir(path), filepath.Base(path))
	if err != nil {
		log.Printf("%v", err)
		return false
	}
	return match
}

//export GHBuildEnvGoVersionSupported
func GHBuildEnvGoVersionSupported() bool {
	if !buildenv.Ok {
		return false
	}
	for _, it := range buildenv.Context.ReleaseTags {
		if it == "go1.13" {
			return true
		}
	}
	return false
}

//export GHHasTag
func GHHasTag(tagstr, lang *C.char, ok *bool) bool {
	tags, err := structtag.Parse(C.GoString(tagstr))
	if err != nil {
		*ok = false
		return false
	}

	tag, _ := tags.Get(C.GoString(lang))
	*ok = true
	return tag != nil
}

// TODO: figure out how to use multiple return values instead of *bool param
//
//export GHAddTag
func GHAddTag(tagstr, lang, tagname *C.char, ok *bool) *C.char {
	tags, err := structtag.Parse(C.GoString(tagstr))
	if err != nil {
		*ok = false
		return nil
	}

	tag, _ := tags.Get(C.GoString(lang))
	if tag == nil {
		tags.Set(&structtag.Tag{
			Key:     C.GoString(lang),
			Name:    C.GoString(tagname),
			Options: []string{},
		})
	}

	return C.CString(tags.String())
}

//export GHOpenURLInBrowser
func GHOpenURLInBrowser(url *C.char) bool {
	err := browser.OpenURL(C.GoString(url))
	return err != nil
}

//export GHGetGoWork
func GHGetGoWork(filepath *C.char) *C.char {
	binpath := GetGoBinaryPath()
	if binpath == "" {
		return nil
	}

	cmd := MakeExecCommand(binpath, "env", "GOWORK")
	cmd.Dir = C.GoString(filepath)

	out, err := cmd.Output()
	if err != nil {
		return nil
	}

	ret := strings.TrimSpace(string(out))
	if ret == "" {
		return nil
	}

	return C.CString(ret)
}

func main() {}
