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

2. Download the .cp95license file (provided to you), and place it in your
   home directory. You can open this directory by pressing <kbd>Win+R</kbd>
   and typing in `%USERPROFILE%`.

3. Run CodePerfect.exe, located in the root of the folder
   you unzipped in Step 1.

### macOS

1. Download CodePerfect.app.zip. Unzip this and drag CodePerfect.app into your Applications folder.

2. Download the .cp95license file (provided to you), and move it to ~/.cp95license.

3. Run CodePerfect.app.

On all platforms, please do not change or move any of files or directories
inside the unzipped application. The autoupdater depends explicitly on the
existing directory structure.

## Automatic Updates

CodePerfect automatically and unintrusively keeps itself up to date in the
background. You don't need to do anything.

## Opening a Project

Right now CodePerfect can only open modules. Your project must be organized as
a single module, with `go.mod` placed at the root of your workspace.

When you open CodePerfect, it will prompt you for a directory to open. Please
select your workspace &mdash; again, the folder with a `go.mod` at its root.

Once your folder is opened, a red INDEXING indicator will appear in the bottom right:

## Starting a Project

CodePerfect doesn't provide any sort of project creation wizard. It just knows
how to read directories containing a Go module. To create a new project, use
`go mod init <module_path>` as usual to initialize a module inside a
directory, then open that directory with CodePerfect.

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

After adding a dependency (i.e. both adding it to your codebase as an import,
and downloading it with `go get`), let the indexer know by going to
<cite>Tools</cite> &gt; <cite>Rescan Index</cite>. This does a quick scan,
checks for any new packages, and parses them. (It doesn't rebuild the whole
index.)

## Troubleshooting the Indexer

If the index is ever broken or out of sync (e.g. it's giving you incorrect
results), there are two immediate actions you can take (and please report the
bug to us!):

- First, close and restart the IDE. On startup, it does a scan to
  see if any packages are missing or have changed, and processes them. 90%
  of the time, this should fix your problem.

- If that doesn't work, you can go to <cite>Tools</cite> > <cite>Obliterate and Recreate Index</cite>.
  This will completely re-index everything (the process that took place when
  you opened the folder for the first time).
