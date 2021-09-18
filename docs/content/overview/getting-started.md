---
title: "Getting Started"
menu:
  docs:
    parent: "overview"
weight: 10
toc: true
---

## Installing and running

Please refer to the instructions for your OS.

### Windows

1. Download the .zip file provided to you. Unzip it anywhere.

2. Download the .cplicense file provided to you, and place it in your
   home directory. You can open this directory by pressing <kbd>Win+R</kbd>
   and typing in `%USERPROFILE%`.

3. Run CodePerfect.exe, located in the root of the folder
   you unzipped in Step 1.

### macOS

1. Download the .zip file provided to you. Unzip it and drag CodePerfect.app into your Applications folder.

2. Download the .cplicense file provided to you, and move it to `~/.cplicense`.

3. Run CodePerfect.app.

### Install notes

On all platforms, please do not change or move any of files or directories
inside the unzipped application. The autoupdater depends explicitly on the
existing directory structure.

## Configuration

In general, CodePerfect tries to require as little configuration as possible.
Currently we just need to know where Go is installed. Create the file
`~/.cpconfig` with the following contents:

```
{
  "go_binary_path": "..."
}
```

Replace the `...` with the **folder containing the `go` binary**, not the
binary itself. So if `go` is located at `/usr/local/bin/go`, write:

```
{
  "go_binary_path": "/usr/local/bin"
}
```

## Opening a project

Right now CodePerfect can only open modules. Your project must be organized as
a single module, with `go.mod` placed at the root of your project fodler.

When you open CodePerfect, it prompts you for a directory to open. Select your
project folder (with a `go.mod` at its root).

## Starting a Project

CodePerfect doesn't provide any sort of project creation wizard. It just knows
how to read Go modules. To start a new project, just initialize a module the
usual way:

```
$ go mod init <module_path>
```

CodePerfect relies on `go list -mod=mod -m all` to find your dependencies. If
you have un-downloaded dependencies, it'll interfere with the output. So if
you're opening an existing project, make sure all dependencies have been
downloaded:

```
$ go mod download
```

(Use `go mod tidy` if you're on an older version of Go.)

## Indexing

CodePerfect automatically scans your code to understand it. When it's busy
doing that, the bottom right will display a red INDEXING indicator:

![](/index-indexing.png)

During this time, CodePerfect cannot provide code intelligence, and features
like autocomplete will be disabled. When it's done indexing, the indicator will
turn into a green INDEX READY:

![](/index-ready.png)

## Adding dependencies

For the most part, the indexer just runs automagically in the background.
However, there is one case where it needs your assistance: when you add a
dependency.

After adding a dependency &mdash; adding it to your codebase as an
import, and downloading it with `go get` &mdash; go to <cite>Tools</cite> &gt;
<cite>Rescan Index</cite>. This scans for new packages and parses them. (It
doesn't rebuild the whole index.)

## Troubleshooting the indexer

If the index is ever broken or out of sync (e.g. it's giving you incorrect
results), there are two fixes you can apply. (And please report the bug to us!)

- Close and restart the IDE. On startup, it scans to see if any packages are
  missing or have changed, and processes them. 90% of the time, this should fix
  your problem.

- If that doesn't work, you can go to <cite>Tools</cite> > <cite>Obliterate and
  Recreate Index</cite>. This will completely re-index everything (the process
  that took place when you opened the folder for the first time).

## Automatic updates

CodePerfect automatically, unintrusively keeps itself up-to-date in the
background.
