---
title: "Automatic Completion"
---

As you type code, CodePerfect automatically suggests completions in a menu. It
performs a few types of completions:

- When you type a `.` after anything, it looks up:

  - Possible completions for the thing that precedes it. For values this
    includes fields and methods. For packages this lists all public
    declarations.

  - [Postfix completions](/docs/postfix-completion), covered in
    the linked page.

- When you type lone identifiers, it looks for:

  - Identifiers declared in the current scope, as well as toplevels in other
    packages you might want to use.

  - Language keywords.

When you select an item, CodePerfect automatically adds all necessary imports
and fills in the completion. It also inserts supporting characters -- for
instance, a space after the `type` keyword, or a `(` after you select a
function.

All completions support fuzzy search, so you don't need to type the whole word
or even a strict substring. For instance, you could type `of` to match
`OpenFile`.

## Keyboard Shortcuts

| Command                  | Shortcut                             |
| ------------------------ | ------------------------------------ |
| Open autocomplete menu\* | <kbd>Ctrl Space</kbd>                |
| Move cursor down         | <kbd>Ctrl J</kbd> or <kbd>Down</kbd> |
| Move cursor up           | <kbd>Ctrl K</kbd> or <kbd>Up</kbd>   |
| Select item              | <kbd>Tab</kbd>                       |

\* The menu opens automatically as you type, but you can also manually trigger it after you've closed it.
