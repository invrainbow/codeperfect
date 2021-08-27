#!/bin/bash
set -e

mkdir -p obj bin

export PAC
cpu_name="$(sysctl -n machdep.cpu.brand_string)"
if [ "$cpu_name" = "Apple M1" ]; then
    arch -arm64 make -j 9 -f Makefile.macos
else
    make -j 5 -f Makefile.macos
fi

cp dynamic_helper.go bin/dynamic_helper.go
cp init.vim bin/init.vim

[ -f "bin/nvim" ] || cp "$(grealpath $(which nvim))" bin/nvim
