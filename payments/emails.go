package main

import (
	"bytes"
	"fmt"
	"text/template"
)

type NewLicenseKeyArgs struct {
	DownloadLink string
	LicenseKey   string
}

const ProductName = "CodePerfect 95"

const SendEmailFrom = "brhs.again@gmail.com"

const NewLicenseKeyTplHtml = fmt.Sprintf(`
Thanks for buying %s! Here's your download link:

<a href="{{.DownloadLink}}">{{.DownloadLink}}</a>

Your license key is:

{{.LicenseKey}}

Best,
The %s Team
`, ProductName, ProductName)

const NewLicenseKeyTplText = fmt.Sprintf(`
{{.Name}},

Thanks for buying %s! Here's your download link:

{{.DownloadLink}}

Your license key is:

{{.LicenseKey}}

Best,
The %s Team
`, ProductName, ProductName)

func RenderTemplate(text string, data interface{}) (string, error) {
	tpl, err := template.New("some_name").Parse("text")
	if err != nil {
		return "", err
	}

	var buf bytes.Buffer
	if err := tpl.Execute(&buf, data); err != nil {
		return "", err
	}

	return buf.String(), nil
}
