package main

import (
	"fmt"
	"math/rand"

	"github.com/codeperfect95/codeperfect/go/cmd/lib"
	"golang.org/x/exp/maps"
)

type Foo struct {
	a string
	b string
	c string
	d string
	e string
}

const str = "1234567890123456789012345678901234567890123456789012345678901234"

func NewFoo() *Foo {
	f := &Foo{}
	f.a = str
	f.b = str
	f.c = str
	f.d = str
	f.e = str
	return f
}

type Baz struct {
	a int
	b bool
	c rune
}

func NewBaz() *Baz {
	ret := &Baz{}
	ret.a = rand.Intn(100)
	ret.b = rand.Intn(100)%2 == 0
	ret.c = rune('a' + rand.Intn(26))
	return ret
}

type Bar struct {
	f []*Foo
	b []*Baz
}

func NewBar() *Bar {
	ret := &Bar{}

	ret.f = []*Foo{}
	for i := 0; i < 4; i++ {
		ret.f = append(ret.f, NewFoo())
	}

	ret.b = []*Baz{}
	for i := 0; i < 4; i++ {
		ret.b = append(ret.b, NewBaz())
	}

	return ret
}

type State struct {
	b map[string]*Bar
}

func NewState() *State {
	ret := &State{}
	ret.b = map[string]*Bar{}
	for i := 0; i < 4; i++ {
		ret.b[lib.GenerateLicenseKey()] = NewBar()
	}
	return ret
}

type Final struct {
	states []*State
}

func NewFinal() *Final {
	ret := &Final{}
	ret.states = []*State{}
	for i := 0; i < 4; i++ {
		ret.states = append(ret.states, NewState())
	}
	return ret
}

func main() {
	final := NewFinal()

	for i := 0; i < 128; i++ {
		b := final.states[0].b
		keys := maps.Keys(b)
		b[keys[0]].f[0].a = fmt.Sprintf("%d", i)
	}

	fmt.Printf("%v", final)
}
