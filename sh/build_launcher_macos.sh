#!/bin/bash

FLAGS=""
if [ "${RELEASE}" = 1 ]; then
    FLAGS="$FLAGS 
fi

pushd gostuff
go build -o ../build/launcher github.com/invrainbow/ide/gostuff/cmd/launcher
popd
