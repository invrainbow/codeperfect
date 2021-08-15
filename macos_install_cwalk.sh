#!/bin/bash

# exit on errors
set -e

TMPDIR="/tmp/cwalk-for-cp95"

# clone
git clone https://github.com/likle/cwalk "$TMPDIR"

# build
mkdir -p "$TMPDIR/build"
cd "$TMPDIR/build"
cmake ..
make

# install
sudo make install

# cd out & delete
cd /tmp
rm -rf "$TMPDIR"
