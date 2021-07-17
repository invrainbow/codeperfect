package main

import (
	"log"
	"net/http"

	"github.com/boltdb/bolt"
	"github.com/gin-gonic/gin"
)

/*
what should we keep track of?
stripe_statuses: cus_id -> stripe_status
keys: key -> cus_id
keysrev: cus_id -> key
emails: email -> cus_id
*/

type AuthBody struct {
	Email      string `json:"email"`
	LicenseKey string `json:"license_key"`
}

func sendError(c *gin.Context, statusCode int, message string) {
	c.JSON(statusCode, gin.H{
		"error": message,
	})
}

func main() {
	defer cleanupDB()

	r := gin.Default()
	r.GET("/auth", func(c *gin.Context) {
		var params AuthBody
		if c.ShouldBindJSON(&params) != nil {
			sendError(c, http.StatusBadRequest, "Invalid data.")
			return
		}

		params.Email
	})
}
