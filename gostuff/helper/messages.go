package helper

type Message struct {
	Message string
	IsPanic bool
}

var MessageQueue []Message

func EnqueueMessage(msg string, ispanic bool) {
	MessageQueue = append(MessageQueue, Message{
		Message: msg,
		IsPanic: ispanic,
	})
}
