---
title: "Vim Keybindings"
menu:
  docs:
    parent: "editor"
weight: 20
toc: true
---

CodePerfect supports Vim keybindings out of the box. We aim to support the
entire feature set of Vim, so officially the answer to "which keys are supported?" is "all of them."

However, some Vim features are tricky. Below are a list of things to note.  Any
bugs here are high priority on our list; Vim integration is a key feature for
us.

- First and foremost, there's no way to turn Vim keybindings off. Currently
  they're baked into the application. We're working on adding an option to disable it and add
  normal editing keybindings.

- Colon `:command`s are not currently supported. Sorry, we know this is an important
  one. We're working out how to support this in a safe way.

  - Importantly, `:%s/find/replace` currently does not work. For now, you can
    get around this by doing a normal `/search`, using <kbd>ce</kbd> or similar to
    modify it, and then using <kbd>n</kbd> and <kbd>.</kbd> to jump to each occurrence and
    repeat the replacement. Sorry again; we know this isn't ideal.

  - Since normal `/search` still works, you may find yourself needing
    `:nohlsearch`. We've bound <kbd>C-/</kbd> to this.

  - Accordingly, custom .vimrc files are also unsupported at this time.

- Macros work on a small scale; pressing <kbd>@@</kbd> a few times is fine;
  but doing <kbd>1000@@</kbd> may freeze, depending on the macro.

- Clipboard integration works out of the box. <kbd>d</kbd>, <kbd>c</kbd>, and <kbd>y</kbd> all copy to the
  clipboard. <kbd>p</kbd> pastes from the clipboard.

- <kbd>gd</kbd> is mapped to Go to Definition.

- <kbd>C-o</kbd> and <kbd>C-i</kbd> (and <kbd>Tab</kbd>) are mapped to our internal implementation
  of Jump Backward/Forward.

- <kbd>q</kbd> and <kbd>k</kbd> are disabled.

- <kbd>&gt;</kbd> and <kbd>&lt;</kbd> in are mapped so that they don't exit Visual mode.
  You can select some text, press <kbd>&gt;</kbd> five times, then press Escape.

- The <kbd>z</kbd> keys, <kbd>H</kbd>, <kbd>M</kbd>, <kbd>L</kbd>, <kbd>C-e</kbd>, and <kbd>C-y</kbd> work as expected.
