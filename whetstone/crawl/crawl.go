package main

import (
	"fmt"
	"gorm.io/driver/sqlite"
	"gorm.io/gorm"
)

func init() {
	db, err := gorm.Open(sqlite.Open("gorm.db"), &gorm.Config{})
}

func NewConn() *Conn {
	conn := &Conn{}
	sqldb := sqlite.Open("gorm.db")
	conn.db = gorm.Open(sqldb, &gorm.Config{})
	return conn
}