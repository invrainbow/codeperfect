@echo off

rem do we need this shit?
rem -ldflags "-s -w" -o

go build -o ../gohelper.dll -buildmode=c-shared "github.com/codeperfect95/codeperfect/go/cmd/helper"
copy /Y ..\gohelper.dll ..\bin\gohelper.dll
