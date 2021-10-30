#include "world.hpp"
#include "ui.hpp"
#include "fzy_match.h"
#include "set.hpp"

#if OS_WIN
#define GLFW_EXPOSE_NATIVE_WIN32
#elif OS_MAC
#define GLFW_EXPOSE_NATIVE_COCOA
#endif

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

World world;

u64 post_insert_dotrepeat_time = 0;
int gargc = 0;
char **gargv = NULL;

bool is_ignored_by_git(ccstr path) {
    // go-gitignore crashes when path == base path
    if (are_filepaths_equal(path, world.current_path))
        return false;
    return GHGitIgnoreCheckFile((char*)path);
}

void History::actually_push(int editor_id, cur2 pos) {
    assert_main_thread();

    auto editor = world.find_editor_by_id(editor_id);

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

        auto prev_editor = world.find_editor_by_id(prev.editor_id);
        if (prev_editor == NULL) break;

        if (prev_editor->cur == prev.pos) break;

        actually_push(prev_editor->id, prev_editor->cur);
    } while (0);

    check_marks();

    actually_push(editor_id, pos);

    check_marks();
}

void History::actually_go(History_Loc *it) {
    assert_main_thread();

    auto editor = world.find_editor_by_id(it->editor_id);
    if (editor == NULL) return;
    if (!editor->is_nvim_ready()) return;

    auto pos = it->mark->pos(); // it->pos

    world.navigation_queue.len = 0;
    auto dest = world.navigation_queue.append();
    dest->editor_id = it->editor_id;
    dest->pos = pos;

    world.focus_editor_by_id(it->editor_id, pos);
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
    if (upper == -1) upper = top;

    for (auto i = start; i != upper; i = inc(i)) {
        auto it = ring[i].mark;
        if (!is_mark_valid(it)) continue;

        // check that mark node contains the mark
        bool found = false;
        for (auto m = it->node->marks; m != NULL; m = m->next) {
            if (m == it) {
                found = true;
                break;
            }
        }
        if (!found) our_panic("mark got detached from its node somehow");

        // check that mark node is still in root
        auto node = it->node;
        while (node->parent != NULL)
            node = node->parent;
        if (it->tree->root != node)
            our_panic("mark node is detached from root!");
    }
}

bool History::go_backward() {
    assert_main_thread();
    check_marks();

    if (curr == start) return false;

    {
        auto editor = world.get_current_editor();
        auto &it = ring[dec(curr)];

        // handle the case of: open file a, open file b, move down 4 lines (something < threshold),
        // go back, go forward, cursor will now be on line 0 instead of line 4
        if (editor == NULL || it.editor_id != editor->id || it.pos != editor->cur) {
            if (editor != NULL) {
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

    auto editor = world.get_current_editor();
    if (editor == NULL) return;

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
    if (mark == NULL) {
        return;
    }

    mark->cleanup();
    world.mark_fridge.free(mark);
    mark = NULL;
}

bool exclude_from_file_tree(ccstr path) {
    if (is_ignored_by_git(path)) return true;

    auto filename = our_basename(path);
    if (streq(filename, ".git")) return true;
    if (streq(filename, ".cpproj")) return true;
    if (streq(filename, ".cpdb")) return true;
    if (streq(filename, ".cpdb.tmp")) return true;
    if (str_ends_with(filename, ".exe")) return true;

    return false;
}

void World::fill_file_tree() {
    SCOPED_MEM(&file_tree_mem);
    file_tree_mem.reset();

    // invalidate pointers
    file_explorer.selection = NULL;
    file_explorer.last_file_copied = NULL;
    file_explorer.last_file_cut = NULL;

    file_tree = alloc_object(FT_Node);
    file_tree->is_directory = true;
    file_tree->depth = -1;

    u32 depth = 0;

    GHGitIgnoreInit(current_path);

    fn<void(ccstr, FT_Node*)> recur = [&](ccstr path, FT_Node *parent) {
        FT_Node *last_child = parent->children;

        list_directory(path, [&](Dir_Entry *ent) {
            do {
                auto fullpath = path_join(path, ent->name);
                if (exclude_from_file_tree(fullpath)) break;

                auto file = alloc_object(FT_Node);
                file->name = our_strcpy(ent->name);
                file->is_directory = (ent->type & FILE_TYPE_DIRECTORY);
                file->num_children = 0;
                file->parent = parent;
                file->children = NULL;
                file->depth = depth;
                file->open = false;
                file->next = NULL;

                if (last_child == NULL) {
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

    recur(current_path, file_tree);
}

bool copy_file(ccstr src, ccstr dest) {
    auto fm = map_file_into_memory(src);
    if (fm == NULL) return false;
    defer { fm->cleanup(); };

    // TODO: map this into memory too?
    File f;
    if (f.init(dest, FILE_MODE_WRITE, FILE_CREATE_NEW) != FILE_RESULT_SUCCESS)
        return false;
    defer { f.cleanup(); };

    return f.write((char*)fm->data, fm->len);
}

void shell(ccstr s, ccstr dir) {
    Process p;
    p.init();
    p.dir = dir;
    p.run(s);
    while (p.status() == PROCESS_WAITING) continue;

    if (p.exit_code != 0) {
        print("`%s` code %d, output below:", p.exit_code);
        char ch;
        while (p.read1(&ch)) {
            printf("%c", ch);
        }
        our_panic("askldjfhalkfh");
    }
}

void prepare_workspace() {
    auto p = [&](ccstr f) {
        return path_join(world.current_path, f);
    };

    if (!copy_file(p("main.go.bak"), p("main.go")))
        our_panic("failed to copy main.go.bak");

    delete_rm_rf(p("db.tmp"));
    delete_rm_rf(p("go.mod"));
    delete_rm_rf(p("go.sum"));

    shell("go mod init github.com/invrainbow/life", world.current_path);
    shell("go mod tidy", world.current_path);
}

void World::init_workspace() {
    resizing_pane = -1;

    panes.init(LIST_FIXED, _countof(_panes), _panes);

#if 1
    // if testing
    if (world.window == NULL) {
        strcpy_safe(current_path, _countof(current_path), "/Users/bh/ide/api");
    } else if (gargc >= 2) {
        strcpy_safe(current_path, _countof(current_path), gargv[1]);
    } else {
        Select_File_Opts opts; ptr0(&opts);
        opts.buf = current_path;
        opts.bufsize = _countof(current_path);
        opts.folder = true;
        opts.save = false;
        if (!let_user_select_file(&opts)) exit(0);
    }
#else
    {
        SCOPED_FRAME();

        File f;
        f.init(".cpdefaultfolder", FILE_MODE_READ, FILE_OPEN_EXISTING);
        defer { f.cleanup(); };

        List<char> chars;
        chars.init();

        char ch;
        while (f.read(&ch, 1) && ch != '\0' && ch != '\r' && ch != '\n')
            chars.append(ch);

        chars.append('\0');
        strcpy_safe(current_path, _countof(current_path), normalize_path_sep(chars.items));
    }
#endif

    GHGitIgnoreInit(current_path);
    xplat_chdir(current_path);

    project_settings.read(path_join(current_path, ".cpproj"));

    /*
    if (project_settings.build_command[0] == '\0')
        strcpy_safe(project_settings.build_command, _countof(project_settings.build_command), "go build --gcflags=\"all=-N -l\" ");
    */
}

void World::init(GLFWwindow *_wnd) {
    ptr0(this);

    window = _wnd;

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
#undef init_mem

    // use frame_mem as the default mem
    MEM = &frame_mem;

    /*
    auto get_path = [&]() -> ccstr {
        Process proc;
        defer { proc.cleanup(); };
        proc.run("echo $PATH");

        Text_Renderer r;
        r.init();
        char ch;
        while (proc.read1(&ch)) r.writechar(ch);
        return r.finish();
    };

    tell_user(our_sprintf("PATH = %s", get_path()), NULL);
    */

    global_mark_tree_lock.init();

    mark_fridge.init(512);
    mark_node_fridge.init(512);

    chunk0_fridge.init(512);
    chunk1_fridge.init(256);
    chunk2_fridge.init(128);
    chunk3_fridge.init(64);
    chunk4_fridge.init(32);
    chunk5_fridge.init(16);
    chunk6_fridge.init(8);

    load_gohelper();

    if (!GHInitConfig())
        our_panic("Unable to load ~/.cpconfig. Please make sure the file exists and is formatted properly (see Getting Started in our docs).");

    {
        auto go_binary_path = GHGetGoBinaryPath();
        if (go_binary_path == NULL)
            our_panic("Please set your Go binary path in ~/.cpconfig.");
        defer { GHFree(go_binary_path); };
        strcpy_safe(world.go_binary_path, _countof(world.go_binary_path), go_binary_path);
    }

    {
        auto delve_path = GHGetDelvePath();
        if (delve_path != NULL) {
            defer { GHFree(delve_path); };
            strcpy_safe(world.delve_path, _countof(world.delve_path), delve_path);
        }
    }

    {
        // do we need world_mem anywhere else?
        // i assume we will have other things that "orchestrate" world
        SCOPED_MEM(&world_mem);
        message_queue.init();
    }

    fzy_init();

    // prepare_workspace();
    // build helper
    // shell("go build helper.go", "w:/helper");

    use_nvim = true;

    init_workspace();
    indexer.init();
    nvim.init();
    dbg.init();
    history.init();

    navigation_queue.init(LIST_FIXED, _countof(_navigation_queue), _navigation_queue);

    fill_file_tree();

    error_list.height = 125;
    file_explorer.selection = NULL;

    {
        SCOPED_MEM(&ui_mem);
        ::ui.init();
    }

    fswatch.init(current_path);

    init_global_colors();

    world.file_explorer.show = true;
}

void World::start_background_threads() {
    indexer.start_background_thread();
    nvim.start_running();
    dbg.start_loop();
}

Pane* World::get_current_pane() {
    if (panes.len == 0) return NULL;

    return &panes[current_pane];
}

Editor* World::get_current_editor() {
    auto pane = get_current_pane();
    if (pane == NULL) return NULL;

    return pane->get_current_editor();
}

Editor* World::find_editor(find_editor_func f) {
    for (auto&& pane : panes)
        For (pane.editors)
            if (f(&it))
                return &it;
    return NULL;
}

Editor* World::find_editor_by_id(u32 id) {
    auto is_match = [&](auto it) { return it->id == id; };
    return find_editor(is_match);
}

Editor* World::find_editor_by_filepath(ccstr filepath) {
    auto is_match = [&](auto it) {
        return are_filepaths_same_file(it->filepath, filepath);
    };
    return find_editor(is_match);
}

Editor *World::focus_editor(ccstr path) {
    return focus_editor(path, new_cur2(-1, -1));
}

Editor *World::focus_editor(ccstr path, cur2 pos) {
    for (auto&& pane : panes) {
        for (u32 i = 0; i < pane.editors.len; i++) {
            auto &it = pane.editors[i];
            if (are_filepaths_same_file(path, it.filepath)) {
                activate_pane(&pane);
                pane.focus_editor_by_index(i, pos);
                return &it;
            }
        }
    }
    return get_current_pane()->focus_editor(path, pos);
}

void World::activate_pane(Pane *pane) {
    activate_pane_by_index(pane - panes.items);
}

void World::activate_pane_by_index(u32 idx) {
    if (idx > panes.len) return;

    if (idx == panes.len) {
        auto panes_width = ::ui.panes_area.w;

        float new_width = panes_width;
        if (panes.len > 0)
            new_width /= panes.len;

        auto pane = panes.append();
        pane->init();
        pane->width = new_width;
    }

    if (current_pane != idx) {
        auto e = world.get_current_editor();
        if (e != NULL) e->trigger_escape();
    }

    current_pane = idx;

    if (world.use_nvim) {
        auto pane = get_current_pane();
        if (pane->current_editor != -1)
            pane->focus_editor_by_index(pane->current_editor);
        /*
        auto editor = pane->get_current_editor();
        if (editor != NULL)
            world.nvim.set_current_window(editor);
        */
    }

    // we might have to add a "are we inside imgui" check
    ImGui::SetWindowFocus(NULL);
}

void init_goto_file() {
    SCOPED_MEM(&world.goto_file_mem);
    world.goto_file_mem.reset();

    auto &wnd = world.wnd_goto_file;
    ptr0(&wnd);

    wnd.filepaths = alloc_list<ccstr>();
    wnd.filtered_results = alloc_list<int>();

    fn<void(FT_Node*, ccstr)> fill_files = [&](auto node, auto path) {
        for (auto it = node->children; it != NULL; it = it->next) {
            auto isdir = it->is_directory;

            // if (isdir && node->parent == NULL && streq(it->name, ".cp")) return;

            auto relpath = path[0] == '\0' ? our_strcpy(it->name) : path_join(path, it->name);
            if (isdir)
                fill_files(it, relpath);
            else
                wnd.filepaths->append(relpath);
        }
    };

    fill_files(world.file_tree, "");
    wnd.show = true;
}

void init_goto_symbol() {
    SCOPED_MEM(&world.goto_symbol_mem);
    world.goto_symbol_mem.reset();

    if (!world.indexer.acquire_lock(IND_READING, true)) return;
    defer { world.indexer.release_lock(IND_READING); };

    world.indexer.fill_goto_symbol();
    world.wnd_goto_symbol.show = true;
}

void kick_off_build(Build_Profile *build_profile) {
    if (build_profile == NULL)
        build_profile = project_settings.get_active_build_profile();

    world.build.cleanup();
    world.build.init();

    auto do_build = [](void* param) {
        auto build_profile = (Build_Profile*)param;
        auto build = &world.build;

        auto& indexer = world.indexer;

        SCOPED_MEM(&build->mem);

        build->id = world.next_build_id++;
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
            defer { if (errors != NULL) GHFreeBuildStatus(errors, num_errors); };

            if (status != GH_BUILD_DONE) continue;

            for (u32 i = 0; i < num_errors; i++) {
                Build_Error err;

                err.message = our_strcpy(errors[i].text);
                err.valid = errors[i].is_valid;

                if (err.valid) {
                    err.file = our_strcpy(errors[i].filename);
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
    if (world.window == NULL) return NULL;

#if OS_WIN
    return (void*)glfwGetWin32Window(world.window);
#elif OS_MAC
    return (void*)glfwGetCocoaWindow(world.window);
#else
    return NULL;
#endif
}

void prompt_delete_all_breakpoints() {
    auto res = ask_user_yes_no(
        "Are you sure you want to delete all breakpoints?",
        NULL,
        "Delete",
        "Don't Delete"
    );

    if (res != ASKUSER_YES) return;
    world.dbg.push_call(DLVC_DELETE_ALL_BREAKPOINTS);
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
        i++;
    }

    // t.log("matching");

    auto scores = alloc_array(double, wnd.filepaths->len);
    auto scores_saved = alloc_array(bool, wnd.filepaths->len);

    auto get_score = [&](int i) {
        if (!scores_saved[i]) {
            scores[i] = fzy_match(wnd.query, wnd.filepaths->at(i));
            scores_saved[i] = true;
        }
        return scores[i];
    };

    wnd.filtered_results->sort([&](int *pa, int *pb) {
        auto a = get_score(*pa);
        auto b = get_score(*pb);
        return a < b ? 1 : (a > b ? -1 : 0);  // reverse
    });

    // t.log("scoring");
}

void filter_symbols() {
    auto &wnd = world.wnd_goto_symbol;

    wnd.filtered_results->len = 0;

    Timer t;
    // t.init("filter_symbols");

    u32 i = 0;
    For (*wnd.symbols) {
        if (fzy_has_match(wnd.query, it))
            wnd.filtered_results->append(i);
        i++;
    }

    // t.log("matching");

    auto scores = alloc_array(double, wnd.symbols->len);
    auto scores_saved = alloc_array(bool, wnd.symbols->len);

    auto get_score = [&](int i) {
        if (!scores_saved[i]) {
            scores[i] = fzy_match(wnd.query, wnd.symbols->at(i));
            scores_saved[i] = true;
        }
        return scores[i];
    };

    wnd.filtered_results->sort([&](int *pa, int *pb) {
        auto a = get_score(*pa);
        auto b = get_score(*pb);
        return a < b ? 1 : (a > b ? -1 : 0);  // reverse
    });

    // t.log("scoring");
}

void run_proc_the_normal_way(Process* proc, ccstr cmd) {
    proc->cleanup();
    proc->init();
    proc->dir = world.current_path;
    proc->run(cmd);
}

Editor* World::focus_editor_by_id(int editor_id, cur2 pos) {
    For (panes) {
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

void goto_file_and_pos(ccstr file, cur2 pos, Ensure_Cursor_Mode mode) {
    auto editor = world.focus_editor(file, pos);
    if (editor == NULL) return; // TODO

    editor->ensure_cursor_on_screen_by_moving_view(mode);

    ImGui::SetWindowFocus(NULL);
}

void goto_jump_to_definition_result(Jump_To_Definition_Result *result) {
    world.history.save_latest();
    goto_file_and_pos(result->file, result->pos, ECM_GOTO_DEF);
}

Jump_To_Definition_Result *get_current_definition(ccstr *filepath, bool display_error) {
    auto show_error = [&](ccstr msg) -> Jump_To_Definition_Result* {
        if (display_error)
            tell_user(msg, NULL);
        return NULL;
    };

    auto editor = world.get_current_editor();
    if (editor == NULL)
        return show_error("Couldn't find anything under your cursor to rename (you don't have an editor focused).");
    if (!editor->is_go_file)
        return show_error("Couldn't find anything under your cursor to rename (you're not in a Go file).");

    defer { world.indexer.ui_mem.reset(); };

    Jump_To_Definition_Result *result = NULL;

    {
        SCOPED_MEM(&world.indexer.ui_mem);

        if (!world.indexer.acquire_lock(IND_READING, true))
            return show_error("The indexer is currently busy; please wait for it to finish.");
        defer { world.indexer.release_lock(IND_READING); };

        result = world.indexer.jump_to_definition(editor->filepath, new_cur2(editor->cur_to_offset(editor->cur), -1));
    }

    if (result == NULL || result->decl == NULL || result->decl->decl == NULL)
        return show_error("Couldn't find anything under your cursor to rename.");

    if (filepath != NULL)
        *filepath = editor->filepath;

    // copy using caller mem
    auto ret = clone(result);
    ret->file = our_strcpy(result->file);
    ret->decl = result->decl->copy_decl();
    return ret;
}

void handle_goto_definition() {
    auto result = get_current_definition();
    if (result != NULL)
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

void open_rename_identifier() {
    ccstr filepath = NULL;

    // TODO: this should be a "blocking" modal, like it disables activity in rest of IDE
    auto result = get_current_definition(&filepath, true);
    if (result == NULL) return;
    if (result->decl->decl->type == GODECL_IMPORT) {
        tell_user("Sorry, we're currently not yet able to rename imports.", NULL);
        return;
    }

    world.rename_identifier_mem.reset();
    SCOPED_MEM(&world.rename_identifier_mem);

    auto &wnd = world.wnd_rename_identifier;
    wnd.rename_to[0] = '\0';
    wnd.show = true;
    wnd.running = false;
    wnd.declres = result->decl->copy_decl();
    wnd.filepath = our_strcpy(filepath);
}

bool kick_off_find_implemented_interfaces() {
    auto result = get_current_definition();
    if (result == NULL) return false;

    auto decl = result->decl->decl;
    if (decl == NULL) return false;

    if (decl->type != GODECL_TYPE) {
        tell_user("The selected object is not a type.", "Error");
        return false;
    }

    // get method set
    // iter over interfaces and see if they match
    // this one is actually relatively easy i feel
}

bool kick_off_find_implementations() {
    auto result = get_current_definition();
    if (result == NULL) return false;

    auto decl = result->decl->decl;
    if (decl == NULL) return false;

    if (decl->type != GODECL_TYPE || decl->gotype->type != GOTYPE_INTERFACE) {
        tell_user("The selected object is not an interface.", "Error");
        return false;
    }

    auto specs = decl->gotype->interface_specs;

    // iterate over types, see which ones implement interface?
    // this is the hard one (compared to find_implemented_interfaces)
}

void World::delete_ft_node(FT_Node *it) {
    SCOPED_FRAME();
    auto rel_path = world.ft_node_to_path(it);
    auto full_path = path_join(world.current_path, rel_path);
    if (it->is_directory)
        delete_rm_rf(full_path);
    else
        delete_file(full_path);

    // delete `it` from file tree
    if (it->parent != NULL && it->parent->children == it)
        it->parent->children = it->next;
    if (it->prev != NULL)
        it->prev->next = it->next;
    if (it->next != NULL)
        it->next->prev = it->prev;
}

ccstr World::ft_node_to_path(FT_Node *node) {
    auto path = alloc_list<FT_Node*>();
    for (auto curr = node; curr != NULL; curr = curr->parent)
        path->append(curr);
    path->len--; // remove root

    Text_Renderer r;
    r.init();
    for (i32 j = path->len - 1; j >= 0; j--) {
        r.write("%s", path->at(j)->name);
        if (j != 0) r.write("/");
    }
    return r.finish();
}

FT_Node *World::find_or_create_ft_node(ccstr relpath, bool is_directory) {
    auto ret = find_ft_node(relpath);
    if (ret != NULL) return ret;

    auto parent = find_ft_node(our_dirname(relpath));
    if (parent == NULL) return NULL;


    {
        SCOPED_MEM(&world.file_tree_mem);

        auto ret = alloc_object(FT_Node);
        ret->is_directory = true;
        ret->name = our_basename(relpath);
        ret->depth = parent->depth + 1;
        ret->num_children = 0;
        ret->parent = parent;
        ret->children = NULL;
        ret->prev = NULL;
        ret->open = parent->open;

        ret->next = parent->children;
        if (parent->children != NULL)
            parent->children->prev = ret;
        parent->children = ret;
        parent->children = world.sort_ft_nodes(parent->children);

        return ret;
    }
}

FT_Node *World::find_ft_node(ccstr relpath) {
    auto path = make_path(relpath);
    auto node = world.file_tree;
    For (*path->parts) {
        FT_Node *next = NULL;
        for (auto child = node->children; child != NULL; child = child->next) {
            if (streqi(child->name, it)) {
                next = child;
                break;
            }
        }
        if (next == NULL) return NULL;
        node = next;
    }
    return node;
}

int World::compare_ft_nodes(FT_Node *a, FT_Node *b) {
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

void World::add_ft_node(FT_Node *parent, fn<void(FT_Node* it)> cb) {
    FT_Node *node = NULL;

    {
        SCOPED_MEM(&file_tree_mem);

        node = alloc_object(FT_Node);
        node->num_children = 0;
        node->depth = parent->depth + 1;
        node->parent = parent;
        node->children = NULL;
        node->prev = NULL;
        cb(node);
    }

    node->next = parent->children;
    if (parent->children != NULL)
        parent->children->prev = node;
    parent->children = node;
    parent->num_children++;
    parent->children = world.sort_ft_nodes(parent->children);
}

FT_Node *World::sort_ft_nodes(FT_Node *nodes) {
    if (nodes == NULL || nodes->next == NULL) return nodes;

    int len = 0;
    for (auto it = nodes; it != NULL; it = it->next)
        len++;

    FT_Node *a = nodes, *b = nodes;
    for (int i = 0; i < len/2; i++)
        b = b->next;
    b->prev->next = NULL;
    b->prev = NULL;

    a = sort_ft_nodes(a);
    b = sort_ft_nodes(b);

    FT_Node *ret = NULL, *curr = NULL;

    while (a != NULL && b != NULL) {
        FT_Node **ptr = (compare_ft_nodes(a, b) <= 0 ? &a : &b);
        if (ret == NULL) {
            ret = *ptr;
        } else {
            curr->next = *ptr;
            (*ptr)->prev = curr;
        }
        curr = *ptr;
        *ptr = (*ptr)->next;
    }

    if (a != NULL) {
        curr->next = a;
        a->prev = curr;
    }

    if (b != NULL) {
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

    auto editor = world.find_editor([&](auto it) {
        return are_filepaths_equal(path, it->filepath);
    });

    // when build finishes, set marks on existing editors
    // when editor opens, get all existing errors and set marks

    if (editor == NULL || !is_mark_valid(error.mark)) {
        goto_file_and_pos(path, pos);
        return;
    }

    world.focus_editor_by_id(editor->id, error.mark->pos());
    ImGui::SetWindowFocus(NULL);
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

    auto node = world.find_or_create_ft_node(relpath, true);
    if (node == NULL) return;

    String_Set current_items;    current_items.init();
    String_Set new_files;        new_files.init();
    String_Set new_directories;  new_directories.init();

    for (auto it = node->children; it != NULL; it = it->next)
        current_items.add(it->name);

    list_directory(path, [&](Dir_Entry *ent) {
        do {
            auto fullpath = path_join(path, ent->name);
            if (exclude_from_file_tree(fullpath)) break;

            auto name = our_strcpy(ent->name);
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
            if (head == NULL)
                head = it;
            else
                curr->next = it;

            it->prev = curr;
            curr = it;
        };

        for (auto it = node->children; it != NULL; it = it->next)
            if (!current_items.has(it->name))
                add_child(it);

        For (*new_files.items()) {
            SCOPED_MEM(&world.file_tree_mem);

            auto child = alloc_object(FT_Node);
            child->name = our_strcpy(it);
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
            child->name = our_strcpy(it);
            child->is_directory = true;
            child->num_children = 0;
            child->parent = node;
            child->children = NULL;
            child->depth = node->depth + 1;
            child->open = node->open;
            add_child(child);
        }

        if (curr != NULL) curr->next = NULL;

        node->children = world.sort_ft_nodes(head);
    }

    // now delete all items in `current_items`
    // and add/recurse/process all items in `new_files` and `new_directories`
    // parent->children = sort_ft_nodes(parent->children);
}

bool move_autocomplete_cursor(Editor *ed, int direction) {
    auto &ac = ed->autocomplete;
    if (ac.ac.results == NULL) return false;

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
    if (thread != NULL) {
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

// TODO: tell user what went wrong if returning false
bool kick_off_find_references() {
    auto result = get_current_definition();
    if (result == NULL) return false;

    auto decl = result->decl;
    if (decl == NULL) return false;

    auto &ind = world.indexer;
    if (!ind.acquire_lock(IND_READING)) return;

    auto thread_proc = [](void *param) {
        auto &wnd = world.wnd_find_references;
        wnd.thread_mem.cleanup();
        wnd.thread_mem.init();
        SCOPED_MEM(&wnd.thread_mem);

        defer { cancel_find_references(); };

        auto files = world.indexer.find_references(wnd.declres);
        if (files == NULL) return;

        // close the thread handle first so it doesn't try to kill the thread
        if (wnd.thread != NULL) {
            close_thread_handle(wnd.thread);
            wnd.thread = NULL;
        }
    };

    auto &wnd = world.wnd_find_references;
    wnd.running = true;
    wnd.thread = create_thread(thread_proc, NULL);
}

void kick_off_rename_identifier() {
    auto &ind = world.indexer;
    if (!ind.acquire_lock(IND_READING)) return;

    auto has_unsaved_files = [&]() {
        For (world.panes)
            For (it.editors)
                if (it.buf->dirty)
                    return true;
        return false;
    };

    if (has_unsaved_files()) {
        tell_user("Please save all your unsaved files before running the Rename operation.", "Unsaved files");
        return;
    }

    // TODO: when do we release the lock?
    // TODO: seems we need a new "running" indexer status

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

        auto files = world.indexer.find_references(wnd.declres);
        if (files == NULL) return;

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

            auto editor = world.find_editor_by_filepath(filepath);
            if (editor != NULL) {
                world.message_queue.add([&](auto msg) {
                    msg->type = MTM_RELOAD_EDITOR;
                    msg->reload_editor_id = editor->id;
                });
            }
        }

        // close the thread handle first so it doesn't try to kill the thread
        if (wnd.thread != NULL) {
            close_thread_handle(wnd.thread);
            wnd.thread = NULL;
        }
    };

    auto &wnd = world.wnd_rename_identifier;
    wnd.running = true;
    wnd.too_late_to_cancel = false;
    wnd.thread = create_thread(thread_proc, NULL);
}

void cancel_find_references() {
    auto &wnd = world.wnd_find_references;
    if (!wnd.running) return;

    if (wnd.thread != NULL) {
        kill_thread(wnd.thread);
        close_thread_handle(wnd.thread);
        wnd.thread = NULL;
    }

    // assume it was acquired by find_references
    if (world.indexer.status == IND_READING)
        world.indexer.release_lock(IND_READING);

    wnd.running = false;
}

void cancel_rename_identifier() {
    auto &wnd = world.wnd_rename_identifier;
    if (!wnd.running) return;

    if (wnd.thread != NULL) {
        kill_thread(wnd.thread);
        close_thread_handle(wnd.thread);
        wnd.thread = NULL;
    }

    // assume it was acquired by rename_identifier
    if (world.indexer.status == IND_READING)
        world.indexer.release_lock(IND_READING);

    wnd.running = false;
}
