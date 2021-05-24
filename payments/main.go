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
	"time"

	"github.com/aws/aws-sdk-go/aws"
	"github.com/aws/aws-sdk-go/service/s3"
	"github.com/gin-contrib/cors"
	"github.com/gin-gonic/gin"
	"github.com/stripe/stripe-go/v72"
	portalsession "github.com/stripe/stripe-go/v72/billingportal/session"
	"github.com/stripe/stripe-go/v72/checkout/session"
	"github.com/stripe/stripe-go/v72/sub"
	"github.com/stripe/stripe-go/v72/webhook"
	"gorm.io/gorm"
)

const StripeAPIKey = "sk_test_51IqLcpBpL0Zd3zdOMyMwr4CfzffzCVaFmsD1tPMLvlHGzQmUv2qCjYv6Oai5hmpF0j9BbCWXHDgLhTie7hU4YhMX00Ba9jADiH"
const StripeWebhookSecret = "whsec_IOO7B9EaYAGkWZyNHbRu11NFrGxvYYm1"

const S3Bucket = "codeperfect95"

type VersionInfo struct {
	Version  int
	Checksum string
	S3Key    string
}

var CurrentVersionInfo = VersionInfo{
	Version:  1,
	Checksum: "eb129b345c239d08446c41d22ff7367b5cbb4440bdc1484b5cebdd3d610d8414",
	S3Key:    "v1/ide.exe",
}

func init() {
	stripe.Key = StripeAPIKey
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

func randomGlueCode() {
	subscription, err := sub.Get("sub_JWJf9lW5jyXUQD", nil)
	if err != nil {
		log.Printf("sub.Get: %v", err)
		return
	}

	fmt.Printf("status is %s", subscription.Status)
	return
}

func main() {
	// randomGlueCode()
	// return

	r := gin.Default()

	r.Use(cors.Default())

	validateLicenseKey := func(c *gin.Context, allowInactive bool) *User {
		key := c.PostForm("license_key")

		var user User
		res := db.First(&user, "license_key = ?", key)
		if errors.Is(res.Error, gorm.ErrRecordNotFound) {
			c.JSON(401, gin.H{"error": "bad_key"})
			return nil
		}

		if !allowInactive {
			if user.StripeSubscriptionStatus != string(stripe.SubscriptionStatusActive) {
				c.JSON(401, gin.H{"error": "trial_expired"})
				return nil
			}
		}

		return &user
	}

	r.POST("/version", func(c *gin.Context) {
		if user := validateLicenseKey(c, false); user == nil {
			return
		}
		c.JSON(200, gin.H{"version": CurrentVersionInfo.Version})
	})

	r.POST("/download", func(c *gin.Context) {
		if user := validateLicenseKey(c, false); user == nil {
			return
		}
		req, _ := s3Client.GetObjectRequest(&s3.GetObjectInput{
			Bucket: aws.String(S3Bucket),
			Key:    aws.String(CurrentVersionInfo.S3Key),
		})
		url, err := req.Presign(15 * time.Minute)
		if err != nil {
			c.JSON(400, gin.H{"error": "unable_to_generate_download_link"})
			return
		}
		c.JSON(200, gin.H{
			"version":       CurrentVersionInfo.Version,
			"checksum":      CurrentVersionInfo.Checksum,
			"download_link": url,
		})
	})

	r.POST("/checkout", func(c *gin.Context) {
		priceID := c.PostForm("price_id")

		successUrl := "http://localhost:3000/payment-success"
		cancelUrl := "http://localhost:3000/payment-canceled"

		params := &stripe.CheckoutSessionParams{
			SuccessURL:         &successUrl,
			CancelURL:          &cancelUrl,
			PaymentMethodTypes: stripe.StringSlice([]string{"card"}),
			Mode:               stripe.String(string(stripe.CheckoutSessionModeSubscription)),
			LineItems: []*stripe.CheckoutSessionLineItemParams{
				&stripe.CheckoutSessionLineItemParams{
					Price:    stripe.String(priceID),
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

	r.POST("/portal", func(c *gin.Context) {
		user := validateLicenseKey(c, true)
		if user == nil {
			return
		}

		returnUrl := "http://localhost:3000/portal-return"
		params := &stripe.BillingPortalSessionParams{
			Customer:  stripe.String(user.StripeCustomerID),
			ReturnURL: stripe.String(returnUrl),
		}
		ps, _ := portalsession.New(params)

		c.JSON(200, gin.H{
			"portal_url": ps.URL,
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
		case "customer.subscription.created", "customer.subscription.updated", "customer.subscription.deleted":
			var subscription stripe.Subscription
			err := json.Unmarshal(event.Data.Raw, &subscription)
			if err != nil {
				log.Printf("json.Unmarshal: %v", err)
				break
			}

			cus := subscription.Customer

			var user User
			res := db.First(&user, "stripe_customer_id = ?", cus.ID)
			if errors.Is(res.Error, gorm.ErrRecordNotFound) {
				licenseKey, err := GenerateLicenseKey()
				if err != nil {
					log.Printf("generateLicenseKey: %v", err)
					break
				}

				user.StripeCustomerID = cus.ID
				user.StripeSubscriptionID = subscription.ID
				user.LicenseKey = licenseKey
				db.Create(&user)
			}

			oldStatus := user.StripeSubscriptionStatus

			user.Email = cus.Email

			// DEBUG: stripe's fixture shit doesn't generate an email
			if user.Email == "" {
				user.Email = "brhs.again@gmail.com"
			}

			user.StripeSubscriptionStatus = string(subscription.Status)
			user.StripeSubscriptionID = subscription.ID
			db.Save(&user)

			if user.Email == "" {
				log.Printf("error: user email doesn't exist? cus id = %v, sub id = %v", cus.ID, subscription.ID)
				break
			}

			if oldStatus != user.StripeSubscriptionStatus {
				if user.StripeSubscriptionStatus == "active" { // nonactive -> active
					if oldStatus == "canceled" { // just got reactivated
						args := &SubscriptionRenewedArgs{
							DownloadLink: "https://codeperfect95.com/download",
							LicenseKey:   user.LicenseKey,
						}

						text, html, err := RenderTemplates(SubscriptionRenewedText, SubscriptionRenewedHtml, args)
						if err != nil {
							log.Printf("RenderTemplates: %v", err)
							break
						}

						subject := fmt.Sprintf("%s: Subscription reactivated", ProductName)
						SendEmail(user.Email, html, text, subject)
					} else {
						args := &NewLicenseKeyArgs{
							DownloadLink: "https://codeperfect95.com/download",
							LicenseKey:   user.LicenseKey,
						}

						text, html, err := RenderTemplates(NewLicenseKeyText, NewLicenseKeyHtml, args)
						if err != nil {
							log.Printf("RenderTemplates: %v", err)
							break
						}

						subject := fmt.Sprintf("%s: Download and license key", ProductName)
						SendEmail(user.Email, html, text, subject)
					}
				} else if oldStatus == "active" { // active -> nonactive
					args := &SubscriptionEndedArgs{
						LicenseKey: user.LicenseKey,
					}

					text, html, err := RenderTemplates(SubscriptionEndedText, SubscriptionEndedHtml, args)
					if err != nil {
						log.Printf("RenderTemplates: %v", err)
						break
					}

					subject := fmt.Sprintf("%s: Subscription canceled", ProductName)
					SendEmail(user.Email, html, text, subject)
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
