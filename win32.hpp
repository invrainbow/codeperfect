#pragma once

#define OEMRESOURCE
#include <ws2tcpip.h>
#include <winsock2.h>
#include <windows.h>
#include "common.hpp"

wchar_t* to_wide(ccstr s, int slen = -1);
ccstr to_utf8(const wchar_t *s, int slen = -1);
