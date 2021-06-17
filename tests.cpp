#include "tests.hpp"
#include "mem.hpp"
#include "editor.hpp"
#include "buffer.hpp"
#include "go.hpp"

bool test_indexing_speed() {
    auto &gi = world.indexer;

    gi.module_resolver.init(world.current_path, gi.gomodcache);

    gi.index.current_path = our_strcpy(world.current_path);
    gi.index.current_import_path = our_strcpy(gi.get_workspace_import_path());
    gi.index.packages = alloc_list<Go_Package>();
    gi.package_lookup.init();

    use_pool_for_tree_sitter = true;

    ccstr import_path = "github.com/gogo/protobuf/proto";
    auto resolved_path = gi.get_package_path(import_path);

    Go_Package *pkg = NULL;

    {
        SCOPED_MEM(&gi.final_mem);
        pkg = gi.index.packages->append();
        pkg->files = alloc_list<Go_File>();
        pkg->import_path = our_strcpy(import_path);
        gi.package_lookup.set(pkg->import_path, pkg);
    }

    Timer t;
    t.init();

    {
        auto filename = "table_marshal.go";

        SCOPED_FRAME();

        auto filepath = path_join(resolved_path, filename);

        auto pf = gi.parse_file(filepath);
        if (pf == NULL) return false;
        defer { gi.free_parsed_file(pf); };

        t.log("parse file");

        auto file = get_ready_file_in_package(pkg, filename);

        t.log("create file");

        ccstr package_name = NULL;
        gi.process_tree_into_gofile(file, pf->root, filepath, &package_name, true);

        t.log("process tree");
    }

    return true;
}

bool run_tests() {
    return false;

    world.init();
    // test_read_write_index();
    test_indexing_speed();

    system("pause");
    return true;
}

bool test_read_write_index() {
    Timer t;
    t.init();

    Go_Index *index = NULL;

    {
        Index_Stream s;
        if (!s.open("W:\\test_db_from_hugo")) return false;
        defer { s.cleanup(); };
        index = s.read_index();
        if (!s.ok) return false;
    }

    t.log("read from index");

    {
        Index_Stream s;
        if (!s.open("W:\\test_db_copy", true, FILE_CREATE_NEW)) return false;
        defer { s.cleanup(); };
        s.write_index(index);
        s.finish_writing();
    }

    t.log("write out to index");
    t.total();

    {
        Index_Stream s;
        if (!s.open("W:\\test_db_copy")) return false;
        defer { s.cleanup(); };
        index = s.read_index();
        if (!s.ok) return false;

        print("break here and inspect index");
    }

    t.log("read index back in");
    return true;
}

void clear_screen() {
    HANDLE h;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD count;
    DWORD cells;
    COORD coords = {0, 0};

    h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h == INVALID_HANDLE_VALUE) return;

    if (!GetConsoleScreenBufferInfo(h, &csbi)) return;
    cells = csbi.dwSize.X * csbi.dwSize.Y;

    if (!FillConsoleOutputCharacterA(h, ' ', cells, coords, &count)) return;
    if (!FillConsoleOutputAttribute(h, csbi.wAttributes, cells, coords, &count)) return;

    SetConsoleCursorPosition(h, coords);
}

void compiler_dont_optimize_me_away() {
    print("%d", world.frame_mem.owns_address((void*)0x49fa98));
    print("%d", world.frame_mem.recount_total_allocated());
}
