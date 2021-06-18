#!/bin/bash

compile() {
    clang++ "$@" \
        -g -mavx -maes -std=c++17 -w -ldl \
        $(pkg-config --cflags --libs gtk+-3.0) \
        $(pkg-config --cflags --libs glfw3) \
        $(pkg-config --cflags --libs glew) \
        # -fcompare-debug-second \
}

if [[ -n "$1" ]]; then
    compile "$1"
else
    compile *.cpp -o bin/ide
fi
