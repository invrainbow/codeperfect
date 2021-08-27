@echo off

rem just make sure ide.exe isn't running
rem remedybg.exe stop-debugging
taskkill /F /IM ide.exe

cl /MP /MDd /nologo /EHsc /w /std:c++latest /DEBUG /Zi /utf-8 /Fe:bin\ide.exe^
    /DGLEW_STATIC^
    /Iw:\packages\glfw.3.3.2\build\native\include^
    /Iw:\packages\glew.1.9.0.1\build\native\include^
    /Iw:\packages\libgit2\include^
    /Iw:\tree-sitter\include^
    /Iw:\tree-sitter\src^
    *.cpp treesitter_go.c tree-sitter/src/lib.c^
    /link /NOLOGO /IGNORE:4099^
    /NODEFAULTLIB:MSVCRT /NODEFAULTLIB:LIBCMTD^
    /LIBPATH:W:\packages\glfw.3.3.2\build\native\lib\static\v142\x64^
    /LIBPATH:w:\packages\glew.1.9.0.1\build\native\lib\v100\x64\Debug\static^
    /LIBPATH:w:\packages\libgit2\lib^
    /LIBPATH:w:\packages\zlib-msvc14-x64.1.2.11.7795\build\native\lib_debug^
    gdi32.lib ole32.lib shell32.lib opengl32.lib glew.lib glfw3.lib ws2_32.lib shlwapi.lib git2.lib^
    advapi32.lib winhttp.lib rpcrt4.lib crypt32.lib zlibstaticd.lib pathcch.lib gohelper.lib winmm.lib
if %errorlevel% neq 0 exit /b %errorlevel%

copy /Y init.vim bin\init.vim
if %errorlevel% neq 0 exit /b %errorlevel%

copy /Y dynamic_helper.go bin\dynamic_helper.go
if %errorlevel% neq 0 exit /b %errorlevel%
