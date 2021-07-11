package main

import (
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"strings"
)

func die(s string) {
	fmt.Fprintln(os.Stderr, "couldn't read dir")
	os.Exit(1)
}

func main() {
	files, err := os.ReadDir(".")
	if err != nil {
		die("couldn't read dir")
	}

	names := []string{}

	for _, file := range files {
		if file.IsDir() {
			continue
		}

		name := file.Name()

		if !strings.HasSuffix(name, ".cpp") && !strings.HasSuffix(name, ".hpp") {
			continue
		}

		if strings.Contains(name, "imgui") {
			continue
		}

		if name == "fonts.cpp" {
			continue
		}

		names = append(names, name)
	}

	cmd := exec.Command("cloc", append([]string{"--json"}, names...)...)
	out, err := cmd.CombinedOutput()
	if err != nil {
		die("couldn't run cloc")
	}

	type Row struct {
		Files   int `json:"nFiles"`
		Blank   int `json:"blank"`
		Comment int `json:"comment"`
		Code    int `json:"code"`
	}

	var data map[string]Row

	if err := json.Unmarshal(out, &data); err != nil {
		die("couldn't read cloc output")
	}

	for key, row := range data {
		if key == "header" || key == "SUM" {
			continue
		}
		fmt.Printf("%s: %d blank, %d comment, %d code\n", key, row.Blank, row.Comment, row.Code)
	}

	row := data["SUM"]
	fmt.Printf("Total: %d blank, %d comment, %d code\n", row.Blank, row.Comment, row.Code)
}
