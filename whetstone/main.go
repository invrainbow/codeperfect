package main

import (
	"fmt"
	"github.com/invrainbow/whetstone/models"
	"github.com/invrainbow/whetstone/controllers"
)

type Bar struct {
	x int
}

func (b *Bar) Print() {
	fmt.Printf("%d", x)
}

type Foo struct {
	*Bar
	y int
}

func main() {
	// models.Item

	foo := &Foo{}
}