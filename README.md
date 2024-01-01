<p align="center">
  <img src="web/public/logo.png" width="64">
</p>

<h3 align='center'>CodePerfect</h3>

<p align="center">
  <a href="https://codeperfect95.com"><b>Website</b></a> ·
  <a href="https://docs.codeperfect95.com"><b>Docs</b></a> ·
  <a href="https://github.com/invrainbow/codeperfect/releases/latest"><b>Download</b></a>
</p>

<br>

CodePerfect is a fast Go IDE written in C/C++/OpenGL like a video game. It
starts instantly, runs at 144 FPS, has near-zero latency, and comes with native,
full-featured code intelligence and integrated debugging with Delve.

See more features [here](https://docs.codeperfect95.com).

It's no longer actively developed, but is now open source and free. It only
supports MacOS; there are no plans to port it.

## Installation

Grab the
[latest release](https://github.com/invrainbow/codeperfect/releases/latest) for
your architecture.

## Building

Clone the repo:

```bash
git clone https://github.com/invrainbow/codeperfect
```

By default it detects based on your machine architecture whether to build for M1
or Intel. If you're on M1 and you want to bypass this to build for Intel, create
an `.x64` file at the root of the repo:

```bash
touch .x64
```

(This lets you build for both architectures from a single M1 machine; I have the
repo cloned twice into separate `codeperfect` and `codeperfect-x64` folders.)

Next, install dependencies:

```bash
sh/install_deps
```

To build the binary, run:

```bash
sh/build
```

This builds the binary to the `./build` folder. To package the full .app, run:

```
sh/package
```

This builds the .app (and zips it up) inside the `./scratch` folder.

## Website

The website is a small, unremarkable React app built with Vite, TypeScript and
Tailwind.

To run it, install [Bun](https://bun.sh/), then cd into `./web` and run
`bun install` and `bun run dev`.

## Procedure to update version

1. Open `versions.go` and bump the version
2. Run `sh/package` for both x64 and ARM
3. Run `git tag <version>` and `git push --all`
4. Create new release in Github, upload .zip files in `./scratch`
