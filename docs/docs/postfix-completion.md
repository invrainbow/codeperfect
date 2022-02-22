---
title: "Postfix Completion"
menu:
  docs:
    parent: "code-intelligence"
weight: 40
toc: true
---

CodePerfect supports a number of postfix completions. Postfix completions are
basically a type of macro that you trigger as if they were methods on whatever
you're typing:

![](/postfix.png)

For instance, if you type `foo.`, it will show you postfix completions such as
`ifnotnil!`. If you select that, it replaces `foo.` with

```go
if foo != nil {
  █
}
```

and places your cursor where the grey block is. (It doesn't add an actual block
character, it's there in the example to mark where the cursor moves.)

Postfix completions provide shortcuts that operate _on_ some expression, to
complete certain idioms or phrases. For instance, `x.append!` becomes
`append(x, █)`, and `x.aappend!` (for "assign append") becomes
`x = append(x, █)`.

:::note

To use a postfix completion, you have to type `x.` and then actually select
`append!` from the menu. Nothing will happen if you just type out `x.append!`
manually.

:::

## Supported Completions

The examples all assume the completion is operating on an expression `foo`, e.g.
`foo.append!`.

<table className="postfixes">
<thead>
  <tr>
   <th>Completion</th>
   <th>Result</th>
  </tr>
</thead>
<tbody>
<tr>
<td><code>.aappend!</code></td>
<td><code>foo = append(foo, █)</code></td>
</tr><tr>
<td><code>.append!</code></td>
<td><code>append(foo, █)</code></td>
</tr><tr>
<td><code>.len!</code></td>
<td><code>len(foo)█</code></td>
</tr><tr>
<td><code>.cap!</code></td>
<td><code>cap(foo)█</code></td>
</tr><tr>
<td><code>.for!</code></td>
<td>

```
for key, val := range foo {
  █
}
```

</td>
</tr><tr>
<td><code>.forkey!</code></td>
<td>

```go
for key := range foo {
  █
}
```

</td>
</tr><tr>
<td><code>.forvalue!</code></td>
<td>

```go
for _, val := range foo {
  █
}
```

</td>
</tr><tr>
<td><code>.nil!</code></td>
<td><code>foo == nil█</code></td>
</tr><tr>
<td><code>.notnil!</code></td>
<td><code>foo != nil█</code></td>
</tr><tr>
<td><code>.not!</code></td>
<td><code>!foo█</code></td>
</tr><tr>
<td><code>.empty!</code></td>
<td><code>foo == nil || len(foo) == 0█</code></td>
</tr><tr>
<td><code>.ifempty!</code></td>

<td>

```go
if foo == nil || len(foo) == 0 {
  █
}
```

</td>

</tr><tr>
<td><code>.if!</code></td>

<td>

```go
if foo {
  █
}
```

</td>

</tr><tr>
<td><code>.ifnot!</code></td>

<td>

```go
if !foo {
  █
}
```

</td>

</tr><tr>
<td><code>.ifnil!</code></td>

<td>

```go
if foo == nil {
  █
}
```

</td>

</tr><tr>
<td><code>.ifnotnil!</code></td>

<td>

```go
if foo != nil {
  █
}
```

</td>

</tr><tr>
<td><code>.check!</code></td>

<td>

```go
val, err := foo
if err != nil {
  █
}
```

</td>

</tr><tr>
<td><code>.defstruct!</code></td>

<td>

```go
type foo struct {
  █
}
```

</td>

</tr><tr>
<td><code>.definterface!</code></td>

<td>

```go
type foo interface {
  █
}
```

</td>

</tr><tr>
<td><code>.switch!</code></td>

<td>

```go
switch foo {
  █
}
```

</td>
</tr>
</tbody>
</table>
