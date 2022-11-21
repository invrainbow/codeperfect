package main

import (
	"github.com/gin-contrib/cors"
	"github.com/gin-gonic/gin"
)

func main() {
	go stripeEventWorker()

	r := gin.Default()
	r.Use(cors.Default())
	r.POST("/auth", PostAuth)
	r.POST("/trial", PostTrial)
	r.POST("/heartbeat", PostHeartbeat)
	r.POST("/stripe-webhook", PostStripeWebhook)
	r.POST("/crash-report", PostCrashReport)
	r.Run()
}
