package main

import (
	"log"

	"github.com/gin-contrib/cors"
	"github.com/gin-gonic/gin"
)

func main() {
	log.Print(1)
	go stripeEventWorker()
	log.Print(2)

	r := gin.Default()
	r.Use(cors.Default())
	r.POST("/auth", PostAuth)
	r.POST("/trial", PostTrial)
	r.POST("/heartbeat", PostHeartbeat)
	r.POST("/stripe-webhook", PostStripeWebhook)
	r.POST("/crash-report", PostCrashReport)

	r.POST("/v2/auth", PostAuthV2)
	r.POST("/v2/trial", PostTrialV2)
	r.POST("/v2/crash-report", PostCrashReportV2)

	log.Print(3)

	r.Run()
}
