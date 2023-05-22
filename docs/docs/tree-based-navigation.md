---
title: "Tree-Based Navigation"
---

You can navigate by traversing the current file's syntax tree. Run the
`Enter Tree-Based Navigation` command to enter tree-based navigation.

- Press `Up` or `Left` to move to the previous sibling.
- Press `Down` or `Right` to move to the next sibling.
- Press `Shift+Up` or `Shift+Left` to move to the parent.
- Press `Shift+Down` or `Shift+Right` to move to the first child.
- The `h` `j` `k` `l` keys can be used instead of the arrow keys.
- Press `Escape` to leave tree-based navigation.

## Vim integration

For ergonomic reasons, if Vim keybindings are enabled, the `s` `c` `x`
keys behave as if the selected node were visual-selected in Vim.

## Keyboard shortcuts

| Command                     | Shortcut                         |
| --------------------------- | -------------------------------- |
| Enter Tree-Based Navigation | `Ctrl+Alt+A` or `gt` in Vim mode |
| Leave Tree-Based Navigation | `Ctrl+Alt+A` or Escape           |
