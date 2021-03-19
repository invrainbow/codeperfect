#include "tests.hpp"
#include "mem.hpp"

bool run_tests() {
    // return false;

    // initialize everything
    world.init();
    compiler_dont_optimize_me_away();
    use_pool_for_tree_sitter = true;

    // run indexer
    Go_Indexer indexer;
    indexer.init();
    indexer.background_thread();

    // indexer.start_background_thread();
    // while (true) continue;

    /*
    Index_Stream s;
    s.open("db", FILE_MODE_WRITE, FILE_CREATE_NEW) == FILE_RESULT_SUCCESS
    s.open("db", FILE_MODE_READ, FILE_OPEN_EXISTING) == FILE_RESULT_SUCCESS
    defer { s.cleanup(); };
    write_object<Go_Index>(&indexer.index, &s);
    auto index = read_object<Go_Index>(&s);
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
