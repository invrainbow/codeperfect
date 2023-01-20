// jblow style tests

#include "world.hpp"
#include "win.hpp"
#include "ostype.hpp"
#include "defer.hpp"

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

void Jblow_Tests::skip_frame() {
    auto start = world.frame_index;
    while (world.frame_index < start + 2)
        sleep_milliseconds(10);

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
    Timeout() { init(100000000); }

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

void Jblow_Tests::run_normal() {
    press_key(CP_KEY_P, CP_MOD_PRIMARY);
    skip_frame();

    type_string("helpermaingo");
    skip_frame();

    press_key(CP_KEY_ENTER);
    skip_frame();

    wait_for_editor("helper/main.go");
}

void Jblow_Tests::run_workspace() {}

void Jblow_Tests::init(ccstr _test_name) {
    ptr0(this);

    on = true;
    ready = false;
    cp_strcpy_fixed(test_name, _test_name);

    h = create_thread([](void* param) {
        ((Jblow_Tests*)param)->run();
    }, this);

    if (!h) cp_panic("unable to create test thread");
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

    while (!world.jblow_tests.ready) sleep_milliseconds(10);

    auto &t = world.jblow_tests;
    if (streq(t.test_name, "workspace")) t.run_workspace();
    if (streq(t.test_name, "normal")) t.run_normal();

    // all good!
    world.message_queue.add([&](auto msg) {
        msg->type = MTM_EXIT;
        msg->exit_code = 0;
    });
}
