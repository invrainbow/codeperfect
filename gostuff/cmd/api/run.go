package main

import (
	"github.com/gin-contrib/cors"
	"github.com/gin-gonic/gin"
)

func main() {
	r := gin.Default()
	r.Use(cors.Default())
	r.GET("/install", GetInstall)
	r.GET("/license", GetLicense)
	r.GET("/download", GetDownload)
	r.POST("/auth-web", PostAuthWeb)
	r.POST("/auth", PostAuth)
	r.POST("/heartbeat", PostHeartbeat)
	r.Run()
}
