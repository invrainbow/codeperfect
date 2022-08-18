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

There are some additional things to note:

- Colon `:command`s are not currently supported. Sorry, we know this is
  important, and are working out how to support this in a safe way.

  - Importantly, `:%s/find/replace` currently does not work. For now, you can
    get around this by using the native in-file search and replace by pressing
    `Primary+H`. If you want to use Vim, you can do a normal `/search`, use
    `ce` or similar to modify it, then use `n.` to jump to each occurrence and
    repeat the replacement.

  - Since normal `/search` still works, you may find yourself needing
    `:nohlsearch`. We've bound `C-/` to this.

  - Custom .vimrc files are also unsupported at this time.

- To copy to the clipboard, press `Primary+C` while the text
  you want is selected in Visual mode.

- To paste from the clipboard, press `Primary+V` in insert mode.

- `gd` is mapped to Go to Definition.

- `C-o` and `C-i` (and `Tab`) are mapped to our internal implementation of Jump
  Backward/Forward.

- `q` and `k` are disabled.

- `>` and `<` are mapped so that they don't exit Visual mode. You can select
  some text, press `>` five times, then press Escape.

- The `z` keys, `H`, `M`, `L`, `C-e`, and `C-y` work as expected.
