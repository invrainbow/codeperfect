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

## Prerequisites

CodePerfect integrates the Delve debugger, so you'll need to
[install](https://github.com/go-delve/delve/blob/master/Documentation/installation/README.md)
the latest version.

Open ~/.cpconfig and add the `delve_path` field:

```
{
  "delve_path": "..."
}
```

Replace `delve_path` with the full path of your Delve binary. Get this value
using `which delve` on Mac, and `where delve` on Windows.

## Configuring the Debugger

To get started, run <cite>Debug</cite> &gt; <cite>Debug Profiles</cite>.

![](/debug-profiles.png)

The list on the left lets you add, remove, and select profiles. Each profile is
a piece of configuration that tells CodePerfect how to debug your program.

There are currently four supported profile types:

### Test Function Under Cursor

> This is used for the [Test Function Under Cursor](/debugging/debug-a-test/)
> feature, which lets you debug an individual test. CodePerfect comes with a
> built-in profile of this type, which can't be removed, except to add
> command-line arguments. You can add additional profiles of this type.
>
> The settings allow you to add command-line arguments.

### Run Package

> This profile lets you run and debug a `main` package.
>
> The settings allow you to either configure the package path, or to use
> whatever package the current file is located in. You can also add
> command-line arguments.

### Run Binary

> This profile lets you run and debug a binary built with `go build`.
>
> The settings allow you to specify a path to the binary as well as extra
> command-line arguments.

### Test Package

> This profile lets you run and debug all the tests in a package.
>
> The settings allow you to either configure the package path, or to just use
> whatever package the current file is located in.
