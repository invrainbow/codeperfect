---
title: "Getting Started"
---

## Install CodePerfect

Currently, CodePerfect is only available for Mac. Both Intel and M1 are
supported through a
[universal binary](https://developer.apple.com/documentation/apple-silicon/building-a-universal-macos-binary).
Get it from the [download page](https://codeperfect95.com/download), unzip it,
and drag CodePerfect.app into your Applications folder.

:::caution

The first time you run CodePerfect, you may get an "Unidentified developer"
warning. If this happens, right click CodePerfect.app and click Open. Then press
Open again. You'll only have to do this once.

:::

## Install Go

CodePerfect requires Go version 1.13+. On Mac, two common ways of installing Go
are using the [official installer](https://go.dev/dl/), or using
[Brew](https://formulae.brew.sh/formula/go).

## Find your Go installation

CodePerfect essentially uses whatever `go` binary Bash uses. Specifically, it
calls `which go` in a Bash shell. If you can run `go version` successfully in a
Bash shell, CodePerfect will know how to find it.

:::info

If you're using a different shell (e.g. zsh) and relying on setting your PATH
manually (e.g. inside `~/.zshrc`) to make Go available, you'll need to edit
`~/.bash_profile` to set your PATH:

```bash
export PATH=$PATH:/path/to/folder/containing/go
```

:::

Below are some notes for some of the common ways that people install Go:

- **Using the official installer.** The installer sets your system `$PATH`
  &mdash; there's nothing else you need to do.

* **Use Brew.** You'll just need to make sure Brew-installed binaries are
  accessible from a terminal. If `which go` resolves successfully in a Bash
  shell, there's nothing else you need to do.

## Projects

CodePerfect doesn't really have the concept of a project. It simply knows how to
read a Go module. When you open CodePerfect, it prompts you to select a folder.
It expects a folder containing a `go.mod` at the root of the folder.

:::note

Currently, nested modules are not supported.

:::

When you open a folder, CodePerfect creates some internally used files inside:

- `.cpproj`
- `.cpdb`

These are responsible for saving your project-specific settings, such as your
build and debug profiles, and writing our index of your code to disk.

We recommend committing `.cpproj` to version control (for consistent build
configurations), but excluding `.cpdb` (as it will be rebuilt on each machine).
Note that none of these files are human-readable.

### Open an existing project

CodePerfect can open any existing codebase, as long as it's organized as a
single Go module. Just select the folder containing the module.

If you're opening a pre-existing project, CodePerfect relies on
`go list -mod=mod -m all` to find your dependencies, and if you have
un-downloaded dependencies, the indexer may not fully work. Make sure all
dependencies have been downloaded:

```
$ go mod tidy
```

### Create a new project

Since CodePerfect simply knows how to open Go modules, you can initialize one
the usual way:

```bash
$ go mod init $YOUR_MODULE_PATH
```

## Indexing

CodePerfect automatically scans and parses your code to understand it. While
it's doing that, the bottom right will display a red <span
class="indexing">INDEXING</span> indicator. During this time, CodePerfect cannot
provide code intelligence, and features like autocomplete will be disabled. When
it's done indexing, the indicator will turn into a green <span
class="index-ready">INDEX READY</span>.

If you click the indicator, you can see what the indexer is doing.

## Troubleshooting the indexer

In theory the indexer runs automagically in the background and keeps up to date
by itself as your code and dependencies change. If it's ever broken or giving
you incorrect results, however, there are two fixes you can try. (And please
report the bug to us!)

- Run `Tools` &gt; `Rescan Index`. This looks for missing and changed packages,
  and processes them. Most of the time, this should fix your problem. This also
  runs when you restart CodePerfect.

- More drastically, run `Tools` &gt; `Obliterate and Recreate Index`. This will
  completely re-index everything. This runs when you open a folder for the first
  time.

## Automatic updates

CodePerfect automatically, unintrusively keeps itself up-to-date in the
background. But if you ever want/need to update CodePerfect manually, you can
grab the latest version from the [download page](https://codeperfect95.com).
