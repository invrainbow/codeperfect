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
	"strings"

	"github.com/gin-contrib/cors"
	"github.com/gin-gonic/gin"

	"github.com/stripe/stripe-go/v72"
	"github.com/stripe/stripe-go/v72/checkout/session"

	// "github.com/stripe/stripe-go/v72/customer"
	"github.com/stripe/stripe-go/v72/webhook"

	"gorm.io/gorm"
)

const StripeAPIKey = "sk_test_51IqLcpBpL0Zd3zdOMyMwr4CfzffzCVaFmsD1tPMLvlHGzQmUv2qCjYv6Oai5hmpF0j9BbCWXHDgLhTie7hU4YhMX00Ba9jADiH"
const StripeWebhookSecret = "whsec_IOO7B9EaYAGkWZyNHbRu11NFrGxvYYm1"

func init() {
	stripe.Key = StripeAPIKey
}

type CheckoutPost struct {
	PriceID string `json:"price_id" binding:"required"`
}

func GenerateLicenseKey() (string, error) {
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

func main() {
	r := gin.Default()

	r.Use(cors.Default())

	r.POST("/checkout", func(c *gin.Context) {
		var data CheckoutPost
		c.BindJSON(&data)

		successUrl := "http://localhost:3000/payment-success"
		cancelUrl := "http://localhost:3000/payment-canceled"

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

		event, err := webhook.ConstructEvent(body, c.Request.Header.Get("Stripe-Signature"), StripeWebhookSecret)
		if err != nil {
			c.JSON(http.StatusBadRequest, gin.H{})
			log.Printf("webhook.ConstructEvent: %v", err)
			return
		}

		log.Printf("event: %v", event.Type)

		switch event.Type {
		case "customer.subscription.created", "customer.subscription.updated":
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
				licenseKey, err := GenerateLicenseKey()
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

			// DEBUG: stripe's fixture shit doesn't generate an email
			if user.Email == "" {
				user.Email = "brhs.again@gmail.com"
			}

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

					SendEmail(
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

	r.Run(":8080")
}
