#pragma once

#define OS_WINBLOWS 0
#define OS_MAC 0
#define OS_LINUX 0

#if defined(OSTYPE_WINDOWS)
#   undef OS_WINBLOWS
#   define OS_WINBLOWS 1
#elif defined(OSTYPE_MAC)
#   undef OS_MAC
#   define OS_MAC 1
#elif defined(OSTYPE_LINUX)
#   undef OS_LINUX
#   define OS_LINUX 1
#else
#   error "what the fuck OS is this?"
#endif
