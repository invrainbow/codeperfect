package helper

type Message struct {
	Text    string
	Title   string
	IsPanic bool
}

var MessageChan chan Message

func PushMessage(text string, title string, ispanic bool) bool {
	m := Message{
		Text:    text,
		Title:   title,
		IsPanic: ispanic,
	}

	select {
	case MessageChan <- m:
		return true
	default:
		return false
	}
}
