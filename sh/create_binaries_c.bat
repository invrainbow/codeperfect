break>binaries.c
call loop %*

:loop
xxd -i %~1 >> binaries.c
shift
if not "%~1"=="" goto loop
