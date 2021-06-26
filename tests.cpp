#include "tests.hpp"
#include "mem.hpp"
#include "editor.hpp"
#include "buffer.hpp"
#include "go.hpp"

bool run_tests() {
    return false;

    world.init();
    // test_read_write_index();

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
        if (!s.open("W:\\test_db_copy", true)) return false;
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

void compiler_dont_optimize_me_away() {
    print("%d", world.frame_mem.owns_address((void*)0x49fa98));
    print("%d", world.frame_mem.recount_total_allocated());
}
