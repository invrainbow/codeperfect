---
title: "Building and Debugging"
# lead: "Doks is a Hugo theme helping you build modern documentation websites that are secure, fast, and SEO-ready — by default."
menu:
  docs:
    parent: "overview"
weight: 100
toc: true
---

CodePerfect uses **Profiles** to configure building and debugging.

## Building

CodePerfect 95 supports building your code using any command, as long as it:

 * Outputs errors in the same format as `go build`
 * Returns non-zero exit code on error

For example, you could literally run `go build`, or you could run a shell
script that performs other things as well. CodePerfect will find any lines that
match the format of `go build` and display them to you.

To configure a profile, select **Build** > **Build Profiles**.




