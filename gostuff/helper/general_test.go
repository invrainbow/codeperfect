package helper

import "testing"

func TestAuthAndUpdate(t *testing.T) {
	if err := ActuallyAuthAndUpdate(); err != nil {
		t.Fatal(err)
	}
}
