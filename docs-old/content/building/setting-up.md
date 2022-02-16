---
title: "Setting Up Builds"
menu:
  docs:
    parent: "building"
    identifier: "setting-up-building"
weight: 10
toc: true
---

CodePerfect supports integrated builds. It will run a build command, grab the
errors, and allow you to easily navigate between them. To get started, go to
<cite>Build</cite> &gt; <cite>Build Profiles...</cite>

![](/build-profiles.png)

Use the list on the left to add, remove, and select remove different profiles.

On the right, you can configure a given profile. Currently all you can do is
edit the name and build command. For the build command, CodePerfect can parse
the output of any command that prints errors in the same format as `go build`.

In the most basic case, you can simply enter `go build` to build the current
module.
