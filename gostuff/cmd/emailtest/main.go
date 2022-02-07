package main

import (
	_ "embed"

	"github.com/invrainbow/codeperfect/gostuff/cmd/lib"
)

//go:embed user_created.txt
var userCreatedText string

//go:embed user_created.html
var userCreatedHtml string

//go:embed user_enabled.txt
var userEnabledText string

//go:embed user_enabled.html
var userEnabledHtml string

//go:embed user_disabled.txt
var userDisabledText string

//go:embed user_disabled.html
var userDisabledHtml string

const TestEmail = "brhs.again@gmail.com"

func main() {
	type EmailParams struct {
		Email      string
		LicenseKey string
		Greeting   string
		PortalLink string
	}

	params := &EmailParams{
		Email:      TestEmail,
		LicenseKey: lib.GenerateLicenseKey(),
		Greeting:   "Hi Brandon,",
		PortalLink: "https://stripe.com",
	}

	type Email struct {
		Text    string
		Html    string
		Subject string
	}

	emails := []Email{
		Email{
			Text:    userCreatedText,
			Html:    userCreatedHtml,
			Subject: "CodePerfect 95: New License",
		},
		Email{
			Text:    userEnabledText,
			Html:    userEnabledHtml,
			Subject: "CodePerfect 95: License Reactivated",
		},
		Email{
			Text:    userDisabledText,
			Html:    userDisabledHtml,
			Subject: "CodePerfect 95: License Deactivated",
		},
	}

	for _, email := range emails {
		txt, err := lib.ExecuteTemplate(email.Text, params)
		if err != nil {
			panic(err)
		}

		html, err := lib.ExecuteTemplate(email.Html, params)
		if err != nil {
			panic(err)
		}

		lib.SendEmail(TestEmail, email.Subject, string(txt), string(html))
	}
}
