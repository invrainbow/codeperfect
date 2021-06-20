#!/bin/bash
set -e
make -j 9 -f Makefile.macos
cp dynamic_helper.go bin/dynamic_helper.go
