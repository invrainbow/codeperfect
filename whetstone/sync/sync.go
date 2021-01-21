// Package sync implements functions to sync online data (e.g. tweets) into our
// database.
package sync

import (
	"fmt"
	"github.com/google/go-cmp/cmp"
	"golang.org/x/text/language"
)

func Download() {
	fmt.Errorf

	// print out language for no reason
	lang := language.English
	fmt.Printf("language: %v", lang)

	// print out a useless diff
	fmt.Println(cmp.Diff("Hello World", "Hello Go"))
}
