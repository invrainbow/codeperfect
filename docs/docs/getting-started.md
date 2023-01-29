---
title: "Getting Started"
---

## Install CodePerfect

CodePerfect currently supports Windows (x64) and Mac (x64, arm64);
[download](https://codeperfect95.com/download) and unzip the appropriate binary.

### Windows

Move the unzipped folder anywhere. A popular choice is your Program Files
folder. Run `CodePerfect.exe`.

### macOS

Drag `CodePerfect.app` into your Applications folder and run it.

:::info

The first time you run it, you may get an "Unidentified developer" warning. If
this happens, right click CodePerfect.app and click Open, then press Open again.
You'll only need to do this once.

:::

## Install Go

CodePerfect requires Go version 1.13+. Preferred ways of installing Go are using
the [official installer](https://go.dev/dl/) for your platform, or using a
package manager of your choice, such as
[brew](https://formulae.brew.sh/formula/go) or
[choco](https://community.chocolatey.org/packages/golang).

## Find your Go installation

CodePerfect basically uses whatever `go` binary your terminal does. It runs:

- macOS: `which go` in `bash`
- Windows: `where go` in `cmd`

The way to make the `go` binary findable is to ensure it's in your `PATH`.

:::note

If you use the official installer, it should set your system `PATH` for you. If
you used a package manager, you may or may not need to set the `PATH` manually.

:::

## Projects

CodePerfect doesn't really have the concept of a project. It simply knows how to
read a Go module or workspace. When you open CodePerfect, it prompts you to
select a folder:

- **Module:** CodePerfect can open a single module. In this case, it expects a
  folder containing a `go.mod` file at the root of the folder.
- **Workspace:** CodePerfect can open any folder that is, or belongs to, a
  workspace. This means any folder that contains a `go.work`, or has a parent
  folder that contains a `go.work`. CodePerfect runs
  `go env GOWORK` to detect what workspace (if any) the current folder belongs to.

When you open a folder, CodePerfect creates some files inside for internal use:

- `.cpproj` &mdash; project-specific settings
- `.cpdb` &mdash; the index of your code

We recommend committing `.cpproj` to version control (for consistent build and debug
configurations), but excluding `.cpdb` (it needs to be rebuilt on each machine).
Note that none of these files are human-readable.

### Open an existing project

CodePerfect can open any Go module or workspace as described above. Just run
CodePerfect and select the right folder.

If you're opening a pre-existing project, CodePerfect uses `go list -m all`
to find your dependencies, and if you have un-downloaded dependencies, the
indexer will naturally be unable to find them. You can ensure they've been
downloaded:

```
go mod tidy
```

### Create a new project

You can initialize a Go module the usual way:

```
go mod init <YOUR_MODULE_PATH>
```

You can also create a workspace; see the
[Go docs](https://go.dev/doc/tutorial/workspaces).

## Indexing

CodePerfect automatically scans and parses your code to understand it. While
it's doing that, the bottom right will display a red <span
class="indexing">INDEXING</span> indicator. During this time, CodePerfect cannot
provide code intelligence, and features like autocomplete will be disabled. When
it's done indexing, the indicator will become a green <span
class="index-ready">INDEX READY</span>.

If you click the indicator, you can see what the indexer is doing.

The indexer needs your dependencies to be downloaded (so it can index them). If
you opened the project, let the indexer run, and then later downloaded the
dependencies, you can run `Tools` &gt; `Rescan Index` to pick up the new
modules.

### Troubleshooting the indexer

In theory the indexer runs in the background and automagically keeps up to date
as your code and dependencies change. If it's ever broken or giving you
incorrect results, however, there are two fixes you can try. (And please
[report the bug](https://github.com/codeperfect95/issue-tracker) to us!)

- Run `Tools` &gt; `Rescan Index`. This looks for missing and changed packages,
  and processes them. Most of the time, this should fix your problem. This also
  runs when you restart CodePerfect.

- More drastically, run `Tools` &gt; `Obliterate and Recreate Index`. This will
  completely re-index everything, as if you were opening a folder for the first
  time.

## Automatic updates

CodePerfect automatically, unintrusively keeps itself up-to-date in the
background. But you can also always grab the latest version from the
[download page](https://codeperfect95.com/download).
