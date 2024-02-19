package main

import (
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"strings"
)

func die(s string) {
	fmt.Fprintln(os.Stderr, s)
	os.Exit(1)
}

func isFileOk(name string) bool {
	if !strings.HasSuffix(name, ".cpp") && !strings.HasSuffix(name, ".hpp") {
		return false
	}
	if strings.Contains(name, "imgui") {
		return false
	}
	if name == "fonts.cpp" || name == "fonts.hpp" {
		return false
	}
	return true
}

func main() {
	files, err := os.ReadDir("src")
	if err != nil {
		die("couldn't read dir")
	}

	names := []string{}
	for _, file := range files {
		if !file.IsDir() {
			name := file.Name()
			if isFileOk(name) {
				names = append(names, fmt.Sprintf("src/%s", name))
			}
		}
	}

	fmt.Printf("%s\n", strings.Join(names, " "))

	out, err := exec.Command("cloc", append([]string{"--json"}, names...)...).CombinedOutput()
	if err != nil {
		die("couldn't run cloc")
	}

	type Row struct {
		Blank   int `json:"blank"`
		Comment int `json:"comment"`
		Code	int `json:"code"`
	}

	var data map[string]Row
	if err := json.Unmarshal(out, &data); err != nil {
		die("couldn't read cloc output")
	}

	row := data["SUM"]
	fmt.Printf("Total: %d blank, %d comment, %d code\n", row.Blank, row.Comment, row.Code)
}
