#pragma once

struct Jblow_Test_Event {
    bool handled;
    Window_Event event;
};

struct Jblow_Input {
    bool is_key;
    union {
        uchar ch;
        struct {
            int key;
            int mods;
        };
    };
};

struct Random_Input_Gen {
    struct Weighted_Item {
        Jblow_Input input;
        int weight;
    };

    List<Weighted_Item> *items;
    int total;

    void init();
    void add(int key, int mods, int weight);
    void add(char ch, int weight);
    Jblow_Input pick();
};

struct Jblow_Tests {
    bool on;
    Lock lock;
    Pool mem;
    Pool thread_mem;
    bool ready;
    char test_name[128];
    bool headless;
    List<Jblow_Test_Event> events;
    Thread_Handle h;

    void init(ccstr _test_name);

    void inject(Window_Event_Type type, fn<void(Window_Event*)> cb);
    void catchup();

    // We need this because some events are not truly processed until the next frame.
    // E.g. if we press cmd+p the imgui window won't be open to process our fuzzy search query until next frame.
    // Skips a whole frame, i.e. if we're on frame 3, it will skip frame 4 entirely and wait until we're on frame 5.
    void skip_frame();

    void type_char(u32 ch) {
        inject(WINEV_CHAR, [&](auto ev) {
            ev->character.ch = ch;
        });
    }

    void type_string(ccstr s) {
        for (auto p = s; *p; p++)
            type_char((u32)(*p));
    }

    void inject_key(int key, int mods, bool press) {
        inject(WINEV_KEY, [&](auto ev) {
            ev->key.key = key;
            ev->key.mods = mods;
            ev->key.press = press;
        });
    }

    void inject_jblow_input(Jblow_Input input) {
        if (input.is_key)
            press_key(input.key, input.mods);
        else
            type_char(input.ch);
    }

    void press_key(int key, int mods = 0) {
        inject_key(key, mods, true);
        skip_frame();
        inject_key(key, mods, false);
    }

    void run();
    Editor* open_editor(ccstr relative_filepath);
    Editor* wait_for_editor(ccstr relative_filepath);
    void wait_for_indexer_ready();
    void move_cursor(cur2 cur);
};

List<ccstr> *list_go_files(ccstr subfolder);
