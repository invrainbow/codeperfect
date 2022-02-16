---
title: "Go to Symbol"
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
search for just the `identifier`, the entire `package.identifier`, the initials
of each word, or any fuzzy substring.

Go To Symbol depends on the indexer. If the indexer is busy, this feature will
be disabled.

## Keyboard Shortcuts

| Command                    | Shortcut                         |
| -------------------------- | -------------------------------- |
| Toggle Go To Symbol window | <kbd>⌘T</kbd>                    |
| Move cursor down           | <kbd>⌘J</kbd> or <kbd>Down</kbd> |
| Move cursor up             | <kbd>⌘K</kbd> or <kbd>Up</kbd>   |
| Select file                | <kbd>Enter</kbd>                 |
