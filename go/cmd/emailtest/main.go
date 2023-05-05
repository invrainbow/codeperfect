package main

import (
	_ "embed"

	"github.com/codeperfect95/codeperfect/go/cmd/api/emails"
	"github.com/codeperfect95/codeperfect/go/cmd/lib"
)

const TestEmail = "bh@codeperfect95.com"

func main() {
	type EmailParams struct {
		Email             string
		LicenseKey        string
		Greeting          string
		BillingPortalLink string
	}

	params := &EmailParams{
		Email:             TestEmail,
		LicenseKey:        lib.GenerateLicenseKey(),
		Greeting:          "Hi Brandon,",
		BillingPortalLink: "https://billing.stripe.com/p/login/test_5kAcNMdzp6encGk4gg",
	}

	type Email struct {
		Text    string
		Html    string
		Subject string
	}

	emails := []Email{
		{
			Text:    emails.EmailUserCreatedText,
			Html:    emails.EmailUserCreatedHtml,
			Subject: "CodePerfect 95: New License",
		},
		{
			Text:    emails.EmailUserEnabledText,
			Html:    emails.EmailUserEnabledHtml,
			Subject: "CodePerfect 95: License Reactivated",
		},
		{
			Text:    emails.EmailUserDisabledText,
			Html:    emails.EmailUserDisabledHtml,
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
