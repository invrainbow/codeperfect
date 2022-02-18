---
title: "Debug Tests"
---

CodePerfect provides a way for you to run a single test, or all the tests in a
package, with the debugger running.

## Debug a single test

Configure this by going to `Debug` &gt; `Debug Profiles...` and opening the
`Test Function Under Cursor` profile. Add any additional command-line arguments
if needed.

To debug the test, move your cursor into the test function, not the function
being tested. If you a have a function `Sum()` and a `TestSum()` test, move your
cursor into the `TestSum()` function (anywhere inside). Then go to `Debug` &gt;
`Debug Test Under Cursor`.

## Debug all tests in a package

Configure this by going to `Debug` &gt; `Debug Profiles...` and opening the
`Test Package` profile. Enter the import path of the package you want to test,
or check `Use package of current file` to use whatever file is open.

Set this debug profile as the default by going to `Debug` &gt;
`Select Active Debug Profile...` &gt; `Test Package`. Then start the debugger as
usual, by going to `Debug` &gt; `Start Debugging`.

## Keyboard Shortcuts

| Command                 | Shortcut |
| ----------------------- | -------- |
| Start Debugging         | `F5`     |
| Debug Test Under Cursor | `F6`     |
