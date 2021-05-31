@echo off

rem just make sure ide.exe isn't running
rem remedybg.exe stop-debugging
taskkill /F /IM ide.exe

cl /MP /MDd /nologo /EHsc /w /std:c++latest /DEBUG /Zi /utf-8 /Fe:ide.exe^
    /DGLEW_STATIC^
    /Iw:\packages\glfw.3.3.2\build\native\include^
    /Iw:\packages\glew.1.9.0.1\build\native\include^
    /Iw:\packages\libgit2\include^
    /Iw:\tree-sitter\include^
    /Iw:\tree-sitter\src^
    main.cpp buffer.cpp common.cpp debugger.cpp editor.cpp go.cpp^
    imgui.cpp imgui_tables.cpp imgui_demo.cpp imgui_draw.cpp imgui_widgets.cpp list.cpp^
    nvim.cpp os.cpp os_windows.cpp meow_hash.cpp hash.cpp^
    settings.cpp tests.cpp ui.cpp utils.cpp fonts.cpp world.cpp^
    impl.cpp fzy_match.cpp mem.cpp tree-sitter/src/lib.c unicode.cpp^
    /link /NOLOGO /IGNORE:4099^
    /NODEFAULTLIB:MSVCRT /NODEFAULTLIB:LIBCMTD^
    /LIBPATH:W:\packages\glfw.3.3.2\build\native\lib\static\v142\x64^
    /LIBPATH:w:\packages\glew.1.9.0.1\build\native\lib\v100\x64\Debug\static^
    /LIBPATH:w:\packages\libgit2\lib^
    /LIBPATH:w:\packages\zlib-msvc14-x64.1.2.11.7795\build\native\lib_debug^
    gdi32.lib ole32.lib shell32.lib opengl32.lib glew.lib glfw3.lib ws2_32.lib shlwapi.lib git2.lib^
    advapi32.lib winhttp.lib rpcrt4.lib crypt32.lib zlibstaticd.lib pathcch.lib
