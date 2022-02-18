---
title: "Examine Program State"
---

When your program is stopped, you can inspect the state of the program.

## Call stack

In the Call Stack window, you can view the list of currently running goroutines,
and within each goroutine, the stack of calls that led to the program's current
execution point. Each new call is a **frame** &mdash; a point in the program's
execution with its own surrounding context of variables.

Clicking a frame in the will take you to it. The Local Variables and Watches
windows will be updated to reflect the values in the current frame.

## Local Variables

When you select a frame, the Local Variables window displays all the locally
declared variables, including function arguments.

This does not include global variables.

## Watches

Watches essentially allow you to evaluate arbitrary expressions. As you step or
switch to different frames, the watch automatically shows the updated value.

To add a watch, go to the Watches window, and click the next available line in
the table. This activates a text field into which you can type the expression
you want to evaluate.

Arbitrary Go expressions are supported, which includes function calls. However,
support for this is still inconsistent; sometimes the function call will fail to
evaluate. CodePerfect integrates the Delve debugger, so any upstream issues are
out of our hands.
