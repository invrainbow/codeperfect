package helper

import (
	"log"
)

type Message struct {
	Text    string
	Title   string
	IsPanic bool
}

var MessageChan chan Message

func init() {
	MessageChan = make(chan Message, 128)
}

func PushMessage(text string, title string, ispanic bool) {
	m := Message{
		Text:    text,
		Title:   title,
		IsPanic: ispanic,
	}
	log.Println(m)
	MessageChan <- m
}
