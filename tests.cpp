#include "tests.hpp"
#include "buffer.hpp"
#include "common.hpp"
#include "dbg.hpp"
#include "editor.hpp"
#include "go.hpp"
#include "mem.hpp"
#include "os.hpp"
#include "world.hpp"
#include "diff.hpp"
#include "defer.hpp"
#include "mtwist_shim.hpp"

void test_diff() {
    auto run = [&](ccstr a, ccstr b) {
        print("diff \"%s\" \"%s\"", a, b);

        auto al = new_list(uchar);
        auto bl = new_list(uchar);
        for (auto p = a; *p != '\0'; p++) al->append(*p);
        for (auto p = b; *p != '\0'; p++) bl->append(*p);

        auto diffs = diff_main(new_dstr(al), new_dstr(bl));
        For (diffs) {
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
    buf.init(&world.frame_mem, false, false);
    buf.read("loldongs", -1);

    Mark_Tree mt; mt.init(&buf);

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
    List<Mark*> marks;
    List<Mtf_Action> actions;
    Mark_Tree tree;
    bool print_flag;

    void init() {
        ptr0(this);

        marks.init();
        actions.init();
        tree.init(NULL);
    }

    void cleanup() {
        tree.cleanup();
        For (&marks) world.mark_fridge.free(it);
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
            print("insert mark: %s", a->insert_mark_pos.str());
            break;
        case MTF_DELETE_MARK:
            print(
                "delete mark: index %d, pos = %s",
                a->delete_mark_index,
                marks[a->delete_mark_index]->pos().str()
            );
            break;
        case MTF_APPLY_EDIT:
            print(
                "edit: start = %s, oldend = %s, newend = %s",
                a->edit_start.str(),
                a->edit_old_end.str(),
                a->edit_new_end.str()
            );
            break;
        }
    }

    void execute_action(Mtf_Action *a) {
        if (print_flag) print_action(a);

        switch (a->type) {
        case MTF_INSERT_MARK: {
            auto mark = world.mark_fridge.alloc();
            tree.insert_mark(MARK_TEST, a->insert_mark_pos, mark);
            marks.append(mark);
            break;
        }
        case MTF_DELETE_MARK:
            marks[a->delete_mark_index]->cleanup();
            world.mark_fridge.free(marks[a->delete_mark_index]);
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
            if (marks.len) {
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
        if (f.init_write("mark_tree_fuzzer_output") != FILE_RESULT_OK)
            return false;
        defer { f.cleanup(); };

        int offset = 0;
        int count = 0;

        cp_assert(f.write((char*)&count, sizeof(count)));
        offset += sizeof(count);

        for (int i = 0; i < 10000; i++) {
            /*
            if (i % 2000 == 0)
                print("running action #%d", i);
                */

            auto a = actions.append();
            generate_random_action(a);
            count++;

            // write the action
            cp_assert(f.write((char*)a, sizeof(*a)));
            offset += sizeof(*a);

            // write the count at the beginning
            f.seek(0);
            cp_assert(f.write((char*)&count, sizeof(count)));
            f.seek(offset);

            // run
            execute_action(a);

            /*
            // ensure marks is still good
            For (&marks) {
                // check that mark node contains the mark
                bool found = false;
                for (auto m = it->node->marks; m; m = m->next) {
                    if (m == it) {
                        found = true;
                        break;
                    }
                }
                if (!found) cp_panic("mark got detached from its node somehow");

                // check that mark node is still in root
                auto node = it->node;
                while (node->parent)
                    node = node->parent;
                if (tree.root != node)
                    cp_panic("mark node is detached from root!");
            }
            */

        }

        return true;
    }

    bool replay() {
        // read actions
        {
            File f;
            if (f.init_read("mark_tree_fuzzer_output") != FILE_RESULT_OK)
                return false;
            defer { f.cleanup(); };

            int len = 0;
            cp_assert(f.read((char*)&len, sizeof(len)));

            actions.ensure_cap(len);
            cp_assert(f.read((char*)actions.items, sizeof(Mtf_Action) * len));
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
    for (int i = 0; i < 10000; i++) {
        SCOPED_FRAME();

        print("trial %d", i);

        Mark_Tree_Fuzzer mtf;
        mtf.init();
        defer { mtf.cleanup(); };

        mtf.print_flag = false;
        cp_assert(mtf.run());
    }
}

void test_mark_tree_fuzz_replay() {
    Mark_Tree_Fuzzer mtf;
    mtf.init();
    defer { mtf.cleanup(); };

    mtf.print_flag = false;
    cp_assert(mtf.replay());
}

void test_bytecounts() {
    mt_seed32(0);

    Bytecounts_Tree bc; bc.init();
    List<int> nums; nums.init();

    // prefill nums and root with 100 values, make sure we start out in sync
    for (int i = 0; i < 100; i++) {
        int num = (mt_lrand() % 100) + 1;
        nums.append(num);
        bc.append(num);
    }
    cp_assert(bc.size() == nums.len);
    Fori (&nums)
        cp_assert(bc.get(i) == it);

    // 10k iterations of random actions: insert, delete, sum
    for (int i = 0; i < 10000; i++) {
        int action = mt_lrand() % 4;

        if (action == 0) { // insert item
            int pos = mt_lrand() % (nums.len + 1);
            int num = (mt_lrand() % 100) + 1;
            print("inserting %d at %d", num, pos);

            if (pos == nums.len)
                nums.append(num);
            else
                nums.insert(pos, num);

            bc.insert(pos, num);
            cp_assert(bc.get(pos) == num);
            cp_assert(bc.size() == nums.len);
        } else if (action == 1) { // delete item
            if (!nums.len) continue;

            int pos = mt_lrand() % nums.len;
            print("deleting at %d", pos);

            bc.remove(pos);
            nums.remove(pos);
            cp_assert(bc.size() == nums.len);
        } else if (action == 2) { // sum range
            if (!nums.len) continue;

            int hi = mt_lrand() % nums.len;

            int want = 0;
            for (int k = 0; k < hi; k++)
                want += nums[k];

            print("getting sum until %d, expecting %d", hi, want);
            cp_assert(bc.sum(hi) == want);
        } else if (action == 3) { // offset to line
            int hi = 1 + (mt_lrand() % (nums.len-1));
            int limit = 0;
            for (int i = 0; i < hi; i++)
                limit += nums[i];
            print("checking offset_to_line up until line %d", hi);

            int rem = 0;

            if (hi == 70 && limit == 3509)
                BREAK_HERE();

            cp_assert(bc.offset_to_line(limit, &rem) == hi);
            cp_assert(rem == 0);

            cp_assert(bc.offset_to_line(limit + nums[hi]-1, &rem) == hi);
            cp_assert(rem == nums[hi]-1);

            cp_assert(bc.offset_to_line(limit - 1, &rem) == hi-1);
            cp_assert(rem == nums[hi-1]-1);
        }
    }
}

void run_tests(ccstr test_name) {
    if (streq(test_name, "diff")) test_diff();
    if (streq(test_name, "mark_tree")) test_mark_tree();
    if (streq(test_name, "mtf")) test_mark_tree_fuzz();
    if (streq(test_name, "mtf_replay")) test_mark_tree_fuzz_replay();
    if (streq(test_name, "bytecounts")) test_bytecounts();
}
