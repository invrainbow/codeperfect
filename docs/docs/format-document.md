---
title: "Format Document"
---

When you save a file, CodePerfect automatically formats your file using
`goimports`. This is managed for you; you don't need to install `goimports`.

To manually format your document, run `Format` &gt; `Format File`.

By default, Format File does _not_ automatically add and remove imports. To
format with import organization, run `Format` &gt;
`Format File and Organize Imports`.

By default, Organize Imports uses our native implementation, which runs
instantly and is more accurate than `goimports` due to our in-memory index. When
the indexer is busy, we fallback to `goimports`, which sometimes has a lag.

## Keyboard shortcuts

| Command                          | Shortcut         |
| -------------------------------- | ---------------- |
| Format File                      | `Option+Shift+F` |
| Format File and Organize Imports | `Option+Shift+O` |
