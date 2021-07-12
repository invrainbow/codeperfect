package main

import (
	"log"

	"github.com/boltdb/bolt"
	"github.com/gin-gonic/gin"
)

func b(x interface{}) []byte {
	return []byte(x)
}

/*
what should we keep track of?
stripe_statuses: cus_id -> stripe_status
keys: key -> cus_id
keysrev: cus_id -> key
*/

func main() {
	db, err := bolt.Open("my.db", 0600, nil)
	if err != nil {
		log.Fatal(err)
	}
	defer db.Close()

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
