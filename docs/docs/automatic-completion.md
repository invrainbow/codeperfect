---
title: "Automatic Completion"
---

As you type code, CodePerfect automatically suggests completions in a menu. It
performs a few types of completions:

- When you type a `.` after anything, it looks up:

  - Possible completions for the thing that precedes it. For values this
    includes fields and methods. For packages this lists all public
    declarations.
  - [Postfix completions](postfix-completion).

- When you type lone identifiers, it looks for:

  - Identifiers declared in the current scope, as well as toplevels in other
    packages you might want to use.
  - Currently imported packages and importable packages.
  - Language keywords.

## Additional ergonomics

When you select an item, CodePerfect automatically adds all necessary imports
and fills in the completion. It also inserts supporting characters &mdash; for
instance, a space after the `type` keyword, or a `(` after you select a
function.

:::note

The `(` inserted after selecting a function can be toggled in Tools &gt;
Options &gt; Code Intelligence.

:::

## Fuzzy search

All completions support fuzzy search, so you don't need to type the whole word
or even a strict substring. E.g. you could type `of` to match
`OpenFile`.

## Keyboard shortcuts

| Command                  | Shortcut     |
| ------------------------ | ------------ |
| Open autocomplete menu\* | `Ctrl+Space` |
| Select item              | `Tab`        |

\* The menu opens automatically as you type, but you can also manually trigger
it after you've closed it.
