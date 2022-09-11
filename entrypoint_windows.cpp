#include "ostype.hpp"

#if OS_WINBLOWS

#include "world.hpp"
#include "win32.hpp"
#include "entrypoint.hpp"

extern "C" {
    // https://github.com/golang/go/issues/42190#issuecomment-1114628523
    extern __declspec(dllexport) void _rt0_amd64_windows_lib();
}

int stub(int argc, char **argv) {
    _rt0_amd64_windows_lib();
    return realmain(argc, argv);
}

#ifdef DEBUG_BUILD
    int main(int argc, char **argv) { return stub(argc, argv); }
#else
    int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) { return stub(__argc, __argv); }
#endif // DEBUG_BUILD

#endif // OS_WINBLOWS
