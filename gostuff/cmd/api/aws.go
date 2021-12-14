package main

import (
	"fmt"
	"time"

	"github.com/aws/aws-sdk-go/aws"
	"github.com/aws/aws-sdk-go/aws/session"
	"github.com/aws/aws-sdk-go/service/s3"
	"github.com/aws/aws-sdk-go/service/sts"
)

var s3Client *s3.S3

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
	fmt.Printf("authed with aws account %v\n", *output.Account)

	// init s3
	s3Client = s3.New(sess)
}

func GetPresignedURL(bucket, key string) (string, error) {
	req, _ := s3Client.GetObjectRequest(&s3.GetObjectInput{
		Bucket: aws.String(bucket),
		Key:    aws.String(key),
	})
	return req.Presign(15 * time.Minute)
}
