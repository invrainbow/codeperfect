---
title: Platform Differences
---

CodePerfect is intended to be cross-platform with the same experience across
platforms and for the most part adheres to this goal. There are, however, a few
differences:

- On Windows and Linux the primary key is `Ctrl`. On Mac it's `Cmd`. This is the
  key the docs are referring to when it says `Primary`, e.g. `Primary+K` would
  mean `Ctrl+K` on Windows and `Cmd+K` on Mac.

- In general, CodePerfect locates binaries that it needs (`go`, `dlv`) by trying
  to find it in a shell. On Mac and Linux it runs `which <binary>` in bash. On
  Windows it runs `where <binary>` in cmd.
