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

On the left, it says <cite>Build Project</cite> in a list. Eventually this list
will allow you to add and remove different profiles. Right now only the one
hardcoded profile is available.

On the right, enter the build command. CodePerfect can parse the output of any
command that uses the same error format as `go build`.

In the most basic case, you will simply want to enter

```
go build <module_path>/<path_to_main_package>
```

replacing `<module_path>` and `<path_to_main_package>` with their respective values.
