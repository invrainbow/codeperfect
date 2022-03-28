---
title: "Generate Implementation"
---

CodePerfect can generate boilerplate methods for a type so that it implements an
interface. For instance, given a type `Foo` that you want to implement the
interface [`io.Reader`](https://pkg.go.dev/io#Reader), CodePerfect can generate:

```go
func (f *Foo) Read(p []byte) (n int, err error) {
  panic("not implemented")
}
```

Move your text cursor on top of a type that you want to generate methods for:

![](/generate-implementation.png)

Here, the cursor is on top of the `AmplitudeEvent` type.

Run `Refactor` &gt; `Generate Implementation`. This will present you with a menu
to select an interface (fuzzy-search enabled). For this example, we'll select
`io.Reader`:

![](/generate-implementation2.png)

Press Enter. In this case, CodePerfect generates the `Read` method directly
beneath the declaration of `AmplitudeEvent`:

![](/generate-implementation3.png)

You can also run `Generate Implementation` with your cursor over the interface.
In that case, CodePerfect will prompt you to select the type (`AmplitudeEvent`
in this case).