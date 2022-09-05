#include "ostype.hpp"

#if OS_WINBLOWS

#include "world.hpp"
#include "win32.hpp"
#include "entrypoint.hpp"

extern "C" {
    // https://github.com/golang/go/issues/42190#issuecomment-1114628523
    extern __declspec(dllexport) void _rt0_amd64_windows_lib();
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    _rt0_amd64_windows_lib();
    return realmain(__argc, __argv);
}

#endif // OS_WINBLOWS
