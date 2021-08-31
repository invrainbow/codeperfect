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

| Command                  | macOS                                       | Windows                                        |
| ------------------------ | ------------------------------------------- | ---------------------------------------------- |
| Toggle Go To File window | <kbd>⌘</kbd><kbd>T</kbd>                    | <kbd>Ctrl</kbd><kbd>T</kbd>                    |
| Move cursor down         | <kbd>⌘</kbd><kbd>J</kbd> or <kbd>Down</kbd> | <kbd>Ctrl</kbd><kbd>J</kbd> or <kbd>Down</kbd> |
| Move cursor up           | <kbd>⌘</kbd><kbd>K</kbd> or <kbd>Up</kbd>   | <kbd>Ctrl</kbd><kbd>K</kbd> or <kbd>Up</kbd>   |
| Select file              | <kbd>Enter</kbd>                            | <kbd>Enter</kbd>                               |
