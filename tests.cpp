#include "buffer.hpp"
#include "common.hpp"
#include "debugger.hpp"
#include "editor.hpp"
#include "go.hpp"
#include "mem.hpp"
#include "nvim.hpp"
#include "os.hpp"
#include "world.hpp"
#include "diff.hpp"

void test_diff() {
    auto run = [&](ccstr a, ccstr b) {
        print("diff \"%s\" \"%s\"", a, b);

        auto al = alloc_list<uchar>();
        auto bl = alloc_list<uchar>();
        for (auto p = a; *p != '\0'; p++) al->append(*p);
        for (auto p = b; *p != '\0'; p++) bl->append(*p);

        auto diffs = diff_main(new_dstr(al), new_dstr(bl));
        For (*diffs) {
            switch (it.type) {
            case DIFF_INSERT: print(" - [insert] %s", it.s.str()); break;
            case DIFF_DELETE: print(" - [delete] %s", it.s.str()); break;
            case DIFF_SAME: print(" - [same] %s", it.s.str()); break;
            }
        }
    };

    run("loldongs", "dongs");
    run("cocks", "roflcockter");
    run("good dog", "bad dog");
}

void test_mark_tree() {
    Buffer buf;
    buf.init(&world.frame_mem, false);
    buf.read_data("loldongs", -1);

    Mark_Tree mt;  mt.init(&buf);

    /*
    auto m1 = mt.insert_mark(MARK_BUILD_ERROR, new_cur2(2, 4));
    auto m2 = mt.insert_mark(MARK_BUILD_ERROR, new_cur2(8, 4));
    auto m3 = mt.insert_mark(MARK_BUILD_ERROR, new_cur2(12, 4));

    cur2 start = new_cur2(0, 1);
    cur2 old_end = new_cur2(0, 1);
    cur2 new_end = new_cur2(0, 2);

    mt.apply_edit(start, old_end, new_end);

	m1->cleanup();
	m2->cleanup();
	m3->cleanup();
    */

	print("break here");
}

int main(int argc, char *argv[]) {
    init_platform_specific_crap();
    world.init(NULL);

    auto match = [&](ccstr s) -> bool {
        if (argc <= 1) return true;
        return streq(argv[1], s);
    };

    if (match("diff")) test_diff();
    if (match("mark_tree")) test_mark_tree();

    return 0;
}

