#include "tests.hpp"
#include "mem.hpp"
#include "editor.hpp"
#include "buffer.hpp"

const char PATH[] = "c:\\users\\brandon\\beego\\task\\task.go";

bool run_tests() {
    return false;
    /*
    Cstr_To_Ustr conv;
    conv.init();
    bool found;
    conv.feed(0xef, &found);
    assert(!found);
    conv.feed(0xbc, &found);
    assert(!found);
    auto ret = conv.feed(0x9a, &found);
    assert(found);
    printf("0x%x", ret);
    return true;
    */

    world.init();

    Pool mem;
    mem.init();
    SCOPED_MEM(&mem);

    Buffer buf;
    buf.init(&mem);

    auto f = fopen(PATH, "r");
    buf.read(f);
    fclose(f);

    char tsinput_buffer[128];

    auto readmore = [&](uint32_t off, uint32_t *read) -> const char* {
        auto it = buf.iter(buf.offset_to_cur(off));
        u32 n = 0;

        while (!it.eof()) {
            auto uch = it.next();
            if (uch == 0) break;

            auto size = uchar_size(uch);
            if (n + size + 1 > _countof(tsinput_buffer)) break;

            uchar_to_cstr(uch, &tsinput_buffer[n], &size);
            n += size;
        }

        *read = n;
        tsinput_buffer[n] = '\0';
        return tsinput_buffer;
    };

    List<unsigned char> arr;
    arr.init();

    uint32_t read;
    uint32_t off = 0;

    while (true) {
        auto data = readmore(off, &read);
        if (read == 0) break;
        for (u32 i = 0; i < read; i++)
            arr.append(data[i]);
        off += read;
    }

    auto ef = read_entire_file(PATH);

    print("ef->len = %d, arr.len = %d", ef->len, arr.len);

    u32 i = 0, j = 0;
    while (i < ef->len && j < arr.len) {
        while (ef->data[i] == '\r' && i < ef->len)
            i++;

        if (ef->data[i] != arr[j]) {
            print("ef pos = %d, arr pos = %d", i, j);
            print("ef = 0x%02x 0x%02x [0x%02x] 0x%02x 0x%02x 0x%02x 0x%02x", ef->data[i-2], ef->data[i-1], ef->data[i], ef->data[i+1], ef->data[i+2], ef->data[i+3], ef->data[i+4]);
            print("arr = 0x%02x 0x%02x [0x%02x] 0x%02x 0x%02x 0x%02x 0x%02x", arr[j-2], arr[j-1], arr[j], arr[j+1], arr[j+2], arr[j+3], arr[j+4]);
            panic("shit done fucked up");
        }

        i++;
        j++;
    }

    print("i = %d, ef->len = %d", i, ef->len);
    print("j = %d, arr.len = %d", i, arr.len);

    return true;

#if 0
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
#endif
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
