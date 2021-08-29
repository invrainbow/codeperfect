#!/bin/bash

set -e

brew install glfw3 glew pcre go coreutils

TMPDIR="/tmp/cwalk-for-cp95"
git clone https://github.com/likle/cwalk "$TMPDIR"
mkdir -p "$TMPDIR/build"
cd "$TMPDIR/build"
cmake ..
make
sudo make install
cd /tmp
rm -rf "$TMPDIR"