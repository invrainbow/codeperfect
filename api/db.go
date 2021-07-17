package main

import (
	"github.com/boltdb/bolt"
)

func b(x interface{}) []byte {
	return []byte(x)
}

var db *bolt.DB

func init() {
	db, err := bolt.Open("my.db", 0600, nil)
	if err != nil {
		log.Fatal(err)
	}

	// ensure buckets exist
	db.Update(func(tx *bolt.Tx) error {
		buckets := []string{
			"stripe_statuses",
			"keys",
			"keysr",
		}
		for _, val := range buckets {
			val, err := tx.CreateBucketIfNotExists(b(val))
			if err != nil {
				log.Fatal(err)
			}
		}
	})
}

func cleanupDB() {
	db.Close()
}
