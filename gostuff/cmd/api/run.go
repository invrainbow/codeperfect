package main

import (
	"github.com/gin-contrib/cors"
	"github.com/gin-gonic/gin"
)

func main() {
	r := gin.Default()
	r.Use(cors.Default())
	r.POST("/auth", PostAuth)
	r.POST("/update", PostUpdate)
	r.POST("/heartbeat", PostHeartbeat)
	r.Run()
}
