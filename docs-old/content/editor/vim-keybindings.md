---
title: "Vim Keybindings"
menu:
  docs:
    parent: "editor"
weight: 20
toc: true
---

CodePerfect supports Vim keybindings out of the box. To enable them, run
<cite>Tools</cite> &gt; <cite>Options</cite>, and enable the <cite>Vim
keybindings</cite> options.

There are some additional things to note:

- Colon `:command`s are not currently supported. Sorry, we know this is
  important, and are working out how to support this in a safe way.

  - Importantly, `:%s/find/replace` currently does not work. For now, you can
    get around this by doing a normal `/search`, using <kbd>ce</kbd> or similar to
    modify it, then using <kbd>n</kbd><kbd>.</kbd> to jump to each occurrence and
    repeat the replacement. Sorry again; it isn't ideal.

  - Since normal `/search` still works, you may find yourself needing
    `:nohlsearch`. We've bound <kbd>C-/</kbd> to this.

  - Custom .vimrc files are also unsupported at this time.

- Clipboard integration works out of the box. <kbd>d</kbd>, <kbd>c</kbd>, and <kbd>y</kbd> all copy to the
  clipboard. <kbd>p</kbd> pastes from the clipboard.

- <kbd>gd</kbd> is mapped to Go to Definition.

- <kbd>C-o</kbd> and <kbd>C-i</kbd> (and <kbd>Tab</kbd>) are mapped to our internal implementation
  of Jump Backward/Forward.

- <kbd>q</kbd> and <kbd>k</kbd> are disabled.

- <kbd>&gt;</kbd> and <kbd>&lt;</kbd> are mapped so that they don't exit Visual mode.
  You can select some text, press <kbd>&gt;</kbd> five times, then press Escape.

- The <kbd>z</kbd> keys, <kbd>H</kbd>, <kbd>M</kbd>, <kbd>L</kbd>, <kbd>C-e</kbd>, and <kbd>C-y</kbd> work as expected.
