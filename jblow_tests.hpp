#pragma once

struct Jblow_Test_Event {
    bool handled;
    Window_Event event;
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
    void init_options();

    void inject(Window_Event_Type type, fn<void(Window_Event*)> cb);
    void catchup();

    // We need this because some events are not truly processed until the next frame.
    // E.g. if we press cmd+p the imgui window won't be open to process our fuzzy search query until next frame.
    // Skips a whole frame, i.e. if we're on frame 3, it will skip frame 4 entirely and wait until we're on frame 5.
    void skip_frame();

    void raw_type_char(u32 ch) {
        inject(WINEV_CHAR, [&](auto ev) {
            ev->character.ch = ch;
        });
    }


    void type_char(u32 ch) {
        raw_type_char(ch);
    }

    void type_string(ccstr s) {
        for (auto p = s; *p; p++)
            raw_type_char((u32)(*p));
    }

    void inject_key(int key, int mods, bool press) {
        inject(WINEV_KEY, [&](auto ev) {
            ev->key.key = key;
            ev->key.mods = mods;
            ev->key.press = press;
        });
    }

    void press_key(int key, int mods = 0) {
        inject_key(key, mods, true);
        inject_key(key, mods, false);
    }

    void run();
    void run_vimfuzzer();
    void run_workspace();
    Editor* open_editor(ccstr relative_filepath);
};

