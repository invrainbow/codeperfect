---
title: "Go to Symbol"
---

CodePerfect indexes every toplevel declaration in your code. To jump to one,
press `Primary+T`.

![](/go-to-symbol.png)

This lets you fuzzy-search for any symbol in your code, with each symbol keyed
as `package.identifier`. Method declarations are keyed as
`package.receiver.method`. Because of the flexibility of fuzzy search, you can
search for just the `identifier`, the entire `package.identifier`, the initials
of each word, or any fuzzy substring.

Go To Symbol depends on the indexer. If the indexer is busy indexing, this
feature will be temporarily disabled.

## Keyboard Shortcuts

| Command                    | Shortcut    |
| -------------------------- | ----------- |
| Toggle Go To Symbol window | `Primary+T` |
| Select file                | `Enter`     |
