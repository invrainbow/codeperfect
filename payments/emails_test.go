package main_test

import (
	"testing"

	"github.com/invrainbow/ide/payments"
)

func TestSendEmail(t *testing.T) {
	main.SendEmail("brhs.again@gmail.com", "<p>hi</p>", "hi", "saying hello to you")
}
