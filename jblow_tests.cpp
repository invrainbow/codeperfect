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
    while (world.frame_index < start + 3)
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
    int timeout;
    Jblow_Tests *jt;

    Timeout(int _timeout, int _line, Jblow_Tests *_jt) {
        timeout = _timeout;
        line = _line;
        jt = _jt;
        start = current_time_milli();
    }

    void operator+(fn<bool()> f) {
        while (current_time_milli() - start <= timeout) {
            if (f()) {
                jt->skip_frame();
                return;
            }
            sleep_milliseconds(10);
        }
        cp_panic(cp_sprintf("timeout at line %d", line));
    }
};

#define WAIT_FOR(t) Timeout(t, __LINE__, this) + [&]()
#define WAIT WAIT_FOR(3000)
#define WAIT_FOREVER WAIT_FOR(999999999)

Editor* Jblow_Tests::wait_for_editor(ccstr relative_filepath) {
    Editor* ret = NULL;
    WAIT {
        auto abspath = path_join(world.current_path, relative_filepath);

        ret = find_editor_by_filepath(abspath);
        if (!ret) return false;
        if (world.use_nvim && !ret->is_nvim_ready()) return false;

        return true;
    };
    return ret;
}

Editor* Jblow_Tests::open_editor(ccstr relative_filepath) {
    // open file fuzzy search
    press_key(CP_KEY_P, CP_MOD_PRIMARY);
    WAIT { return world.wnd_goto_file.show; };

    // open main.go
    type_string(relative_filepath);
    press_key(CP_KEY_ENTER);

    // wait for editor
    return wait_for_editor(relative_filepath);
}

void Jblow_Tests::run_nonvimfuzzer() {
    struct Input {
        bool ischar;
        union {
            char ch; // ischar
            struct {
                int mods; // !ischar
                int key; // !ischar
            };
        };
    };

    auto generate_inputs = [&](int seed, int n) {
        mt_seed32(seed);
        auto ret = alloc_list<Input>();

        auto add_char = [&](char c) {
            Input it; ptr0(&it);
            it.ischar = true;
            it.ch = c;
            ret->append(&it);
        };

        auto add_key = [&](char key, int mods) {
            Input it; ptr0(&it);
            it.ischar = false;
            it.key = key;
            it.mods = mods;
            ret->append(&it);
        };

        for (int i = 0; i < n; i++) {
            int n = mt_lrand() % 100;
            if (n < 10) {
                add_char('a' + (mt_lrand() % ('z' - 'a')));
            } else if (n < 20) {
                add_char('A' + (mt_lrand() % ('Z' - 'A')));
            } else if (n < 30) {
                add_char('0' + (mt_lrand() % ('9' - '0')));
            } else if (n < 45) {
                int keys[] = {CP_KEY_BACKSPACE, CP_KEY_ENTER};
                add_key(keys[mt_lrand() % _countof(keys)], 0);
            } else if (n < 70) {
                int keys[] = {CP_KEY_UP, CP_KEY_LEFT, CP_KEY_RIGHT, CP_KEY_DOWN};
                int flags = 0;
                if (mt_lrand() % 2 == 0) flags |= CP_MOD_SHIFT;
                if (mt_lrand() % 2 == 0) flags |= CP_MOD_TEXT;
                add_key(keys[mt_lrand() % _countof(keys)], flags);
            } else {
                int keys[] = {CP_KEY_E, CP_KEY_A, CP_KEY_N, CP_KEY_P, CP_KEY_SPACE};
                int flags = CP_MOD_CTRL;
                if (mt_lrand() % 2 == 0) flags |= CP_MOD_SHIFT;
                add_key(keys[mt_lrand() % _countof(keys)], flags);
            }
        }
        return ret;
    };

    world.dont_prompt_on_close_unsaved_tab = true;

    for (int seed = 0; seed < 16; seed++) {
        // wait for editor
        auto editor = open_editor("main.go");

        auto inputs = generate_inputs(seed, 500);
        Fori (inputs) {
            if (it.ischar)
                type_char(it.ch);
            else
                press_key(it.key, it.mods);
            if (i % 10 == 0) sleep_milliseconds(25);
        }

        // close editor
        press_key(CP_KEY_W, CP_MOD_PRIMARY);
        WAIT { return get_current_editor() == NULL; };

        // wait for imgui input queue to finish
        WAIT_FOREVER { return !ImGui::GetCurrentContext()->InputEventsQueue.Size; };
    }
}

void Jblow_Tests::run_vimfuzzer() {
    ccstr chars = (
        "KKKKKKKKKKKKKK"                // does nothing in normal mode
        "aioaioaioaio"                  // enter insert mode
        "\x01\x01\x01\x01\x01\x01\x01"  // escape
        "\x02\x02\x02\x02\x02\x02\x02"  // backspace
        "\n\n\n\n\n\n"                  // new line
    );

    int chars_len = strlen(chars);

    auto generate_inputs = [&](int seed, int n) -> List<char>* {
        mt_seed32(seed);
        auto ret = alloc_list<char>();
        for (int i = 0; i < n; i++)
            ret->append(chars[mt_lrand() % chars_len]);
        return ret;
    };

    world.dont_prompt_on_close_unsaved_tab = true;

    for (int seed = 0; seed < 16; seed++) {
        // wait for editor
        auto editor = open_editor("main.go");

        auto inputs = generate_inputs(seed, 500);
        Fori (inputs) {
            if (it == 0x01)        press_key(CP_KEY_ESCAPE);
            else if (it == 0x02)   press_key(CP_KEY_BACKSPACE);
            else if (it == '\n')   press_key(CP_KEY_ENTER);
            else                   type_char(it);

            if (i % 10 == 0) sleep_milliseconds(25);
        }

        // ensure we leave insert mode with some time before & after
        press_key(CP_KEY_ESCAPE);
        sleep_milliseconds(200);

        // close editor
        press_key(CP_KEY_W, CP_MOD_PRIMARY);
        WAIT { return get_current_editor() == NULL; };

        // wait for imgui input queue to finish
        WAIT_FOREVER { return !ImGui::GetCurrentContext()->InputEventsQueue.Size; };
    }
}

void Jblow_Tests::run_workspace() {
    WAIT_FOREVER {
        if (world.indexer.status == IND_READY)
            if (world.indexer.index.packages->len)
                return true;
        return false;
    };

    auto editor = open_editor("packer/provisioner/file/version/version.go");
    type_string("jjjjjjj$");

    WAIT {
        auto &cur = editor->cur;
        return cur.x == 48 && cur.y == 7;
    };

    type_string("gd");

    editor = wait_for_editor("packer-plugin-sdk/version/version.go");
    WAIT {
        auto cur = editor->cur;
        return cur.x == 5 && cur.y == 50;
    };
}

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
    if (streq(test_name, "vimfuzzer")) {
        options.enable_vim_mode = true;
    }

    if (streq(test_name, "nonvimfuzzer")) {
        options.enable_vim_mode = false;
    }

    if (streq(test_name, "workspace")) {
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

    if (world.use_nvim) {
        WAIT_FOR(10000) { return world.nvim.is_ui_attached; };
    }

    if (streq(test_name, "workspace")) run_workspace();
    if (streq(test_name, "vimfuzzer")) run_vimfuzzer();
    if (streq(test_name, "nonvimfuzzer")) run_nonvimfuzzer();

    // all good!
    world.message_queue.add([&](auto msg) {
        msg->type = MTM_EXIT;
        msg->exit_code = 0;
    });
}
