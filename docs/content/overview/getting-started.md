---
title: "Getting Started"
menu:
  docs:
    parent: "overview"
weight: 10
toc: true
---

## Installing and Running

Please refer to the instructions for your OS.

### Windows

1. Download CodePerfect.zip (provided to you). Unzip this anywhere.

2. Download the `.cp95license` file (provided to you), and place it in your
   home directory. You can open this directory by pressing <kbd>Win+R</kbd>
   and typing in `%USERPROFILE%`.

3. Run CodePerfect.exe, located in the root of the folder
   you unzipped in Step 1.

### macOS

1. Download CodePerfect.app.zip. Unzip this and drag CodePerfect.app into your Applications folder.

2. Download the `.cp95license` file (provided to you), and place it in `~/.cp95license`.

3. Run CodePerfect.app.

On all platforms, please do not change or move any of files or directories inside the unzipped
application. The autoupdater depends explicitly on the existing directory
structure.

## Opening a Project

Right now CodePerfect can only open modules. Your project must be organized as
a single module, with `go.mod` placed at the root of your workspace.

When you open CodePerfect, it will prompt you for a directory to open. Please
select your workspace &mdash; again, the folder with a `go.mod` at its root.

Once your folder is opened, a red INDEXING indicator will appear in the bottom right:

(img here)

When it's done indexing, the indicator will turn into a green READY:

(img here)

// ???

## Working With the Indexer

The indexer is the part of CodePerfect that parses your code and builds a
database of information about it. It has a single goal: keep that database up
to date. For the most part, the indexer automagically updates itself in the background
without you knowing.

There is one case where human intervention is still needed:

- After new dependencies have been downloaded and added to go.mod, the Indexer
  needs to be manually notified. You can do this by going to Tools > Rescan
  Index:

  (img here)

  This doesn't re-build the entire index. It just does a quick scan, checks if
  there are any new packages, and parses them.

If the index is ever broken or out of sync (e.g. it's giving you incorrect
results), there are two immediate actions you can take (and please report the
bug to us!):

- First, close and restart the IDE. On startup, it does a scan to
  see if any packages are missing or have changed, and processes them. 90%
  of the time, this should fix your problem.

- If that doesn't work, you can go to Tools > Obliterate and Recreate Index.
  This will completely re-index everything (the process that took place when
  you opened the folder for the first time).
