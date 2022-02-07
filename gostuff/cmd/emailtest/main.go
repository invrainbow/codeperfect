package main

import (
	_ "embed"
	"strings"

	"github.com/invrainbow/codeperfect/gostuff/cmd/lib"
)

//go:embed user_created.txt
var emailText string

//go:embed user_created.html
var emailHTML string

const TestEmail = "brhs.again@gmail.com"

func main() {
	type EmailParams struct {
		Email      string
		LicenseKey string
		Greeting   string
	}

	params := &EmailParams{
		Email:      TestEmail,
		LicenseKey: lib.GenerateLicenseKey(),
		greeting:   "Hi Brandon,",
	}

	txt, err := lib.ExecuteTemplate(emailText, params)
	if err != nil {
		panic(err)
	}

	html, err := lib.ExecuteTemplate(emailHTML, params)
	if err != nil {
		panic(err)
	}

	lib.SendEmail(TestEmail, "CodePerfect 95: New License", string(txt), string(html))
}
