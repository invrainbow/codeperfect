#!/bin/bash
cd "$(dirname "$0")"

go build -o gohelper.dylib -buildmode=c-shared "github.com/invrainbow/ide/gostuff/cmd/helper"
cp gohelper.dylib ../bin/gohelper.dylib
