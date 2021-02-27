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
            pr("%s -> %s", it.import_path, it.resolved_path);
            SCOPED_PUSH_DEPTH();

            pr("status: %d", it.status);

            {
                pr("individual imports: %d", it.individual_imports->len);
                SCOPED_PUSH_DEPTH();
                For (*it.individual_imports)
                    pr("[%s] %s \"%s\"", it.file, it.package_name, it.import_path);
            }

            {
                pr("dependencies: %d", it.dependencies->len);
                SCOPED_PUSH_DEPTH();
                For (*it.dependencies) {
                    pr("%s \"%s\" -> \"%s\"", it.package_name, it.import_path, it.resolved_path);
                }
            }

            pr("package_name: %s", it.package_name);
            pr("is_hash_ready: %d", it.is_hash_ready);
        }
    }

#undef SCOPED_PUSH_DEPTH
};

