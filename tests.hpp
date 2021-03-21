#pragma once

#include "debugger.hpp"
#include "common.hpp"
#include "world.hpp"
#include "nvim.hpp"

#include <windows.h>

void clear_screen();
bool run_tests();
void compiler_dont_optimize_me_away();

struct Index_Printer {
    int depth;

    void init() {
        ptr0(this);
    }

    void pr(ccstr fmt, ...) {
        for (u32 i = 0; i < depth * 2; i++) putchar(' ');
        if (depth > 0) printf("- ");

        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);

        printf("\n");
    }

#define SCOPED_PUSH_DEPTH() depth++; defer { depth--; }

    void print_index(Go_Index *index) {
        pr("\n\n-------\n\npackages:");
        SCOPED_PUSH_DEPTH();

        For (*index->packages) {
            pr("%s", it.import_path);
            SCOPED_PUSH_DEPTH();
        }
    }

#undef SCOPED_PUSH_DEPTH
};

