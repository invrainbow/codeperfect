#!/bin/bash

compile() {
    g++ \
        -g \
        "$@" \
        $(pkg-config --cflags --libs gtk+-3.0) \
        $(pkg-config --cflags --libs glfw3) \
        $(pkg-config --cflags --libs glew) \
        -std=c++17 \
        -fcompare-debug-second \
        -lstdc++fs \
        -ldl
}

if [[ -n "$1" ]]; then
    compile "$1"
else
    compile *.cpp -o ide
fi
