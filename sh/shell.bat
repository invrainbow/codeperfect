@echo off

rem stop cursor from blinking
rem https://github.com/microsoft/terminal/issues/1379#issuecomment-737121370
rem microsoft does the most asinine shit sometimes i swear
@echo [2 q

"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x86_amd64 >NUL
