#!/bin/bash
set -e

mkdir -p obj bin

export RELEASE=1

cpu_name="$(sysctl -n machdep.cpu.brand_string)"
if [ "$cpu_name" = "Apple M1" ]; then
    arch -arm64 make -j 9 -f Makefile.macos
else
    make -j 5 -f Makefile.macos
fi

cp dynamic_helper.go bin/dynamic_helper.go
cp init.vim bin/init.vim
cp "$(grealpath $(which nvim))" bin/nvim

rm -rf dist
mkdir dist
cp -R bin/ dist/bin/

pushd gostuff
go build -o ../launcher github.com/invrainbow/ide/gostuff/cmd/launcher
popd

mv launcher dist/
rm -rf dist.zip
zip -r dist.zip dist/

current-version() {
    grep CurrentVersion gostuff/versions/versions.go | rev | cut -d' ' -f 1 | rev
}

aws s3 cp dist.zip "s3://codeperfect95/darwin_v$(current-version).zip"




