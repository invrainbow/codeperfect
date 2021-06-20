@echo off

rem do we need this shit?
rem -ldflags "-s -w" -o 

go build -o ../gohelper.dll -buildmode=c-shared 
copy /Y ..\gohelper.dll ..\bin\gohelper.dll
