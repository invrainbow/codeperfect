#!/bin/bash
cd "$(dirname "$0")"

go build -o ../gohelper.dylib -buildmode=c-shared 
cp ../gohelper.dylib ../bin/gohelper.dylib
