#include "tests.hpp"
#include "mem.hpp"

bool run_tests() {
    return false;

    world.init();
    compiler_dont_optimize_me_away();

    use_pool_for_tree_sitter = true;

    Go_Indexer indexer;
    Index_Stream s;

    indexer.init();
    indexer.package_lookup.init("c:\\users\\brandon\\compose-cli");
    indexer.start_background_thread();

    while (true) continue;

    /*
    indexer.crawl_index();
    // indexer.background_thread();

    // write
    if (s.open("db", FILE_MODE_WRITE, FILE_CREATE_NEW) != FILE_RESULT_SUCCESS) return false;
    write_object<Go_Index>(&indexer.index, &s);
    s.cleanup();
    print("done writing");

    // read back in
    s.open("db", FILE_MODE_READ, FILE_OPEN_EXISTING);
    defer { s.cleanup(); };
    Pool mem;
    mem.init();
    {
        SCOPED_MEM(&mem);
        auto index = read_object<Go_Index>(&s);
        print("done reading, mem used is %d", mem.mem_allocated);
    }
    */

    system("pause");
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
