package main

import (
	"log"
	"time"

	"github.com/aws/aws-sdk-go/aws"
	"github.com/aws/aws-sdk-go/aws/awserr"
	"github.com/aws/aws-sdk-go/aws/session"
	"github.com/aws/aws-sdk-go/service/s3"
	"github.com/aws/aws-sdk-go/service/ses"
	"github.com/aws/aws-sdk-go/service/sts"
)

var s3Client *s3.S3
var sesClient *ses.SES

func init() {
	sess := session.Must(session.NewSession(&aws.Config{
		Region: aws.String("us-east-2")},
	))

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

func GetPresignedURL(bucket, key string) (string, error) {
	req, _ := s3Client.GetObjectRequest(&s3.GetObjectInput{
		Bucket: aws.String(bucket),
		Key:    aws.String(key),
	})
	return req.Presign(15 * time.Minute)
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
			ToAddresses: []*string{aws.String(to)},
		},
		Message: &ses.Message{
			Body: &ses.Body{
				Html: makeContent(bodyHtml),
				Text: makeContent(bodyText),
			},
			Subject: makeContent(subject),
		},
		Source: aws.String("bh@codeperfect95.com"),
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
