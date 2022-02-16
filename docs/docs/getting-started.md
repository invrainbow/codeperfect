---
title: "Getting Started"
menu:
  docs:
    parent: "overview"
weight: 10
toc: true
---

## Install

Please follow the instructions on your download page.

## Opening a project

CodePerfect can only open modules (GOPATH is not supported). Your
project must be organized as a single module, with `go.mod` at the root
of your project folder.

When you open CodePerfect, it prompts you to select your project folder.

## Starting a project

CodePerfect doesn't have a "create project" wizard; it just reads Go modules.
Initialize a module the usual way:

```
$ go mod init <module_path>
```

CodePerfect relies on `go list -mod=mod -m all` to find your dependencies. If
you have un-downloaded dependencies, the indexer won't return complete results.
So with existing projects, make sure dependencies have been
downloaded:

```
$ go mod tidy
```

## Indexing

CodePerfect automatically scans your code to understand it. While it's doing
that, the bottom right will display a red <span
class="indexing">INDEXING</span> indicator. During this time, CodePerfect
cannot provide code intelligence, and features like autocomplete will be
disabled. When it's done indexing, the indicator will turn into a green <span
class="index-ready">INDEX READY</span>.

If you click the indicator, you can see what the indexer is doing.

## Troubleshooting the indexer

In theory the indexer runs automagically in the background and keeps up to date
by itself. If it's ever broken or giving you incorrect results, however, there
are two fixes you can try. (And please report the bug to us!)

- Run `Tools` &gt; `Rescan Index`. This looks for
  missing and changed packages, and processes them. Most of the time, this
  should fix your problem. (This runs when you restart CodePerfect).

- More drastically, run `Tools` &gt; `Obliterate and Recreate Index`. This will completely re-index everything. (This runs
  when you open a folder for the first time.)

## Automatic updates

CodePerfect automatically, unintrusively keeps itself up-to-date in the
background.
