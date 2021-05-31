#include "tests.hpp"
#include "mem.hpp"
#include "editor.hpp"
#include "buffer.hpp"

const char PATH[] = "c:\\users\\brandon\\beego\\task\\task.go";

bool run_tests() {
    return false;

    // world.init();
    // return true;
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
