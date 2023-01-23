// jblow style tests

#include "world.hpp"
#include "win.hpp"
#include "ostype.hpp"
#include "defer.hpp"

extern "C" {
#ifdef __cplusplus
#   undef __cplusplus
#   include "mtwist.h"
#   define __cplusplus
#else
#   include "mtwist.h"
#endif
}

void Jblow_Tests::inject(Window_Event_Type type, fn<void(Window_Event*)> cb) {
    Jblow_Test_Event te; ptr0(&te);
    te.handled = false;
    te.event.type = type;
    {
        SCOPED_MEM(&mem);
        cb(&te.event);
        {
            SCOPED_LOCK(&lock);
            events.append(&te);
        }
    }
}

void Jblow_Tests::catchup() {
    auto check = [&]() {
        SCOPED_LOCK(&lock);
        For (&events)
            if (!it.handled)
                return false;

        // all events are processed, while we still have the lock, clear the queue
        mem.reset();
        events.len = 0;
        return true;
    };

    while (!check()) sleep_milliseconds(10);
}

void Jblow_Tests::skip_frame() {
    auto start = world.frame_index;
    while (world.frame_index < start + 5)
        sleep_milliseconds(10);
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
    int line;
    u64 start;

    Timeout(int _line) {
        line = _line;
        start = current_time_milli();
    }

    void operator+(fn<bool()> f) {
        while (current_time_milli() - start <= 3000) {
            if (f()) return;
            sleep_milliseconds(10);
        }
        cp_panic(cp_sprintf("timeout at line %d", line));
    }
};

#define WAIT_FOR Timeout(__LINE__) + [&]()

Editor* wait_for_editor(ccstr relative_filepath) {
    Editor* ret = NULL;
    WAIT_FOR {
        auto ed = get_current_editor();
        if (!ed) return false;
        if (world.use_nvim && !ed->is_nvim_ready()) return false;

        auto relpath = get_path_relative_to(ed->filepath, world.current_path);
        if (!streq(relpath, relative_filepath)) return false;

        ret = ed;
        return true;
    };
    return ret;
}

Editor* Jblow_Tests::open_editor(ccstr relative_filepath) {
    // open file fuzzy search
    press_key(CP_KEY_P, CP_MOD_PRIMARY);
    skip_frame();

    // open main.go
    type_string(relative_filepath);
    press_key(CP_KEY_ENTER);

    // wait for editor
    return wait_for_editor(relative_filepath);
}

void Jblow_Tests::run_normal() {
    // wait for editor
    auto editor = open_editor("main.go");
    print("%s", editor->filepath);

    // spam a bunch of keys lol
    ccstr chars = (
        "KKKKKKKKKKKKKK"                // does nothing in normal mode
        "aioaioaioaio"                  // enter insert mode
        "\x01\x01\x01\x01\x01\x01\x01"  // escape
    );
    int chars_len = strlen(chars);

    int seed = 0;
    {
        auto seed_str = getenv("JBLOW_TESTS_NORMAL_SEED");
        if (seed_str) seed = atoi(seed_str);
    }
    mt_seed32(seed);

    auto input = alloc_list<char>();

    for (int i = 0; i < 1000; i++) {
        auto it = chars[mt_lrand() % chars_len];
        if (it == 0x01)
            press_key(CP_KEY_ESCAPE);
        else
            type_char(it);

        if (i % 100 == 0) catchup();
    }

    sleep_milliseconds(999999999);
}

void Jblow_Tests::run_workspace() {}

void Jblow_Tests::init(ccstr _test_name) {
    ptr0(this);

    on = true;
    ready = false;
    cp_strcpy_fixed(test_name, _test_name);

    // Initialize options with some defaults.
    {
        Options opts; // reinitialize with defaults
        memcpy(&options, &opts, sizeof(Options));

        // set overrides for all tests
        options.send_crash_reports = false;

        // per-test overrides
        init_options();
    }

    h = create_thread([](void* param) {
        ((Jblow_Tests*)param)->run();
    }, this);

    if (!h) cp_panic("unable to create test thread");
}

void Jblow_Tests::init_options() {
    // TODO

    if (streq(test_name, "normal")) {
        options.enable_vim_mode = true;
    }
}

void Jblow_Tests::run() {
    mem.init();
    thread_mem.init();

    {
        SCOPED_MEM(&mem);
        events.init();
    }

    lock.init();

    Pool pool;
    pool.init();
    defer { pool.cleanup(); };
    SCOPED_MEM(&pool);

    while (!ready) sleep_milliseconds(10);

    if (streq(test_name, "workspace")) run_workspace();
    if (streq(test_name, "normal")) run_normal();

    // all good!
    world.message_queue.add([&](auto msg) {
        msg->type = MTM_EXIT;
        msg->exit_code = 0;
    });
}
