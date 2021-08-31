---
title: "Setting Up the Debugger"
menu:
  docs:
    parent: "debugging"
    identifier: "setting-up-debugging"
weight: 10
toc: true
---

CodePerfect comes with an integrated debugger, allowing you to debug your programs directly from inside CodePerfect.

## Configuring the Debugger

To get started, go to <cite>Debug</cite> &gt; <cite>Debug Profiles</cite>.

Each debug profile represents a different type of debugging. Later you'll
select an active profile, and that's the profile that will when you run the
debugger.

There are currently four supported profile types:

### Test Function Under Cursor

> This is a built-in profile used for the Test Function Under Cursor feature, which
> lets you debug an individual test.
>
> Because this is a built-in profile, it can't be modified, except to add
> command-line arguments. Unless you know specifically what you're doing,
> mostly you can just leave this blank.

### Run Package

> This profile lets you run and debug a `main` package. The settings allow you
> to either configure the package path, or to just use whatever package the
> current file is located in.
>
> You can also add command-line arguments.

### Run Binary

> This profile lets you run and debug a binary built with `go build`. The
> settings allow you to specify a path to the binary as well as extra
> command-line arguments.

### Test Package

> This profile lets you run and debug all the tests in a package. The settings
> allow you to either configure the package path, or to just use whatever
> package the current file is located in.
