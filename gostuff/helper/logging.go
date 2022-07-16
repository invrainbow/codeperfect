//go:build !windows

package main

import (
    "log"
	"log/syslog"
)

func init() {
	logwriter, err := syslog.New(syslog.LOG_NOTICE, "codeperfect")
	if err != nil {
		// i mean, don't crash
		log.Print(err)
		return
	}
	log.SetOutput(logwriter)
}
