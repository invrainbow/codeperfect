#include "ostype.hpp"

#if OS_UNIX

#include "world.hpp"
#include "entrypoint.hpp"

int main(int argc, char **argv) {
    return realmain(argc, argv);
}

#endif // OS_UNIX
