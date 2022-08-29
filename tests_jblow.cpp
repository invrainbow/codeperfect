// jblow style tests

#include "world.hpp"
#include "win.hpp"
#include "ostype.hpp"
#include "defer.hpp"

void inject_event(Window_Event_Type type, fn<void(Window_Event*)> cb) {
    auto &ev = world.testing.event;

    ptr0(&ev);
    ev.type = type;
    cb(&ev);

    world.testing.processed_event = false;
    world.testing.inject_event = true;

    while (!world.testing.processed_event)
        sleep_milliseconds(10);
}

void inject_key(int key, int mods, bool press) {
    inject_event(WINEV_KEY, [&](auto ev) {
        ev->key.key = key;
        ev->key.mods = mods;
        ev->key.press = press;
    });
}

void skip_frame() {
    auto start = world.frame_index;
    while (world.frame_index == start)
        sleep_milliseconds(10);
}

void press_key(int key, int mods = 0) {
    inject_key(key, mods, true);
    inject_key(key, mods, false);
    skip_frame();
}

void raw_type_char(u32 ch) {
    inject_event(WINEV_CHAR, [&](auto ev) {
        ev->character.ch = ch;
    });
}

void type_char(u32 ch) {
    raw_type_char(ch);
    skip_frame();
}

void type_string(ccstr s) {
    for (auto p = s; *p; p++)
        raw_type_char((u32)(*p));
    skip_frame();
}

bool is_editor_selected(ccstr relative_filepath) {
    auto ed = get_current_editor();
    if (ed) {
        auto relpath = get_path_relative_to(ed->filepath, world.current_path);
        return streq(relpath, relative_filepath);
    }
    return false;
}

struct Timeout {
    int n;
    u64 start;

    Timeout(int _n) { init(_n); }
    Timeout() { init(1000); }

    void init(int _n) {
        n = _n;
        start = current_time_milli();
    }

    void inc() {
        if (current_time_milli() - start > n)
            cp_panic("timeout");
        sleep_milliseconds(10);
    }
};

#define WAIT_FOR(x) for (Timeout t; !(x);) t.inc()

void wait_for_editor(ccstr relative_filepath) {
    WAIT_FOR(is_editor_selected(relative_filepath));
}

void run_tests() {
    Pool pool;
    pool.init();
    defer { pool.cleanup(); };
    SCOPED_MEM(&pool);

    while (!world.testing.ready) sleep_milliseconds(10);

    press_key(CP_KEY_P, CP_MOD_PRIMARY);
    type_string("helpermaingo");
    press_key(CP_KEY_ENTER);

    wait_for_editor("helper/main.go");

    // all good!
    world.message_queue.add([&](auto msg) {
        msg->type = MTM_EXIT;
        msg->exit_code = 0;
    });
}
