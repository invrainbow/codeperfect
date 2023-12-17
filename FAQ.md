# FAQ

## What makes it fast?

There isn't any single big trick, besides just eschewing all the cumulative
bloat that makes modern software slow. In particular we

- Use a low level language (C/C++) and render our own UI with OpenGL.
- Write simple, linear-control-flow,
  [non-pessimized](https://www.youtube.com/watch?v=pgoetgxecw8) code, without
  unnecessary abstractions.
- Manage memory manually with arena allocators.
- Limit use of third-party libraries and frameworks.

This lets us own our entire stack and maintain visibility into exactly what our
application is doing.

Where it makes sense, we use some optimizations (e.g. file mappings and
multithreading), but mostly we're just coding in a straightforward way, and it's
the hardware that's fast.

## What does the name mean?

It's a throwback to an era when software was way
[faster](https://www.youtube.com/watch?v=GC-0tCy4P1U&t=2168s), despite running
on hardware much slower than a phone today.

## Got other questions?

Reach out at [support@codeperfect95.com](mailto:support@codeperfect95.com).
