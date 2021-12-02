---
title: "Format Document"
menu:
  docs:
    parent: "editor"
weight: 30
toc: true
---

When you save a file, CodePerfect automatically formats your file using
`goimports`. This is managed for you; you don't need to install `goimports`.

To manually format your document, run <cite>Format</cite> &gt; <cite>Format
File</cite>.

By default, Format File does _not_ automatically add and remove imports. To
format with import organization, run <cite>Format</cite> &gt; <cite>Format
File and Organize Imports</cite>.

By default, Organize Imports uses our native implementation, which runs
instantly and is more accurate than `goimports` due to our in-memory index. When
the indexer is busy, we fallback to `goimports`, which sometimes has a lag.

## Keyboard Shortcuts

| Command                          | Shortcut                             |
| -------------------------------- | ------------------------------------ |
| Format File                      | <kbd>⌥</kbd><kbd>⇧</kbd><kbd>F</kbd> |
| Format File and Organize Imports | <kbd>⌥</kbd><kbd>⇧</kbd><kbd>O</kbd> |
