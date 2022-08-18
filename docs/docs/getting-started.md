---
title: "Getting Started"
---

## Install CodePerfect

CodePerfect is supported on Windows (x64), Mac (x64, ARM), and Linux (x64).
Download the appropriate binary
from the [download page](https://codeperfect95.com/download) and unzip it.

### Windows

Move the unzipped folder anywhere. A popular choice is your Program Files folder.
Run `CodePerfect.exe`.

### Linux

Move the unzipped folder anywhere. Run `./codeperfect`.

### macOS

Drag `CodePerfect.app` into your Applications folder and run it.

:::info

The first time you run it, you may get an "Unidentified developer"
warning. If this happens, right click CodePerfect.app and click Open, then press
Open again. You'll only need to do this once.

:::

## Install Go

CodePerfect requires Go version 1.13+. Preferred ways of installing Go are
using the [official installer](https://go.dev/dl/) for your platform, or using
a package manager of your choice, such as
[brew](https://formulae.brew.sh/formula/go),
[choco](https://community.chocolatey.org/packages/golang), or
[apt](https://github.com/golang/go/wiki/Ubuntu#using-ppa).

## Find your Go installation

CodePerfect essentially uses whatever `go` binary is returned by running `which
go` in bash on macOS/Linux and `where go` in cmd on Windows. More practically,
if you can run `go version` successfully in a terminal, CodePerfect will know
how to find it. The way to set this up is to make sure the `go` binary is in your `PATH`.

Below are some notes for some of the common ways that people install Go:

- **Using the official installer.** The installer sets your system `PATH` —
  there's nothing else you need to do.

- **Using a package manager.** Chances are the package manager will set your
  `PATH` for you. In any case, you'll just need to make sure Brew-installed
  binaries are accessible from a terminal. You can verify this by running
  `which go` (`where go` on Windows). If it works, there's nothing else you
  need to do. If it doesn't, you'll need to update your `PATH` manually.

## Projects

CodePerfect doesn't really have the concept of a project. It simply knows how to
read a Go module. When you open CodePerfect, it prompts you to select a folder.
It expects a folder containing a `go.mod` at the root of the folder.

:::note

Currently, nested modules are not supported.

:::

When you open a folder, CodePerfect creates some files inside for internal use:

- `.cpproj`
- `.cpdb`

These are responsible for saving your project-specific settings, such as your
build and debug profiles, and writing our index of your code to disk.

We recommend committing `.cpproj` to version control (for consistent build
configurations), but excluding `.cpdb` (as it will be rebuilt on each machine).
Note that none of these files are human-readable.

### Open an existing project

CodePerfect can open any codebase organized as a
single Go module. Just select the folder containing the root go.mod.

If you're opening a pre-existing project, CodePerfect relies on
`go list -mod=mod -m all` to find your dependencies, and if you have
un-downloaded dependencies, the indexer may not fully work. Make sure all
dependencies have been downloaded:

```
go mod download
```

### Create a new project

Since CodePerfect simply knows how to open Go modules, you can initialize one
the usual way:

```
go mod init <YOUR_MODULE_PATH>
```

## Indexing

CodePerfect automatically scans and parses your code to understand it. While
it's doing that, the bottom right will display a red <span
class="indexing">INDEXING</span> indicator. During this time, CodePerfect cannot
provide code intelligence, and features like autocomplete will be disabled. When
it's done indexing, the indicator will turn into a green <span
class="index-ready">INDEX READY</span>.

If you click the indicator, you can see what the indexer is doing.

The indexer needs your dependencies to be downloaded (so it can index them). If
you opened the project, let the indexer run, and then later downloaded the
dependencies, you can run `Tools` &gt; `Rescan Index` to pick up the new
modules.

### Troubleshooting the indexer

In theory the indexer runs automagically in the background and keeps up to date
by itself as your code and dependencies change. If it's ever broken or giving
you incorrect results, however, there are two fixes you can try. (And please
[report the bug](https://github.com/codeperfect95/issue-tracker) to us!)

- Run `Tools` &gt; `Rescan Index`. This looks for missing and changed packages,
  and processes them. Most of the time, this should fix your problem. This also
  runs when you restart CodePerfect.

- More drastically, run `Tools` &gt; `Obliterate and Recreate Index`. This will
  completely re-index everything. This runs when you open a folder for the first
  time.

## Automatic updates

CodePerfect automatically, unintrusively keeps itself up-to-date in the
background. But if you ever want/need to update CodePerfect manually, you can
grab the latest version from the [download page](https://codeperfect95.com/download).
