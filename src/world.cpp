#include "world.hpp"
#include "ui.hpp"
#include "fzy_match.h"
#include "set.hpp"
#include "defer.hpp"
#include "glcrap.hpp"
#include "jblow_tests.hpp"

World world = {0};

u64 post_insert_dotrepeat_time = 0;
int gargc = 0;
char **gargv = NULL;

bool is_ignored_by_git(ccstr path) {
    // go-gitignore crashes when path == base path
    if (are_filepaths_equal(path, world.current_path)) return false;
    return GHGitIgnoreCheckFile((char*)path);
}

void History::actually_push(int editor_id, cur2 pos) {
    assert_main_thread();

    auto editor = find_editor_by_id(editor_id);

    check_marks();

    for (int i = curr; i != top; i = inc(i))
        ring[i].cleanup();

    ring[curr].editor_id = editor_id;
    ring[curr].pos = pos; // do we even need this anymore?
    ring[curr].mark = editor->buf->insert_mark(MARK_HISTORY, pos);

    top = curr = inc(curr);
    if (curr == start) {
        ring[start].cleanup();
        start = inc(start);
    }

    check_marks();
}

void History::push(int editor_id, cur2 pos) {
    auto should_push = [&]() -> bool {
        if (curr == start) return true;
        if (curr != top) return true;

        auto &it = ring[dec(curr)];
        if (it.editor_id != editor_id) return true;

        auto delta = abs((int)pos.y - (int)it.pos.y);
        return delta >= 10;
    };

    if (!should_push()) return;

    check_marks();

    // if there's something in history
    do {
        if (curr == start) break;

        auto &prev = ring[dec(curr)];
        if (prev.editor_id == editor_id) break;

        auto prev_editor = find_editor_by_id(prev.editor_id);
        if (!prev_editor) break;

        if (prev_editor->cur == prev.pos) break;

        actually_push(prev_editor->id, prev_editor->cur);
    } while (0);

    check_marks();

    actually_push(editor_id, pos);

    check_marks();
}

void History::actually_go(History_Loc *it) {
    assert_main_thread();

    auto editor = find_editor_by_id(it->editor_id);
    if (!editor) return;

    auto pos = is_mark_valid(it->mark) ? it->mark->pos() : it->pos;

    auto old = world.dont_push_history;
    world.dont_push_history = true;
    defer { world.dont_push_history = old; };

    focus_editor_by_id(it->editor_id, pos);
}

bool History::go_forward(int count) {
    assert_main_thread();
    check_marks();

    if (curr == top) return false;

    check_marks();

    for (int i = 0; i < count; i++) {
        curr = inc(curr);
        if (curr == top) break;
    }

    actually_go(&ring[dec(curr)]);
    check_marks();
    return true;
}

void History::check_marks(int upper) {
#ifdef DEBUG_BUILD
    if (upper == -1) upper = top;

    for (auto i = start; i != upper; i = inc(i)) {
        auto it = ring[i].mark;
        if (!is_mark_valid(it)) continue;

        // check that mark node contains the mark
        bool found = false;
        for (auto m = it->node->marks; m; m = m->next) {
            if (m == it) {
                found = true;
                break;
            }
        }
        if (!found) cp_panic("mark got detached from its node somehow");

        // check that mark node is still in root
        auto node = it->node;
        while (node->parent)
            node = node->parent;

        if (it->buf->mark_tree->root != node) cp_panic("mark node is detached from root!");
    }
#endif
}

bool History::go_backward(int count) {
    cp_assert(count > 0);
    assert_main_thread();
    check_marks();

    if (curr == start) return false;

    auto pos = curr;

    {
        // for first jump, handle the case of: open file a, open file b, move
        // down 4 lines (something < threshold), go back, go forward, cursor
        // will now be on line 0 instead of line 4
        auto &last = ring[dec(pos)];
        auto editor = get_current_editor();

        if (!editor || last.editor_id != editor->id || last.pos != editor->cur) {
            if (editor)
                actually_push(editor->id, editor->cur);
            count--;
        }
    }

    for (int i = 0; i < count; i++) {
        auto next = dec(pos);
        if (next == start)
            break;
        pos = next;
    }

    curr = pos;
    cp_assert(curr != start);
    actually_go(&ring[dec(curr)]);
    return true;
}

void History::save_latest() {
    assert_main_thread();

    auto editor = get_current_editor();
    if (!editor) return;

    if (curr == start) return; // does this ever happen?

    auto &it = ring[dec(curr)];
    if (it.editor_id == editor->id && it.pos == editor->cur)
        return;

    check_marks();
    actually_push(editor->id, editor->cur);
    check_marks();
}

void History::remove_entries_for_editor(int editor_id) {
    assert_main_thread();

    int i = start, j = start;

    auto run_until = [&](int until) {
        for (; i != until; i = inc(i)) {
            if (ring[i].editor_id == editor_id) {
                cp_assert(ring[i].mark);
                ring[i].mark->cleanup();
                ring[i].mark = NULL;
                continue;
            }
            if (i != j)
                memcpy(&ring[j], &ring[i], sizeof(ring[j]));
            j = inc(j);
        }
    };

    check_marks();

    run_until(curr);
    curr = j;

    check_marks();

    run_until(top);
    top = j;

    check_marks();
}

void History_Loc::cleanup() {
    if (!mark) return;
    mark->cleanup();
    mark = NULL;
}

bool exclude_from_file_tree(ccstr path) {
    if (is_ignored_by_git(path)) return true;

    auto filename = cp_basename(path);
    if (streq(filename, ".git")) return true;
    if (streq(filename, ".cpproj")) return true;
    if (streq(filename, ".cpdb")) return true;
    if (streq(filename, ".cpdb.tmp")) return true;
    if (str_ends_with(filename, ".exe")) return true;

    return false;
}

void crawl_path_into_ftnode(ccstr path, FT_Node *parent) {
    FT_Node *last_child = parent->children;

    list_directory(path, [&](Dir_Entry *ent) {
        do {
            auto fullpath = path_join(path, ent->name);
            if (exclude_from_file_tree(fullpath)) break;

            FT_Node *file = NULL;

            {
                SCOPED_MEM(&world.file_tree_mem);

                file = new_object(FT_Node);
                file->name = cp_strdup(ent->name);
                file->is_directory = (ent->type & FILE_TYPE_DIRECTORY);
                file->num_children = 0;
                file->parent = parent;
                file->children = NULL;
                file->depth = parent->depth + 1;
                file->open = false;
                file->next = NULL;

                if (!last_child) {
                    parent->children = last_child = file;
                } else {
                    last_child->next = file;
                    file->prev = last_child;
                    last_child = file;
                }

                parent->num_children++;
            }

            if (file->is_directory) crawl_path_into_ftnode(fullpath, file);
        } while (0);

        return true;
    });

    {
        SCOPED_MEM(&world.file_tree_mem);
        parent->children = sort_ft_nodes(parent->children);
    }
}

void fill_file_tree() {
    if (world.file_tree_busy) return;

    world.file_tree_busy = true;

    auto t = create_thread([](void*) {
        defer { world.file_tree_busy = false; };

        world.file_tree_mem.reset();

        Pool pool;
        pool.init("fill_file_tree_thread");
        defer { pool.cleanup(); };
        SCOPED_MEM(&pool);

        // invalidate pointers
        world.file_explorer.selection = NULL;
        world.file_explorer.last_file_copied = NULL;
        world.file_explorer.last_file_cut = NULL;
        world.file_explorer.scroll_to = NULL;
        world.file_explorer.dragging_source = NULL;
        world.file_explorer.dragging_dest = NULL;

        {
            SCOPED_MEM(&world.file_tree_mem);
            world.file_tree = new_object(FT_Node);
            world.file_tree->is_directory = true;
            world.file_tree->depth = 0;
            world.file_tree->open = true;
        }

        GHGitIgnoreInit(world.current_path); // why do we need this here?
        crawl_path_into_ftnode(world.current_path, world.file_tree);
    });

    if (t) close_thread_handle(t);
}

void World::init() {
    Timer t; t.init("world::init", false);

    ptr0(this);

    {
        auto ver = GHGetVersionString();
        if (!ver) cp_panic("couldn't get version");
        defer { GHFree(ver); };
        cp_strcpy_fixed(gh_version, ver);
    }

    t.log("getversionstring");

#define init_mem(x) x.init(#x)
    init_mem(world_mem);
    init_mem(frame_mem);
    init_mem(file_tree_mem);
    init_mem(autocomplete_mem);
    init_mem(parameter_hint_mem);
    init_mem(goto_file_mem);
    init_mem(goto_symbol_mem);
    init_mem(scratch_mem);
    init_mem(build_index_mem);
    init_mem(ui_mem);
    init_mem(find_references_mem);
    init_mem(rename_identifier_mem);
    init_mem(run_command_mem);
    init_mem(generate_implementation_mem);
    init_mem(find_implementations_mem);
    init_mem(find_interfaces_mem);
    init_mem(caller_hierarchy_mem);
    init_mem(callee_hierarchy_mem);
    init_mem(project_settings_mem);
    init_mem(fst_mem);
    init_mem(search_marks_mem);
    init_mem(workspace_mem_1);
    init_mem(workspace_mem_2);
#undef init_mem

    t.log("init mem");

    MEM = &frame_mem;

    init_treesitter_go_trie();

    global_mark_tree_lock.init();
    build_lock.init();

    mark_fridge.init(512);
    avl_node_fridge.init(512);
    change_fridge.init(512);
    treap_fridge.init(512);

    chunk0_fridge.init(512);
    chunk1_fridge.init(256);
    chunk2_fridge.init(128);
    chunk3_fridge.init(64);
    chunk4_fridge.init(32);
    chunk5_fridge.init(16);
    chunk6_fridge.init(8);
    chunk7_fridge.init(8);

    t.log("init random shit");

    {
        auto tmp = GHGetConfigDir();
        if (!tmp) cp_panic("couldn't get config dir");
        defer { GHFree(tmp); };

        cp_strcpy_fixed(configdir, tmp);
    }

    t.log("get config dir");

    {
        auto go_binary_path = GHGetGoBinaryPath();
        if (!go_binary_path) {
            cp_exit("Unable to find a go binary.\n\nUsually, CodePerfect searches for go by running `which go` inside `bash`, but we did that and couldn't find anything.\n\nPlease visit docs.codeperfect95.com to see how to manually tell CodePerfect where go is.");
        }

        defer { GHFree(go_binary_path); };
        cp_strcpy_fixed(world.go_binary_path, go_binary_path);
    }

    t.log("get go binary path");

    {
        // do we need world_mem anywhere else?
        // i assume we will have other things that "orchestrate" world
        SCOPED_MEM(&world_mem);
        message_queue.init();
        last_closed = new_list(Last_Closed);
        konami = new_list(int);
    }

    {
        SCOPED_MEM(&world_mem);
        frameskips.init();
#ifdef DEBUG_BUILD
        // show_frameskips = true;
#endif
    }

    fzy_init();

    t.log("init more random shit");

    bool already_read_current_path = false;
    bool make_testing_headless = false;

    for (int i = 1; i < gargc; i++) {
        auto it = gargv[i];

#ifndef RELEASE_MODE

        if (streq(it, "--test")) {
            if (i+1 >= gargc) cp_panic("missing test name");
            auto name = gargv[++i];

            cp_strcpy_fixed(test_name, name);
            test_running = true;
        }

        else if (streq(it, "--jblow-tests")) {
            if (i+1 >= gargc) cp_panic("missing test name");

            auto name = gargv[++i];
            jblow_tests.init(name);

            auto path = cp_getcwd();
            path = cp_dirname(path); // bin
            path = cp_dirname(path); // build
            path = path_join(path, "jblow_tests");
            path = path_join(path, name);

            cp_strcpy_fixed(current_path, path);
            already_read_current_path = true;
        }

        else if (streq(it, "--headless")) {
            make_testing_headless = true;
        }

#endif // RELEASE_MODE

        else if (!already_read_current_path) {
            cp_strcpy_fixed(current_path, it);
            already_read_current_path = true;
        }
    }

    t.log("parse argv");

    // read options from disk
    do {
        if (world.jblow_tests.on) break;

        auto filepath = path_join(configdir, ".options");

        File f;
        if (f.init_read(filepath) != FILE_RESULT_OK)
            break;
        defer { f.cleanup(); };

        Serde serde;
        serde.init(&f);
        serde.read_type(&options, SERDE_OPTIONS);
    } while (0);

    vim.on = options.enable_vim_mode;

    if (vim.on) {
        vim.mem.init("vim_mem");
        SCOPED_MEM(&vim.mem);
        vim.yank_register.init();
        vim.dotrepeat.mem_working.init("vim_dotrepeat_working");
        vim.dotrepeat.mem_finished.init("vim_dotrepeat_finished");
    }

    if (make_testing_headless) {
        if (!jblow_tests.on)
            cp_panic("headless only valid when --test");
        jblow_tests.headless = true;
    }

    t.log("more shit");

    // init workspace
    {
        resizing_pane = -1;
        panes.init(LIST_FIXED, _countof(_panes), _panes);

#ifdef TESTING_BUILD
        cp_strcpy_fixed(current_path, "/Users/bh/ide/go");
#else
        if (!already_read_current_path) {
            ccstr last_folder = NULL;

            do {
                if (!options.open_last_folder) break;

                auto last_folder_path = path_join(configdir, ".last_folder");

                auto fm = map_file_into_memory(last_folder_path);
                if (!fm) break;

                defer {
                    fm->cleanup();
                    delete_file(last_folder_path);
                };

                auto result = new_list(char);
                for (int i = 0; i < fm->len; i++) {
                    auto it = fm->data[i];
                    if (it == '\n' || it == '\0')
                        break;
                    result->append(it);
                }
                result->append('\0');

                auto path = result->items;
                if (check_path(path) == CPR_DIRECTORY)
                    last_folder = path;
            } while (0);

            if (last_folder) {
                cp_strcpy_fixed(current_path, last_folder);
            } else {
                Select_File_Opts opts; ptr0(&opts);
                opts.buf = current_path;
                opts.bufsize = _countof(current_path);
                opts.folder = true;
                opts.save = false;
                if (!let_user_select_file(&opts)) exit(0);
            }
        }
#endif

        if (check_path(current_path) != CPR_DIRECTORY)
            cp_exit("Unable to open selected folder (not a directory).");

        GHGitIgnoreInit(current_path);
        cp_chdir(current_path);
    }

    t.log("init workspace");

    // read project settings
    // TODO: handle errors
    {
        auto read_project_settings = [&]() {
            auto filepath = path_join(current_path, ".cpproj");
            File f;
            if (f.init_read(filepath) != FILE_RESULT_OK)
                return false;
            defer { f.cleanup(); };

            Serde serde;
            serde.init(&f);
            {
                SCOPED_MEM(&world.project_settings_mem);
                serde.read_type(&project_settings, SERDE_PROJECT_SETTINGS);
            }
            return serde.ok;
        };

        if (!read_project_settings()) {
            SCOPED_MEM(&world.project_settings_mem);
            project_settings.load_defaults();
        }
    }

    t.log("read project settings");

    indexer.init();
    t.log("init indexer");

    dbg.init();
    t.log("init debugger");

    history.init();
    t.log("init history");

    searcher.init();
    t.log("init searcher");

    navigation_queue.init(LIST_FIXED, _countof(_navigation_queue), _navigation_queue);

    t.log("init navigation queue");
    fill_file_tree();
    t.log("fill file tree");

    // set defaults
    error_list.height = 125;
    file_explorer.selection = NULL;
    wnd_search_and_replace.search_go_files_only = true;

    {
        SCOPED_MEM(&ui_mem);
        ::ui.init();
    }

    t.log("init ui");

    fswatch.init(current_path);

    // init ui shit
    init_global_colors();
    init_command_info_table();

    world.file_explorer.show = true;

    last_manually_run_command = CMD_INVALID;

#ifdef DEBUG_BUILD
    // if (!use_nvim) world.wnd_history.show = true;

    show_frame_index = false;
#endif

    t.log("rest of shit");

    if (vim.on) {
        // anything else?
        vim_set_mode(VI_NORMAL);
    }
}

void World::start_background_threads() {
    indexer.start_background_thread();

    dbg.start_loop();

#ifdef DEBUG_BUILD
    // VSCode debugger frequently fails to break when I press break, but it
    // works if I set a breakpoint which is hit. My asinine solution is to
    // create a background thread that runs in a loop, and set a breakpoint in
    // it when I need to break.

    auto microsoft_programmers_are_fucking_monkeys = [](void*) {
        while (true) sleep_milli(1000);
    };

    {
        auto t = create_thread(microsoft_programmers_are_fucking_monkeys, NULL);
        if (!t) cp_panic("couldn't create thread");
        close_thread_handle(t);
    }
#endif
}

Pane* get_current_pane() {
    if (!world.panes.len) return NULL;

    // happens when we're closing shit
    if (world.current_pane == world.panes.len) return NULL;

    return &world.panes[world.current_pane];
}

Editor* get_current_editor() {
    auto pane = get_current_pane();
    if (!pane) return NULL;

    return pane->get_current_editor();
}

Editor* find_editor(find_editor_func f) {
    For (get_all_editors())
        if (f(it))
            return it;
    return NULL;
}

Editor* find_editor_by_id(u32 id) {
    auto is_match = [&](auto it) { return it->id == id; };
    return find_editor(is_match);
}

Editor* find_editor_by_filepath(ccstr filepath) {
    auto is_match = [&](auto it) {
        return are_filepaths_same_file(it->filepath, filepath);
    };
    return find_editor(is_match);
}

Editor *focus_editor(ccstr path) {
    return focus_editor(path, NULL_CUR);
}

Editor *focus_editor(ccstr path, cur2 pos, bool pos_in_byte_format) {
    for (auto&& pane : world.panes) {
        for (u32 i = 0; i < pane.editors.len; i++) {
            auto &it = pane.editors[i];
            if (are_filepaths_same_file(path, it.filepath)) {
                activate_pane(&pane);
                pane.focus_editor_by_index(i, pos, pos_in_byte_format);
                return &it;
            }
        }
    }
    return get_current_pane()->focus_editor(path, pos, pos_in_byte_format);
}

void activate_pane(Pane *pane) {
    activate_pane_by_index(pane - world.panes.items);
}

void activate_pane_by_index(u32 idx) {
    if (idx > world.panes.len) return;

    if (idx == world.panes.len) {
        auto panes_width = ::ui.panes_area.w;

        float new_width = panes_width;
        if (world.panes.len)
            new_width /= world.panes.len;

        auto pane = world.panes.append();
        pane->init();
        pane->width = new_width;
    }

    if (world.current_pane != idx)
        reset_everything_when_switching_editors(get_current_editor());

    world.current_pane = idx;
    world.cmd_unfocus_all_windows = true;
}

void init_goto_file() {
    if (world.file_tree_busy) {
        tell_user_error("The file tree is currently being generated.");
        return;
    }

    SCOPED_MEM(&world.goto_file_mem);
    world.goto_file_mem.reset();

    auto &wnd = world.wnd_goto_file;
    ptr0(&wnd);

    wnd.filepaths = new_list(ccstr);
    wnd.filtered_results = new_list(int);

    fstlog("init_goto_file - start");

    fn<void(FT_Node*, ccstr)> fill_files = [&](auto node, auto path) {
        for (auto it = node->children; it; it = it->next) {
            auto isdir = it->is_directory;
            // if (isdir && !node->parent && streq(it->name, ".cp")) return;

            auto relpath = path[0] == '\0' ? cp_strdup(it->name) : path_join(path, it->name);
            if (isdir)
                fill_files(it, relpath);
            else
                wnd.filepaths->append(relpath);
        }
    };

    fill_files(world.file_tree, "");
    fstlog("init_goto_file - fill");
    wnd.show = true;
}

void kick_off_build(Build_Profile *build_profile) {
    SCOPED_LOCK(&world.build_lock);

    if (!build_profile)
        build_profile = project_settings.get_active_build_profile();

    if (!build_profile) {
        tell_user("You have no build profile selected.", "No profile selected");
        return;
    }

    world.build.cleanup();
    world.build.init();

    auto do_build = [](void* param) {
        auto build_profile = (Build_Profile*)param;
        auto build = &world.build;

        auto& indexer = world.indexer;

        SCOPED_MEM(&build->mem);

        build->id = world.next_build_id++;
        build->build_profile_name = cp_strdup(build_profile->label);
        build->started = true;

        if (!GHStartBuild((char*)build_profile->cmd)) {
            build->build_itself_had_error = true;
            build->done = true;
            return;
        }

        enum {
            GH_BUILD_INACTIVE = 0,
            GH_BUILD_DONE,
            GH_BUILD_RUNNING,
        };

        for (;; sleep_milli(100)) {
            GoInt num_errors = 0;
            GoInt status = 0;
            auto errors = GHGetBuildStatus(&status, &num_errors);
            defer { if (errors) GHFreeBuildStatus(errors, num_errors); };

            if (status != GH_BUILD_DONE) continue;

            for (u32 i = 0; i < num_errors; i++) {
                Build_Error err;

                err.message = cp_strdup(errors[i].text);
                err.valid = errors[i].is_valid;

                if (err.valid) {
                    err.file = cp_strdup(errors[i].filename);
                    err.row = errors[i].line;
                    err.col = errors[i].col;
                    // errors[i].is_vcol;
                }

                build->errors.append(&err);
            }

            build->current_error = -1;
            build->done = true;
            build->started = false;

            if (!build->errors.len) {
                // world.error_list.show = false;
            }

            For (get_all_editors()) {
                auto editor = it;
                auto path = get_path_relative_to(it->filepath, world.current_path);

                For (&build->errors) {
                    if (!it.valid) continue;
                    if (!are_filepaths_equal(path, it.file)) continue;

                    it.mark = editor->buf->insert_mark(MARK_BUILD_ERROR, new_cur2(it.col-1, it.row-1));
                }
            }
            break;
        }
    };

    world.build.thread = create_thread(do_build, build_profile);
}

void* get_native_window_handle() {
    if (!world.window) return NULL;
    return world.window->get_native_handle();
}

void filter_files() {
    auto &wnd = world.wnd_goto_file;

    wnd.filtered_results->len = 0;

    fstlog("filter_files - start");

    u32 i = 0, j = 0;
    For (wnd.filepaths) {
        if (fzy_has_match(wnd.query, it)) {
            wnd.filtered_results->append(i);
            if (j++ > 10000)
                break;
        }
        i++;
    }

    fstlog("filter_files - match");

    fuzzy_sort_filtered_results(
        wnd.query,
        wnd.filtered_results,
        wnd.filepaths->len,
        [&](auto i) { return wnd.filepaths->at(i); }
    );

    fstlog("filter_files - score/sort");
}

void run_proc_the_normal_way(Process* proc, ccstr cmd) {
    proc->cleanup();
    proc->init();
    proc->dir = world.current_path;
    proc->run(cmd);
}

Editor* focus_editor_by_id(int editor_id, cur2 pos, bool pos_in_byte_format) {
    For (&world.panes) {
        for (int j = 0; j < it.editors.len; j++) {
            auto &editor = it.editors[j];
            if (editor.id == editor_id) {
                activate_pane(&it);
                it.focus_editor_by_index(j, pos);
                return &editor;
            }
        }
    }
    return NULL;
}

bool is_build_debug_free() {
    if (world.build.started) return false;
    if (world.dbg.mt_state.state_flag != DLV_STATE_INACTIVE) return false;

    return true;
}

Editor* goto_file_and_pos(ccstr file, cur2 pos, bool pos_in_byte_format, Ensure_Cursor_Mode mode) {
    auto editor = focus_editor(file, pos, pos_in_byte_format);
    if (!editor) return NULL; // TODO

    if (world.vim.on && world.vim_mode() != VI_NORMAL)
        editor->trigger_escape();

    editor->ensure_cursor_on_screen_by_moving_view(mode);

    world.cmd_unfocus_all_windows = true;
    return editor;
}

void goto_jump_to_definition_result(Jump_To_Definition_Result *result) {
    world.history.save_latest();
    goto_file_and_pos(result->file, result->pos, true, ECM_GOTO_DEF);
}

Jump_To_Definition_Result *get_current_definition(ccstr *filepath, bool display_error, cur2 pos) {
    auto show_error = [&](ccstr msg) -> Jump_To_Definition_Result* {
        if (display_error)
            tell_user_error(msg);
        return NULL;
    };

    auto editor = get_current_editor();
    if (!editor)
        return show_error("Couldn't find anything under your cursor (you don't have an editor focused).");
    if (editor->lang != LANG_GO)
        return show_error("Couldn't find anything under your cursor (you're not in a Go file).");

    defer { world.indexer.ui_mem.reset(); };

    Jump_To_Definition_Result *result = NULL;

    {
        SCOPED_MEM(&world.indexer.ui_mem);

        if (!world.indexer.acquire_lock(IND_READING, true))
            return show_error("The indexer is currently busy.");
        defer { world.indexer.release_lock(IND_READING); };

        if (pos == NULL_CUR) pos = editor->cur;
        auto off = editor->cur_to_offset(pos);
        result = world.indexer.jump_to_definition(editor->filepath, new_cur2(off, -1));
    }

    if (!result || !result->decl)
        return show_error("Couldn't find anything under your cursor.");

    if (filepath)
        *filepath = editor->filepath;

    // copy using caller mem
    return result->copy();
}

void handle_goto_definition(cur2 pos) {
    auto result = get_current_definition(NULL, false, pos);
    if (result)
        goto_jump_to_definition_result(result);
}

void save_all_unsaved_files() {
    For (get_all_editors()) {
        // TODO: handle untitled files; and when we do, put it behind a
        // flag, because save_all_unsaved_files() is now depended on and we
        // can't break existing functionality
        if (it->is_untitled) continue;
        if (!path_has_descendant(world.current_path, it->filepath)) continue;

        it->handle_save(false);
    }
}

bool ft_tree_contains_node(FT_Node *tree, FT_Node *node) {
    if (tree == node) return true;

    for (auto child = tree->children; child; child = child->next)
        if (ft_tree_contains_node(child, node))
            return true;
    return false;
}

void delete_ft_node(FT_Node *it, bool delete_on_disk) {
    if (delete_on_disk) {
        SCOPED_FRAME();
        auto rel_path = ft_node_to_path(it);
        auto full_path = path_join(world.current_path, rel_path);
        if (it->is_directory)
            delete_rm_rf(full_path);
        else
            delete_file(full_path);
    }

    auto &fe = world.file_explorer;
    if (fe.selection && ft_tree_contains_node(it, fe.selection))
        fe.selection = NULL;

    // delete `it` from file tree
    if (it->parent && it->parent->children == it)
        it->parent->children = it->next;
    if (it->prev)
        it->prev->next = it->next;
    if (it->next)
        it->next->prev = it->prev;
}

ccstr ft_node_to_path(FT_Node *node) {
    cp_assert(node);

    auto path = new_list(FT_Node*);
    for (auto curr = node; curr; curr = curr->parent)
        path->append(curr);
    path->len--; // remove root

    Text_Renderer r;
    r.init();
    for (i32 j = path->len - 1; j >= 0; j--) {
        r.write("%s", path->at(j)->name);
        if (j) r.write("/");
    }
    return r.finish();
}

FT_Node *find_ft_node(ccstr relpath) {
    auto path = make_path(relpath);
    auto node = world.file_tree;
    For (path->parts) {
        FT_Node *next = NULL;
        for (auto child = node->children; child; child = child->next) {
            if (streqi(child->name, it)) {
                next = child;
                break;
            }
        }
        if (!next) return NULL;
        node = next;
    }
    return node;
}

int compare_ft_nodes(FT_Node *a, FT_Node *b) {
    auto score_node = [](FT_Node *it) -> int {
        if (it->is_directory) return 0;
        if (str_ends_with(it->name, ".go")) return 1;
        return 2;
    };

    auto sa = score_node(a);
    auto sb = score_node(b);
    if (sa < sb) return -1;
    if (sa > sb) return 1;
    return strcmpi(a->name, b->name);
}

void add_ft_node(FT_Node *parent, fn<void(FT_Node* it)> cb) {
    FT_Node *node = NULL;

    {
        SCOPED_MEM(&world.file_tree_mem);

        node = new_object(FT_Node);
        node->num_children = 0;
        node->depth = parent->depth + 1;
        node->parent = parent;
        node->children = NULL;
        node->prev = NULL;
        cb(node);
    }

    node->next = parent->children;
    if (parent->children)
        parent->children->prev = node;
    parent->children = node;
    parent->num_children++;
    parent->children = sort_ft_nodes(parent->children);
}

FT_Node *sort_ft_nodes(FT_Node *nodes) {
    if (!nodes || !nodes->next) return nodes;

    int len = 0;
    for (auto it = nodes; it; it = it->next)
        len++;

    FT_Node *a = nodes, *b = nodes;
    for (int i = 0; i < len/2; i++)
        b = b->next;
    b->prev->next = NULL;
    b->prev = NULL;

    a = sort_ft_nodes(a);
    b = sort_ft_nodes(b);

    FT_Node *ret = NULL, *curr = NULL;

    while (a && b) {
        FT_Node **ptr = (compare_ft_nodes(a, b) <= 0 ? &a : &b);
        if (!ret) {
            ret = *ptr;
        } else {
            curr->next = *ptr;
            (*ptr)->prev = curr;
        }
        curr = *ptr;
        *ptr = (*ptr)->next;
    }

    if (a) {
        curr->next = a;
        a->prev = curr;
    }

    if (b) {
        curr->next = b;
        b->prev = curr;
    }

    return ret;
}

void goto_error(int index) {
    auto &b = world.build;
    if (index < 0 || index >= b.errors.len) return;

    auto &error = b.errors[index];

    SCOPED_FRAME();

    auto path = path_join(world.current_path, error.file);
    auto pos = new_cur2(error.col-1, error.row-1);

    auto editor = find_editor([&](auto it) {
        return are_filepaths_equal(path, it->filepath);
    });

    // when build finishes, set marks on existing editors
    // when editor opens, get all existing errors and set marks

    if (!editor || !is_mark_valid(error.mark)) {
        goto_file_and_pos(path, pos, false); // TODO: byte format?
        return;
    }

    focus_editor_by_id(editor->id, error.mark->pos());
    world.cmd_unfocus_all_windows = true;
    b.scroll_to = index;
}

void reload_file_subtree(ccstr relpath) {
    // TODO: if the file tree is busy, add the path to a queue of subtrees to reload
    // and pull the stuff underneath into like actually_reload_file_subtree()
    // and then when file tree is finished, call actually_reload_file_subtree() on everything in the queue
    if (world.file_tree_busy) return;

    auto path = path_join(world.current_path, relpath);
    if (is_ignored_by_git(path))
        return;

    switch (check_path(path)) {
    case CPR_NONEXISTENT: {
        auto node = find_ft_node(relpath);
        if (node)
            delete_ft_node(node, false);
        return;
    }
    case CPR_FILE:
        return;
    }

    auto find_or_create_ft_node = [&](ccstr relpath, bool is_directory) -> FT_Node * {
        auto ret = find_ft_node(relpath);
        if (ret) return ret;

        auto parent = find_ft_node(cp_dirname(relpath));
        if (!parent) return NULL;

        {
            SCOPED_MEM(&world.file_tree_mem);

            auto ret = new_object(FT_Node);
            ret->is_directory = true;
            ret->name = cp_basename(relpath);
            ret->depth = parent->depth + 1;
            ret->num_children = 0;
            ret->parent = parent;
            ret->children = NULL;
            ret->prev = NULL;
            ret->open = parent->open;

            ret->next = parent->children;
            if (parent->children)
                parent->children->prev = ret;
            parent->children = ret;
            parent->children = sort_ft_nodes(parent->children);

            return ret;
        }
    };

    auto node = find_or_create_ft_node(relpath, true);
    if (!node) return;

    String_Set current_items;    current_items.init();
    String_Set new_files;        new_files.init();
    String_Set new_directories;  new_directories.init();

    for (auto it = node->children; it; it = it->next)
        current_items.add(it->name);

    list_directory(path, [&](Dir_Entry *ent) {
        do {
            auto fullpath = path_join(path, ent->name);
            if (exclude_from_file_tree(fullpath)) break;

            auto name = cp_strdup(ent->name);
            if (ent->type & FILE_TYPE_DIRECTORY)
                new_directories.add(name);
            else
                new_files.add(name);
        } while (0);

        return true;
    });

    For (current_items.items()) {
        // TODO: check for type mismatch (e.g. used to be file, now is a directory)

        if (new_files.has(it))  {
            current_items.remove(it);
            new_files.remove(it);
        } else if (new_directories.has(it))  {
            current_items.remove(it);
            new_directories.remove(it);
        }
    }

    // current_items contains stale items we want to delete
    // new_files and new_directories contain new items we want to add

    {
        FT_Node *head = NULL, *curr = NULL;

        auto add_child = [&](FT_Node *it) {
            if (!head)
                head = it;
            else
                curr->next = it;

            it->prev = curr;
            curr = it;
        };

        for (auto it = node->children; it; it = it->next)
            if (!current_items.has(it->name))
                add_child(it);

        For (new_files.items()) {
            SCOPED_MEM(&world.file_tree_mem);

            auto child = new_object(FT_Node);
            child->name = cp_strdup(it);
            child->is_directory = false;
            child->num_children = 0;
            child->parent = node;
            child->children = NULL;
            child->depth = node->depth + 1;
            child->open = false;
            add_child(child);
        }

        For (new_directories.items()) {
            SCOPED_MEM(&world.file_tree_mem);

            // TODO: recurse into directory

            auto child = new_object(FT_Node);
            child->name = cp_strdup(it);
            child->is_directory = true;
            child->num_children = 0;
            child->parent = node;
            child->children = NULL;
            child->depth = node->depth + 1;
            child->open = node->open;
            add_child(child);
        }

        if (curr) curr->next = NULL;

        node->children = sort_ft_nodes(head);
    }

    // now delete all items in `current_items`
    // and add/recurse/process all items in `new_files` and `new_directories`
    // parent->children = sort_ft_nodes(parent->children);
}

bool move_autocomplete_cursor(Editor *editor, int direction) {
    auto &ac = editor->autocomplete;
    if (!ac.ac.results) return false;

    if (!ac.filtered_results->len) {
        // Is this a good place to handle "close autocomplete"? Right now this
        // function is called when user has just tried to go up/down in
        // autocomplete menu and the menu is empty. Right now the next action
        // is to move their cursor up or down. Not sure if this function will
        // be used in other cases.
        ac.ac.results = NULL;

        return false;
    }

    if (!ac.selection && direction == -1)
        ac.selection = ac.filtered_results->len - 1;
    else
        ac.selection = (ac.selection + direction) % ac.filtered_results->len;

    if (ac.selection >= ac.view + AUTOCOMPLETE_WINDOW_ITEMS)
        ac.view = ac.selection - AUTOCOMPLETE_WINDOW_ITEMS + 1;
    if (ac.selection < ac.view)
        ac.view = ac.selection;
    return true;
}

void Build::cleanup() {
    if (thread) {
        kill_thread(thread);
        close_thread_handle(thread);
    }

    For (&errors) {
        if (it.mark) {
            it.mark->cleanup();
            it.mark = NULL;
        }
    }
    errors.len = 0;
    mem.cleanup();
}

bool has_unsaved_files() {
    For (get_all_editors())
        if (it->is_unsaved())
            return true;
    return false;
}

bool handle_unsaved_files() {
    if (!has_unsaved_files()) return true;

    auto msg = "This operation requires all unsaved files first be saved. Do you want to do that?";
    auto result = ask_user_yes_no(msg, "Unsaved files", "Yes", "No");
    if (result != ASKUSER_YES) return false;

    save_all_unsaved_files();
    return true;
}


void rename_identifier_thread(void *param) {
    auto &wnd = world.wnd_rename_identifier;
    wnd.thread_mem.cleanup();
    wnd.thread_mem.init("rename_identifier_thread");
    SCOPED_MEM(&wnd.thread_mem);

    defer {
        cancel_rename_identifier();
        wnd.show = false;
        world.flag_defocus_imgui = true;
    };

    auto files = world.indexer.find_references(wnd.declres, true);
    if (!files) return;

    wnd.too_late_to_cancel = true;

    auto symbol = wnd.declres->decl->name;
    auto symbol_len = strlen(symbol);

    For (files) {
        auto filepath = it.filepath;

        File_Replacer fr;
        if (!fr.init(filepath, "refactor_rename")) continue;

        For (it.results) {
            if (fr.done()) break;

            auto ref = it.reference;

            cur2 start, end;
            if (ref->is_sel) {
                start = ref->sel_start;
                end = ref->sel_end;
            } else {
                start = ref->start;
                end = ref->end;
            }

            // i think this is a safe assumption?
            if (start.y != end.y) continue;

            if (end.x - start.x != symbol_len) continue;

            fr.goto_next_replacement(start);

            bool matches = true;
            for (int i = 0; i < symbol_len; i++) {
                if (symbol[i] != fr.fmr->data[fr.read_pointer + i]) {
                    matches = false;
                    break;
                }
            }
            if (!matches) continue;

            fr.do_replacement(end, wnd.rename_to);
        }

        fr.finish();

        auto editor = find_editor_by_filepath(filepath);
        if (editor) {
            world.message_queue.add([&](auto msg) {
                msg->type = MTM_RELOAD_EDITOR;
                msg->reload_editor_id = editor->id;
            });
            editor->disable_file_watcher_until = current_time_nano() + (2 * 1000000000);
        }
    }

    // close the thread handle first so it doesn't try to kill the thread
    if (wnd.thread) {
        close_thread_handle(wnd.thread);
        wnd.thread = NULL;
    }
}

void kick_off_rename_identifier() {
    bool ok = false;

    auto &ind = world.indexer;
    if (!ind.acquire_lock(IND_READING)) {
        tell_user_error("The indexer is currently busy.");
        return;
    }

    defer { if (!ok) ind.release_lock(IND_READING); };

    if (!handle_unsaved_files()) return;

    ind.reload_all_editors();

    auto &wnd = world.wnd_rename_identifier;
    wnd.running = true;
    wnd.too_late_to_cancel = false;
    wnd.thread = create_thread(rename_identifier_thread, NULL);
    if (!wnd.thread) {
        tell_user_error("Unable to kick off Rename Identifier.");
        return;
    }

    ok = true;
}

void cancel_find_interfaces() {
    auto &wnd = world.wnd_find_interfaces;

    if (wnd.thread) {
        kill_thread(wnd.thread);
        close_thread_handle(wnd.thread);
        wnd.thread = NULL;
    }

    // assume it was acquired by find_interfaces
    if (world.indexer.status == IND_READING)
        world.indexer.release_lock(IND_READING);

    wnd.done = true;
}

void cancel_find_references() {
    auto &wnd = world.wnd_find_references;

    if (wnd.thread) {
        kill_thread(wnd.thread);
        close_thread_handle(wnd.thread);
        wnd.thread = NULL;
    }

    // assume it was acquired by find_references
    if (world.indexer.status == IND_READING)
        world.indexer.release_lock(IND_READING);

    wnd.done = true;
}

void cancel_caller_hierarchy() {
    auto &wnd = world.wnd_caller_hierarchy;

    if (wnd.thread) {
        kill_thread(wnd.thread);
        close_thread_handle(wnd.thread);
        wnd.thread = NULL;
    }

    // assume it was acquired by find_implementations
    if (world.indexer.status == IND_READING)
        world.indexer.release_lock(IND_READING);

    wnd.done = true;
}

void cancel_callee_hierarchy() {
    auto &wnd = world.wnd_callee_hierarchy;

    if (wnd.thread) {
        kill_thread(wnd.thread);
        close_thread_handle(wnd.thread);
        wnd.thread = NULL;
    }

    // assume it was acquired by find_implementations
    if (world.indexer.status == IND_READING)
        world.indexer.release_lock(IND_READING);

    wnd.done = true;
}

void cancel_find_implementations() {
    auto &wnd = world.wnd_find_implementations;

    if (wnd.thread) {
        kill_thread(wnd.thread);
        close_thread_handle(wnd.thread);
        wnd.thread = NULL;
    }

    // assume it was acquired by find_implementations
    if (world.indexer.status == IND_READING)
        world.indexer.release_lock(IND_READING);

    wnd.done = true;
}

void cancel_rename_identifier() {
    auto &wnd = world.wnd_rename_identifier;

    if (wnd.thread) {
        kill_thread(wnd.thread);
        close_thread_handle(wnd.thread);
        wnd.thread = NULL;
    }

    // assume it was acquired by rename_identifier
    if (world.indexer.status == IND_READING)
        world.indexer.release_lock(IND_READING);

    wnd.running = false;
}

Command_Info command_info_table[_CMD_COUNT_];

bool is_command_enabled(Command cmd) {
    switch (cmd) {
    case CMD_RENAME: {
        auto editor = get_current_editor();
        if (!editor) return false;
        if (!path_has_descendant(world.current_path, editor->filepath)) return false;
        if (editor->lang != LANG_GO) return false;
        return true;
    }

    case CMD_GO_TO_NEXT_EDITOR:
    case CMD_GO_TO_PREVIOUS_EDITOR: {
        auto pane = get_current_pane();
        return pane && pane->editors.len > 1;
    }

    case CMD_FIND_NEXT:
    case CMD_FIND_PREVIOUS: {
        auto editor = get_current_editor();
        return editor && editor->buf->search_tree->get_size();
    }

    case CMD_GO_TO_NEXT_SEARCH_RESULT:
    case CMD_GO_TO_PREVIOUS_SEARCH_RESULT:
        if (world.searcher.mt_state.type == SEARCH_SEARCH_DONE)
            if (world.searcher.mt_state.results->len)
                return true;
        return false;

    case CMD_ZOOM_ORIGINAL:
        return options.zoom_level != 100;

    case CMD_ZOOM_IN:
        return options.zoom_level < ZOOM_LEVELS[ZOOM_LEVELS_COUNT-1];

    case CMD_ZOOM_OUT:
        return options.zoom_level > ZOOM_LEVELS[0];

    case CMD_CLOSE_ALL_EDITORS: {
        if (is_imgui_hogging_keyboard()) return false;

        if (world.panes.len > 1) return true;
        auto pane = get_current_pane();
        if (!pane) return false;
        return pane->editors.len;
    }

    case CMD_CLOSE_EDITOR: {
        if (is_imgui_hogging_keyboard()) return false;

        auto pane = get_current_pane();
        if (!pane) return false;

        auto editor = get_current_editor();
        if (editor) return true;

        return world.panes.len > 1;
    }

    case CMD_OPEN_LAST_CLOSED_EDITOR:
        return world.last_closed->len;

    case CMD_COMMAND_PALETTE:
        return !world.wnd_command.show;

    case CMD_BUILD_PROFILE_1:
    case CMD_BUILD_PROFILE_2:
    case CMD_BUILD_PROFILE_3:
    case CMD_BUILD_PROFILE_4:
    case CMD_BUILD_PROFILE_5:
    case CMD_BUILD_PROFILE_6:
    case CMD_BUILD_PROFILE_7:
    case CMD_BUILD_PROFILE_8:
    case CMD_BUILD_PROFILE_9:
    case CMD_BUILD_PROFILE_10:
    case CMD_BUILD_PROFILE_11:
    case CMD_BUILD_PROFILE_12:
    case CMD_BUILD_PROFILE_13:
    case CMD_BUILD_PROFILE_14:
    case CMD_BUILD_PROFILE_15:
    case CMD_BUILD_PROFILE_16: {
        int index = (int)cmd - CMD_BUILD_PROFILE_1;
        return index < project_settings.build_profiles->len;
    }

    case CMD_DEBUG_PROFILE_1:
    case CMD_DEBUG_PROFILE_2:
    case CMD_DEBUG_PROFILE_3:
    case CMD_DEBUG_PROFILE_4:
    case CMD_DEBUG_PROFILE_5:
    case CMD_DEBUG_PROFILE_6:
    case CMD_DEBUG_PROFILE_7:
    case CMD_DEBUG_PROFILE_8:
    case CMD_DEBUG_PROFILE_9:
    case CMD_DEBUG_PROFILE_10:
    case CMD_DEBUG_PROFILE_11:
    case CMD_DEBUG_PROFILE_12:
    case CMD_DEBUG_PROFILE_13:
    case CMD_DEBUG_PROFILE_14:
    case CMD_DEBUG_PROFILE_15:
    case CMD_DEBUG_PROFILE_16: {
        int index = (int)cmd - CMD_DEBUG_PROFILE_1;
        return index < project_settings.debug_profiles->len;
    }

    case CMD_AST_NAVIGATION: {
        auto editor = get_current_editor();
        return editor && editor->lang == LANG_GO && editor->buf->tree;
    }

    case CMD_GO_FORWARD:
        return world.history.curr != world.history.top;
    case CMD_GO_BACK:
        return world.history.curr != world.history.start;

    case CMD_FIND:
        return (bool)get_current_editor();

    case CMD_ADD_JSON_TAG:
    case CMD_ADD_YAML_TAG:
    case CMD_ADD_XML_TAG:
    case CMD_ADD_ALL_JSON_TAGS:
    case CMD_ADD_ALL_YAML_TAGS:
    case CMD_ADD_ALL_XML_TAGS:
    case CMD_REMOVE_TAG:
    case CMD_REMOVE_ALL_TAGS: {
        auto editor = get_current_editor();
        if (!editor) return false;
        if (editor->lang != LANG_GO) return false;

        if (!path_has_descendant(world.current_path, editor->filepath)) return false;
        if (!editor->buf->tree) return false;

        bool ret = false;

        Parser_It it;
        it.init(editor->buf);
        auto root_node = new_ast_node(ts_tree_root_node(editor->buf->tree), &it);

        find_nodes_containing_pos(root_node, editor->cur, true, [&](auto it) -> Walk_Action {
            if (it->type() == TS_STRUCT_TYPE) {
                ret = true;
                return WALK_ABORT;
            }
            return WALK_CONTINUE;
        });

        return ret;
    }

    case CMD_REPLACE:
    case CMD_TOGGLE_COMMENT:
    case CMD_SAVE_FILE:
    case CMD_FORMAT_FILE:
    case CMD_ORGANIZE_IMPORTS: {
        auto editor = get_current_editor();
        return editor && editor->is_modifiable();
    }

    case CMD_SAVE_ALL:
        For (get_all_editors())
            if (it->is_modifiable())
                return true;
        return false;

    case CMD_GO_TO_PREVIOUS_ERROR:
    case CMD_GO_TO_NEXT_ERROR: {
        auto &b = world.build;
        bool has_valid = false;
        For (&b.errors) {
            if (it.valid) {
                has_valid = true;
                break;
            }
        }
        return b.ready() && has_valid;
    }

    case CMD_RESCAN_INDEX:
        return world.indexer.status == IND_READY;

    case CMD_START_DEBUGGING:
        return world.dbg.mt_state.state_flag == DLV_STATE_INACTIVE;

    case CMD_CONTINUE:
        return world.dbg.mt_state.state_flag == DLV_STATE_PAUSED;

    case CMD_FORMAT_SELECTION:
        return false;

    case CMD_DEBUG_TEST_UNDER_CURSOR: {
        if (world.dbg.mt_state.state_flag != DLV_STATE_INACTIVE) return false;

        auto editor = get_current_editor();
        if (!editor) return false;
        if (editor->lang != LANG_GO) return false;
        if (!str_ends_with(editor->filepath, "_test.go")) return false;
        if (!path_has_descendant(world.current_path, editor->filepath)) return false;
        if (!editor->buf->tree) return false;

        bool ret = false;

        Parser_It it;
        it.init(editor->buf);
        auto root_node = new_ast_node(ts_tree_root_node(editor->buf->tree), &it);

        find_nodes_containing_pos(root_node, editor->cur, true, [&](auto it) -> Walk_Action {
            if (it->type() == TS_SOURCE_FILE)
                return WALK_CONTINUE;

            if (it->type() == TS_FUNCTION_DECLARATION) {
                auto name = it->field(TSF_NAME);
                if (name)
                    ret = str_starts_with(name->string(), "Test");
            }

            return WALK_ABORT;
        });

        return ret;
    }

    case CMD_BREAK_ALL:
        return world.dbg.mt_state.state_flag == DLV_STATE_RUNNING;

    case CMD_STOP_DEBUGGING:
        return world.dbg.mt_state.state_flag != DLV_STATE_INACTIVE;

    case CMD_STEP_OVER:
    case CMD_STEP_INTO:
    case CMD_STEP_OUT:
        return world.dbg.mt_state.state_flag == DLV_STATE_PAUSED;

    case CMD_UNDO:
    case CMD_REDO:
    case CMD_PASTE:
    case CMD_SELECT_ALL:
        // TODO: also check if we *can* undo/redo (to be done after we actually implement it)
        return get_current_editor();

    case CMD_CUT:
    case CMD_COPY: {
        auto editor = get_current_editor();
        return editor && (editor->selecting || world.vim_mode() == VI_VISUAL);
    }

    case CMD_GENERATE_IMPLEMENTATION:
    case CMD_FIND_REFERENCES:
    case CMD_FIND_IMPLEMENTATIONS:
    case CMD_FIND_INTERFACES:
    case CMD_VIEW_CALLER_HIERARCHY:
    case CMD_GENERATE_FUNCTION:
        return get_current_editor();
    }

    return true;
}

ccstr get_command_name(Command cmd) {
    auto info = command_info_table[cmd];

    switch (cmd) {
    case CMD_CLOSE_EDITOR: {
        auto editor = get_current_editor();
        return editor ? "Close Editor" : "Close Pane";
    }

    case CMD_AST_NAVIGATION: {
        auto editor = get_current_editor();
        if (!editor) break;

        if (editor->ast_navigation.on)
            return "Leave Tree-Based Navigation";
        return "Enter Tree-Based Navigation";
    }

    case CMD_BUILD_PROFILE_1:
    case CMD_BUILD_PROFILE_2:
    case CMD_BUILD_PROFILE_3:
    case CMD_BUILD_PROFILE_4:
    case CMD_BUILD_PROFILE_5:
    case CMD_BUILD_PROFILE_6:
    case CMD_BUILD_PROFILE_7:
    case CMD_BUILD_PROFILE_8:
    case CMD_BUILD_PROFILE_9:
    case CMD_BUILD_PROFILE_10:
    case CMD_BUILD_PROFILE_11:
    case CMD_BUILD_PROFILE_12:
    case CMD_BUILD_PROFILE_13:
    case CMD_BUILD_PROFILE_14:
    case CMD_BUILD_PROFILE_15:
    case CMD_BUILD_PROFILE_16: {
        int index = (int)cmd - CMD_BUILD_PROFILE_1;
        if (index >= project_settings.build_profiles->len) break; // shouldn't happen
	    auto profile = project_settings.build_profiles->at(index);
        return cp_sprintf("Build: %s", profile.label);
    }

    case CMD_DEBUG_PROFILE_1:
    case CMD_DEBUG_PROFILE_2:
    case CMD_DEBUG_PROFILE_3:
    case CMD_DEBUG_PROFILE_4:
    case CMD_DEBUG_PROFILE_5:
    case CMD_DEBUG_PROFILE_6:
    case CMD_DEBUG_PROFILE_7:
    case CMD_DEBUG_PROFILE_8:
    case CMD_DEBUG_PROFILE_9:
    case CMD_DEBUG_PROFILE_10:
    case CMD_DEBUG_PROFILE_11:
    case CMD_DEBUG_PROFILE_12:
    case CMD_DEBUG_PROFILE_13:
    case CMD_DEBUG_PROFILE_14:
    case CMD_DEBUG_PROFILE_15:
    case CMD_DEBUG_PROFILE_16: {
        int index = (int)cmd - CMD_DEBUG_PROFILE_1;
        if (index >= project_settings.debug_profiles->len) break;
	    auto profile = project_settings.debug_profiles->at(index);
        return cp_sprintf("Start Debugging: %s", profile.label);
    }

    case CMD_SAVE_FILE: {
        auto editor = get_current_editor();
        if (editor) {
            if (editor->is_untitled)
                return "Save untitled file...";
            return cp_sprintf("Save %s...", cp_basename(editor->filepath));
        }
        return "Save file...";
    }
    }

    return info.name;
}

void init_command_info_table() {
    auto add_shortcut = [&](Command_Info *info, int mods, int key) {
        SCOPED_MEM(&world.world_mem); // will need to change this when we make shortcuts customizable

        auto sc = new_object(Command_Shortcut);
        sc->key = key;
        sc->mods = mods;

        sc->next = info->shortcuts;
        info->shortcuts = sc;
    };

    auto k = [&](int mods, int key, ccstr name, bool allow_shortcut_when_imgui_focused = false) {
        Command_Info ret; ptr0(&ret);
        ret.name = name;
        ret.allow_shortcut_when_imgui_focused = allow_shortcut_when_imgui_focused;
        if (mods || key) add_shortcut(&ret, mods, key);
        return ret;
    };

    mem0(command_info_table, sizeof(command_info_table));

    command_info_table[CMD_EXIT] = k(CP_MOD_CMD, CP_KEY_Q, "Quit");
    command_info_table[CMD_NEW_FILE] = k(CP_MOD_PRIMARY, CP_KEY_N, "New File");
    command_info_table[CMD_SAVE_FILE] = k(CP_MOD_PRIMARY, CP_KEY_S, "Save File");
    command_info_table[CMD_SAVE_ALL] = k(CP_MOD_PRIMARY | CP_MOD_SHIFT, CP_KEY_S, "Save All");
    command_info_table[CMD_SEARCH] = k(CP_MOD_PRIMARY | CP_MOD_SHIFT, CP_KEY_F, "Search...", true);
    command_info_table[CMD_SEARCH_AND_REPLACE] = k(CP_MOD_PRIMARY | CP_MOD_SHIFT, CP_KEY_H, "Search and Replace...", true);
    command_info_table[CMD_FILE_EXPLORER] = k(CP_MOD_PRIMARY | CP_MOD_SHIFT, CP_KEY_E, "File Explorer", true);
    command_info_table[CMD_GO_TO_FILE] = k(CP_MOD_PRIMARY, CP_KEY_P, "Go To File", true);
    command_info_table[CMD_GO_TO_SYMBOL] = k(CP_MOD_PRIMARY, CP_KEY_T, "Go To Symbol", true);
    command_info_table[CMD_GO_TO_NEXT_ERROR] = k(CP_MOD_ALT, CP_KEY_RIGHT_BRACKET, "Go To Next Error");
    command_info_table[CMD_GO_TO_PREVIOUS_ERROR] = k(CP_MOD_ALT, CP_KEY_LEFT_BRACKET, "Go To Previous Error");
    command_info_table[CMD_GO_TO_DEFINITION] = k(CP_MOD_PRIMARY, CP_KEY_G, "Go To Definition");

    command_info_table[CMD_GO_TO_NEXT_EDITOR] = k(CP_MOD_CTRL, CP_KEY_TAB, "Go To Next Editor");
    command_info_table[CMD_GO_TO_PREVIOUS_EDITOR] = k(CP_MOD_CTRL | CP_MOD_SHIFT, CP_KEY_TAB, "Go To Previous Editor");

    add_shortcut(&command_info_table[CMD_GO_TO_NEXT_EDITOR], CP_MOD_CMD | CP_MOD_SHIFT, CP_KEY_RIGHT_BRACKET);
    add_shortcut(&command_info_table[CMD_GO_TO_PREVIOUS_EDITOR], CP_MOD_CMD | CP_MOD_SHIFT, CP_KEY_LEFT_BRACKET);

    command_info_table[CMD_FIND_NEXT] = k(0, CP_KEY_F3, "Find: Go To Next", true);
    command_info_table[CMD_FIND_PREVIOUS] = k(CP_MOD_SHIFT, CP_KEY_F3, "Find: Go To Previous", true);
    command_info_table[CMD_FIND_CLEAR] = k(CP_MOD_CTRL, CP_KEY_SLASH, "Find: Clear");
    command_info_table[CMD_GO_TO_NEXT_SEARCH_RESULT] = k(0, CP_KEY_F4, "Search: Go To Next", true);
    command_info_table[CMD_GO_TO_PREVIOUS_SEARCH_RESULT] = k(CP_MOD_SHIFT, CP_KEY_F4, "Search: Go To Previous", true);

    command_info_table[CMD_FIND_REFERENCES] = k(CP_MOD_PRIMARY | CP_MOD_ALT, CP_KEY_R, "Find References");
    command_info_table[CMD_FORMAT_FILE] = k(CP_MOD_ALT | CP_MOD_SHIFT, CP_KEY_F, "Format File");
    command_info_table[CMD_ORGANIZE_IMPORTS] = k(CP_MOD_ALT | CP_MOD_SHIFT, CP_KEY_O, "Organize Imports");
    command_info_table[CMD_RENAME] = k(CP_MOD_NONE, CP_KEY_F12, "Rename");
    command_info_table[CMD_BUILD] = k(CP_MOD_PRIMARY | CP_MOD_SHIFT, CP_KEY_B, "Build", true);
    command_info_table[CMD_CONTINUE] = k(CP_MOD_NONE, CP_KEY_F5, "Continue", true);
    command_info_table[CMD_START_DEBUGGING] = k(CP_MOD_NONE, CP_KEY_F5, "Start Debugging", true);
    command_info_table[CMD_STOP_DEBUGGING] = k(CP_MOD_SHIFT, CP_KEY_F5, "Stop Debugging", true);
    command_info_table[CMD_DEBUG_TEST_UNDER_CURSOR] = k(CP_MOD_NONE, CP_KEY_F6, "Debug Test Under Cursor");
    command_info_table[CMD_STEP_OVER] = k(CP_MOD_NONE, CP_KEY_F10, "Step Over", true);
    command_info_table[CMD_STEP_INTO] = k(CP_MOD_NONE, CP_KEY_F11, "Step Into", true);
    command_info_table[CMD_STEP_OUT] = k(CP_MOD_SHIFT, CP_KEY_F11, "Step Out", true);
    command_info_table[CMD_RUN_TO_CURSOR] = k(CP_MOD_SHIFT, CP_KEY_F10, "Run To Cursor", true);
    command_info_table[CMD_TOGGLE_BREAKPOINT] = k(CP_MOD_NONE, CP_KEY_F9, "Toggle Breakpoint");
    command_info_table[CMD_DELETE_ALL_BREAKPOINTS] = k(CP_MOD_SHIFT, CP_KEY_F9, "Delete All Breakpoints");
    command_info_table[CMD_REPLACE] = k(CP_MOD_PRIMARY, CP_KEY_H, "Replace...");
    command_info_table[CMD_FIND] = k(CP_MOD_PRIMARY, CP_KEY_F, "Find...");
    command_info_table[CMD_OPTIONS] = k(CP_MOD_PRIMARY, CP_KEY_COMMA, "Options");
    command_info_table[CMD_TOGGLE_COMMENT] = k(CP_MOD_PRIMARY | CP_MOD_ALT, CP_KEY_SLASH, "Toggle Comment");
    command_info_table[CMD_UNDO] = k(CP_MOD_PRIMARY, CP_KEY_Z, "Undo");
    command_info_table[CMD_REDO] = k(CP_MOD_PRIMARY | CP_MOD_SHIFT, CP_KEY_Z, "Redo");
    command_info_table[CMD_CUT] = k(CP_MOD_PRIMARY, CP_KEY_X, "Cut");
    command_info_table[CMD_COPY] = k(CP_MOD_PRIMARY, CP_KEY_C, "Copy");
    command_info_table[CMD_PASTE] = k(CP_MOD_PRIMARY, CP_KEY_V, "Paste");
    command_info_table[CMD_SELECT_ALL] = k(CP_MOD_PRIMARY, CP_KEY_A, "Select All");
    command_info_table[CMD_VIEW_CALLER_HIERARCHY] = k(CP_MOD_PRIMARY, CP_KEY_I, "View Caller Hierarchy");
    command_info_table[CMD_VIEW_CALLEE_HIERARCHY] = k(CP_MOD_PRIMARY | CP_MOD_SHIFT, CP_KEY_I, "View Callee Hierarchy");
    command_info_table[CMD_AST_NAVIGATION] = k(CP_MOD_CTRL | CP_MOD_ALT, CP_KEY_A, "Enter Tree-Based Navigation");
    /**/
    command_info_table[CMD_ERROR_LIST] = k(0, 0, "Error List");
    command_info_table[CMD_FORMAT_SELECTION] = k(0, 0, "Format Selection");
    command_info_table[CMD_ADD_NEW_FILE] = k(0, 0, "Add New File");
    command_info_table[CMD_ADD_NEW_FOLDER] = k(0, 0, "Add New Folder");
    command_info_table[CMD_PROJECT_SETTINGS] = k(0, 0, "Project Settings");
    command_info_table[CMD_BUILD_RESULTS] = k(0, 0, "Build Results");
    command_info_table[CMD_BUILD_PROFILES] = k(0, 0, "Build Profiles");
    command_info_table[CMD_BREAK_ALL] = k(0, 0, "Break All");
    command_info_table[CMD_RUN_TO_CURSOR] = k(0, 0, "Run To Cursor");
    command_info_table[CMD_DEBUG_OUTPUT] = k(0, 0, "Debug Output");
    command_info_table[CMD_DEBUG_PROFILES] = k(0, 0, "Debug Profiles");
    command_info_table[CMD_RESCAN_INDEX] = k(0, 0, "Rescan Index");
    command_info_table[CMD_OBLITERATE_AND_RECREATE_INDEX] = k(0, 0, "Obliterate and Recreate Index");
    command_info_table[CMD_GENERATE_IMPLEMENTATION] = k(0, 0, "Generate Implementation");
    command_info_table[CMD_GENERATE_FUNCTION] = k(0, 0, "[Experimental] Generate Function From Call");
    command_info_table[CMD_FIND_IMPLEMENTATIONS] = k(0, 0, "Find Implementations");
    command_info_table[CMD_FIND_INTERFACES] = k(0, 0, "Find Interfaces");
    command_info_table[CMD_DOCUMENTATION] = k(0, 0, "Documentation");
    command_info_table[CMD_ADD_JSON_TAG] = k(0, 0, "Struct: Add JSON tag");
    command_info_table[CMD_ADD_YAML_TAG] = k(0, 0, "Struct: Add YAML tag");
    command_info_table[CMD_ADD_XML_TAG] = k(0, 0, "Struct: Add XML tag");
    command_info_table[CMD_ADD_ALL_JSON_TAGS] = k(0, 0, "Struct: Add all JSON tags");
    command_info_table[CMD_ADD_ALL_YAML_TAGS] = k(0, 0, "Struct: Add all YAML tags");
    command_info_table[CMD_ADD_ALL_XML_TAGS] = k(0, 0, "Struct: Add all XML tags");
    command_info_table[CMD_REMOVE_TAG] = k(0, 0, "Struct: Remove tag");
    command_info_table[CMD_REMOVE_ALL_TAGS] = k(0, 0, "Struct: Remove all tags");
    command_info_table[CMD_COMMAND_PALETTE] = k(CP_MOD_PRIMARY, CP_KEY_K, "Command Palette", true);
    command_info_table[CMD_OPEN_FILE_MANUALLY] = k(CP_MOD_PRIMARY, CP_KEY_O, "Open File...", true);
    command_info_table[CMD_CLOSE_EDITOR] = k(CP_MOD_PRIMARY, CP_KEY_W, "Close Editor");
    command_info_table[CMD_CLOSE_ALL_EDITORS] = k(CP_MOD_PRIMARY | CP_MOD_SHIFT, CP_KEY_W, "Close All Editors");
    command_info_table[CMD_OPEN_LAST_CLOSED_EDITOR] = k(CP_MOD_PRIMARY | CP_MOD_SHIFT, CP_KEY_T, "Open Last Closed Editor");
    command_info_table[CMD_OPEN_FOLDER] = k(CP_MOD_PRIMARY | CP_MOD_SHIFT, CP_KEY_O, "Open Folder...", true);
    command_info_table[CMD_ZOOM_IN] = k(CP_MOD_PRIMARY, CP_KEY_EQUAL, "Zoom In", true);
    command_info_table[CMD_ZOOM_OUT] = k(CP_MOD_PRIMARY, CP_KEY_MINUS, "Zoom Out", true);
    command_info_table[CMD_ZOOM_ORIGINAL] = k(CP_MOD_PRIMARY, CP_KEY_0, "Original Size", true);
    command_info_table[CMD_GO_BACK] = k(CP_MOD_CTRL, CP_KEY_MINUS, "Go Back");
    command_info_table[CMD_GO_FORWARD] = k(CP_MOD_CTRL, CP_KEY_EQUAL, "Go Forward");
}

void do_find_interfaces() {
    auto &wnd = world.wnd_find_interfaces;

    if (wnd.show && !wnd.done) return; // already in progress

    auto &ind = world.indexer;
    if (!ind.acquire_lock(IND_READING)) {
        tell_user_error("The indexer is currently busy.");
        return;
    }

    ind.reload_all_editors();

    auto thread_proc = [](void *param) {
        auto &wnd = world.wnd_find_interfaces;
        wnd.thread_mem.cleanup();
        wnd.thread_mem.init("find_interfaces_thread");
        SCOPED_MEM(&wnd.thread_mem);

        defer { cancel_find_interfaces(); };

        // TODO: how do we handle errors?
        // right now it just freezes on "Searching..."
        auto results = ind.find_interfaces(wnd.declres, wnd.search_everywhere);
        if (!results) return;

        {
            SCOPED_MEM(&world.find_interfaces_mem);

            auto newresults = new_list(Find_Decl*, results->len);
            For (results) newresults->append(it.copy());

            wnd.results = newresults;
            wnd.workspace = ind.index.workspace->copy();
        }

        // close the thread handle first so it doesn't try to kill the thread
        if (wnd.thread) {
            close_thread_handle(wnd.thread);
            wnd.thread = NULL;
        }
    };

    if (wnd.show)
        wnd.cmd_focus = true;
    else
        wnd.show = true;
    wnd.done = false;
    wnd.results = NULL;

    wnd.thread = create_thread(thread_proc, NULL);
    if (!wnd.thread) {
        tell_user_error("Unable to kick off Find Interfaces.");
        return;
    }
}

void init_goto_symbol() {
    auto &wnd = world.wnd_goto_symbol;

    wnd.show = true;

    if (!world.indexer.acquire_lock(IND_READING, true)) {
        wnd.state = GOTO_SYMBOL_WAITING;
        return;
    }

    if (!wnd.fill_thread)
        kill_thread(wnd.fill_thread);

    wnd.query[0] = '\0';
    wnd.symbols = NULL;
    wnd.filtered_results = NULL;

    world.indexer.reload_all_editors();

    auto worker = [](void*) {
        defer { world.indexer.release_lock(IND_READING); };

        auto &wnd = world.wnd_goto_symbol;

        SCOPED_MEM(&wnd.fill_thread_pool);

        auto symbols = new_list(Go_Symbol);
        world.indexer.fill_goto_symbol(symbols);
        if (!symbols->len) return;

        {
            SCOPED_MEM(&world.goto_symbol_mem);
            wnd.symbols = new_list(Go_Symbol, symbols->len);
            For (symbols) wnd.symbols->append(it.copy());

            wnd.filtered_results = new_list(int);
            wnd.workspace = world.indexer.index.workspace->copy();
        }

        wnd.state = GOTO_SYMBOL_READY;

        if (wnd.fill_thread) {
            close_thread_handle(wnd.fill_thread);
            wnd.fill_thread = NULL;
        }
    };

    wnd.fill_thread_pool.cleanup();
    wnd.fill_thread_pool.init("goto_symbol_thread");

    wnd.fill_time_started_ms = current_time_milli();
    wnd.fill_thread = create_thread(worker, NULL);

    if (!wnd.fill_thread) {
        world.indexer.release_lock(IND_READING);
        wnd.state = GOTO_SYMBOL_ERROR;
    } else {
        wnd.state = GOTO_SYMBOL_RUNNING;
    }
}

void do_find_implementations() {
    auto &wnd = world.wnd_find_implementations;

    auto &ind = world.indexer;
    if (!ind.acquire_lock(IND_READING)) {
        tell_user_error("The indexer is currently busy.");
        return;
    }

    ind.reload_all_editors();

    auto thread_proc = [](void *param) {
        auto &wnd = world.wnd_find_implementations;
        wnd.thread_mem.cleanup();
        wnd.thread_mem.init("find_implementations_thread");
        SCOPED_MEM(&wnd.thread_mem);

        defer { cancel_find_implementations(); };

        auto &ind = world.indexer;
        auto results = ind.find_implementations(wnd.declres, wnd.search_everywhere);
        if (!results) return;

        {
            SCOPED_MEM(&world.find_implementations_mem);

            auto newresults = new_list(Find_Decl*, results->len);
            For (results) newresults->append(it.copy());

            wnd.results = newresults;
            wnd.workspace = ind.index.workspace->copy();
        }

        // close the thread handle first so it doesn't try to kill the thread
        if (wnd.thread) {
            close_thread_handle(wnd.thread);
            wnd.thread = NULL;
        }
    };

    if (wnd.show)
        wnd.cmd_focus = true;
    else
        wnd.show = true;
    wnd.done = false;
    wnd.results = NULL;

    wnd.thread = create_thread(thread_proc, NULL);
    if (!wnd.thread) {
        tell_user_error("Unable to kick off Find Implementations.");
        return;
    }
}

bool initiate_rename_identifier(cur2 pos) {
    // TODO: this should be a "blocking" modal, like it disables activity in rest of IDE
    auto result = get_current_definition(NULL, true, pos);
    if (!result || !result->decl) return false;
    if (result->decl->decl->type == GODECL_IMPORT) {
        tell_user_error("Sorry, we're currently not yet able to rename imports.");
        return false;
    }

    if (!path_has_descendant(world.current_path, result->file)) {
        tell_user_error("Sorry, we're unable to rename things outside your project.");
        return false;
    }

    world.rename_identifier_mem.reset();
    SCOPED_MEM(&world.rename_identifier_mem);

    auto &wnd = world.wnd_rename_identifier;
    wnd.rename_to[0] = '\0';
    wnd.show = true;
    wnd.cmd_focus = true;
    wnd.running = false;
    wnd.declres = result->decl->copy_decl();
    return true;
}

void initiate_find_references(cur2 pos) {
    auto &wnd = world.wnd_find_references;

    auto result = get_current_definition(NULL, true, pos);
    if (!result) return;
    if (!result->decl) return;

    world.find_references_mem.reset();
    {
        SCOPED_MEM(&world.find_references_mem);
        wnd.declres = result->decl->copy_decl();
    }

    auto &ind = world.indexer;
    if (!ind.acquire_lock(IND_READING)) {
        tell_user_error("The indexer is currently busy.");
        return;
    }

    ind.reload_all_editors();

    auto thread_proc = [](void *param) {
        auto &wnd = world.wnd_find_references;
        wnd.thread_mem.cleanup();
        wnd.thread_mem.init("find_references_thread");
        SCOPED_MEM(&wnd.thread_mem);

        // kinda confusing that cancel_find_references is responsible for wnd.done = true
        defer { cancel_find_references(); };

        auto files = world.indexer.find_references(wnd.declres, false);
        if (isempty(files)) return;

        wnd.current_file = 0;
        wnd.current_result = 0;

        {
            SCOPED_MEM(&world.find_references_mem);

            auto newfiles = new_list(Find_References_File, files->len);
            For (files) newfiles->append(it.copy());

            wnd.results = newfiles;
            wnd.workspace = world.indexer.index.workspace->copy();
        }

        // close the thread handle first so it doesn't try to kill the thread
        if (wnd.thread) {
            close_thread_handle(wnd.thread);
            wnd.thread = NULL;
        }
    };

    if (wnd.show)
        wnd.cmd_focus = true;
    else
        wnd.show = true;
    wnd.done = false;
    wnd.results = NULL;
    wnd.thread = create_thread(thread_proc, NULL);
    wnd.current_file = -1;
    wnd.current_result = -1;
    wnd.scroll_to_file = -1;
    wnd.scroll_to_result = -1;

    if (!wnd.thread)
        tell_user_error("Unable to kick off Find References.");
}

void handle_command(Command cmd, bool from_menu) {
    // make this a precondition (actually, I think it might already be)
    if (!is_command_enabled(cmd)) return;

    switch (cmd) {
    case CMD_GO_TO_NEXT_EDITOR: {
        auto pane = get_current_pane();
        if (!pane->editors.len) break;

        auto idx = (pane->current_editor + 1) % pane->editors.len;
        pane->focus_editor_by_index(idx);
        break;
    }

    case CMD_GO_TO_PREVIOUS_EDITOR: {
        auto pane = get_current_pane();
        if (!pane->editors.len) break;

        u32 idx;
        if (!pane->current_editor)
            idx = pane->editors.len - 1;
        else
            idx = pane->current_editor - 1;
        pane->focus_editor_by_index(idx);
        break;
    }

    case CMD_FIND_CLEAR:
        For (get_all_editors()) it->reset_search_results();
        world.wnd_local_search.query[0] = '\0';
        world.wnd_local_search.permanent_query[0] = '\0';
        break;

    case CMD_ZOOM_ORIGINAL:
        set_zoom_level(100);
        break;

    case CMD_ZOOM_IN:
    case CMD_ZOOM_OUT: {
        int idx = -1;
        for (int i = 0; i < ZOOM_LEVELS_COUNT; i++) {
            if (options.zoom_level == ZOOM_LEVELS[i]) {
                idx = i;
                break;
            }
        }

        int new_level = 100;
        if (idx != -1) {
            if (cmd == CMD_ZOOM_IN) {
                if (idx == ZOOM_LEVELS_COUNT-1) return;
                new_level = ZOOM_LEVELS[idx+1];
            } else {
                if (idx == 0) return;
                new_level = ZOOM_LEVELS[idx-1];
            }
        }
        set_zoom_level(new_level);
        break;
    }

    case CMD_OPEN_FOLDER: {
        auto buf = new_array(char, MAX_PATH);

        Select_File_Opts opts; ptr0(&opts);
        opts.buf = buf;
        opts.bufsize = MAX_PATH;
        opts.folder = true;
        opts.save = false;

        if (!let_user_select_file(&opts)) break;

        auto args = new_list(char*);
        args->append(buf);
        fork_self(args, false);
        break;
    }

    case CMD_OPEN_FILE_MANUALLY: {
        Select_File_Opts opts; ptr0(&opts);
        opts.buf = new_array(char, 256);
        opts.bufsize = 256;
        opts.starting_folder = cp_strdup(world.current_path);
        opts.folder = false;
        opts.save = false;
        if (!let_user_select_file(&opts)) break;
        if (!goto_file_and_pos(opts.buf, new_cur2(0, 0))) {
            tell_user_error(cp_sprintf("Unable to open %s.", cp_basename(opts.buf)));
            break;
        }
        break;
    }

    case CMD_COMMAND_PALETTE: {
        SCOPED_MEM(&world.run_command_mem);
        world.run_command_mem.reset();

        auto &wnd = world.wnd_command;
        wnd.query[0] = '\0';
        wnd.actions = new_list(Command);
        wnd.filtered_results = new_list(int);
        wnd.selection = 0;

        for (int i = 0; i < _CMD_COUNT_; i++) {
            auto fuck_cpp = (Command)i;
            if (is_command_enabled(fuck_cpp))
                wnd.actions->append(fuck_cpp);
        }

        world.wnd_command.show = true;
        break;
    }

    case CMD_GENERATE_FUNCTION:
        do_generate_function();
        break;

    case CMD_ADD_ALL_JSON_TAGS:
    case CMD_ADD_ALL_XML_TAGS:
    case CMD_ADD_ALL_YAML_TAGS:
    case CMD_ADD_JSON_TAG:
    case CMD_ADD_XML_TAG:
    case CMD_ADD_YAML_TAG:
    case CMD_REMOVE_TAG:
    case CMD_REMOVE_ALL_TAGS: {
        ccstr lang = NULL;
        auto op = (Generate_Struct_Tags_Op)0;

        switch (cmd) {
        case CMD_ADD_ALL_JSON_TAGS: lang = "json";  op = GSTOP_ADD_ALL;    break;
        case CMD_ADD_ALL_XML_TAGS:  lang = "xml";   op = GSTOP_ADD_ALL;    break;
        case CMD_ADD_ALL_YAML_TAGS: lang = "yaml";  op = GSTOP_ADD_ALL;    break;
        case CMD_ADD_JSON_TAG:      lang = "json";  op = GSTOP_ADD_ONE;    break;
        case CMD_ADD_XML_TAG:       lang = "xml";   op = GSTOP_ADD_ONE;    break;
        case CMD_ADD_YAML_TAG:      lang = "yaml";  op = GSTOP_ADD_ONE;    break;
        case CMD_REMOVE_TAG:        lang = "";      op = GSTOP_REMOVE_ONE; break;
        case CMD_REMOVE_ALL_TAGS:   lang = "";      op = GSTOP_REMOVE_ALL; break;
        }

        cp_assert(lang);

        auto editor = get_current_editor();
        if (!editor) break;
        if (!editor->is_modifiable()) break;

        auto &ind = world.indexer;
        defer { ind.ui_mem.reset(); };
        {
            SCOPED_MEM(&ind.ui_mem);

            if (!ind.acquire_lock(IND_READING, true)) {
                tell_user_error("The indexer is currently busy.");
                return;
            }
            defer { ind.release_lock(IND_READING); };

            auto result = ind.generate_struct_tags(editor->filepath, editor->cur, op, lang, (Case_Style)options.struct_tag_case_style);
            if (!result) {
                tell_user_error("Unable to generate struct tags.");
                return;
            }

            int miny = result->highlight_start.y;
            int maxy = result->highlight_end.y;

            {
                SCOPED_BATCH_CHANGE(editor->buf);

                Fori (result->insert_starts) {
                    auto start = it;
                    auto end = result->insert_ends->at(i);
                    auto text = result->insert_texts->at(i);

                    auto utext = cstr_to_ustr(text);
                    if (!utext) continue;

                    if (start.y < miny) miny = start.y;
                    if (end.y > maxy) maxy = end.y;

                    editor->buf->apply_edit(start, end, utext->items, utext->len);
                }
            }

            editor->highlight_snippet(result->highlight_start, result->highlight_end);
        }
        break;
    }

    case CMD_TOGGLE_COMMENT: {
        auto editor = get_current_editor();
        if (!editor) break;
        if (!editor->is_modifiable()) break;

        int start, end;
        auto sel = editor->get_selection();
        if (sel) {
            start = sel->ranges->at(0).start.y;
            end = sel->ranges->last()->end.y;
        } else {
            start = editor->cur.y;
            end = editor->cur.y;
        }
        editor->toggle_comment(start, end);
            break;
    }

    case CMD_REPLACE:
    case CMD_FIND:
        open_current_file_search(cmd == CMD_REPLACE, false);
        break;

    case CMD_DOCUMENTATION:
        GHOpenURLInBrowser("https://docs.codeperfect95.com/");
        break;

    case CMD_CUT:
    case CMD_COPY: {
        auto editor = get_current_editor();
        if (!editor) break;

        auto sel = editor->get_selection();
        if (!sel) break;

        auto s = editor->get_selection_text(sel);
        world.window->set_clipboard_string(s);

        if (cmd == CMD_CUT) {
            auto start = editor->delete_selection(sel);
            editor->move_cursor(start);
            if (world.vim.on) {
                // we shouldn't be here otherwise
                cp_assert(world.vim_mode() == VI_VISUAL);
                editor->vim_return_to_normal_mode();
            } else {
                editor->selecting = false;
            }
        }
        break;
    }

    case CMD_SELECT_ALL: {
        // TODO: integrate with vim

        auto editor = get_current_editor();
        if (!editor) break;

        if (world.vim.on) {
            editor->move_cursor(new_cur2(0, 0));
            if (world.vim_mode() != VI_NORMAL)
                editor->vim_handle_key(CP_KEY_ESCAPE, 0);
            editor->vim_handle_char('v');
            editor->move_cursor(editor->buf->end_pos());
        } else {
            editor->select_start = new_cur2(0, 0);
            editor->move_cursor(editor->buf->end_pos());
            if (editor->select_start != editor->cur)
                editor->selecting = true;
        }
        break;
    }

    case CMD_PASTE: {
        // TODO: integrate with vim

        auto editor = get_current_editor();
        if (!editor) break;

        auto clipboard_contents = world.window->get_clipboard_string();
        if (!clipboard_contents) return;

        if (editor->selecting) {
            auto a = editor->select_start;
            auto b = editor->cur;
            ORDER(a, b);

            editor->buf->remove(a, b);
            editor->move_cursor(a);
            editor->selecting = false;
        }

        editor->insert_text_in_insert_mode(clipboard_contents);
        return;
    }

    case CMD_UNDO:
    case CMD_REDO: {
        // TODO: handle this for vim too; do we just use `u` and `C-r`?
        auto editor = get_current_editor();
        if (!editor) break;

        auto buf = editor->buf;

        // is this gonna cause any issues if we start to "hook" on-leave-insert and do more stuff there?
        // why would we tho? before all the stuff we were doing was hacks to work around nvim bullshit
        // now that all state is in memory it's just a bunch of direct state updates
        if (world.vim.on && world.vim_mode() != VI_NORMAL)
            editor->vim_return_to_normal_mode(); // should we call this or trigger_escape()?

        cur2 undo_end = NULL_CUR;
        auto pos = (cmd == CMD_UNDO ? buf->hist_undo(&undo_end) : buf->hist_redo());
        if (pos == NULL_CUR) break;

        // should we make this part of move_cursor?
        // this code basically says, if we are in normal mode, don't put the cursor at eol
        // whose job is it to ensure that?
        if (world.vim.on)
            if (pos.x == editor->buf->lines[pos.y].len && pos.x)
                pos.x = editor->buf->lines[pos.y].len-1;

        auto opts = default_move_cursor_opts();
        opts->is_user_movement = true;

        if (undo_end != NULL_CUR && !world.vim.on) {
            auto a = pos;
            auto b = undo_end;
            ORDER(a, b);

            editor->selecting = true;
            editor->select_start = a;
            editor->move_cursor(b, opts);
        } else {
            editor->move_cursor(pos, opts);
        }
        break;
    }

    case CMD_NEW_FILE:
        get_current_pane()->open_empty_editor();
        break;

    case CMD_SAVE_FILE: {
        auto editor = get_current_editor();
        if (editor) editor->handle_save();
        break;
    }
    case CMD_SAVE_ALL:
        save_all_unsaved_files();
        break;

    case CMD_CLOSE_ALL_EDITORS: {
        auto &panes = world.panes;
        while (true) {
            auto pane = panes.last();
            while (pane->editors.len) {
                auto editor = pane->editors.last();
                if (!world.dont_prompt_on_close_unsaved_tab)
                    if (!editor->ask_user_about_unsaved_changes())
                        goto getout;
                editor->cleanup();
                pane->editors.len--;
            }

            if (panes.len == 1) break;
            close_pane(panes.len-1);
        }
    getout:
        break;
    }

    case CMD_CLOSE_EDITOR: {
        auto pane = get_current_pane();
        if (!pane) break;

        auto editor = pane->get_current_editor();
        if (!editor)
            close_pane(world.current_pane);
        else
            close_editor(pane, pane->current_editor);
        break;
    }

    case CMD_OPEN_LAST_CLOSED_EDITOR: {
        cp_assert(world.last_closed->len);

        auto lc = world.last_closed->pop();
        goto_file_and_pos(lc.filepath, lc.pos);
        break;
    }

    case CMD_EXIT:
        world.window->should_close = true;
        break;

    case CMD_SEARCH:
    case CMD_SEARCH_AND_REPLACE: {
        auto &wnd = world.wnd_search_and_replace;
        if (wnd.show) {
            wnd.cmd_focus = true;
            wnd.cmd_focus_textbox = true;
        }
        wnd.show = true;
        wnd.replace = (cmd == CMD_SEARCH_AND_REPLACE);
        break;
    }

    case CMD_FILE_EXPLORER:
        if (from_menu) {
            world.file_explorer.show ^= 1;
        } else {
            auto &wnd = world.file_explorer;
            if (wnd.show) {
                if (!wnd.focused) {
                    wnd.cmd_focus = true;
                } else {
                    wnd.show = false;
                }
            } else {
                wnd.show = true;
            }
        }
        break;

    case CMD_ERROR_LIST:
        world.error_list.show ^= 1;
        break;

    case CMD_GO_TO_FILE: {
        auto &wnd = world.wnd_goto_file;
        if (wnd.show) {
            if (from_menu || world.wnd_goto_file.focused)
                wnd.show = false;
            else
                wnd.cmd_focus = true;
        } else {
            init_goto_file();
        }
        break;
    }

    case CMD_GO_TO_SYMBOL:
        if (world.wnd_goto_symbol.show) {
            if (from_menu)
                world.wnd_goto_symbol.show = false;
        } else {
            init_goto_symbol();
        }
        break;

    case CMD_GO_TO_NEXT_ERROR:
    case CMD_GO_TO_PREVIOUS_ERROR: {
        auto &b = world.build;

        bool has_valid = false;
        For (&b.errors) {
            if (it.valid) {
                has_valid = true;
                break;
            }
        }

        if (!b.ready() || !has_valid) break;

        auto old = b.current_error;
        do {
            b.current_error += (cmd == CMD_GO_TO_NEXT_ERROR ? 1 : -1);
            if (b.current_error < 0)
                b.current_error = b.errors.len - 1;
            if (b.current_error >= b.errors.len)
                b.current_error = 0;
        } while (b.current_error != old && !b.errors[b.current_error].valid);

        goto_error(b.current_error);
        break;
    }

    case CMD_GO_TO_DEFINITION:
        handle_goto_definition();
        break;

    case CMD_FIND_NEXT:
    case CMD_FIND_PREVIOUS: {
        auto editor = get_current_editor();
        if (!editor) break;

        auto tree = editor->buf->search_tree;
        if (!tree->get_size()) break;

        auto idx = editor->move_file_search_result(cmd == CMD_FIND_NEXT, 1);
        if (idx == -1) break;

        auto node = tree->get_node(idx);
        if (!node) break;

        editor->file_search.current_idx = idx;
        editor->move_cursor(node->pos);
        break;
    }

    case CMD_GO_TO_NEXT_SEARCH_RESULT:
    case CMD_GO_TO_PREVIOUS_SEARCH_RESULT:
        if (world.searcher.mt_state.type == SEARCH_SEARCH_DONE)
            if (world.searcher.mt_state.results->len)
                move_search_result(cmd == CMD_GO_TO_NEXT_SEARCH_RESULT, 1);
        break;

    case CMD_FIND_REFERENCES:
        initiate_find_references(NULL_CUR);
        break;

    case CMD_FORMAT_FILE: {
        auto editor = get_current_editor();
        if (!editor) break;
        if (!editor->is_modifiable()) break;

        editor->format_on_save();
        break;
    }

    case CMD_ORGANIZE_IMPORTS: {
        auto editor = get_current_editor();
        if (!editor) break;
        if (!editor->is_modifiable()) break;

        {
            SCOPED_BATCH_CHANGE(editor->buf);
            editor->optimize_imports();
            // editor->format_on_save();
        }
        break;
    }

    case CMD_FORMAT_SELECTION:
        // TODO
        // how do we even get the visual selection?
        break;

    case CMD_RENAME:
        initiate_rename_identifier(NULL_CUR);
        break;

    case CMD_ADD_NEW_FILE:
        open_add_file_or_folder(false);
        break;

    case CMD_ADD_NEW_FOLDER:
        open_add_file_or_folder(true);
        break;

    case CMD_PROJECT_SETTINGS:
        ui.open_project_settings();
        break;

    case CMD_BUILD:
        world.error_list.show = true;
        world.error_list.cmd_make_visible_but_dont_focus = true;
        save_all_unsaved_files();
        kick_off_build();
        break;

    case CMD_BUILD_PROFILE_1:
    case CMD_BUILD_PROFILE_2:
    case CMD_BUILD_PROFILE_3:
    case CMD_BUILD_PROFILE_4:
    case CMD_BUILD_PROFILE_5:
    case CMD_BUILD_PROFILE_6:
    case CMD_BUILD_PROFILE_7:
    case CMD_BUILD_PROFILE_8:
    case CMD_BUILD_PROFILE_9:
    case CMD_BUILD_PROFILE_10:
    case CMD_BUILD_PROFILE_11:
    case CMD_BUILD_PROFILE_12:
    case CMD_BUILD_PROFILE_13:
    case CMD_BUILD_PROFILE_14:
    case CMD_BUILD_PROFILE_15:
    case CMD_BUILD_PROFILE_16: {
        int index = (int)cmd - CMD_BUILD_PROFILE_1;
        if (index >= project_settings.build_profiles->len) break; // shouldn't happen

	    auto profile = &project_settings.build_profiles->items[index];

        world.error_list.show = true;
        world.error_list.cmd_make_visible_but_dont_focus = true;
        save_all_unsaved_files();
        kick_off_build(profile);
        break;
    }

    case CMD_BUILD_RESULTS:
        world.error_list.show ^= 1;
        break;

    case CMD_BUILD_PROFILES:
        ui.open_project_settings();
        world.wnd_project_settings.focus_build_profiles = true;
        break;

    case CMD_RESCAN_INDEX:
        world.indexer.message_queue.add([&](auto msg) {
            msg->type = GOMSG_RESCAN_INDEX;
        });
        break;

    case CMD_OBLITERATE_AND_RECREATE_INDEX:
        world.indexer.message_queue.add([&](auto msg) {
            msg->type = GOMSG_OBLITERATE_AND_RECREATE_INDEX;
        });
        break;

    case CMD_BREAK_ALL:
        world.dbg.push_call(DLVC_BREAK_ALL);
        break;

    case CMD_STOP_DEBUGGING:
        world.dbg.push_call(DLVC_STOP);
        break;

    case CMD_STEP_OVER:
        world.dbg.push_call(DLVC_STEP_OVER);
        break;

    case CMD_STEP_INTO:
        world.dbg.push_call(DLVC_STEP_INTO);
        break;

    case CMD_STEP_OUT:
        world.dbg.push_call(DLVC_STEP_OUT);
        break;

    case CMD_TOGGLE_BREAKPOINT: {
        auto editor = get_current_editor();
        if (editor) {
            world.dbg.push_call(DLVC_TOGGLE_BREAKPOINT, [&](auto call) {
                call->toggle_breakpoint.filename = cp_strdup(editor->filepath);
                call->toggle_breakpoint.lineno = editor->cur.y + 1;
            });
        }
        break;
    }

    case CMD_DELETE_ALL_BREAKPOINTS: {
        auto res = ask_user_yes_no(
            "Are you sure you want to delete all breakpoints?",
            NULL,
            "Delete",
            "Don't Delete"
        );

        if (res == ASKUSER_YES)
            world.dbg.push_call(DLVC_DELETE_ALL_BREAKPOINTS);
        break;
    }

    case CMD_DEBUG_TEST_UNDER_CURSOR:
        world.dbg.push_call(DLVC_DEBUG_TEST_UNDER_CURSOR);
        break;

    case CMD_DEBUG_OUTPUT:
        world.wnd_debug_output.show ^= 1;
        break;

    case CMD_CONTINUE:
        world.dbg.push_call(DLVC_CONTINUE_RUNNING);
        break;

    case CMD_START_DEBUGGING:
        save_all_unsaved_files();
        world.dbg.push_call(DLVC_START);
        break;

    case CMD_DEBUG_PROFILE_1:
    case CMD_DEBUG_PROFILE_2:
    case CMD_DEBUG_PROFILE_3:
    case CMD_DEBUG_PROFILE_4:
    case CMD_DEBUG_PROFILE_5:
    case CMD_DEBUG_PROFILE_6:
    case CMD_DEBUG_PROFILE_7:
    case CMD_DEBUG_PROFILE_8:
    case CMD_DEBUG_PROFILE_9:
    case CMD_DEBUG_PROFILE_10:
    case CMD_DEBUG_PROFILE_11:
    case CMD_DEBUG_PROFILE_12:
    case CMD_DEBUG_PROFILE_13:
    case CMD_DEBUG_PROFILE_14:
    case CMD_DEBUG_PROFILE_15:
    case CMD_DEBUG_PROFILE_16: {
        int index = (int)cmd - CMD_DEBUG_PROFILE_1;
        if (index >= project_settings.debug_profiles->len) break;

        save_all_unsaved_files();
        world.dbg.push_call(DLVC_START, [&](auto call) {
            call->start.use_custom_profile = true;
            call->start.profile_index = index;
        });
        break;
    }

    case CMD_DEBUG_PROFILES:
        ui.open_project_settings();
        world.wnd_project_settings.focus_debug_profiles = true;
        break;

    case CMD_GENERATE_IMPLEMENTATION: {
        auto &wnd = world.wnd_generate_implementation;

        if (!wnd.fill_thread)
            kill_thread(wnd.fill_thread);

        wnd.query[0] = '\0';
        wnd.symbols = NULL;
        wnd.filtered_results = NULL;

        if (!handle_unsaved_files()) break;

        auto &ind = world.indexer;

        bool found_something = false;
        defer {
            if (!found_something)
                tell_user("Couldn't find anything under cursor for Generate Implementation.", "Error");
        };

        auto result = get_current_definition();
        if (!result || !result->decl) break;

        auto decl = result->decl->decl;
        if (!decl->gotype) break;

        found_something = true;

        if (decl->type != GODECL_TYPE) {
            tell_user("The selected object is not a type.", "Error");
            break;
        }

        world.generate_implementation_mem.reset();

        if (!ind.acquire_lock(IND_READING, true)) break;
        defer { ind.release_lock(IND_READING); };

        ind.reload_all_editors();

        auto gofile = ind.find_gofile_from_ctx(result->decl->ctx);
        if (!gofile) break;

        {
            SCOPED_MEM(&world.generate_implementation_mem);

            wnd.file_hash_on_open = gofile->hash;
            wnd.declres = result->decl->copy_decl();
            wnd.filtered_results = new_list(int);

            auto gotype = wnd.declres->decl->gotype;
            wnd.selected_interface = (gotype->type == GOTYPE_INTERFACE);
        }

        auto worker = [](void*) {
            auto &wnd = world.wnd_generate_implementation;

            SCOPED_MEM(&wnd.fill_thread_pool);

            auto symbols = new_list(Go_Symbol);
            ind.fill_generate_implementation(symbols, wnd.selected_interface);
            if (!symbols->len) return;

            {
                SCOPED_MEM(&world.generate_implementation_mem);
                wnd.symbols = new_list(Go_Symbol, symbols->len);
                For (symbols) wnd.symbols->append(it.copy());
            }

            wnd.fill_running = false;
            if (wnd.fill_thread) {
                auto t = wnd.fill_thread;
                wnd.fill_thread = NULL;
                close_thread_handle(t);
            }
        };

        wnd.fill_thread_pool.cleanup();
        wnd.fill_thread_pool.init("generate_implementation_thread");

        wnd.fill_time_started_ms = current_time_milli();
        wnd.fill_running = true;
        wnd.fill_thread = create_thread(worker, NULL);
        wnd.show = true;
        break;
    }

    case CMD_VIEW_CALLEE_HIERARCHY: {
        auto &wnd = world.wnd_callee_hierarchy;

        auto &ind = world.indexer;
        if (!ind.acquire_lock(IND_READING)) {
            tell_user_error("The indexer is currently busy.");
            break;
        }

        bool ok = false;
        defer { if (!ok && ind.status == IND_READING) ind.release_lock(IND_READING); };

        auto editor = get_current_editor();
        if (!editor) break;
        if (editor->lang != LANG_GO) break;

        Goresult *result = NULL;
        {
            SCOPED_MEM(&world.indexer.ui_mem);
            result = ind.find_enclosing_toplevel(editor->filepath, editor->cur);
        }

        if (!result) break;
        if (!result->decl) break;

        auto decl = result->decl;
        if (!decl) break;

        if (!decl->gotype || decl->gotype->type != GOTYPE_FUNC) {
            tell_user("The declaration under your cursor is not a function.", "Error");
            break;
        }

        world.callee_hierarchy_mem.reset();
        {
            SCOPED_MEM(&world.callee_hierarchy_mem);
            wnd.declres = result->copy_decl();
        }

        auto thread_proc = [](void *param) {
            auto &wnd = world.wnd_callee_hierarchy;
            wnd.thread_mem.cleanup();
            wnd.thread_mem.init("view_callee_hierarchy_thread");
            SCOPED_MEM(&wnd.thread_mem);

            defer { cancel_callee_hierarchy(); };

            auto results = ind.generate_callee_hierarchy(wnd.declres);
            if (!results) return;

            {
                SCOPED_MEM(&world.callee_hierarchy_mem);

                auto newresults = new_list(Call_Hier_Node, results->len);
                For (results) newresults->append(it.copy());

                wnd.results = newresults;
                wnd.workspace = ind.index.workspace->copy();
            }

            // close the thread handle first so it doesn't try to kill the thread
            if (wnd.thread) {
                close_thread_handle(wnd.thread);
                wnd.thread = NULL;
            }
        };

        if (wnd.show)
            wnd.cmd_focus = true;
        else
            wnd.show = true;
        wnd.done = false;
        wnd.results = NULL;

        wnd.thread = create_thread(thread_proc, NULL);
        if (!wnd.thread) {
            tell_user_error("Unable to kick off View Call Hierarchy.");
            break;
        }
        break;
    }

    case CMD_VIEW_CALLER_HIERARCHY: {
        auto &wnd = world.wnd_caller_hierarchy;

        auto &ind = world.indexer;
        if (!ind.acquire_lock(IND_READING)) {
            tell_user_error("The indexer is currently busy.");
            break;
        }

        bool ok = false;
        defer { if (!ok && ind.status == IND_READING) ind.release_lock(IND_READING); };

        auto editor = get_current_editor();
        if (!editor) break;
        if (editor->lang != LANG_GO) break;

        Goresult *result = NULL;
        {
            SCOPED_MEM(&world.indexer.ui_mem);
            result = ind.find_enclosing_toplevel(editor->filepath, editor->cur);
        }

        if (!result) break;
        if (!result->decl) break;

        auto decl = result->decl;
        if (!decl) break;

        if (!decl->gotype || decl->gotype->type != GOTYPE_FUNC) {
            tell_user("The declaration under your cursor is not a function.", "Error");
            break;
        }

        world.caller_hierarchy_mem.reset();
        {
            SCOPED_MEM(&world.caller_hierarchy_mem);
            wnd.declres = result->copy_decl();
        }

        auto thread_proc = [](void *param) {
            auto &wnd = world.wnd_caller_hierarchy;
            wnd.thread_mem.cleanup();
            wnd.thread_mem.init("view_caller_hierarchy_thread");
            SCOPED_MEM(&wnd.thread_mem);

            defer { cancel_caller_hierarchy(); };

            auto results = ind.generate_caller_hierarchy(wnd.declres);
            if (!results) return;

            {
                SCOPED_MEM(&world.caller_hierarchy_mem);

                auto newresults = new_list(Call_Hier_Node, results->len);
                For (results) newresults->append(it.copy());

                wnd.results = newresults;
                wnd.workspace = ind.index.workspace->copy();
            }

            // close the thread handle first so it doesn't try to kill the thread
            if (wnd.thread) {
                close_thread_handle(wnd.thread);
                wnd.thread = NULL;
            }
        };

        if (wnd.show)
            wnd.cmd_focus = true;
        else
            wnd.show = true;
        wnd.done = false;
        wnd.results = NULL;

        wnd.thread = create_thread(thread_proc, NULL);
        if (!wnd.thread) {
            tell_user_error("Unable to kick off View Call Hierarchy.");
            break;
        }

        ok = true;
        break;
    }

    case CMD_FIND_IMPLEMENTATIONS: {
        auto &wnd = world.wnd_find_implementations;

        auto result = get_current_definition(NULL, true);
        if (!result || !result->decl) break;

        auto decl = result->decl->decl;
        if (!decl) break;

        if (decl->type != GODECL_TYPE || decl->gotype->type != GOTYPE_INTERFACE) {
            tell_user("The selected object is not an interface.", "Error");
            break;
        }

        auto specs = decl->gotype->interface_specs;
        if (!specs || !specs->len) {
            auto msg = "The selected interface is empty. That means every type will match it. Do you still want to just list every type I can find?";
            if (ask_user_yes_no(msg, "Warning", "Yes, continue", "No") != ASKUSER_YES)
                break;
        }

        world.find_implementations_mem.reset();
        {
            SCOPED_MEM(&world.find_implementations_mem);
            wnd.declres = result->decl->copy_decl();
        }

        wnd.search_everywhere = false;
        do_find_implementations();
        break;
    }

    case CMD_FIND_INTERFACES: {
        auto &wnd = world.wnd_find_interfaces;

        auto result = get_current_definition(NULL, true);
        if (!result || !result->decl) break;

        auto decl = result->decl->decl;
        if (!decl) break;

        if (decl->type != GODECL_TYPE) {
            tell_user("The selected object is not an interface.", "Error");
            break;
        }

        if (decl->gotype->type == GOTYPE_INTERFACE) {
            tell_user("The selected type is an interface. (Did you mean to use Find Implementations?)", "Error");
            break;
        }

        world.find_interfaces_mem.reset();
        {
            SCOPED_MEM(&world.find_interfaces_mem);
            wnd.declres = result->decl->copy_decl();
        }

        wnd.search_everywhere = false;
        do_find_interfaces();
        break;
    }

    case CMD_OPTIONS:
        if (world.wnd_options.show) {
            world.wnd_options.cmd_focus = true;
        } else {
            world.wnd_options.show = true;
            world.wnd_options.something_that_needs_restart_was_changed = false;
            memcpy(&world.wnd_options.tmp, &options, sizeof(Options));
        }
        break;

    case CMD_GO_BACK:
        world.history.go_backward();
        break;

    case CMD_GO_FORWARD:
        world.history.go_forward();
        break;

    case CMD_AST_NAVIGATION: {
        auto editor = get_current_editor();

        auto tree = editor->buf->tree;
        if (!tree) break;

        auto &nav = editor->ast_navigation;
        if (nav.on) {
            nav.on = false;
            break;
        }

        // if we're not in normal mode it fucks it up
        if (world.vim.on)
            if (world.vim_mode() != VI_NORMAL)
                editor->vim_handle_key(CP_KEY_ESCAPE, 0);

        // or reset?
        nav.mem.cleanup();
        nav.mem.init("ast_navigation");

        Parser_It *it = NULL;
        {
            SCOPED_MEM(&nav.mem);
            it = new_object(Parser_It);
            it->init(editor->buf);
        }

        auto root = new_ast_node(ts_tree_root_node(tree), it);

        Ast_Node *node = NULL;
        find_nodes_containing_pos(root, editor->cur, true, [&](auto it) -> Walk_Action {
            if (it->type() == TS_SOURCE_FILE) return WALK_CONTINUE;

            if (node) {
                if (it->start() == node->start())
                    if (it->end() == node->end())
                        return WALK_CONTINUE;
            } else {
                if (!it->prev() && !it->next())
                    return WALK_CONTINUE;
            }

            node = it->dup();
            return WALK_CONTINUE;
        });

        if (!node) {
            // if we're not on a node, first try to jump to the next node
            Ast_Node *last = NULL;
            FOR_NODE_CHILDREN (root) {
                if (it->start() > editor->cur) {
                    node = it->dup();
                    break;
                }
                last = it;
            }

            // if we're past the last node, just go to the last node
            if (!node) {
                if (!last) return;
                node = last->dup();
            }

            editor->move_cursor(node->start());
        }

        nav.on = true;
        nav.tree_version = editor->buf->tree_version;
        {
            SCOPED_MEM(&nav.mem);
            nav.siblings = new_list(Ast_Node*);
        }
        editor->update_selected_ast_node(node);
        editor->trigger_escape();
    }
    }
}

void open_add_file_or_folder(bool folder, FT_Node *dest) {
    if (world.file_tree_busy) {
        tell_user_error("The file tree is currently being generated.");
        return;
    }

    if (!dest) dest = world.file_explorer.selection;

    FT_Node *node = dest;
    auto &wnd = world.wnd_add_file_or_folder;

    auto is_root = [&]() {
        if (!node) return true;
        if (node == world.file_tree) return true;
        if (node->is_directory) return false;

        return node->parent == world.file_tree;
    };

    wnd.location_is_root = is_root();
    if (!wnd.location_is_root) {
        if (!node->is_directory) {
            node = node->parent;
            cp_assert(node);
        }
        cp_strcpy_fixed(wnd.location, ft_node_to_path(node));
    }

    wnd.folder = folder;
    wnd.show = true;
    wnd.name[0] = '\0';
}

void do_generate_function() {
    if (!handle_unsaved_files()) return;

    auto editor = get_current_editor();
    if (!editor) {
        tell_user_error("Couldn't find anything under your cursor (you don't have an editor focused).");
        return;
    }

    if (editor->lang != LANG_GO) {
        tell_user_error("Couldn't find anything under your cursor (you're not in a Go file).");
        return;
    }

    auto &ind = world.indexer;

    defer { ind.ui_mem.reset(); };

    {
        SCOPED_MEM(&ind.ui_mem);

        if (!ind.acquire_lock(IND_READING, true)) {
            tell_user_error("The indexer is currently busy.");
            return;
        }
        defer { ind.release_lock(IND_READING); };

        auto off = editor->cur_to_offset(editor->cur);
        auto result = ind.generate_function_signature(editor->filepath, new_cur2(off, -1));
        if (!result) {
            tell_user_error("Unable to generate function.");
            return;
        }

        if (result->existing_decl_filepath) {
            goto_file_and_pos(result->existing_decl_filepath, result->existing_decl_pos, true, ECM_GOTO_DEF); // new ECM_*?
            return;
        }

        auto editor = find_editor_by_filepath(result->insert_filepath);

        if (editor) {
            // if it's already open, just make the change
            auto uchars = cstr_to_ustr(result->insert_code);

            if (result->insert_pos == NULL_CUR)
                editor->buf->insert(editor->buf->end_pos(), uchars->items, uchars->len);
            else
                editor->buf->insert(result->insert_pos, uchars->items, uchars->len);
        } else {
            // otherwise, make the change on disk
            File_Replacer fr;
            if (!fr.init(result->insert_filepath, "generate_function")) {
                tell_user_error("Failed to open the file where the generated function should go.");
                return;
            }

            if (result->insert_pos == NULL_CUR) {
                // abuse the file replacer to insert it at end lol fuck me
                while (!fr.done()) fr.write(fr.advance_read_pointer());
                fr.do_replacement(fr.read_cur, result->insert_code);
            } else {
                fr.goto_next_replacement(result->insert_pos);
                fr.do_replacement(result->insert_pos, result->insert_code);
            }
            fr.finish();
        }

        {
            // open and go to the editor
            auto editor = goto_file_and_pos(result->insert_filepath, result->jump_to_pos, true, ECM_GOTO_DEF); // new ECM_*?
            editor->highlight_snippet(result->highlight_start, result->highlight_end);
        }

        // TODO: add imports
        // Fori (result->imports_needed_names)
        //     print("%s \"%s\"", it, result->imports_needed->at(i));
    }
}

void do_generate_implementation() {
    auto &wnd = world.wnd_generate_implementation;
    if (!wnd.filtered_results->len) return;

    auto &ind = world.indexer;
    if (!ind.try_acquire_lock(IND_READING)) return;
    defer { ind.release_lock(IND_READING); };

    if (!handle_unsaved_files()) return;

    auto &symbol = wnd.symbols->at(wnd.filtered_results->at(wnd.selection));

    // check that wnd.declres and symbol.decl haven't changed

    auto check_filehash_hasnt_changed = [&](Go_Ctx *ctx, u64 want) -> bool {
        auto gofile = ind.find_gofile_from_ctx(ctx);
        return gofile && gofile->hash == want;
    };

    {
        bool ok = (
            check_filehash_hasnt_changed(wnd.declres->ctx, wnd.file_hash_on_open) &&
            check_filehash_hasnt_changed(symbol.decl->ctx, symbol.filehash)
        );
        if (!ok) {
            tell_user("It looks like one or more files involved has changed since you opened Generate Implementation.\n\nPlease save all files and try again", "File mismatch");
            return;
        }
    }

    Goresult *src = NULL, *dest = NULL;
    if (wnd.selected_interface) {
        src = wnd.declres;
        dest = symbol.decl;
    } else {
        src = symbol.decl;
        dest = wnd.declres;
    }

    auto import_table = new_table(ccstr);
    auto import_table_r = new_table(ccstr);

    auto dest_gofile = ind.find_gofile_from_ctx(dest->ctx);
    For (dest_gofile->imports) {
        auto package_name = ind.get_import_package_name(&it);
        if (!package_name) continue;

        import_table->set(it.import_path, package_name);
        import_table_r->set(package_name, it.import_path);
    }

    auto src_gotype = src->decl->gotype;
    auto dest_gotype = dest->decl->gotype;

    if (src_gotype->type == GOTYPE_BUILTIN)
        src_gotype = src_gotype->builtin_underlying_base;

    if (!src_gotype) return;
    if (src_gotype->type != GOTYPE_INTERFACE) return;

    if (!dest_gotype) return;
    if (dest_gotype->type == GOTYPE_INTERFACE) return;

    auto src_methods = ind.list_interface_methods(src->wrap(src_gotype));
    if (!src_methods) return;

    auto dest_methods = new_list(Goresult);
    if (!ind.list_type_methods(dest->decl->name, dest->ctx->import_path, dest_methods))
        return;

    auto methods_to_add = new_list(Goresult);
    For (src_methods) {
        auto &srcmeth = it;
        For (dest_methods)
            if (streq(srcmeth.decl->name, it.decl->name))
                if (ind.are_gotypes_equal(src->wrap(srcmeth.decl->gotype), dest->wrap(it.decl->gotype)))
                    goto skip;
        methods_to_add->append(&it);
    skip:;
    }

    auto type_name = dest->decl->name;

    auto generate_type_var = [&]() {
        auto s = new_list(char);
        for (int i = 0, len = strlen(type_name); i < len && s->len < 3; i++)
            if (isupper(type_name[i]))
                s->append(tolower(type_name[i]));
        s->append('\0');
        return s->items;
    };

    auto type_var = generate_type_var();

    Text_Renderer rend; rend.init();

    struct Import_To_Add {
        ccstr import_path;
        ccstr package_name;
        bool declare_explicitly;
    };

    auto imports_to_add = new_list(Import_To_Add);
    auto errors = new_list(ccstr);

    auto add_error = [&](ccstr fmt, ...) {
        va_list vl;
        va_start(vl, fmt);
        auto ret = cp_vsprintf(fmt, vl);
        va_end(vl);

        errors->append(ret);
    };

    auto render_type = [&](Goresult *res, ccstr method_name) -> ccstr {
        bool ok = true;

        auto handler = [&](Type_Renderer *tr, Gotype *t) -> bool {
            switch (t->type) {
            case GOTYPE_ID:
            case GOTYPE_SEL:
                break;
            default:
                return false;
            }

            Goresult *declres = NULL;
            if (t->type == GOTYPE_ID) {
                declres = ind.find_decl_of_id(t->id_name, t->id_pos, res->ctx);
            } else {
                auto import_path = ind.find_import_path_referred_to_by_id(t->sel_name, res->ctx);
                declres = ind.find_decl_in_package(t->sel_sel, import_path);
            }

            if (!declres || declres->decl->type != GODECL_TYPE) {
                ok = false;
                return false;
            }

            auto decl = declres->decl;
            if (decl->gotype->type == GOTYPE_BUILTIN) {
                // handle normally with default handler
                return false;
            }

            // see what the import is
            auto import_path = declres->ctx->import_path;

            bool found = false;
            auto package_name = import_table->get(import_path, &found);
            if (!found) {
                auto pkg = ind.find_up_to_date_package(import_path);
                if (pkg) {
                    auto actual_name = pkg->package_name;
                    bool alias_package = false;

                    for (int i = 0;; i++) {
                        auto new_name = actual_name;
                        if (i)
                            new_name = cp_sprintf("%s%d", new_name, i);

                        import_table_r->get(new_name, &found);
                        if (!found) {
                            package_name = new_name;
                            alias_package = i > 0;
                            break;
                        }
                    }

                    import_table->set(import_path, package_name);
                    import_table_r->set(package_name, import_path);

                    Import_To_Add imp; ptr0(&imp);
                    imp.import_path = import_path;
                    imp.package_name = package_name;
                    imp.declare_explicitly = alias_package;
                    imports_to_add->append(&imp);
                }
            }

            if (!package_name) {
                ok = false;
                return false;
            }

            tr->write("%s.%s", package_name, t->type == GOTYPE_ID ? t->id_name : t->sel_sel);
            return true;
        };

        auto t = res->gotype;

        Type_Renderer tr; tr.init();
        tr.full = true;
        tr.write_type(t, handler);

        if (!ok) {
            auto get_typestr = [&]() -> ccstr {
                if (t->type == GOTYPE_ID)
                    return t->id_name;
                return cp_sprintf("%s.%s", t->sel_name, t->sel_sel);
            };

            add_error("Unable to add method %s because we couldn't resolve type %s.", method_name, get_typestr());
            return NULL;
        }

        return tr.finish();
    };

    bool error = false;

    For (methods_to_add) {
        auto method_ctx = it.ctx;

        auto gotype = it.decl->gotype;
        if (!gotype) continue;
        if (gotype->type != GOTYPE_FUNC) continue;

        auto &sig = gotype->func_sig;

        rend.write("\n\n");

        if (isupper(it.decl->name[0]))
            rend.write("// %s still needs a comment.\n", it.decl->name);

        rend.write("func (%s *%s) %s(", type_var, type_name, it.decl->name);

        bool first = true;
        For (sig.params) {
            if (first)
                first = false;
            else
                rend.write(", ");

            auto typestr = render_type(make_goresult(it.gotype, method_ctx), it.name);
            if (!typestr) {
                error = true;
                goto done_writing;
            }

            rend.write("%s %s", it.name, typestr);
        }

        rend.write(") ");

        if (!isempty(sig.result)) {
            if (sig.result->len > 1) rend.write("(");

            Fori (sig.result) {
                if (i > 0) rend.write(", ");

                auto typestr = render_type(make_goresult(it.gotype, method_ctx), it.name);
                if (!typestr) {
                    error = true;
                    goto done_writing;
                }

                rend.write("%s", typestr);
            }

            if (sig.result->len > 1) rend.write(")");

            rend.write(" ");
        }

        rend.write("{\n\tpanic(\"not implemented\")\n}");
    }

done_writing:

    if (error) {
        // TODO: notify user of errors?
        return;
    }

    auto s = rend.finish();

    // we now know that src and dest accurately reflect what's on disk

    Buffer buf; buf.init(MEM, LANG_GO, false, false);
    defer { buf.cleanup(); };

    auto filepath = ind.ctx_to_filepath(dest->ctx);
    if (!filepath) return;

    auto fm = map_file_into_memory(filepath);
    if (!fm) return;
    defer { fm->cleanup(); };

    buf.read(fm);

    auto uchars = cstr_to_ustr(s);

    // add the generated methods
    buf.insert(dest->decl->decl_end, uchars->items, uchars->len);

    int cursor_offset = 0;

    // add the imports
    {
        auto iter = new_object(Parser_It);
        iter->init(&buf);
        auto root = new_ast_node(ts_tree_root_node(buf.tree), iter);

        Ast_Node *package_node = NULL;
        Ast_Node *imports_node = NULL;

        FOR_NODE_CHILDREN (root) {
            if (it->type() == TS_PACKAGE_CLAUSE) {
                package_node = it;
            } else if (it->type() == TS_IMPORT_DECLARATION) {
                imports_node = it;
                break;
            }
        }

        if (!imports_node && !package_node) return;

        Text_Renderer rend;
        rend.init();
        rend.write("import (\n");

        // write all existing imports
        if (imports_node) {
            auto speclist = imports_node->child();
            switch (speclist->type()) {
            case TS_IMPORT_SPEC_LIST:
                FOR_NODE_CHILDREN (speclist) {
                    rend.write("\t%s\n", it->string());
                }
                break;
            case TS_IMPORT_SPEC:
                rend.write("\t%s\n", speclist->string());
                break;
            default:
                return;
            }
        }

        For (imports_to_add) {
            if (it.declare_explicitly) {
                rend.write("\t%s \"%s\"\n", it.package_name, it.import_path);
            } else {
                rend.write("\t\"%s\"\n", it.import_path);
            }
        }

        rend.write(")\n");

        GHFmtStart();
        GHFmtAddLine(rend.finish());
        GHFmtAddLine("");

        // format with gofmt, not gofumpt, because gofumpt can't handle
        // snippets (i believe it requires a full file)
        auto new_contents = gh_fmt_finish(false);
        if (!new_contents) return;
        defer { GHFree(new_contents); };

        auto new_contents_len = strlen(new_contents);
        if (!new_contents_len) return;

        while (new_contents[new_contents_len-1] == '\n') {
            new_contents[new_contents_len-1] = '\0';
            new_contents_len--;
        }

        cur2 start, old_end;
        if (imports_node) {
            start = imports_node->start();
            old_end = imports_node->end();
        } else {
            start = package_node->end();
            old_end = package_node->end();
        }

        auto chars = new_list(uchar);
        if (!imports_node) {
            // add two newlines, it's going after the package decl
            chars->append('\n');
            chars->append('\n');
        }

        int lines_in_new = 0;
        {
            auto ustr = cstr_to_ustr(new_contents);
            For (ustr) {
                chars->append(it);
                if (it == '\n') lines_in_new++;
            }
        }

        buf.apply_edit(start, old_end, chars->items, chars->len);
        cursor_offset = lines_in_new - (old_end.y - start.y);
    }

    // write to disk
    {
        File f;
        if (f.init_write(filepath) != FILE_RESULT_OK)
            return;
        defer { f.cleanup(); };
        buf.write(&f);
    }

    buf.cleanup();

    auto editor = find_editor_by_filepath(filepath);
    if (editor) {
        editor->disable_file_watcher_until = current_time_nano() + (2 * 1000000000);
        editor->reload_file();

        auto c = editor->cur;
        auto newc = new_cur2(c.x, c.y + cursor_offset);
        if (c != newc)
            editor->move_cursor(newc);
    }

    // TODO: refresh existing editors if `filepath` is open
    // tho i think writing to disk just automatically does that?
    // or should we not write to disk for editors that are open
}

void fuzzy_sort_filtered_results(ccstr query, List<int> *list, int total_results, fn<ccstr(int)> get_name) {
    auto names = new_array(ccstr, total_results);
    auto scores = new_array(double, total_results);

    fstlog("fuzzysort - start");

    For (list) {
        names[it] = get_name(it);
        scores[it] = fzy_match(query, names[it]);
    }

    fstlog("fuzzysort - generate list of scores");

    list->sort([&](int* pa, int* pb) {
        auto a = scores[*pa];
        auto b = scores[*pb];
        if (a != b) return b > a ? 1 : -1;

        return (int)(strlen(names[*pa]) - strlen(names[*pb]));
    });

    fstlog("fuzzysort - actual sorting");
}

bool write_project_settings() {
    auto filepath = path_join(world.current_path, ".cpproj");

    File f;
    if (f.init_write(filepath) != FILE_RESULT_OK)
        return false;
    defer { f.cleanup(); };

    Serde serde;
    serde.init(&f);
    serde.write_type(&project_settings, SERDE_PROJECT_SETTINGS);
    return serde.ok;
}

void handle_window_focus(bool focus) {
    if (GImGui) {
        auto &io = ImGui::GetIO();
        io.AddFocusEvent(focus);

        // do we need this stuff?
        // io.AddKeyEvent(ImGuiMod_Ctrl, false);
        // io.AddKeyEvent(ImGuiMod_Shift, false);
        // io.AddKeyEvent(ImGuiMod_Alt,  false);
        // io.AddKeyEvent(ImGuiMod_Super, false);
    }

    mem0(world.ui.mouse_down, sizeof(world.ui.mouse_down));
    mem0(world.ui.mouse_just_pressed, sizeof(world.ui.mouse_just_pressed));
    mem0(world.ui.mouse_just_released, sizeof(world.ui.mouse_just_released));
}

void fstlog(ccstr fmt, ...) {
    va_list vl;
    va_start(vl, fmt);
    auto s = cp_vsprintf(fmt, vl);
    va_end(vl);

    world.fst.log(s);
}

void set_zoom_level(int level) {
    options.zoom_level = level;
    if (world.wnd_options.show)
        world.wnd_options.tmp.zoom_level = level;

    recalc_display_size();

    // write out options
    File f;
    auto filepath = path_join(world.configdir, ".options");
    if (f.init_write(filepath) == FILE_RESULT_OK) {
        defer { f.cleanup(); };
        Serde serde;
        serde.init(&f);
        serde.write_type(&options, SERDE_OPTIONS);
    }
}

List<Editor*> *get_all_editors() {
    auto ret = new_list(Editor*);
    For (&world.panes)
        For (&it.editors)
            ret->append(&it);
    return ret;
}

void reset_everything_when_switching_editors(Editor *old_editor) {
    if (old_editor) {
        if (world.vim.on) {
            // this should clear out most vim related things, because in vim mode
            // it calls handle_input(CP_KEY_ESCAPE)
            old_editor->trigger_escape();
            cp_assert(world.vim_mode() == VI_NORMAL);
        }
    }

    // do this after triggering escape in the old editor so the escape gets saved
    if (world.vim.on) {
        if (world.vim.macro_state != MACRO_IDLE) {
            world.vim.macro_state = MACRO_IDLE;
            world.vim.macro_record.macro = 0;
        }
    }
}

void open_current_file_search(bool replace, bool from_vim) {
    auto &wnd = world.wnd_local_search;
    wnd.replace = replace;
    wnd.opened_from_vim = from_vim;
    if (from_vim) wnd.use_regex = true;

    if (wnd.show) {
        wnd.cmd_focus = true;
        wnd.cmd_focus_textbox = true;
    } else {
        wnd.show = true;
        cp_strcpy_fixed(wnd.query, wnd.permanent_query);
        if (wnd.query[0]) {
            auto editor = get_current_editor();
            if (editor)
                editor->trigger_file_search();
        }
    }
}

void move_search_result(bool forward, int count) {
    /*
     * cases:
     *  - nothing selected
     *  - file selected
     *  - result selected
     */

    auto &wnd = world.wnd_search_and_replace;
    auto results = world.searcher.mt_state.results;

    if (!results->len) return;

    // at the end after all the calculations:
    // - scroll to the selected file/result
    // - jump to its position
    defer {
        wnd.scroll_file = wnd.sel_file;
        wnd.scroll_result = wnd.sel_result;

        auto &file = results->at(wnd.sel_file);
        auto result = file.results->at(wnd.sel_result);
        goto_file_and_pos(file.filepath, result.match_start, true);
    };

    if (wnd.sel_file == -1 && wnd.sel_result == -1) {
        if (forward) {
            wnd.sel_file = 0;
            wnd.sel_result = 0;
        } else {
            wnd.sel_file = results->len-1;
            wnd.sel_result = results->at(wnd.sel_file).results->len-1;
        }
        if (--count == 0) return;
    }

    // at this point, count is still positive, and we're have at
    // least a file selected

    if (wnd.sel_result == -1) {
        if (forward) {
            wnd.sel_result = 0;
        } else {
            if (wnd.sel_file == 0)
                wnd.sel_file = results->len-1;
            else
                wnd.sel_file--;
            wnd.sel_result = results->at(wnd.sel_file).results->len-1;
        }
        if (--count == 0) return;
    }

    // at this point, count is still positive, and we're guaranteed to have
    // both file and result selected

    // express position as index of all search results & do the math on that

    int total = 0;
    For (results)
        total += it.results->len;

    int index = 0;
    for (int i = 0; i < wnd.sel_file; i++)
        index += results->at(i).results->len;
    index += wnd.sel_result;

    if (forward)
        index += count;
    else
        index += total - (count % total);
    index %= total;

    // now convert back to file+result

    Fori (results) {
        if (index < it.results->len) {
            wnd.sel_file = i;
            wnd.sel_result = index;
            break;
        }
        index -= it.results->len;
    }
}

bool close_pane(int idx) {
    auto &panes = world.panes;

    if (idx >= panes.len) return false;
    if (panes.len == 1) return false;

    panes[idx].cleanup();
    panes.remove(idx);
    if (world.current_pane >= panes.len)
        activate_pane_by_index(panes.len - 1);
    return true;
}

bool close_editor(Pane *pane, int editor_index) {
    auto &editor = pane->editors[editor_index];

    if (!world.dont_prompt_on_close_unsaved_tab)
        if (!editor.ask_user_about_unsaved_changes())
            return false;

    Last_Closed lc; ptr0(&lc);
    if (strlen(editor.filepath)+1 <= _countof(lc.filepath)) {
        cp_strcpy_fixed(lc.filepath, editor.filepath);
        lc.pos = editor.cur;
        world.last_closed->append(&lc);
    }

    editor.cleanup();
    pane->editors.remove(editor_index);

    if (!pane->editors.len) {
        pane->set_current_editor(-1);
    } else if (pane->current_editor == editor_index) {
        auto new_idx = pane->current_editor;
        if (new_idx >= pane->editors.len)
            new_idx = pane->editors.len - 1;
        pane->focus_editor_by_index(new_idx);
    } else if (pane->current_editor > editor_index) {
        pane->set_current_editor(pane->current_editor - 1);
    }
    return true;
}

bool is_imgui_hogging_keyboard() {
    if (world.ui.keyboard_captured_by_imgui) return true;
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) return true;

    return false;
}

void create_search_marks_for_editor(Searcher_Result_File *file, Editor *editor) {
    auto out = world.search_marks->append();
    {
        SCOPED_MEM(&world.search_marks_mem);
        out->filepath = cp_strdup(file->filepath);
        out->mark_starts = new_list(Mark*);
        out->mark_ends = new_list(Mark*);
    }

    auto buf = editor->buf;

    For (file->results) {
        out->mark_starts->append(buf->insert_mark(MARK_SEARCH_RESULT, it.match_start));
        out->mark_ends->append(buf->insert_mark(MARK_SEARCH_RESULT, it.match_end));
    }
}

cur2 get_search_mark_pos(ccstr filepath, int index, bool start) {
    if (!world.search_marks) return NULL_CUR;

    auto file = world.search_marks->find([&](auto it) { return are_filepaths_equal(it->filepath, filepath); });
    if (!file) return NULL_CUR;

    return (start ? file->mark_starts : file->mark_ends)->at(index)->pos();
}
