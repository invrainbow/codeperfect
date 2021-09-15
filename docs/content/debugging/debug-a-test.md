---
title: "Debug Tests"
menu:
  docs:
    parent: "debugging"
weight: 60
toc: true
---

CodePerfect provides a way for you to run a single test, or all the tests in a
package, with the debugger running.

## Debug a single test

Configure this by going to <cite>Debug</cite> &gt; <cite>Debug
Profiles...</cite> and opening the <cite>Test Function Under Cursor</cite>
profile. Add any additional command-line arguments if needed.

To debug the test, move your cursor into the test function, not the function
being tested. If you a have a function `Sum()` and a `TestSum()` test, move
your cursor into the `TestSum()` function (anywhere inside). Then go to
<cite>Debug</cite> &gt; <cite>Debug Test Under Cursor</cite>.

## Debug all tests in a package

Configure this by going to <cite>Debug</cite> &gt; <cite>Debug
Profiles...</cite> and opening the <cite>Test Package</cite> profile. Enter the
import path of the package you want to test, or check <cite>Use package of
current file</cite> to use whatever file is open.

Set this debug profile as the default by doing to
<cite>Debug</cite> &gt; <cite>Select Active Debug Profile...</cite> &gt; <cite>Test Package</cite>. Then start the debugger as usual, by going to <cite>Debug</cite> &gt; <cite>Start Debugging</cite>.

## Keyboard Shortcuts

| Command                 | macOS         | Windows       |
| ----------------------- | ------------- | ------------- |
| Start Debugging         | <kbd>F5</kbd> | <kbd>F5</kbd> |
| Debug Test Under Cursor | <kbd>F6</kbd> | <kbd>F6</kbd> |
