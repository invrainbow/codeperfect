<img src="logo.png" width="64">

# CodePerfect

CodePerfect was an experiment to build a faster IDE. It eschews the modern tech
stack and is instead written from scratch in C/C++/OpenGL like a video game.

It starts instantly, runs at 144 FPS, has near-zero latency, and comes with
native, full-featured code intelligence and integrated debugging with Delve.
See more of the features [here](https://docs.codeperfect95.com).

It's no longer actively developed, but is now open source and free. It only
supports MacOS and there are currently no plans to port it.

### Links

- [Website](https://codeperfect95.com)
- [Docs](https://docs.codeperfect95.com)
- [Changelog](https://docs.codeperfect95.com/changelog)
- [Download](https://codeperfect95.com/download)

### Build

#### Install dependencies

```
sh/install_deps
```

#### Build raw binary

```
sh/build
```

#### Build .app

```
SKIP_UPLOAD=1 sh/package
```

Without `SKIP_UPLOAD=1` it tries to upload to the S3 bucket, which only I can
access.
