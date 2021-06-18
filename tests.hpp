#pragma once

#include "debugger.hpp"
#include "common.hpp"
#include "world.hpp"
#include "nvim.hpp"
#include "os.hpp"

#if OS_WIN
#include <windows.h>
#endif

bool run_tests();
void compiler_dont_optimize_me_away();
