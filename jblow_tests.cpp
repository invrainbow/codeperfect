// jblow style tests

#include "world.hpp"
#include "win.hpp"
#include "defer.hpp"
#include "enums.hpp"
#include "mtwist_shim.hpp"

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

    while (!check()) sleep_milli(10);
}

void Jblow_Tests::skip_frame() {
    auto start = world.frame_index;
    while (world.frame_index < start + 3)
        sleep_milli(5);
}

bool is_editor_selected(ccstr relative_filepath) {
    auto editor = get_current_editor();
    if (editor) {
        auto relpath = get_path_relative_to(editor->filepath, world.current_path);
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
            sleep_milli(10);
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
    skip_frame();
    press_key(CP_KEY_ENTER);

    // wait for editor
    return wait_for_editor(relative_filepath);
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
    }

    options.send_crash_reports = false;

    if (streq(test_name, "vim_fuzzer")) {
        options.enable_vim_mode = true;
    }

    else if (streq(test_name, "non_vim_fuzzer")) {
        options.enable_vim_mode = false;
    }

    else if (streq(test_name, "workspace")) {
        options.enable_vim_mode = true;
    }

    else if (streq(test_name, "autocomplete")) {
        options.enable_vim_mode = false;
    }

    else if (streq(test_name, "jump_to_def")) {
        options.enable_vim_mode = false;
    }

    h = create_thread([](void* p) { ((Jblow_Tests*)p)->run(); }, this);
    if (!h) cp_panic("unable to create test thread");
}

void Jblow_Tests::wait_for_indexer_ready() {
    WAIT_FOREVER {
        if (world.indexer.status == IND_READY)
            if (world.indexer.index.packages->len)
                return true;
        return false;
    };
}

template<typename T>
T& random_choice(List<T> *arr) {
    cp_assert(arr->len);
    return arr->at(mt_lrand() % arr->len);
}

void Jblow_Tests::move_cursor(cur2 cur) {
    auto editor = get_current_editor();

    world.message_queue.add([&](auto msg) {
        msg->type = MTM_TEST_MOVE_CURSOR;
        msg->test_move_cursor = cur;
    });

    WAIT { return editor->cur == cur; };
}

List<ccstr> *list_go_files(ccstr subfolder) {
    auto go_files = new_list(ccstr);

    auto queue = new_list(ccstr);
    queue->append(subfolder);

    auto cwd = cp_getcwd();

    while (queue->len) {
        auto path = queue->pop();
        auto pre = "jblow_tests/autocomplete";
        auto fullpath = streq(path, "") ? pre : path_join(pre, path);

        list_directory(fullpath, [&](auto ent) {
            if (ent->type == DIRENT_FILE) {
                if (str_ends_with(ent->name, ".go"))
                    if (is_file_included_in_build(path_join(cwd, fullpath, ent->name)))
                        go_files->append(path_join(path, ent->name));
            } else {
                queue->append(path_join(path, ent->name));
            }
            return true;
        });
    }
    return go_files;
}

void Jblow_Tests::run() {
    mem.init("jblow_tests_mem");
    thread_mem.init("jblow_thread_mem");

    {
        SCOPED_MEM(&mem);
        events.init();
    }

    lock.init();

    Pool pool;
    pool.init("jblow_tests_local");
    defer { pool.cleanup(); };
    SCOPED_MEM(&pool);

    world.dont_prompt_on_close_unsaved_tab = true;

    while (!ready) sleep_milli(10);

    {
        SCOPED_FRAME();
        auto exe = get_executable_path();
        auto newcwd = cp_dirname(cp_dirname(cp_dirname(exe)));
        cp_chdir(newcwd);
    }

    if (streq(test_name, "workspace")) {
        wait_for_indexer_ready();

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

    else if (streq(test_name, "vim_fuzzer")) {
        Random_Input_Gen gen; gen.init();

        for (char x = 'a'; x <= 'z'; x++) gen.add(x, 1);
        for (char x = 'A'; x <= 'Z'; x++) gen.add(x, 1);
        for (int x = CP_KEY_A; x <= CP_KEY_Z; x++) gen.add(x, CP_MOD_CTRL, 1);
        gen.add(CP_KEY_BACKSPACE, 0, 5);
        gen.add(CP_KEY_ESCAPE, 0, 5);
        gen.add(CP_KEY_ENTER, 0, 5);

        mt_seed32(0); // try multiple seeds?

        // wait for editor
        auto editor = open_editor("main.go");
        int last_count = 0;

        for (int i = 0; i < 10000; i++) {
            if (!last_count && mt_lrand() % 7 == 0) {
                int count = (mt_lrand() % 99999) + 1;
                type_string(cp_sprintf("%d", count));
                last_count = count;
            } else {
                Jblow_Input input;
                // don't do large pastes for now until we fix hang
                do {
                    input = gen.pick();
                } while (last_count > 1000 && !input.is_key && tolower(input.ch) == 'p');
                inject_jblow_input(input);
                last_count = 0;
            }
            if (i % 10 == 0) sleep_milli(25);
        }
    }

    else if (streq(test_name, "non_vim_fuzzer")) {
        Random_Input_Gen gen; gen.init();

        for (char ch = 'a'; ch <= 'z'; ch++) gen.add(ch, 10);
        for (char ch = 'A'; ch <= 'Z'; ch++) gen.add(ch, 10);
        for (char ch = '0'; ch <= '9'; ch++) gen.add(ch, 10);
        gen.add(CP_KEY_BACKSPACE, 0, 100);
        gen.add(CP_KEY_ENTER, 0, 100);
        gen.add(CP_KEY_UP, 0, 30);
        gen.add(CP_KEY_LEFT, 0, 30);
        gen.add(CP_KEY_RIGHT, 0, 30);
        gen.add(CP_KEY_DOWN, 0, 30);

        gen.add(CP_KEY_E, CP_MOD_CTRL, 5);
        gen.add(CP_KEY_A, CP_MOD_CTRL, 5);
        gen.add(CP_KEY_N, CP_MOD_CTRL, 5);
        gen.add(CP_KEY_P, CP_MOD_CTRL, 5);
        gen.add(CP_KEY_SPACE, CP_MOD_CTRL, 5);

        gen.add(CP_KEY_E, CP_MOD_CTRL | CP_MOD_SHIFT, 5);
        gen.add(CP_KEY_A, CP_MOD_CTRL | CP_MOD_SHIFT, 5);
        gen.add(CP_KEY_N, CP_MOD_CTRL | CP_MOD_SHIFT, 5);
        gen.add(CP_KEY_P, CP_MOD_CTRL | CP_MOD_SHIFT, 5);
        gen.add(CP_KEY_SPACE, CP_MOD_CTRL | CP_MOD_SHIFT, 5);

        for (int seed = 0; seed < 16; seed++) {
            mt_seed32(seed);

            // wait for editor
            auto editor = open_editor("main.go");

            for (int i = 0; i < 500; i++) {
                inject_jblow_input(gen.pick());
                if (i % 10 == 0)
                    sleep_milli(25);
            }

            // close editor
            press_key(CP_KEY_W, CP_MOD_PRIMARY);
            WAIT { return get_current_editor() == NULL; };

            // wait for imgui input queue to finish
            WAIT_FOREVER { return !ImGui::GetCurrentContext()->InputEventsQueue.Size; };
        }
    }

    else if (streq(test_name, "autocomplete")) {
        wait_for_indexer_ready();

        auto ci_files = list_go_files("controller-idioms");
        if (!ci_files->len)
            cp_panic("no go files found");

        auto hugo_go_files = list_go_files("hugo");
        if (!hugo_go_files->len)
            cp_panic("no go files found");

        auto process_file = [&](ccstr filename, int samples) {
            auto editor = open_editor(filename);
            auto buf = editor->buf;

            WAIT { return buf->tree != NULL; };

            auto iter = new_object(Parser_It);
            iter->init(buf);
            auto root = new_ast_node(ts_tree_root_node(buf->tree), iter);

            struct Target {
                cur2 dot;
                ccstr name;
            };

            auto targets = new_list(Target);

            walk_ast_node(root, true, [&](auto it, auto, auto) {
                switch (it->type()) {
                case TS_SELECTOR_EXPRESSION:
                case TS_QUALIFIED_TYPE: {
                    auto node = it;

                    Target target; ptr0(&target);
                    bool found = false;

                    FOR_ALL_NODE_CHILDREN (node) {
                        if (it->type() == TS_ANON_DOT) {
                            target.dot = it->start();
                            found = true;
                            break;
                        }
                    }

                    if (!found) {
                        print("no dot found");
                        break;
                    }

                    auto field_node = it->field(it->type() == TS_QUALIFIED_TYPE ? TSF_NAME : TSF_FIELD);
                    if (!field_node) {
                        print("couldn't get sel");
                        break;
                    }

                    target.name = field_node->string();
                    targets->append(&target);
                    return WALK_SKIP_CHILDREN;
                }
                }
                return WALK_CONTINUE;
            });

            /*
            struct Exception {
                ccstr filename;
                cur2 dot;
            };

            Exception exceptions[] = {
                // {"controller-idioms/queue/controls.go", NULL_CUR},
            };
            */

            if (targets->len) {
                auto process_target = [&](cur2 dot, ccstr name) {
                    /*
                    auto should_skip = [&]() {
                        for (auto &&it : exceptions)
                            if (streq(filename, it.filename))
                                if ((dot == it.dot) || (it.dot == NULL_CUR))
                                    return true;
                        return false;
                    };

                    if (should_skip()) continue;
                    */

                    move_cursor(new_cur2(dot.x+1, dot.y));

                    press_key(CP_KEY_BACKSPACE);
                    type_char('.');

                    WAIT {
                        auto results = editor->autocomplete.ac.results;
                        if (results != NULL)
                            For (results)
                                if (it.type == ACR_DECLARATION)
                                    if (streq(it.name, name))
                                        return true;
                        return false;
                    };

                    press_key(CP_KEY_ESCAPE);

                    WAIT { return editor->autocomplete.ac.results == NULL; };
                };

                if (samples < targets->len) {
                    for (int j = 0; j < samples; j++) {
                        auto &it = random_choice(targets);
                        process_target(it.dot, it.name);
                    }
                } else {
                    For (targets)
                        process_target(it.dot, it.name);
                }
            }

            // close editor
            press_key(CP_KEY_W, CP_MOD_PRIMARY);
            WAIT { return get_current_editor() == NULL; };
        };

        // there are far fewer ci files, just go thru all of them
        for (int seed = 0; seed < 4; seed++) {
            mt_seed32(seed);
            For (ci_files) process_file(it, 10);
        }

        // sample hugo files randomly
        for (int seed = 0; seed < 4; seed++) {
            mt_seed32(seed);
            for (int i = 0; i < 64; i++)
                process_file(random_choice(hugo_go_files), 5);
        }
    }

    else if (streq(test_name, "jump_to_def")) {
        wait_for_indexer_ready();

        auto ci_files = list_go_files("controller-idioms");
        if (!ci_files->len)
            cp_panic("no go files found");

        auto hugo_go_files = list_go_files("hugo");
        if (!hugo_go_files->len)
            cp_panic("no go files found");

        auto process_file = [&](ccstr filename, int samples) {
            auto editor = open_editor(filename);
            auto buf = editor->buf;

            WAIT { return buf->tree != NULL; };

            auto iter = new_object(Parser_It);
            iter->init(buf);
            auto root = new_ast_node(ts_tree_root_node(buf->tree), iter);

            String_Set bad; bad.init();
            {
                ccstr bad_words[] = {
                    "ComplexType", "FloatType", "IntegerType", "Type",
                    "Type1", "any", "append", "bool", "byte", "cap",
                    "close", "comparable", "complex", "complex128",
                    "complex64", "copy", "delete", "error", "false",
                    "float32", "float64", "imag", "int", "int16", "int32",
                    "int64", "int8", "iota", "len", "make", "new", "nil",
                    "panic", "print", "println", "real", "recover",
                    "rune", "string", "true", "uint", "uint16", "uint32",
                    "uint64", "uint8", "uintptr", "Error", "init",
                };
                For (&bad_words) bad.add(it);
            }

            struct Target {
                cur2 pos;
                ccstr name;
            };

            auto targets = new_list(Target);

            walk_ast_node(root, true, [&](auto it, auto, auto) {
                switch (it->type()) {
                case TS_IDENTIFIER:
                case TS_FIELD_IDENTIFIER:
                case TS_PACKAGE_IDENTIFIER:
                case TS_TYPE_IDENTIFIER: {
                    auto name = it->string();
                    if (name[0] == '\0' || name[1] == '\0') break;
                    if (bad.has(name)) break;

                    auto curr = it->parent();
                    if (curr && curr->type() == TS_LITERAL_ELEMENT) {
                        curr = curr->parent();
                        if (curr && curr->type() == TS_KEYED_ELEMENT) {
                            curr = curr->parent();
                            if (curr && curr->type() == TS_LITERAL_VALUE)
                                break;
                        }
                    }

                    Target target;
                    target.pos = it->start();
                    target.name = it->string();
                    targets->append(&target);

                    return WALK_SKIP_CHILDREN;
                }
                }
                return WALK_CONTINUE;
            });

            if (targets->len) {
                auto process_target = [&](cur2 pos, ccstr name) {
                    WAIT { return world.indexer.status == IND_READY; };

                    cur2 cur = new_cur2(pos.x+1, pos.y);
                    move_cursor(cur);

                    press_key(CP_KEY_G, CP_MOD_PRIMARY);

                    // wait for us to move
                    WAIT {
                        auto curr = get_current_editor();
                        if (curr->id != editor->id) return true;
                        if (curr->cur != cur) return true;

                        return false;
                    };

                    // if we jumped to a new file, close the editor and
                    // ensure we're back in the original editor
                    auto curr = get_current_editor();
                    if (curr->id != editor->id) {
                        press_key(CP_KEY_W, CP_MOD_PRIMARY);

                        WAIT {
                            auto curr = get_current_editor();
                            return curr && curr->id == editor->id;
                        };
                    }
                };

                if (samples < targets->len) {
                    for (int j = 0; j < samples; j++) {
                        auto &it = random_choice(targets);
                        process_target(it.pos, it.name);
                    }
                } else {
                    For (targets)
                        process_target(it.pos, it.name);
                }
            }

            // close editor
            press_key(CP_KEY_W, CP_MOD_PRIMARY);
            WAIT { return get_current_editor() == NULL; };
        };

        // there are far fewer ci files, just go thru all of them
        for (int seed = 0; seed < 4; seed++) {
            mt_seed32(seed);
            For (ci_files) process_file(it, 10);
        }

        // sample hugo files randomly
        for (int seed = 0; seed < 4; seed++) {
            mt_seed32(seed);
            for (int i = 0; i < 64; i++)
                process_file(random_choice(hugo_go_files), 5);
        }
    }

    // all good!
    world.message_queue.add([&](auto msg) {
        msg->type = MTM_EXIT;
        msg->exit_code = 0;
    });
}

void Random_Input_Gen::init() {
    ptr0(this);
    items = new_list(Weighted_Item);
    total = 0;
}

void Random_Input_Gen::add(int key, int mods, int weight) {
    Weighted_Item out; ptr0(&out);
    out.input.is_key = true;
    out.input.key = key;
    out.input.mods = mods;
    out.weight = weight;
    items->append(out);
    total += weight;
}

void Random_Input_Gen::add(char ch, int weight) {
    Weighted_Item out; ptr0(&out);
    out.input.is_key = false;
    out.input.ch = ch;
    out.weight = weight;
    items->append(out);
    total += weight;
}

Jblow_Input Random_Input_Gen::pick() {
    int k = mt_lrand() % total;
    For (items) {
        if (k < it.weight)
            return it.input;
        k -= it.weight;
    }
    cp_panic("this shouldn't happen");
}
