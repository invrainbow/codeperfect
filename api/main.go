package main

import (
	"fmt"
	"log"
	"net/http"
	"os"

	"github.com/boltdb/bolt"
	"github.com/gin-gonic/gin"

	stripe "github.com/stripe/stripe-go/v72"
	"github.com/stripe/stripe-go/v72/subscriptions"
	"github.com/stripe/stripe-go/v72/webhook"
)

/*
what should we keep track of?
stripe_statuses: cus_id -> stripe_status
keys: key -> cus_id
keysrev: cus_id -> key
emails: email -> cus_id
*/

const AdminPassword = os.Getenv("ADMIN_PASSWORD")

func sendError(c *gin.Context, message string) {
	c.JSON(http.StatusBadRequest, gin.H{
		"error": message,
	})
}

func main() {
	defer cleanupDB()

	r := gin.Default()

	type AuthBody struct {
		Email      string `json:"email"`
		LicenseKey string `json:"license_key"`
	}

	r.POST("/auth", func(c *gin.Context) {
		var body AuthBody
		if c.ShouldBindJSON(&body) != nil {
			sendError(c, "Invalid data.")
			return
		}

		cusIdFromEmail, found := boltGet("emails", body.Email)
		if !found {
			sendError(c, "Email not found.")
		}

		cusIdFromKey, found := boltGet("keys", body.LicenseKey)
		if !found || (cusIdFromEmail != cusIdFromKey) {
			sendError(c, "Invalid license key.")
			return
		}

		stripeStatus, found := boltGet("stripe_statuses", body.Email)
		if !found || stripeStatus != stripe.SubscriptionStatusActive {
			sendError(c, "Your subscription has expired. Please contact support for details.")
		}

		c.JSON(http.StatusOK, gin.H{"ok": true})
	})

	/*
		type CreateNewUserBody struct {
			Email string `json:"email"`
			Email string `json:"email"`
		}
	*/

	r.POST("/create-new-user", func(c *gin.Context) {
		c.Request.Body
	})
}
