---
title: "Go to Symbol"
---

CodePerfect indexes every toplevel declaration in your code. To jump to one,
press `Cmd+T`.

![](/go-to-symbol.png)

This lets you fuzzy-search for any symbol in your code, with each symbol keyed
as `package.identifier`. Method declarations are keyed as
`package.receiver.method`. Because of the flexibility of fuzzy search, you can
search for just the `identifier`, the entire `package.identifier`, the initials
of each word, or any fuzzy substring.

Unlike Go To File, Go To Symbol depends on the indexer. If the indexer is busy,
this feature will be disabled.

## Keyboard Shortcuts

| Command                    | Shortcut          |
| -------------------------- | ----------------- |
| Toggle Go To Symbol window | `Cmd+T`           |
| Move cursor down           | `Cmd+J` or `Down` |
| Move cursor up             | `Cmd+K` or `Up`   |
| Select file                | `Enter`           |
