package lib

import (
	"bytes"
	"crypto/rand"
	"log"
	"strings"
	"text/template"
	"time"

	"github.com/aws/aws-sdk-go/aws"
	"github.com/aws/aws-sdk-go/aws/awserr"
	"github.com/aws/aws-sdk-go/aws/session"
	"github.com/aws/aws-sdk-go/service/s3"
	"github.com/aws/aws-sdk-go/service/ses"
	"github.com/aws/aws-sdk-go/service/sts"
	"github.com/btcsuite/btcutil/base58"
)

var (
	s3Client  *s3.S3
	sesClient *ses.SES
)

func init() {
	sess := session.Must(session.NewSession(&aws.Config{
		Region: aws.String("us-east-2"),
	}))

	// try to auth
	stsClient := sts.New(sess)
	output, err := stsClient.GetCallerIdentity(&sts.GetCallerIdentityInput{})
	if err != nil {
		panic("unable to auth with aws credentials")
	}
	log.Printf("authed with aws account %v\n", *output.Account)

	// init clients
	s3Client = s3.New(sess)
	sesClient = ses.New(sess)
}

func UploadFileToS3(bucket, key, data string) error {
	_, err := s3Client.PutObject(&s3.PutObjectInput{
		Bucket: &bucket,
		Key:    &key,
		Body:   bytes.NewReader([]byte(data)),
	})
	return err
}

func GetPresignedURL(bucket, key string, expire time.Duration) (string, error) {
	req, _ := s3Client.GetObjectRequest(&s3.GetObjectInput{
		Bucket: aws.String(bucket),
		Key:    aws.String(key),
	})
	return req.Presign(expire) // 15 * time.Minute)
}

func SendEmail(to, subject, bodyText, bodyHtml string) error {
	makeContent := func(s string) *ses.Content {
		return &ses.Content{
			Charset: aws.String("UTF-8"),
			Data:    aws.String(s),
		}
	}

	input := &ses.SendEmailInput{
		Destination: &ses.Destination{
			ToAddresses:  []*string{aws.String(to)},
			BccAddresses: []*string{aws.String("automated@codeperfect95.com")},
		},
		Message: &ses.Message{
			Body: &ses.Body{
				Html: makeContent(bodyHtml),
				Text: makeContent(bodyText),
			},
			Subject: makeContent(subject),
		},
		Source: aws.String("The CodePerfect Team <support@codeperfect95.com>"),
	}

	_, err := sesClient.SendEmail(input)
	if err != nil {
		if aerr, ok := err.(awserr.Error); ok {
			log.Print(aerr.Error())
		} else {
			log.Print(err.Error())
		}
	}
	return err
}

func GenerateBytes(n int) []byte {
	b := make([]byte, n)
	n, err := rand.Read(b)
	if err != nil || n != cap(b) {
		panic("shit done fucked up (this should never happen)")
	}
	return b
}

func GenerateKey(nbytes int) string {
	b := GenerateBytes(nbytes)
	return base58.Encode(b)
}

func ExecuteTemplate(text string, params interface{}) ([]byte, error) {
	tpl, err := template.New("install").Parse(text)
	if err != nil {
		return nil, err
	}

	var buf bytes.Buffer
	if err := tpl.Execute(&buf, params); err != nil {
		return nil, err
	}

	return buf.Bytes(), nil
}

func GenerateLicenseKey() string {
	s := strings.ToUpper(GenerateKey(36))
	ret := []string{}
	for i := 0; i < 36; i += 6 {
		ret = append(ret, s[i:i+6])
	}
	return strings.Join(ret, "-")
}
