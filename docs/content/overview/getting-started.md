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

When you open CodePerfect, it prompts you for a directory to open. Select your
project folder.

## Starting a project

CodePerfect doesn't have any sort of "create project" wizard; it just reads Go
modules. Initialize a module the usual way:

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

## Adding dependencies

For the most part, the indexer runs automagically in the background.
In one case, however, it needs your assistance: when you add a dependency.

After adding a dependency to your codebase as an import and installing it
either with `go get` or `go mod tidy`, go to <cite>Tools</cite> &gt;
<cite>Rescan Index</cite>. This scans for any new packages and parses them.

## Troubleshooting the indexer

If the index is ever broken or out of sync (e.g. it's giving you incorrect
results), there are two fixes you can apply. (And please report the bug to us!)

- Go to <cite>Tools</cite> &gt; <cite>Rescan Index</cite>.  This looks for
  missing and changed packages, and processes them. 90% of the time, this
  should fix your problem. (This also runs when you restart CodePerfect).

- More drastically, go to <cite>Tools</cite> &gt; <cite>Obliterate and
  Recreate Index</cite>. This will completely re-index everything (the process
  that took place when you opened the folder for the first time).

## Automatic updates

CodePerfect automatically, unintrusively keeps itself up-to-date in the
background.
