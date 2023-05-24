---
title: "Generate Function"
---

CodePerfect can generate a function signature based on a function call to a
non-existent function, provided that the types of all the arguments can be
deduced. For instance, if you have a call:

```go
dog := Dog{}
bark(dog, 1, false)
```

Place your cursor over `bark` and run `Generate Function From Call`. This will
generate a function:

```go
func bark(v0 Dog, v1 int, v2 bool) {
  panic("not implemented")
}
```

This works on normal function calls `foo()`, method calls `obj.foo()`, and calls
to functions in other packages `pkg.foo()`.

:::note

This is only able to generate code inside your workspace. If you try to generate
a function in a third-party package, or a method on a type defined in a
third-party package, it won't let you.

:::
