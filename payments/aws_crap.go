package main

import (
	"github.com/aws/aws-sdk-go/aws"
	"github.com/aws/aws-sdk-go/aws/session"
	"github.com/aws/aws-sdk-go/service/s3"
	"github.com/aws/aws-sdk-go/service/ses"
)

var sesClient *ses.SES
var s3Client *s3.S3

func init() {
	sess, err := session.NewSession(&aws.Config{
		Region: aws.String("us-east-2")},
	)
	if err != nil {
		panic(err)
	}

	sesClient = ses.New(sess)
	s3Client = s3.New(sess)
}
