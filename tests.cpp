#include "tests.hpp"
#include "mem.hpp"
#include "editor.hpp"
#include "buffer.hpp"
#include "go.hpp"

#if 0
void test_search() {
    // shit to test:
    //   - [x] normal search
    //   - [x] regex search
    //   - [x] normal search and replace
    //   - [x] regex search and replace
    //   - [x] regex search with group replace
    //   - [ ] filter by files
    //   - [ ] undo

    SCOPED_MEM(&world.search_mem);

    Search_Opts opts;
    opts.case_sensitive = true;
    opts.literal = false;

    Searcher s;
    s.init();
    s.start_search("git(.*?)fork", "|$0|", &opts);

    while (s.state == SEARCH_SEARCH_IN_PROGRESS)
        continue;

    int total = 0;

    For (s.search_results) {
        print("=== %s ===", it.filepath);

        if (it.results == NULL) continue;

        For (*it.results) {
            print(
                "\t%s to %s (len = %d): %s",
                format_cur(it.match_start),
                format_cur(it.match_end),
                it.match_len,
                it.match
            );

            auto groups = it.groups;
            for (int i = 0; i < groups->len; i++) {
                auto it = groups->at(i);
                print("\t\tmatch #%d: %s", i + 1, it);
            }
        }

        total += it.results->len;
    }

    print("found %d total", total);

    s.start_replace();
}
#endif

bool run_tests() {
    return false;

    world.init();

    // test_search();
    // test_read_write_index();

    system(OS_WIN ? "pause" : "read -p \"Press enter to exit: \"");
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
