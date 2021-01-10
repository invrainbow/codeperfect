#include "tests.hpp"

#include "debugger.hpp"
#include "common.hpp"
#include "world.hpp"
#include "nvim.hpp"

bool test_debugger() {
    SCOPED_MEM(&world.debugger_mem);

    Debugger dbg;

    if (!dbg.init()) return false;
    defer { dbg.cleanup(); };

    dbg.set_breakpoint(get_normalized_path("gotest.go"), 24);
    dbg.exec_continue(true); // hit the breakpoint

    auto stackframe = dbg.get_stackframe();

    return true;
}

void test_process() {
    SCOPED_FRAME();
    auto unformatted = R"({"asdf": [{"a":"b"}]})";
    print("%s", our_format_json(unformatted));
}

void test_json() {
    Json_Navigator js;
    js.string = R"({"asdf": [{"a":"b"}]})";

    auto num_toks = parse_json_with_jsmn(js.string, NULL, 0);
    js.tokens = alloc_list<jsmntok_t>(num_toks);
    js.tokens->len = num_toks;

    parse_json_with_jsmn(js.string, js.tokens->items, num_toks);

    auto idx = js.get(0, ".asdf[0].a");
    print("idx = %d, string = \"%s\"", idx, js.str(idx));
}

void test_parser() {
    SCOPED_MEM(&world.parser_mem);

    // FILE *f = fopen("/usr/local/go/src/fmt/scan.go", "r");
    FILE* f = fopen("/Users/mac/dev/ide/lol.go", "r");
    if (f == NULL) {
        error("Unable to open file.");
        return;
    }

    auto it = alloc_object(Parser_It);
    it->init(f);

    auto p = alloc_object(Parser);
    p->init(it);

    auto ast = p->parse_file();
    auto func_body = ast->source.decls->list.items[3]->func_decl.body;
    auto sel = func_body->block.stmts->list.items[1]->expr_stmt.x->selector_expr.sel;
}

void test_arena() {
    Arena arena;
    arena.init(20);
    SCOPED_ARENA(&arena);
    
    {
        SCOPED_FRAME();
        arena.alloc(128);
        arena.alloc(128);
        arena.alloc(128);
        arena.alloc(128);
        arena.alloc(128);
    }

    print("%d", arena.get_total_allocated_from_os());
}

void test_index() {
#if 1
    Golang go;

    if (!go.delete_index()) {
        error("well we couldn't delete the index lmao");
        return;
    }

    go.build_index();
    go.read_index();
#else
    world.build_index_arena.cleanup();
    world.build_index_arena.init();
    SCOPED_ARENA(&world.build_index_arena);
    auto ast = parse_file_into_ast("./test.go");
#endif
}

bool run_tests() {
    // return test_arena(), true;
    return test_index(), true;
    // return test_debugger(), true;
    // return test_parameter_hint(), true;
    // return test_process(), true;

    return false;
}
