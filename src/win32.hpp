#pragma once

#define OEMRESOURCE
#include <ws2tcpip.h>
#include <winsock2.h>
#include <windows.h>
#include "common.hpp"

wchar_t* to_wide(ccstr s, int slen = -1);
ccstr to_utf8(const wchar_t *s, int slen = -1);

// enable the new styles or something
#if defined _WIN64
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
