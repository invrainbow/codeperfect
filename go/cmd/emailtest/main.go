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
		PaymentLinkType   string
	}

	params := &EmailParams{
		Email:             TestEmail,
		LicenseKey:        lib.GenerateLicenseKey(),
		Greeting:          "Hi Brandon,",
		BillingPortalLink: "https://billing.stripe.com/p/login/test_5kAcNMdzp6encGk4gg",
		PaymentLinkType:   "license_and_sub",
	}

	txt, err := lib.ExecuteTemplate(emails.EmailNewUserText, params)
	if err != nil {
		panic(err)
	}

	html, err := lib.ExecuteTemplate(emails.EmailNewUserHtml, params)
	if err != nil {
		panic(err)
	}

	lib.SendEmail(TestEmail, "Your new CodePerfect license", string(txt), string(html))
}
