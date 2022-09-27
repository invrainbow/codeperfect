---
title: "Struct Tags"
---

CodePerfect can modify struct tags to add tags to a specific field, add tags to
every field, remove tags from a specific field, or remove all tags. This can be
done by putting your cursor over the struct or field in question, then running
one of the following commands:

- `Struct: Add JSON tag`
- `Struct: Add YAML tag`
- `Struct: Add XML tag`
- `Struct: Add all JSON tags`
- `Struct: Add all YAML tags`
- `Struct: Add all XML tags`
- `Struct: Remove tag`
- `Struct: Remove all tags`

:::note

The `Struct: Add all xxx tags` and `Struct: Remove all tags` commands work
recursively, starting from the **innermost** struct that the cursor is currently
inside.

:::