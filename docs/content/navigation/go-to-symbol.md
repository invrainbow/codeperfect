---
title: "Go to symbol"
menu:
  docs:
    parent: "navigation"
weight: 20
toc: true
---

CodePerfect indexes every toplevel declaration in your code. To jump to one,
press <kbd>⌘T</kbd>.

![](/go-to-symbol.png)

This lets you fuzzy-search for any symbol in your code, with each symbol keyed
as `package.identifier`. Method declarations are keyed as
`package.receiver.method`. Because of the flexibility of fuzzy search, you can
search for just the `identifier`, the entire `package.identifier`, the initials of each word, or any fuzzy
substring.

## Keyboard Shortcuts

| Description                | Key                                            |
| -------------------------- | ---------------------------------------------- |
| Toggle Go To Symbol window | <kbd>⌘T</kbd>                                  |
| Move cursor down           | <kbd>Ctrl</kbd><kbd>J</kbd> or <kbd>Down</kbd> |
| Move cursor up             | <kbd>Ctrl</kbd><kbd>K</kbd> or <kbd>Up</kbd>   |
| Select symbol              | <kbd>Enter</kbd>                               |
