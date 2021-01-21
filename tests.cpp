#include "tests.hpp"

#include "debugger.hpp"
#include "common.hpp"
#include "world.hpp"
#include "nvim.hpp"

bool run_tests() {
    // return false;

    /*
    auto ast = parse_file_into_ast("testfile.go");
    For (ast->source.decls->list) {
        if (it->type == AST_FUNC_DECL) {
            auto stmt = it->func_decl.body->block.stmts->list[0];
            print("...");
        }
    }

    return true;
    return false;
    */

    Go_Index index;
    index.delete_index();
    index.main_loop();
    return false;

    system("pause");
    return true;
}
