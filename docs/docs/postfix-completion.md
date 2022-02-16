---
title: "Postfix Completion"
menu:
  docs:
    parent: "code-intelligence"
weight: 40
toc: true
---

CodePerfect supports a number of postfix completions. Postfix
completions are basically a type of macro that you trigger as if they
were methods on whatever you're typing:

![](/postfix.png)

For instance, if you type `foo.`, it
will show you postfix completions such as `ifnotnil!`. If you select that, it replaces `foo.` with

```
if foo != nil {
  █
}
```

and places your cursor where the grey block is. (It doesn't actually add a
block character, it's there in the example to mark where the cursor moves.)

Postfix completions provide shortcuts that operate
_on_ some expression, to complete certain idioms or phrases. For
instance, `x.append!` becomes `append(x, █)`, and `x.aappend!`
(for "assign append") becomes `x = append(x, █)`.

Please note that you have to type `x.` and then actually select
`append!` from the menu. Nothing will happen if you just type out
`x.append!` manually.

## Supported Completions

The examples all assume the completion is operating on an expression
`foo`, e.g. `foo.append!`.
