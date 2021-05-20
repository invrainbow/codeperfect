package main

import (
	"bytes"
	"fmt"
	"log"
	"text/template"

	"github.com/aws/aws-sdk-go/aws"
	// "github.com/aws/aws-sdk-go/aws/awserr"
	awsSession "github.com/aws/aws-sdk-go/aws/session"
	"github.com/aws/aws-sdk-go/service/ses"
)

var sesClient *ses.SES

func init() {
	sess, err := awsSession.NewSession(&aws.Config{
		Region: aws.String("us-east-2")},
	)
	if err != nil {
		panic(err)
	}
	sesClient = ses.New(sess)
}

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

var NewLicenseKeyTplHtml = fmt.Sprintf(`
<p>Thanks for buying %s! Here's your download link:</p>

<p><a href="{{.DownloadLink}}">{{.DownloadLink}}</a></p>

<p>Your license key is:</p>

<pre>{{.LicenseKey}}</pre>

<p>Best,<br>
The %s Team</p>
`, ProductName, ProductName)

var NewLicenseKeyTplText = fmt.Sprintf(`
Thanks for buying %s! Here's your download link:

{{.DownloadLink}}

Your license key is:

{{.LicenseKey}}

Best,
The %s Team
`, ProductName, ProductName)

func RenderTemplate(text string, data interface{}) (string, error) {
	tpl, err := template.New("some_name").Parse(text)
	if err != nil {
		return "", err
	}

	var buf bytes.Buffer
	if err := tpl.Execute(&buf, data); err != nil {
		return "", err
	}

	return buf.String(), nil
}
