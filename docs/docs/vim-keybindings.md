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
    get around this by doing a normal `/search`, using `ce` or similar to modify
    it, then using `n.` to jump to each occurrence and repeat the replacement.
    Sorry again; it isn't ideal.

  - Since normal `/search` still works, you may find yourself needing
    `:nohlsearch`. We've bound `C-/` to this.

  - Custom .vimrc files are also unsupported at this time.

- Clipboard integration works out of the box. `d`, `c`, and `y` all copy to the
  clipboard. `p` pastes from the clipboard.

- `gd` is mapped to Go to Definition.

- `C-o` and `C-i` (and `Tab`) are mapped to our internal implementation of Jump
  Backward/Forward.

- `q` and `k` are disabled.

- `>` and `<` are mapped so that they don't exit Visual mode. You can select
  some text, press `>` five times, then press Escape.

- The `z` keys, `H`, `M`, `L`, `C-e`, and `C-y` work as expected.
