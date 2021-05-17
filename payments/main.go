package main

import (
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"strings"

	"github.com/gin-gonic/gin"
	"github.com/stripe/stripe-go/v72"
	"github.com/stripe/stripe-go/v72/checkout/session"

	// "github.com/stripe/stripe-go/v72/customer"
	"github.com/stripe/stripe-go/v72/webhook"
	"gorm.io/gorm"

	"github.com/aws/aws-sdk-go/aws"
	// "github.com/aws/aws-sdk-go/aws/awserr"
	awsSession "github.com/aws/aws-sdk-go/aws/session"
	"github.com/aws/aws-sdk-go/service/ses"
)

var sesClient *ses.SES

func initSesClient() {
	sess, err := awsSession.NewSession(&aws.Config{
		Region: aws.String("us-west-2")},
	)
	if err != nil {
		panic(err)
	}
	sesClient = ses.New(sess)
}

func init() {
	stripe.Key = os.Getenv("STRIPE_API_KEY")
	// "sk_test_51IqLcpBpL0Zd3zdOMyMwr4CfzffzCVaFmsD1tPMLvlHGzQmUv2qCjYv6Oai5hmpF0j9BbCWXHDgLhTie7hU4YhMX00Ba9jADiH"

	initDB()
	initSesClient()
}

type CheckoutPost struct {
	PriceID string `json:"price_id" binding:"required"`
}

func generateLicenseKey() (string, error) {
	b := make([]byte, 16)
	n, err := rand.Read(b)
	if err != nil {
		return "", err
	}

	if n != 16 {
		return "", fmt.Errorf("failed to generate 16 bytes")
	}

	parts := []string{}
	for i := 0; i < len(b); i += 4 {
		parts = append(parts, hex.EncodeToString(b[i:i+4]))
	}

	return strings.Join(parts, "-"), nil
}

func sendEmail(to, html, text, subject string) error {
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

func main() {
	r := gin.Default()

	r.POST("/checkout", func(c *gin.Context) {
		var data CheckoutPost
		c.BindJSON(&data)

		successUrl := "http://localhost:3000/payment_success"
		cancelUrl := "http://localhost:3000/payment_canceled"

		params := &stripe.CheckoutSessionParams{
			SuccessURL:         &successUrl,
			CancelURL:          &cancelUrl,
			PaymentMethodTypes: stripe.StringSlice([]string{"card"}),
			Mode:               stripe.String(string(stripe.CheckoutSessionModeSubscription)),
			LineItems: []*stripe.CheckoutSessionLineItemParams{
				&stripe.CheckoutSessionLineItemParams{
					Price:    stripe.String(data.PriceID),
					Quantity: stripe.Int64(1),
				},
			},
		}

		sess, err := session.New(params)
		if err != nil {
			c.JSON(200, gin.H{
				"error": err.Error(),
			})
			return
		}

		c.JSON(200, gin.H{
			"session_id": sess.ID,
		})
	})

	r.POST("/stripe-webhook", func(c *gin.Context) {
		body, err := ioutil.ReadAll(c.Request.Body)
		if err != nil {
			c.JSON(http.StatusBadRequest, gin.H{})
			log.Printf("ioutil.Readall: %v", err)
			return
		}

		event, err := webhook.ConstructEvent(body, c.Request.Header.Get("Stripe-Signature"), os.Getenv("STRIPE_WEBHOOK_SECRET"))
		if err != nil {
			c.JSON(http.StatusBadRequest, gin.H{})
			log.Printf("webhook.ConstructEvent: %v", err)
			return
		}

		log.Printf("event: %v", event.Type)

		switch event.Type {
		case "customer.subscription.created":
		case "customer.subscription.updated":
			var sub stripe.Subscription
			err := json.Unmarshal(event.Data.Raw, &sub)
			if err != nil {
				log.Printf("json.Unmarshal: %v", err)
				break
			}

			// does this work? does webhook.ConstructEvent automatically fetch this?
			cus := sub.Customer
			/*
				cus, err := customer.Get(sub.Customer, nil)
				if err != nil {
					log.Printf("customer.Get: %v", err)
					break
				}
			*/

			var user User
			res := db.First(&user, "stripe_customer_id = ? AND stripe_subscription_id = ?", cus.ID, sub.ID)
			if errors.Is(res.Error, gorm.ErrRecordNotFound) {

				licenseKey, err := generateLicenseKey()
				if err != nil {
					log.Printf("generateLicenseKey: %v", err)
					break
				}

				user.StripeCustomerID = cus.ID
				user.StripeSubscriptionID = sub.ID
				user.LicenseKey = licenseKey
				db.Create(&user)
			}

			oldStatus := user.StripeSubscriptionStatus

			user.Email = cus.Email
			user.StripeSubscriptionStatus = string(sub.Status)
			db.Save(&user)

			if user.Email == "" {
				log.Printf("error: user email doesn't exist? cus id = %v, sub id = %v", cus.ID, sub.ID)
				break
			}

			if oldStatus != user.StripeSubscriptionStatus {
				if user.StripeSubscriptionStatus == "active" { // nonactive -> active
					args := &NewLicenseKeyArgs{
						DownloadLink: "https://google.com",
						LicenseKey:   user.LicenseKey,
					}

					emailHtml, err := RenderTemplate(NewLicenseKeyTplHtml, &args)
					if err != nil {
						log.Printf("RenderTemplate: %v", err)
						break
					}

					emailText, err := RenderTemplate(NewLicenseKeyTplText, &args)
					if err != nil {
						log.Printf("RenderTemplate: %v", err)
						break
					}

					sendEmail(
						user.Email,
						emailHtml,
						emailText,
						fmt.Sprintf("%s download and license key", ProductName),
					)
				} else if oldStatus == "active" { // active -> nonactive
					// TODO: email user about their shit not working
					// send them to portal
				}
			}

		case "checkout.session.completed":
		case "invoice.paid":
		case "invoice.payment_failed":
			// what do we even do here? any reason we can't just handle customer.subscription.updated?
		}
	})

	r.Run()
}
