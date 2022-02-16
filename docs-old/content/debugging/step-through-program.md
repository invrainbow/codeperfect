---
title: "Step Through Program"
menu:
  docs:
    parent: "debugging"
weight: 40
toc: true
---

When your program is paused, you can continue execution in a controlled way.

## Step Over

<cite>Debug</cite> &gt; <cite>Step Over</cite>

Step Over executes the current line and breaks at the next line.

## Step Into

<cite>Debug</cite> &gt; <cite>Step Into</cite>

If the current line contains a function call, Step Into will call the function and
break inside the first line of the function body.

If the current line contains no function call, this behaves like Step Over.

## Step Out

<cite>Debug</cite> &gt; <cite>Step Out</cite>

This runs until the current function finishes, then breaks in the caller of the
current function on the line after the function call.

## Keyboard Shortcuts

| Command   | Shortcut                   |
| --------- | -------------------------- |
| Step Over | <kbd>F10</kbd>             |
| Step Into | <kbd>F11</kbd>             |
| Step Out  | <kbd>⇧</kbd><kbd>F11</kbd> |
