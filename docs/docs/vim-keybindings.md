---
title: "Vim Keybindings"
menu:
  docs:
    parent: "editor"
weight: 20
toc: true
---

CodePerfect supports Vim keybindings out of the box. To enable them, go to
`Tools` &gt; `Options` and enable the `Enable Vim keybindings` option.

We generally aim to support the bulk of the Vim feature set, but some things
either don't have an analogue in CodePerfect or provided limited value while
requiring significant effort, so we skipped those.

Some additional things to note:

- Colon `:command`s are currently unsupported.

  - Importantly, `:%s/find/replace` currently does not work. For now, you can
    get around this by using the native in-file search and replace by pressing
    `Cmd+H`. Less efficiently, you can also do a normal `/search`, use `ce` or
    similar to modify it, then use `n.` to jump to each occurrence and repeat
    the replacement.

  - Since normal `/search` still works, you may find yourself needing
    `:nohlsearch`. We've bound `C-/` to this.

- Custom `.vimrc` files are currently unsupported.

- Yanking and pasting are integrated with the system clipboard. You can also
  press `Cmd+C`/`Cmd+X` on text selected in Visual mode to copy/cut.

- `gd` is mapped to Go to Definition.

- `C-o` and `C-i` (and `Tab`) are mapped to our internal implementation of Jump
  Backward/Forward.
