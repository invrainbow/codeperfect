package main

type Foo struct {
	X int
	Y int
	Z struct {
		A int `xml:"a"`
		B int `xml:"b"`
		C struct {
			Lol   int `xml:"lol"`
			Dongs int `xml:"dongs"`
		} `xml:"c"`
		D int `xml:"d"`
	}
}
