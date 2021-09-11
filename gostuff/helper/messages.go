package helper

import (
	"fmt"
	"sync"
)

type Message struct {
	Text    string
	Title   string
	IsPanic bool
}

type MessageQueue struct {
	mu    sync.Mutex
	queue []Message
}

func (m *MessageQueue) Push(text, title string, ispanic bool) {
	m.mu.Lock()
	defer m.mu.Unlock()

	fmt.Printf("(%d) %s: %s\n", BoolToInt(ispanic), title, text)

	m.queue = append(m.queue, Message{
		Text:    text,
		Title:   title,
		IsPanic: ispanic,
	})
}

func (m *MessageQueue) Pop() *Message {
	if len(m.queue) == 0 {
		return nil
	}

	m.mu.Lock()
	defer m.mu.Unlock()

	var ret Message
	ret, m.queue = m.queue[0], m.queue[1:]
	return &ret
}

var globalMQ MessageQueue
