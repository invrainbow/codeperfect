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

func boltGet(bucket string, key string) (string, bool) {
	ret, found := "", false

	db.View(func(tx *bolt.Tx) error {
		bucket := tx.Bucket(b(bucket))
		val := bucket.Get(b(key))
		if val != nil {
			ret = string(val)
			found = true
		}
	})

	return ret, found
}

func boltPut(bucket string, key string, val string) error {
	return db.Update(func(tx *bolt.Tx) error {
		bucket := tx.Bucket(b(bucket))
		return bucket.Put(b(key), b(val))
	})
}

func cleanupDB() {
	db.Close()
}
