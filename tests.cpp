#include "tests.hpp"

#include "debugger.hpp"
#include "common.hpp"
#include "world.hpp"
#include "nvim.hpp"

bool run_tests() {
    // return false;

    Gomod_Parser parser;
    Go_Index index;
    index.main_loop();

    system("pause");
    return true;
}
