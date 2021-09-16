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

enum Mtf_Action_Type {
    MTF_INSERT_MARK,
    MTF_DELETE_MARK,
    MTF_APPLY_EDIT,
};

struct Mtf_Action {
    Mtf_Action_Type type;

    union {
        cur2 insert_mark_pos;
        int delete_mark_index;

        struct {
            cur2 edit_start;
            cur2 edit_old_end;
            cur2 edit_new_end;
        };
    };
};

const cur2 NOBOUND = {-1, -1};

struct Mark_Tree_Fuzzer {
    Fridge<Mark> mark_fridge;
    List<Mark*> marks;
    List<Mtf_Action> actions;
    Mark_Tree tree;
    bool print_flag;

    void init() {
        ptr0(this);

        marks.init();
        actions.init();
        tree.init(NULL);
        mark_fridge.init(512);
    }

    void cleanup() {
        mark_fridge.cleanup();
    }

    // assume 100x100 grid
    cur2 random_pos(cur2 lo, cur2 hi) {
        if (lo == NOBOUND) lo = new_cur2(0, 0);
        if (hi == NOBOUND) hi = new_cur2(100, 100);

        int rangesize = (hi.y - lo.y - 1) * 100 + (100 - lo.x) + hi.x;
        int r = rand() % rangesize;

        lo.x += r;
        if (lo.x > 100) {
            lo.x -= 100;
            lo.y += (lo.x / 100) + 1;
            lo.x = lo.x % 100;
        }
        return lo;
    }

    void print_action(Mtf_Action *a) {
        switch (a->type) {
        case MTF_INSERT_MARK:
            print("insert mark: %s", format_cur(a->insert_mark_pos));
            break;
        case MTF_DELETE_MARK:
            print(
                "delete mark: index %d, pos = %s",
                a->delete_mark_index,
                format_cur(marks[a->delete_mark_index]->pos())
            );
            break;
        case MTF_APPLY_EDIT:
            print(
                "edit: start = %s, oldend = %s, newend = %s",
                format_cur(a->edit_start),
                format_cur(a->edit_old_end),
                format_cur(a->edit_new_end)
            );
            break;
        }
    }

    void execute_action(Mtf_Action *a) {
        if (print_flag) print_action(a);

        switch (a->type) {
        case MTF_INSERT_MARK:
            {
                auto mark = mark_fridge.alloc();
                tree.insert_mark(MARK_TEST, a->insert_mark_pos, mark);
                marks.append(mark);
            }
            break;
        case MTF_DELETE_MARK:
            marks[a->delete_mark_index]->cleanup();
            mark_fridge.free(marks[a->delete_mark_index]);
            marks.remove(a->delete_mark_index);
            break;
        case MTF_APPLY_EDIT:
            tree.apply_edit(a->edit_start, a->edit_old_end, a->edit_new_end);
            break;
        }
    }

    void generate_random_action(Mtf_Action *a) {
        auto r = rand() % 100;
        if (r < 33) {
            // insert
            a->type = MTF_INSERT_MARK;
            a->insert_mark_pos = random_pos(NOBOUND, NOBOUND);
        } else if (r < 66) {
            // delete
            if (marks.len > 0) {
                a->type = MTF_DELETE_MARK;
                a->delete_mark_index = rand() % marks.len;
            } else {
                generate_random_action(a);
            }
            return;
        } else {
            // apply edit
            a->type = MTF_APPLY_EDIT;
            a->edit_start = random_pos(NOBOUND, NOBOUND);

            r = rand() % 100;
            if (r < 33) {
                // pure delete
                a->edit_new_end = a->edit_start;
                a->edit_old_end = random_pos(a->edit_start, NOBOUND);
            } else if (r < 66) {
                // pure insert
                a->edit_old_end = a->edit_start;
                a->edit_new_end = random_pos(a->edit_start, NOBOUND);
            } else {
                a->edit_old_end = random_pos(a->edit_start, NOBOUND);
                a->edit_new_end = random_pos(a->edit_start, NOBOUND);
            }
        }
    }

    bool run() {
        File f;
        if (f.init("mark_tree_fuzzer_output", FILE_MODE_WRITE, FILE_CREATE_NEW) != FILE_RESULT_SUCCESS)
            return false;
        defer { f.cleanup(); };

        int offset = 0;
        int count = 0;

        our_assert(f.write((char*)&count, sizeof(count)), "f.write");
        offset += sizeof(count);

        for (int i = 0; i < 100000; i++) {
            if (i % 1000 == 0)
                print("running action #%d", i);

            auto a = actions.append();
            generate_random_action(a);
            count++;

            // write the action
            our_assert(f.write((char*)a, sizeof(*a)), "f.write");
            offset += sizeof(*a);

            // write the count at the beginning
            f.seek(0);
            our_assert(f.write((char*)&count, sizeof(count)), "f.write");
            f.seek(offset);

            // run
            execute_action(a);
        }

        return true;
    }

    bool replay() {
        // read actions
        {
            File f;
            if (f.init("mark_tree_fuzzer_output", FILE_MODE_READ, FILE_OPEN_EXISTING) != FILE_RESULT_SUCCESS)
                return false;
            defer { f.cleanup(); };

            int len = 0;
            our_assert(f.read((char*)&len, sizeof(len)), "f.read");

            actions.ensure_cap(len);
            our_assert(f.read((char*)actions.items, sizeof(Mtf_Action) * len), "f.read");
            actions.len = len;
        }

        for (int i = 0; i < actions.len-2; i++)
            execute_action(&actions[i]);

        // execute the last 2 action
        execute_action(&actions[actions.len-2]);
        execute_action(&actions[actions.len-1]);

        return true;
    }
};

void test_mark_tree_fuzz() {
    Pool pool;
    pool.init();
    defer { pool.cleanup(); };
    SCOPED_MEM(&pool);

    Mark_Tree_Fuzzer mtf;
    mtf.init();
    defer { mtf.cleanup(); };

    mtf.print_flag = false;
    our_assert(mtf.run(), "mtf.run");
}

void test_mark_tree_fuzz_replay() {
    Pool pool;
    pool.init();
    defer { pool.cleanup(); };
    SCOPED_MEM(&pool);

    Mark_Tree_Fuzzer mtf;
    mtf.init();
    defer { mtf.cleanup(); };

    mtf.print_flag = false;
    our_assert(mtf.replay(), "mtf.replay");
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
    if (match("mtf")) test_mark_tree_fuzz();
    if (match("mtf_replay")) test_mark_tree_fuzz_replay();

    return 0;
}

