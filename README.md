# CodePerfect 95

CodePerfect was an experiment to try and build a faster IDE. It eschews the
modern tech stack, is written from scratch in C/C++ like a video game. It starts
instantly, runs at 144 FPS, and has a near-zero latency experience.

It comes with native full-featured code intelligence and integrated debugging
with Delve. See more of the features [here](https://codeperfect95.com/features).

It's no longer in active development, but is now open source and available for
download.

### Install dependencies

```
sh/install_deps
```

### Building raw binary

```
sh/build
```

### Building an .app

```
SKIP_UPLOAD=1 sh/package
```

Without `SKIP_UPLOAD=1` it tries to upload to the S3 bucket, which only I have
access to.
