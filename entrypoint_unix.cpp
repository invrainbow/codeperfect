#include "ostype.hpp"

#if OS_UNIX

#include "world.hpp"
#include "entrypoint.hpp"

int main(int argc, char **argv) {
    // setup crash handlers
    signal(SIGSEGV, crash_handler);
    signal(SIGILL, crash_handler);
    signal(SIGBUS, crash_handler);
    signal(SIGABRT, crash_handler);
    signal(SIGFPE, crash_handler);

    return realmain(argc, argv);
}

#endif // OS_UNIX
