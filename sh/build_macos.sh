#!/bin/bash
set -e

mkdir -p obj build/bin

cpu_name="$(sysctl -n machdep.cpu.brand_string)"
if [ "$cpu_name" = "Apple M1" ]; then
    arch -arm64 make -j 9 -f Makefile.macos
else
    make -j 5 -f Makefile.macos
fi

[ -f "build/bin/nvim" ] || cp "$(grealpath $(which nvim))" build/bin/nvim
