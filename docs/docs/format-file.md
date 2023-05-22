---
title: "Format File"
---

You can format your file using `goimports` by running the `Format File` command.

:::note

This is managed for you; you don't need to install `goimports`.

:::

By default CodePerfect will run Format File when you save a file. You can toggle
this behavior under settings (see below).

By default, Format File does _not_ automatically add and remove imports. To
format with import organization, run `Format File and Organize Imports`. This
organizes imports using our native implementation, which uses our in-memory
index to provide faster and more accurate results.

Settings related to Format File can be found under `Tools` > `Options` >
`Editor Settings`:

- To format files on save, tick `Format on save`.
- To organize imports on save, tick `Fix imports after formatting`.
- To format with [`gofumpt`](https://github.com/mvdan/gofumpt)
  instead of `goimports`, tick `Use gofumpt`.

## Keyboard shortcuts

| Command                          | Shortcut         |
| -------------------------------- | ---------------- |
| Format File                      | `Option+Shift+F` |
| Format File and Organize Imports | `Option+Shift+O` |
