package main

import (
	"bytes"
	"fmt"
	"log"
	"text/template"

	"github.com/aws/aws-sdk-go/aws"
	"github.com/aws/aws-sdk-go/service/ses"
)

func SendEmail(to, html, text, subject string) error {
	makeContent := func(body string) *ses.Content {
		return &ses.Content{
			Charset: aws.String("UTF-8"),
			Data:    aws.String(body),
		}
	}

	input := &ses.SendEmailInput{
		Destination: &ses.Destination{
			CcAddresses: []*string{},
			ToAddresses: []*string{
				aws.String(to),
			},
		},
		Message: &ses.Message{
			Body: &ses.Body{
				Html: makeContent(html),
				Text: makeContent(text),
			},
			Subject: makeContent(subject),
		},
		Source: aws.String(SendEmailFrom),
		// Uncomment to use a configuration set
		//ConfigurationSetName: aws.String(ConfigurationSet),
	}

	_, err := sesClient.SendEmail(input)
	if err != nil {
		log.Printf("error while sending email to %v: %v", to, err)
	}
	return err
}

type NewLicenseKeyArgs struct {
	DownloadLink string
	LicenseKey   string
}

const ProductName = "CodePerfect 95"

const SendEmailFrom = "brhs.again@gmail.com"

var NewLicenseKeyHtml = fmt.Sprintf(`
<p>Thanks for buying %s! Here's your download link:</p>

<p><a href="{{.DownloadLink}}">{{.DownloadLink}}</a></p>

<p>Your license key is:</p>

<pre>{{.LicenseKey}}</pre>

<p>Best,<br>
The %s Team</p>
`, ProductName, ProductName)

var NewLicenseKeyText = fmt.Sprintf(`
Thanks for buying %s! Here's your download link:

{{.DownloadLink}}

Your license key is:

{{.LicenseKey}}

Best,
The %s Team
`, ProductName, ProductName)

type SubscriptionRenewedArgs struct {
	DownloadLink string
	LicenseKey   string
}

var SubscriptionRenewedHtml = fmt.Sprintf(`
<p>Your subscription has been reactivated! Here are your download link and
license key, in case you need them again:</p>

<p>Download link:<br>
<a href="{{.DownloadLink}}">{{.DownloadLink}}</a></p>

<p>License key:<br>
<code>{{.LicenseKey}}</code></p>

<p>Best,<br>
The %s Team</p>
`, ProductName)

var SubscriptionRenewedText = fmt.Sprintf(`
Your subscription has been reactivated! Here are your download link and license
key, in case you need them again:

Download link:
{{.DownloadLink}}

License key:
{{.LicenseKey}}

Best,
The %s Team
`, ProductName)

type SubscriptionEndedArgs struct {
	LicenseKey string
}

var SubscriptionEndedHtml = fmt.Sprintf(`
<p>Your %s subscription has ended.</p>

<p>If this was because you canceled it, we're sorry to see you go! We'd love to
hear your feedback at <a
href="mailto:support@codeperfect95.com">support@codeperfect95.com</a> if you
have any.</p>

<p>If this comes as a surprise to you, possibly your card stopped working.
Please log in to our payment portal to update your payment method:</p>

<p><a
href="https://codeperfect95.com/portal">https://codeperfect95.com/portal</a></p>

<p>You'll need your license key, which is:</p>

<pre>{{.LicenseKey}}</pre>

<p>Best,<br>
The %s Team</p>
`, ProductName, ProductName)

var SubscriptionEndedText = fmt.Sprintf(`
Your %s subscription has ended.

If this was because canceled it, we're sorry to see you go! We'd love to
hear your feedback at support@codeperfect95.com if you have any.

If this comes as a surprise to you, it might be because your card stopped
working. Please log in to our payment portal to update your payment method:

https://codeperfect95.com/portal

You'll need your license key, which is:

    {{.LicenseKey}}

Best,
The %s Team
`, ProductName, ProductName)

func RenderTemplate(text string, args interface{}) (string, error) {
	tpl, err := template.New("some_name").Parse(text)
	if err != nil {
		return "", err
	}

	var buf bytes.Buffer
	if err := tpl.Execute(&buf, args); err != nil {
		return "", err
	}

	return buf.String(), nil
}

func RenderTemplates(textTemplate, htmlTemplate string, args interface{}) (string, string, error) {
	text, err := RenderTemplate(textTemplate, args)
	if err != nil {
		return "", "", err
	}

	html, err := RenderTemplate(htmlTemplate, args)
	if err != nil {
		return "", "", err
	}

	return text, html, nil
}
