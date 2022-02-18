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

`Debug` &gt; `Step Over`

Step Over executes the current line and breaks at the next line.

## Step Into

`Debug` &gt; `Step Into`

If the current line contains a function call, Step Into will call the function
and break inside the first line of the function body.

If the current line contains no function call, this behaves like Step Over.

## Step Out

`Debug` &gt; `Step Out`

This runs until the current function finishes, then breaks in the caller of the
current function on the line after the function call.

## Keyboard Shortcuts

| Command   | Shortcut    |
| --------- | ----------- |
| Step Over | `F10`       |
| Step Into | `F11`       |
| Step Out  | `Shift+F11` |
