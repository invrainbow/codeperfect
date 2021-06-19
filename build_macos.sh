#!/bin/bash

set -e

compile() {
    clang++ "$@" tree-sitter/src/go.c \
        -g -mavx -maes -std=c++17 -w -ldl -lcwalk \
        $(pkg-config --cflags --libs gtk+-3.0) \
        $(pkg-config --cflags --libs glfw3) \
        $(pkg-config --cflags --libs glew) \
        $(pkg-config --cflags --libs tree-sitter) \
        -framework CoreVideo -framework OpenGL -framework IOKit -framework Cocoa -framework Carbon
}

if [[ -n "$1" ]]; then
    compile "$1"
else
    compile *.cpp -o bin/ide
fi
