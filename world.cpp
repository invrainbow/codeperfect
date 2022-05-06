#include "world.hpp"
#include "ui.hpp"
#include "fzy_match.h"
#include "set.hpp"
#include "defer.hpp"

#include "glew.h"

World world;

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
    ring[curr].mark = world.mark_fridge.alloc();
    editor->buf->mark_tree.insert_mark(MARK_HISTORY, pos, ring[curr].mark);

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

    if (!should_push()) {
        return;
    }

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
    if (!editor->is_nvim_ready()) return;

    auto pos = it->mark->pos(); // it->pos

    world.navigation_queue.len = 0;
    auto dest = world.navigation_queue.append();
    dest->editor_id = it->editor_id;
    dest->pos = pos;

    focus_editor_by_id(it->editor_id, pos);
}

bool History::go_forward() {
    assert_main_thread();
    check_marks();

    if (curr == top) return false;

    check_marks();

    curr = inc(curr);
    actually_go(&ring[dec(curr)]);
    check_marks();
    return true;
}

void History::check_marks(int upper) {
#ifdef DEBUG_MODE
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
        if (it->tree->root != node)
            cp_panic("mark node is detached from root!");
    }
#endif
}

bool History::go_backward() {
    assert_main_thread();
    check_marks();

    if (curr == start) return false;

    {
        auto editor = get_current_editor();
        auto &it = ring[dec(curr)];

        // handle the case of: open file a, open file b, move down 4 lines (something < threshold),
        // go back, go forward, cursor will now be on line 0 instead of line 4
        if (!editor || it.editor_id != editor->id || it.pos != editor->cur) {
            if (editor) {
                actually_push(editor->id, editor->cur);
                curr = dec(curr);
            }

            check_marks();

            actually_go(&it);

            check_marks();

            return true;
        }
    }

    check_marks();

    if (dec(curr) == start) return false;

    check_marks();

    curr = dec(curr);
    actually_go(&ring[dec(curr)]);

    check_marks();

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

void History::remove_invalid_marks() {
    assert_main_thread();

    int i = start, j = start;

    check_marks();

    for (; i != curr; i = inc(i)) {
        if (!is_mark_valid(ring[i].mark))
            continue;
        if (i != j)
            memcpy(&ring[j], &ring[i], sizeof(ring[j]));
        j = inc(j);
    }


    curr = j;
    check_marks(j);

    for (; i != top; i = inc(i))  {
        if (!is_mark_valid(ring[i].mark))
            continue;
        if (i != j)
            memcpy(&ring[j], &ring[i], sizeof(ring[j]));
        j = inc(j);
    }

    top = j;
    check_marks();
}

void History_Loc::cleanup() {
    if (!mark) {
        return;
    }

    mark->cleanup();
    world.mark_fridge.free(mark);
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

void fill_file_tree() {
    SCOPED_MEM(&world.file_tree_mem);
    world.file_tree_mem.reset();

    // invalidate pointers
    world.file_explorer.selection = NULL;
    world.file_explorer.last_file_copied = NULL;
    world.file_explorer.last_file_cut = NULL;

    world.file_tree = alloc_object(FT_Node);
    world.file_tree->is_directory = true;
    world.file_tree->depth = -1;

    u32 depth = 0;

    GHGitIgnoreInit(world.current_path); // why do we need this here?

    fn<void(ccstr, FT_Node*)> recur = [&](ccstr path, FT_Node *parent) {
        FT_Node *last_child = parent->children;

        list_directory(path, [&](Dir_Entry *ent) {
            do {
                auto fullpath = path_join(path, ent->name);
                if (exclude_from_file_tree(fullpath)) break;

                auto file = alloc_object(FT_Node);
                file->name = cp_strdup(ent->name);
                file->is_directory = (ent->type & FILE_TYPE_DIRECTORY);
                file->num_children = 0;
                file->parent = parent;
                file->children = NULL;
                file->depth = depth;
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

                if (file->is_directory) {
                    depth++;
                    recur(fullpath, file);
                    depth--;
                }
            } while (0);

            return true;
        });

        parent->children = sort_ft_nodes(parent->children);
    };

    recur(world.current_path, world.file_tree);
}

bool copy_file(ccstr src, ccstr dest) {
    auto fm = map_file_into_memory(src);
    if (!fm) return false;
    defer { fm->cleanup(); };

    // TODO: map this into memory too?
    File f;
    if (f.init_write(dest) != FILE_RESULT_OK)
        return false;
    defer { f.cleanup(); };

    return f.write((char*)fm->data, fm->len);
}

void World::init(Window *_wnd) {
    ptr0(this);

    window = _wnd;
    gh_version = GHGetVersion();

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
#undef init_mem

    MEM = &frame_mem;

    init_treesitter_go_trie();

    global_mark_tree_lock.init();

    mark_fridge.init(512);
    mark_node_fridge.init(512);
    change_fridge.init(512);

    chunk0_fridge.init(512);
    chunk1_fridge.init(256);
    chunk2_fridge.init(128);
    chunk3_fridge.init(64);
    chunk4_fridge.init(32);
    chunk5_fridge.init(16);
    chunk6_fridge.init(8);

    // read options from disk
    do {
        auto configpath = GHGetConfigDir();
        if (!configpath) break;

        auto filepath = path_join(configpath, ".options");

        File f;
        if (f.init_read(filepath) != FILE_RESULT_OK)
            break;
        defer { f.cleanup(); };

        Serde serde;
        serde.init(&f);
        serde.read_type(&options, SERDE_OPTIONS);
    } while (0);

    world.use_nvim = options.enable_vim_mode;

    {
        auto go_binary_path = GHGetGoBinaryPath();
        if (!go_binary_path)
            cp_panic("Unable to find Go. Please make sure it's installed, and accessible from a Bash shell (with .bashrc).");
        defer { GHFree(go_binary_path); };
        cp_strcpy_fixed(world.go_binary_path, go_binary_path);
    }

    {
        // do we need world_mem anywhere else?
        // i assume we will have other things that "orchestrate" world
        SCOPED_MEM(&world_mem);
        message_queue.init();
    }

    {
        SCOPED_MEM(&world_mem);
        frameskips.init();
#ifdef DEBUG_MODE
        show_frameskips = true;
#endif
    }

    fzy_init();

    // init workspace
    {
        resizing_pane = -1;
        panes.init(LIST_FIXED, _countof(_panes), _panes);

        // if testing
        if (!world.window) {
            cp_strcpy_fixed(current_path, "/Users/bh/ide/gostuff");
        } else if (gargc >= 2) {
            cp_strcpy_fixed(current_path, gargv[1]);
        } else {
            Select_File_Opts opts; ptr0(&opts);
            opts.buf = current_path;
            opts.bufsize = _countof(current_path);
            opts.folder = true;
            opts.save = false;
            if (!let_user_select_file(&opts)) exit(0);
        }

        if (check_path(current_path) != CPR_DIRECTORY) {
            cp_panic("Unable to open selected folder.");
        }

        GHGitIgnoreInit(current_path);
        xplat_chdir(current_path);
    }

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

    indexer.init();
    if (use_nvim) nvim.init();
    dbg.init();
    history.init();

    navigation_queue.init(LIST_FIXED, _countof(_navigation_queue), _navigation_queue);

    fill_file_tree();

    error_list.height = 125;
    file_explorer.selection = NULL;

    {
        SCOPED_MEM(&ui_mem);
        if (!(::ui.init())) {
            cp_panic("Unable to initialize UI.");
        }
    }

    fswatch.init(current_path);

    // init ui shit
    init_global_colors();
    init_command_info_table();

    world.file_explorer.show = true;

#ifdef DEBUG_MODE
    if (!use_nvim) world.wnd_history.show = true;

    show_frame_index = false;
#endif
}

void World::start_background_threads() {
    indexer.start_background_thread();
    if (use_nvim) nvim.start_running();
    dbg.start_loop();

#if DEBUG_MODE
    // VSCode debugger frequently fails to break when I press break.  But, it
    // works if I set a breakpoint which is hit. So the asinine solution I've
    // devised is to create a background thread that runs in a loop, so I can
    // set a breakpoint in it at any time.
    auto microsoft_programmers_are_fucking_monkeys = [](void*) {
        while (true)
            sleep_milliseconds(1000);
    };

    {
        auto t = create_thread(microsoft_programmers_are_fucking_monkeys, NULL);
        if (!t)
            cp_panic("couldn't create thread");
        close_thread_handle(t);
    }
#endif
}

Pane* get_current_pane() {
    if (!world.panes.len) return NULL;

    return &world.panes[world.current_pane];
}

Editor* get_current_editor() {
    auto pane = get_current_pane();
    if (!pane) return NULL;

    return pane->get_current_editor();
}

Editor* find_editor(find_editor_func f) {
    for (auto&& pane : world.panes)
        For (pane.editors)
            if (f(&it))
                return &it;
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
    return focus_editor(path, new_cur2(-1, -1));
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

    if (world.current_pane != idx) {
        auto e = get_current_editor();
        if (e) e->trigger_escape();
    }

    world.current_pane = idx;

    if (world.use_nvim) {
        auto pane = get_current_pane();
        if (pane->current_editor != -1)
            pane->focus_editor_by_index(pane->current_editor);
        /*
        auto editor = pane->get_current_editor();
        if (editor)
            world.nvim.set_current_window(editor);
        */
    }

    world.cmd_unfocus_all_windows = true;
}

void init_goto_file() {
    SCOPED_MEM(&world.goto_file_mem);
    world.goto_file_mem.reset();

    auto &wnd = world.wnd_goto_file;
    ptr0(&wnd);

    wnd.filepaths = alloc_list<ccstr>();
    wnd.filtered_results = alloc_list<int>();

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
    wnd.show = true;
}

void kick_off_build(Build_Profile *build_profile) {
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
            build->done = true;
            build->build_itself_had_error = true;
            return;
        }

        enum {
            GH_BUILD_INACTIVE = 0,
            GH_BUILD_DONE,
            GH_BUILD_RUNNING,
        };

        for (;; sleep_milliseconds(100)) {
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

                err.mark = world.mark_fridge.alloc();
                build->errors.append(&err);
            }

            build->current_error = -1;
            build->done = true;
            build->started = false;

            if (build->errors.len == 0) {
                // world.error_list.show = false;
            }

            For (world.panes) {
                For (it.editors) {
                    auto &editor = it;
                    auto path = get_path_relative_to(it.filepath, world.current_path);

                    For (build->errors) {
                        if (!it.valid) continue;
                        if (!are_filepaths_equal(path, it.file)) continue;

                        auto pos = new_cur2(it.col - 1, it.row - 1);
                        editor.buf->mark_tree.insert_mark(MARK_BUILD_ERROR, pos, it.mark);
                    }
                }
            }
            break;
        }
    };

    world.build.thread = create_thread(do_build, build_profile);
}

void* get_native_window_handle() {
    if (!world.window) return NULL;

    return world.window->ns_window;
}

void filter_files() {
    auto &wnd = world.wnd_goto_file;

    wnd.filtered_results->len = 0;

    Timer t;
    // t.init("filter_files");

    u32 i = 0;
    For (*wnd.filepaths) {
        if (fzy_has_match(wnd.query, it))
            wnd.filtered_results->append(i);
        if (i++ > 1000)
            break;
    }

    // t.log("matching");

    fuzzy_sort_filtered_results(
        wnd.query,
        wnd.filtered_results,
        wnd.filepaths->len,
        [&](auto i) { return wnd.filepaths->at(i); }
    );

    // t.log("scoring");
}

void run_proc_the_normal_way(Process* proc, ccstr cmd) {
    proc->cleanup();
    proc->init();
    proc->dir = world.current_path;
    proc->run(cmd);
}

Editor* focus_editor_by_id(int editor_id, cur2 pos, bool pos_in_byte_format) {
    For (world.panes) {
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
    if (world.dbg.state_flag != DLV_STATE_INACTIVE) return false;

    return true;
}

void goto_file_and_pos(ccstr file, cur2 pos, bool pos_in_byte_format, Ensure_Cursor_Mode mode) {
    auto editor = focus_editor(file, pos, pos_in_byte_format);
    if (!editor) return; // TODO

    editor->ensure_cursor_on_screen_by_moving_view(mode);

    world.cmd_unfocus_all_windows = true;
}

void goto_jump_to_definition_result(Jump_To_Definition_Result *result) {
    world.history.save_latest();
    goto_file_and_pos(result->file, result->pos, true, ECM_GOTO_DEF);
}

Jump_To_Definition_Result *get_current_definition(ccstr *filepath, bool display_error, cur2 pos) {
    auto show_error = [&](ccstr msg) -> Jump_To_Definition_Result* {
        if (display_error)
            tell_user(msg, NULL);
        return NULL;
    };

    auto editor = get_current_editor();
    if (!editor)
        return show_error("Couldn't find anything under your cursor (you don't have an editor focused).");
    if (!editor->is_go_file)
        return show_error("Couldn't find anything under your cursor (you're not in a Go file).");

    defer { world.indexer.ui_mem.reset(); };

    Jump_To_Definition_Result *result = NULL;

    {
        SCOPED_MEM(&world.indexer.ui_mem);

        if (!world.indexer.acquire_lock(IND_READING, true))
            return show_error("The indexer is currently busy.");
        defer { world.indexer.release_lock(IND_READING); };

        if (pos.x == -1)
            pos = editor->cur;
        auto off = editor->cur_to_offset(pos);
        result = world.indexer.jump_to_definition(editor->filepath, new_cur2(off, -1));
    }

    if (!result || !result->decl || !result->decl->decl)
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
    for (auto&& pane : world.panes) {
        For (pane.editors) {
            // TODO: handle untitled files; and when we do, put it behind a
            // flag, because save_all_unsaved_files() is now depended on and we
            // can't break existing functionality
            if (it.is_untitled) continue;

            if (!path_has_descendant(world.current_path, it.filepath)) continue;

            it.handle_save(false);
        }
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
    auto path = alloc_list<FT_Node*>();
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

FT_Node *find_or_create_ft_node(ccstr relpath, bool is_directory) {
    auto ret = find_ft_node(relpath);
    if (ret) return ret;

    auto parent = find_ft_node(cp_dirname(relpath));
    if (!parent) return NULL;


    {
        SCOPED_MEM(&world.file_tree_mem);

        auto ret = alloc_object(FT_Node);
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
}

FT_Node *find_ft_node(ccstr relpath) {
    auto path = make_path(relpath);
    auto node = world.file_tree;
    For (*path->parts) {
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

        node = alloc_object(FT_Node);
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

void goto_next_error(int direction) {
    auto &b = world.build;

    bool has_valid = false;
    For (b.errors) {
        if (it.valid) {
            has_valid = true;
            break;
        }
    }

    if (!b.ready() || !has_valid) return;

    auto old = b.current_error;
    do {
        b.current_error += direction;
        if (b.current_error < 0)
            b.current_error = b.errors.len - 1;
        if (b.current_error >= b.errors.len)
            b.current_error = 0;
    } while (b.current_error != old && !b.errors[b.current_error].valid);

    goto_error(b.current_error);
}

void reload_file_subtree(ccstr relpath) {
    auto path = path_join(world.current_path, relpath);
    if (is_ignored_by_git(path))
        return;

    switch (check_path(path)) {
    case CPR_DIRECTORY:
        break;
    case CPR_NONEXISTENT: {
        auto node = find_ft_node(relpath);
        if (node)
            delete_ft_node(node, false);
        return;
    }
    case CPR_FILE:
        return;
    }

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

    For (*current_items.items()) {
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

        For (*new_files.items()) {
            SCOPED_MEM(&world.file_tree_mem);

            auto child = alloc_object(FT_Node);
            child->name = cp_strdup(it);
            child->is_directory = false;
            child->num_children = 0;
            child->parent = node;
            child->children = NULL;
            child->depth = node->depth + 1;
            child->open = false;
            add_child(child);
        }

        For (*new_directories.items()) {
            SCOPED_MEM(&world.file_tree_mem);

            // TODO: recurse into directory

            auto child = alloc_object(FT_Node);
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

bool move_autocomplete_cursor(Editor *ed, int direction) {
    auto &ac = ed->autocomplete;
    if (!ac.ac.results) return false;

    if (ac.selection == 0 && direction == -1)
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

    For (errors) {
        it.mark->cleanup();
        world.mark_fridge.free(it.mark);
    }
    errors.len = 0;

    mem.cleanup();
}

bool has_unsaved_files() {
    For (world.panes)
        For (it.editors)
            if (it.is_unsaved())
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

void kick_off_rename_identifier() {
    bool ok = false;

    auto &ind = world.indexer;
    if (!ind.acquire_lock(IND_READING)) {
        tell_user("The indexer is currently busy.", NULL);
        return;
    }

    defer { if (!ok) ind.release_lock(IND_READING); };

    if (!handle_unsaved_files()) return;

    auto thread_proc = [](void *param) {
        // TODO: put a sleep here so we can test out cancellation shit

        auto &wnd = world.wnd_rename_identifier;
        wnd.thread_mem.cleanup();
        wnd.thread_mem.init();
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

        For (*files) {
            auto filepath = it.filepath;

            File_Replacer fr;
            if (!fr.init(filepath, "refactor_rename")) continue;

            For (*it.references) {
                if (fr.done()) break;

                cur2 start, end;
                if (it.is_sel) {
                    start = it.sel_start;
                    end = it.sel_end;
                } else {
                    start = it.start;
                    end = it.end;
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
            }

            editor->disable_file_watcher_until = current_time_nano() + (2 * 1000000000);
        }

        // close the thread handle first so it doesn't try to kill the thread
        if (wnd.thread) {
            close_thread_handle(wnd.thread);
            wnd.thread = NULL;
        }
    };

    auto &wnd = world.wnd_rename_identifier;
    wnd.running = true;
    wnd.too_late_to_cancel = false;
    wnd.thread = create_thread(thread_proc, NULL);
    if (!wnd.thread) {
        tell_user("Unable to kick off Rename Identifier.", NULL);
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
    case CMD_SAVE_FILE: {
        auto editor = get_current_editor();
        if (!editor) return false;
        if (!editor->is_modifiable()) return false;
        return true;
    }

    case CMD_SAVE_ALL:
        For (world.panes)
            if (it.editors.len)
                return true;
        return false;

    case CMD_GO_TO_PREVIOUS_ERROR:
    case CMD_GO_TO_NEXT_ERROR:
        {
            auto &b = world.build;
            bool has_valid = false;
            For (b.errors) {
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
        return world.dbg.state_flag == DLV_STATE_INACTIVE;

    case CMD_CONTINUE:
        return world.dbg.state_flag == DLV_STATE_PAUSED;

    case CMD_FORMAT_FILE:
    case CMD_FORMAT_FILE_AND_ORGANIZE_IMPORTS:
        {
            auto editor = get_current_editor();
            return editor && editor->is_modifiable();
        }

    case CMD_FORMAT_SELECTION:
        return false;

    case CMD_DEBUG_TEST_UNDER_CURSOR:
        {
            if (world.dbg.state_flag != DLV_STATE_INACTIVE) return false;

            auto editor = get_current_editor();
            if (!editor) return false;
            if (!editor->is_go_file) return false;
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
                    if (!name->null)
                        ret = str_starts_with(name->string(), "Test");
                }

                return WALK_ABORT;
            });

            return ret;
        }

    case CMD_BREAK_ALL:
        return world.dbg.state_flag == DLV_STATE_RUNNING;

    case CMD_STOP_DEBUGGING:
        return world.dbg.state_flag != DLV_STATE_INACTIVE;

    case CMD_STEP_OVER:
    case CMD_STEP_INTO:
    case CMD_STEP_OUT:
        return world.dbg.state_flag == DLV_STATE_PAUSED;

    case CMD_UNDO:
    case CMD_REDO:
        // TODO: also check if we *can* undo/redo (to be done after we actually implement it)
        return get_current_editor();

    case CMD_GENERATE_IMPLEMENTATION:
    case CMD_FIND_REFERENCES:
    case CMD_FIND_IMPLEMENTATIONS:
    case CMD_FIND_INTERFACES:
        return get_current_editor();

    }

    return true;
}

ccstr get_command_name(Command cmd) {
    auto info = command_info_table[cmd];

    switch (cmd) {
    case CMD_SAVE_FILE: {
        auto editor = get_current_editor();
        if (editor) {
            if (editor->is_untitled)
                return "Save untitled file...";
            return cp_sprintf("Save %s...", cp_basename(editor->filepath));
        }
        return "Save file...";
    }
    case CMD_ENTER_LICENSE:
        if (world.auth.state == AUTH_REGISTERED)
            return "Re-enter License...";
        return "Enter License...";
    }

    return info.name;
}

void init_command_info_table() {
    auto k = [&](int mods, int key, ccstr name) {
        Command_Info ret;
        ret.mods = mods;
        ret.key = key;
        ret.name = name;
        return ret;
    };

    mem0(command_info_table, sizeof(command_info_table));

#if OS_WIN
    command_info_table[CMD_EXIT] = k(CP_MOD_ALT, CP_KEY_F4, "Exit");
#elif OS_MAC
    command_info_table[CMD_EXIT] = k(CP_MOD_CMD, CP_KEY_Q, "Quit");
#endif
    command_info_table[CMD_NEW_FILE] = k(CP_MOD_PRIMARY, CP_KEY_N, "New File");
    command_info_table[CMD_SAVE_FILE] = k(CP_MOD_PRIMARY, CP_KEY_S, "Save File");
    command_info_table[CMD_SAVE_ALL] = k(CP_MOD_PRIMARY | CP_MOD_SHIFT, CP_KEY_S, "Save All");
    command_info_table[CMD_SEARCH] = k(CP_MOD_PRIMARY | CP_MOD_SHIFT, CP_KEY_F, "Search");
    command_info_table[CMD_SEARCH_AND_REPLACE] = k(CP_MOD_PRIMARY | CP_MOD_SHIFT, CP_KEY_H, "Search and Replace");
    command_info_table[CMD_FILE_EXPLORER] = k(CP_MOD_PRIMARY | CP_MOD_SHIFT, CP_KEY_E, "File Explorer");
    command_info_table[CMD_GO_TO_FILE] = k(CP_MOD_PRIMARY, CP_KEY_P, "Go To File");
    command_info_table[CMD_GO_TO_SYMBOL] = k(CP_MOD_PRIMARY, CP_KEY_T, "Go To Symbol");
    command_info_table[CMD_GO_TO_NEXT_ERROR] = k(CP_MOD_ALT, CP_KEY_RIGHT_BRACKET, "Go To Next Error");
    command_info_table[CMD_GO_TO_PREVIOUS_ERROR] = k(CP_MOD_ALT, CP_KEY_LEFT_BRACKET, "Go To Previous Error");
    command_info_table[CMD_GO_TO_DEFINITION] = k(CP_MOD_PRIMARY, CP_KEY_G, "Go To Definition");
    command_info_table[CMD_FIND_REFERENCES] = k(CP_MOD_PRIMARY | CP_MOD_ALT, CP_KEY_R, "Find References");
    command_info_table[CMD_FORMAT_FILE] = k(CP_MOD_ALT | CP_MOD_SHIFT, CP_KEY_F, "Format File");
    command_info_table[CMD_FORMAT_FILE_AND_ORGANIZE_IMPORTS] = k(CP_MOD_ALT | CP_MOD_SHIFT, CP_KEY_O, "Format File and Organize Imports");
    command_info_table[CMD_RENAME] = k(CP_MOD_NONE, CP_KEY_F12, "Rename");
    command_info_table[CMD_BUILD] = k(CP_MOD_PRIMARY | CP_MOD_SHIFT, CP_KEY_B, "Build");
    command_info_table[CMD_CONTINUE] = k(CP_MOD_NONE, CP_KEY_F5, "Continue");
    command_info_table[CMD_START_DEBUGGING] = k(CP_MOD_NONE, CP_KEY_F5, "Start Debugging");
    command_info_table[CMD_STOP_DEBUGGING] = k(CP_MOD_SHIFT, CP_KEY_F5, "Stop Debugging");
    command_info_table[CMD_DEBUG_TEST_UNDER_CURSOR] = k(CP_MOD_NONE, CP_KEY_F6, "Debug Test Under Cursor");
    command_info_table[CMD_STEP_OVER] = k(CP_MOD_NONE, CP_KEY_F10, "Step Over");
    command_info_table[CMD_STEP_INTO] = k(CP_MOD_NONE, CP_KEY_F11, "Step Into");
    command_info_table[CMD_STEP_OUT] = k(CP_MOD_SHIFT, CP_KEY_F11, "Step Out");
    command_info_table[CMD_RUN_TO_CURSOR] = k(CP_MOD_SHIFT, CP_KEY_F10, "Run To Cursor");
    command_info_table[CMD_TOGGLE_BREAKPOINT] = k(CP_MOD_NONE, CP_KEY_F9, "Toggle Breakpoint");
    command_info_table[CMD_DELETE_ALL_BREAKPOINTS] = k(CP_MOD_SHIFT, CP_KEY_F9, "Delete All Breakpoints");
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
    command_info_table[CMD_OPTIONS] = k(CP_MOD_PRIMARY, CP_KEY_COMMA, "Options");
    command_info_table[CMD_ABOUT] = k(0, 0, "About");
    command_info_table[CMD_GENERATE_IMPLEMENTATION] = k(0, 0, "Generate Implementation");

    command_info_table[CMD_FIND_IMPLEMENTATIONS] = k(0, 0, "Find Implementations");
    command_info_table[CMD_FIND_INTERFACES] = k(0, 0, "Find Interfaces");
    command_info_table[CMD_UNDO] = k(CP_MOD_PRIMARY, CP_KEY_Z, "Undo");
    command_info_table[CMD_REDO] = k(CP_MOD_PRIMARY | CP_MOD_SHIFT, CP_KEY_Z, "Redo");
    command_info_table[CMD_DOCUMENTATION] = k(0, 0, "Documentation");
    command_info_table[CMD_VIEW_CALLER_HIERARCHY] = k(CP_MOD_PRIMARY, CP_KEY_I, "View Caller Hierarchy");
    command_info_table[CMD_VIEW_CALLEE_HIERARCHY] = k(CP_MOD_PRIMARY | CP_MOD_SHIFT, CP_KEY_I, "View Callee Hierarchy");
    command_info_table[CMD_BUY_LICENSE] = k(0, 0, "Buy a License");
    command_info_table[CMD_ENTER_LICENSE] = k(0, 0, "Enter License...");
}

void do_find_interfaces() {
    auto &wnd = world.wnd_find_interfaces;

    if (wnd.show && !wnd.done) return; // already in progress

    auto &ind = world.indexer;
    if (!ind.acquire_lock(IND_READING)) {
        tell_user("The indexer is currently busy.", NULL);
        return;
    }

    auto thread_proc = [](void *param) {
        auto &wnd = world.wnd_find_interfaces;
        wnd.thread_mem.cleanup();
        wnd.thread_mem.init();
        SCOPED_MEM(&wnd.thread_mem);

        defer { cancel_find_interfaces(); };

        // TODO: how do we handle errors?
        // right now it just freezes on "Searching..."
        auto results = ind.find_interfaces(wnd.declres, wnd.search_everywhere);
        if (!results) return;

        {
            SCOPED_MEM(&world.find_interfaces_mem);

            auto newresults = alloc_list<Find_Decl*>(results->len);
            For (*results) newresults->append(it.copy());

            wnd.results = newresults;
            wnd.current_import_path = cp_strdup(world.indexer.index.current_import_path);
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
        tell_user("Unable to kick off Find Interfaces.", NULL);
        return;
    }
}

void init_goto_symbol() {
    if (!world.indexer.acquire_lock(IND_READING, true)) return;
    defer { world.indexer.release_lock(IND_READING); };

    auto &wnd = world.wnd_goto_symbol;

    if (!wnd.fill_thread)
        kill_thread(wnd.fill_thread);

    wnd.query[0] = '\0';
    wnd.symbols = NULL;
    wnd.filtered_results = NULL;

    auto worker = [](void*) {
        auto &wnd = world.wnd_goto_symbol;

        SCOPED_MEM(&wnd.fill_thread_pool);

        auto symbols = alloc_list<Go_Symbol>();
        world.indexer.fill_goto_symbol(symbols);
        if (symbols->len == 0) return;

        {
            SCOPED_MEM(&world.goto_symbol_mem);
            wnd.symbols = alloc_list<Go_Symbol>(symbols->len);
            For (*symbols) wnd.symbols->append(it.copy());

            wnd.filtered_results = alloc_list<int>();
            wnd.current_import_path = cp_strdup(world.indexer.index.current_import_path);
        }

        wnd.fill_running = false;

        if (wnd.fill_thread) {
            auto t = wnd.fill_thread;
            wnd.fill_thread = NULL;
            close_thread_handle(t);
        }
    };

    wnd.fill_thread_pool.cleanup();
    wnd.fill_thread_pool.init();

    wnd.fill_time_started_ms = current_time_milli();
    wnd.fill_running = true;
    wnd.fill_thread = create_thread(worker, NULL);
    wnd.show = true;
}

void do_find_implementations() {
    auto &wnd = world.wnd_find_implementations;

    auto &ind = world.indexer;
    if (!ind.acquire_lock(IND_READING)) {
        tell_user("The indexer is currently busy.", NULL);
        return;
    }

    auto thread_proc = [](void *param) {
        auto &wnd = world.wnd_find_implementations;
        wnd.thread_mem.cleanup();
        wnd.thread_mem.init();
        SCOPED_MEM(&wnd.thread_mem);

        defer { cancel_find_implementations(); };

        auto results = ind.find_implementations(wnd.declres, wnd.search_everywhere);
        if (!results) return;

        {
            SCOPED_MEM(&world.find_implementations_mem);

            auto newresults = alloc_list<Find_Decl*>(results->len);
            For (*results) newresults->append(it.copy());

            wnd.results = newresults;
            wnd.current_import_path = cp_strdup(world.indexer.index.current_import_path);
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
        tell_user("Unable to kick off Find Implementations.", NULL);
        return;
    }
}

void handle_command(Command cmd, bool from_menu) {
    // make this a precondition (actually, I think it might already be)
    if (!is_command_enabled(cmd)) return;

    switch (cmd) {
    case CMD_BUY_LICENSE:
        open_webbrowser("https://codeperfect95.com/buy-license");
        break;

    case CMD_ENTER_LICENSE: {
        auto &wnd = world.wnd_enter_license;
        if (wnd.show) {
            wnd.cmd_focus = true;
        } else {
            wnd.email[0] = '\0';
            wnd.license[0] = '\0';
            wnd.show = true;
        }
        break;
    }

    case CMD_DOCUMENTATION:
        open_webbrowser("https://docs.codeperfect95.com/");
        break;

    case CMD_UNDO:
    case CMD_REDO: {
        // TODO: handle this for vim too; do we just use `u` and `C-r`?
        if (world.use_nvim) break;

        auto editor = get_current_editor();
        if (!editor) break;

        auto buf = editor->buf;
        auto pos = (cmd == CMD_UNDO ?  buf->hist_undo() : buf->hist_redo());

        if (pos.x != -1) {
            auto opts = default_move_cursor_opts();
            opts->is_user_movement = true;
            editor->raw_move_cursor(pos, opts);
        }

        break;
    }

    case CMD_NEW_FILE:
        get_current_pane()->open_empty_editor();
        break;

    case CMD_SAVE_FILE:
        {
            auto editor = get_current_editor();
            if (editor) editor->handle_save();
        }
        break;

    case CMD_SAVE_ALL:
        save_all_unsaved_files();
        break;

    case CMD_EXIT:
        world.window->should_close = true;
        break;

    case CMD_SEARCH:
    case CMD_SEARCH_AND_REPLACE:
        {
            auto &wnd = world.wnd_search_and_replace;
            if (wnd.show) {
                wnd.cmd_focus = true;
                wnd.focus_textbox = 1;
            }
            wnd.show = true;
            wnd.replace = (cmd == CMD_SEARCH_AND_REPLACE);
        }
        break;

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

    case CMD_GO_TO_FILE:
        if (world.wnd_goto_file.show) {
            if (from_menu)
                world.wnd_goto_file.show = false;
        } else {
            Timer t;
            t.init();
            init_goto_file();
            t.log("init_goto_file");
        }
        break;

    case CMD_GO_TO_SYMBOL:
        if (world.wnd_goto_symbol.show) {
            if (from_menu)
                world.wnd_goto_symbol.show = false;
        } else {
            init_goto_symbol();
        }
        break;

    case CMD_GO_TO_NEXT_ERROR:
        goto_next_error(1);
        break;

    case CMD_GO_TO_PREVIOUS_ERROR:
        goto_next_error(-1);
        break;

    case CMD_GO_TO_DEFINITION:
        handle_goto_definition();
        break;

    case CMD_FIND_REFERENCES: {
        auto &wnd = world.wnd_find_references;

        auto result = get_current_definition(NULL, true);
        if (!result) break;
        if (!result->decl) break;

        world.find_references_mem.reset();
        {
            SCOPED_MEM(&world.find_references_mem);
            wnd.declres = result->decl->copy_decl();
        }

        auto &ind = world.indexer;
        if (!ind.acquire_lock(IND_READING)) {
            tell_user("The indexer is currently busy.", NULL);
            break;
        }

        auto thread_proc = [](void *param) {
            auto &wnd = world.wnd_find_references;
            wnd.thread_mem.cleanup();
            wnd.thread_mem.init();
            SCOPED_MEM(&wnd.thread_mem);

            // kinda confusing that cancel_find_references is responsible for wnd.done = true
            defer { cancel_find_references(); };

            auto files = world.indexer.find_references(wnd.declres, false);
            if (!files) return;

            {
                SCOPED_MEM(&world.find_references_mem);

                auto newfiles = alloc_list<Find_References_File>(files->len);
                For (*files) newfiles->append(it.copy());

                wnd.results = newfiles;
                wnd.current_import_path = cp_strdup(world.indexer.index.current_import_path);
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
            tell_user("Unable to kick off Find References.", NULL);
            break;
        }
        break;
    }

    case CMD_FORMAT_FILE: {
        auto editor = get_current_editor();
        if (!editor) break;
        if (!editor->is_modifiable()) break;

        editor->format_on_save(GH_FMT_GOIMPORTS);
        break;
    }

    case CMD_FORMAT_FILE_AND_ORGANIZE_IMPORTS: {
        auto editor = get_current_editor();
        if (!editor) break;
        if (!editor->is_modifiable()) break;

        if (editor->optimize_imports())
            editor->format_on_save(GH_FMT_GOIMPORTS);
        else
            editor->format_on_save(GH_FMT_GOIMPORTS_WITH_AUTOIMPORT);
        break;
    }

    case CMD_FORMAT_SELECTION:
        // TODO
        // how do we even get the visual selection?
        break;

    case CMD_RENAME: {
        ccstr filepath = NULL;

        // TODO: this should be a "blocking" modal, like it disables activity in rest of IDE
        auto result = get_current_definition(&filepath, true);
        if (!result) return;
        if (result->decl->decl->type == GODECL_IMPORT) {
            tell_user("Sorry, we're currently not yet able to rename imports.", NULL);
            return;
        }

        world.rename_identifier_mem.reset();
        SCOPED_MEM(&world.rename_identifier_mem);

        auto &wnd = world.wnd_rename_identifier;
        wnd.rename_to[0] = '\0';
        wnd.show = true;
        wnd.cmd_focus = true;
        wnd.running = false;
        wnd.declres = result->decl->copy_decl();
        break;
    }

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

    case CMD_TOGGLE_BREAKPOINT:
        {
            auto editor = get_current_editor();
            if (editor) {
                world.dbg.push_call(DLVC_TOGGLE_BREAKPOINT, [&](auto call) {
                    call->toggle_breakpoint.filename = cp_strdup(editor->filepath);
                    call->toggle_breakpoint.lineno = editor->cur.y + 1;
                });
            }
        }
        break;

    case CMD_DELETE_ALL_BREAKPOINTS:
        {
            auto res = ask_user_yes_no(
                "Are you sure you want to delete all breakpoints?",
                NULL,
                "Delete",
                "Don't Delete"
            );

            if (res == ASKUSER_YES)
                world.dbg.push_call(DLVC_DELETE_ALL_BREAKPOINTS);
        }
        break;

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

    case CMD_DEBUG_PROFILES:
        ui.open_project_settings();
        world.wnd_project_settings.focus_debug_profiles = true;
        break;

    case CMD_ABOUT:
        world.wnd_about.show = true;
        break;

    case CMD_GENERATE_IMPLEMENTATION:
        {
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
            if (!result) break;
            if (!result->decl) break;

            auto decl = result->decl->decl;
            if (!decl->gotype) break;

            found_something = true;

            if (decl->type != GODECL_TYPE) {
                tell_user("The selected object is not a type.", "Error");
                break;
            }

            if (!ind.acquire_lock(IND_READING, true)) break;
            defer { ind.release_lock(IND_READING); };

            world.generate_implementation_mem.reset();

            auto gofile = ind.find_gofile_from_ctx(result->decl->ctx);
            if (!gofile) break;

            {
                SCOPED_MEM(&world.generate_implementation_mem);

                wnd.file_hash_on_open = gofile->hash;
                wnd.declres = result->decl->copy_decl();
                wnd.filtered_results = alloc_list<int>();

                auto gotype = wnd.declres->decl->gotype;
                wnd.selected_interface = (gotype->type == GOTYPE_INTERFACE);
            }

            auto worker = [](void*) {
                auto &wnd = world.wnd_generate_implementation;

                SCOPED_MEM(&wnd.fill_thread_pool);

                auto symbols = alloc_list<Go_Symbol>();
                ind.fill_generate_implementation(symbols, wnd.selected_interface);
                if (symbols->len == 0) return;

                {
                    SCOPED_MEM(&world.generate_implementation_mem);
                    wnd.symbols = alloc_list<Go_Symbol>(symbols->len);
                    For (*symbols) wnd.symbols->append(it.copy());
                }

                wnd.fill_running = false;
                if (wnd.fill_thread) {
                    auto t = wnd.fill_thread;
                    wnd.fill_thread = NULL;
                    close_thread_handle(t);
                }
            };

            wnd.fill_thread_pool.cleanup();
            wnd.fill_thread_pool.init();

            wnd.fill_time_started_ms = current_time_milli();
            wnd.fill_running = true;
            wnd.fill_thread = create_thread(worker, NULL);
            wnd.show = true;
        }
        break;

    case CMD_VIEW_CALLEE_HIERARCHY: {
        auto &wnd = world.wnd_callee_hierarchy;

        auto &ind = world.indexer;
        if (!ind.acquire_lock(IND_READING)) {
            tell_user("The indexer is currently busy.", NULL);
            break;
        }

        bool ok = false;
        defer { if (!ok && ind.status == IND_READING) ind.release_lock(IND_READING); };

        auto editor = get_current_editor();
        if (!editor) break;
        if (!editor->is_go_file) break;

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
            wnd.thread_mem.init();
            SCOPED_MEM(&wnd.thread_mem);

            defer { cancel_callee_hierarchy(); };

            auto results = ind.generate_callee_hierarchy(wnd.declres);
            if (!results) return;

            {
                SCOPED_MEM(&world.callee_hierarchy_mem);

                auto newresults = alloc_list<Call_Hier_Node>(results->len);
                For (*results) newresults->append(it.copy());

                wnd.results = newresults;
                wnd.current_import_path = cp_strdup(world.indexer.index.current_import_path);
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
            tell_user("Unable to kick off View Call Hierarchy.", NULL);
            break;
        }
        break;
    }

    case CMD_VIEW_CALLER_HIERARCHY: {
        auto &wnd = world.wnd_caller_hierarchy;

        auto &ind = world.indexer;
        if (!ind.acquire_lock(IND_READING)) {
            tell_user("The indexer is currently busy.", NULL);
            break;
        }

        bool ok = false;
        defer { if (!ok && ind.status == IND_READING) ind.release_lock(IND_READING); };

        auto editor = get_current_editor();
        if (!editor) break;
        if (!editor->is_go_file) break;

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
            wnd.thread_mem.init();
            SCOPED_MEM(&wnd.thread_mem);

            defer { cancel_caller_hierarchy(); };

            auto results = ind.generate_caller_hierarchy(wnd.declres);
            if (!results) return;

            {
                SCOPED_MEM(&world.caller_hierarchy_mem);

                auto newresults = alloc_list<Call_Hier_Node>(results->len);
                For (*results) newresults->append(it.copy());

                wnd.results = newresults;
                wnd.current_import_path = cp_strdup(world.indexer.index.current_import_path);
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
            tell_user("Unable to kick off View Call Hierarchy.", NULL);
            break;
        }

        ok = true;
        break;
    }

    case CMD_FIND_IMPLEMENTATIONS: {
        auto &wnd = world.wnd_find_implementations;

        auto result = get_current_definition(NULL, true);
        if (!result) break;
        if (!result->decl) break;

        auto decl = result->decl->decl;
        if (!decl) break;

        if (decl->type != GODECL_TYPE || decl->gotype->type != GOTYPE_INTERFACE) {
            tell_user("The selected object is not an interface.", "Error");
            break;
        }

        auto specs = decl->gotype->interface_specs;
        if (!specs || specs->len == 0) {
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
        if (!result) break;
        if (!result->decl) break;

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
    }
}

void open_add_file_or_folder(bool folder, FT_Node *dest) {
    if (!dest) dest = world.file_explorer.selection;

    FT_Node *node = NULL;
    auto &wnd = world.wnd_add_file_or_folder;

    auto is_root = [&]() {
        node = dest;

        if (!node) return true;
        if (node->is_directory) return false;

        node = node->parent;
        return (!node->parent);
    };

    wnd.location_is_root = is_root();
    if (!wnd.location_is_root)
        cp_strcpy_fixed(wnd.location, ft_node_to_path(node));

    wnd.folder = folder;
    wnd.show = true;
    wnd.name[0] = '\0';
}

void do_generate_implementation() {
    auto &wnd = world.wnd_generate_implementation;
    if (wnd.filtered_results->len == 0) return;

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

    Table<ccstr> import_table; import_table.init();
    Table<ccstr> import_table_r; import_table_r.init();

    auto dest_gofile = ind.find_gofile_from_ctx(dest->ctx);
    For (*dest_gofile->imports) {
        auto package_name = ind.get_import_package_name(&it);

        import_table.set(it.import_path, package_name);
        import_table_r.set(package_name, it.import_path);
    }

    auto src_gotype = src->decl->gotype;
    auto dest_gotype = dest->decl->gotype;

    if (src_gotype->type == GOTYPE_BUILTIN)
        src_gotype = src_gotype->builtin_underlying_type;

    if (!src_gotype) return;
    if (src_gotype->type != GOTYPE_INTERFACE) return;

    if (!dest_gotype) return;
    if (dest_gotype->type == GOTYPE_INTERFACE) return;

    auto src_methods = ind.list_interface_methods(src->wrap(src_gotype));
    if (!src_methods) return;

    auto dest_methods = alloc_list<Goresult>();
    if (!ind.list_type_methods(dest->decl->name, dest->ctx->import_path, dest_methods))
        return;

    auto methods_to_add = alloc_list<Goresult>();
    For (*src_methods) {
        auto &srcmeth = it;
        For (*dest_methods)
            if (streq(srcmeth.decl->name, it.decl->name))
                if (ind.are_gotypes_equal(src->wrap(srcmeth.decl->gotype), dest->wrap(it.decl->gotype)))
                    goto skip;
        methods_to_add->append(&it);
    skip:;
    }

    auto type_name = dest->decl->name;

    auto generate_type_var = [&]() {
        auto s = alloc_list<char>();
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

    auto imports_to_add = alloc_list<Import_To_Add>();
    auto errors = alloc_list<ccstr>();

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
            auto package_name = import_table.get(import_path, &found);
            if (!found) {
                auto pkg = ind.find_up_to_date_package(import_path);
                if (pkg) {
                    auto actual_name = pkg->package_name;
                    bool alias_package = false;

                    for (int i = 0;; i++) {
                        auto new_name = actual_name;
                        if (i)
                            new_name = cp_sprintf("%s%d", new_name, i);

                        import_table_r.get(new_name, &found);
                        if (!found) {
                            package_name = new_name;
                            alias_package = i > 0;
                            break;
                        }
                    }

                    import_table.set(import_path, package_name);
                    import_table_r.set(package_name, import_path);

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

    For (*methods_to_add) {
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
        For (*sig.params) {
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

            bool first = true;
            For (*sig.result) {
                if (first)
                    first = false;
                else
                    rend.write(", ");

                auto typestr = render_type(make_goresult(it.gotype, method_ctx), it.name);
                if (!typestr) {
                    error = true;
                    goto done_writing;
                }

                rend.write("%s", typestr);
            }

            if (sig.result->len > 1) rend.write(")");
        }

        rend.write(" {\n\tpanic(\"not implemented\")\n}");
    }

done_writing:

    if (error) {
        // TODO: notify user of errors?
        return;
    }

    auto s = rend.finish();

    // we now know that src and dest accurately reflect what's on disk

    Buffer buf; buf.init(MEM, true, false);
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
        auto iter = alloc_object(Parser_It);
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
            if (speclist->type() != TS_IMPORT_SPEC_LIST)
                return;
            FOR_NODE_CHILDREN (speclist) {
                rend.write("\t%s\n", it->string());
            }
        }

        For (*imports_to_add) {
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

        auto new_contents = GHFmtFinish(GH_FMT_GOIMPORTS);
        if (!new_contents) return;
        defer { GHFree(new_contents); };

        auto new_contents_len = strlen(new_contents);
        if (new_contents_len == 0) return;

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

        auto chars = alloc_list<uchar>();
        if (!imports_node) {
            // add two newlines, it's going after the package decl
            chars->append('\n');
            chars->append('\n');
        }

        int lines_in_new = 0;
        {
            auto ustr = cstr_to_ustr(new_contents);
            For (*ustr) {
                chars->append(it);
                if (it == '\n') lines_in_new++;
            }
        }

        if (start != old_end)
            buf.remove(start, old_end);
        buf.insert(start, chars->items, chars->len);
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
        editor->reload_file();

        auto c = editor->cur;
        auto newc = new_cur2(c.x, c.y + cursor_offset);
        editor->move_cursor(newc);
    }

    // TODO: refresh existing editors if `filepath` is open
    // tho i think writing to disk just automatically does that?
    // or should we not write to disk for editors that are open
}

void fuzzy_sort_filtered_results(ccstr query, List<int> *list, int total_results, fn<ccstr(int)> get_name) {
    auto names = alloc_array(ccstr, total_results);
    auto scores = alloc_array(double, total_results);

    // Timer t;

    For (*list) {
        names[it] = get_name(it);
        scores[it] = fzy_match(query, names[it]);
    }

    list->sort([&](int* pa, int* pb) {
        auto a = scores[*pa];
        auto b = scores[*pb];

        if (a < b) return 1;
        if (a > b) return -1;

        auto alen = strlen(names[*pa]);
        auto blen = strlen(names[*pb]);
        return alen < blen ? -1 : (alen > blen ? 1 : 0);
    });
}

ccstr get_auth_filepath() {
    auto configpath = GHGetConfigDir();
    if (!configpath) cp_panic("Unable to open config directory.");
    return path_join(configpath, ".auth");
}

void read_auth() {
    File f;
    if (f.init_read(get_auth_filepath()) != FILE_RESULT_OK) {
        ptr0(&world.auth);
        return;
    }
    defer { f.cleanup(); };
    f.read((char*)&world.auth, sizeof(world.auth));
}

void write_auth() {
    File f;
    if (f.init_write(get_auth_filepath()) != FILE_RESULT_OK)
        return;
    defer { f.cleanup(); };
    f.write((char*)&world.auth, sizeof(world.auth));
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
