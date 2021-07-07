#include "world.hpp"
#include "ui.hpp"
#include "fzy_match.h"

#if OS_WIN
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

World world;

bool is_ignored_by_git(ccstr path) {
    return GHGitIgnoreCheckFile((char*)path);
}

void History::push(int editor_id, cur2 pos, bool force) {
    if (navigating_in_progress) return;

    auto should_push = [&]() -> bool {
        if (curr == start) return true;

        auto &it = ring[dec(curr)];
        if (it.editor_id != editor_id) return true;

        auto delta = abs((int)pos.y - (int)it.pos.y);
        return delta >= 10;
    };

    if (!force && !should_push()) return;

    ring[curr].editor_id = editor_id;
    ring[curr].pos = pos;

    top = curr = inc(curr);
    if (curr == start)
        start = inc(start);
}

void History::actually_go(History_Loc *it) {
    auto editor = world.find_editor_by_id(it->editor_id);
    if (editor == NULL) return;
    if (!editor->is_nvim_ready()) return;

    editor->nvim_data.is_navigating_to = true;
    editor->nvim_data.navigating_to_pos = it->pos;
    world.focus_editor_by_id(it->editor_id, it->pos);
}

bool History::go_forward() {
    if (curr == top) return false;

    curr = inc(curr);
    actually_go(&ring[dec(curr)]);
    return true;
}

bool History::go_backward() {
    if (curr == start) return false;

    {
        auto editor = world.get_current_editor();
        auto &it = ring[dec(curr)];

        if (editor == NULL || it.editor_id != editor->id || it.pos != editor->cur) {
            if (editor != NULL) {
                push(editor->id, editor->cur, true);
                curr = dec(curr);
            }

            actually_go(&it);
            return true;
        }
    }

    if (dec(curr) == start) return false;

    curr = dec(curr);
    actually_go(&ring[dec(curr)]);
    return true;
}

void History::remove_editor_from_history(int editor_id) {
    int i = start, j = start;

    for (; i != curr; i = inc(i)) {
        if (ring[i].editor_id != editor_id) {
            if (i != j)
                memcpy(&ring[j], &ring[i], sizeof(ring[j]));
            j = inc(j);
        }
    }

    curr = j;

    for (; i != top; i = inc(i))  {
        if (ring[i].editor_id != editor_id) {
            if (i != j)
                memcpy(&ring[j], &ring[i], sizeof(ring[j]));
            j = inc(j);
        }
    }

    top = j;
}

void World::fill_file_tree() {
    SCOPED_MEM(&file_tree_mem);
    file_tree_mem.reset();

    // invalidate pointer
    file_explorer.selection = NULL;

    file_tree = alloc_object(File_Tree_Node);
    file_tree->is_directory = true;
    file_tree->depth = -1;

    u32 depth = 0;

    fn<void(ccstr, File_Tree_Node*)> recur = [&](ccstr path, File_Tree_Node *parent) {
        File_Tree_Node *last_child = parent->children;

        list_directory(path, [&](Dir_Entry *ent) {
            do {
                auto fullpath = path_join(path, ent->name);
                if (is_ignored_by_git(fullpath)) break;
                if (streq(ent->name, ".git")) break;
                if (streq(ent->name, ".ideproj")) break;
                if (str_ends_with(ent->name, ".exe")) break;

                auto file = alloc_object(File_Tree_Node);
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
        panic("askldjfhalkfh");
    }
}

void prepare_workspace() {
    auto p = [&](ccstr f) {
        return path_join(world.current_path, f);
    };

    if (!copy_file(p("main.go.bak"), p("main.go")))
        panic("failed to copy main.go.bak");

    delete_rm_rf(p("db.tmp"));
    delete_rm_rf(p("go.mod"));
    delete_rm_rf(p("go.sum"));

    shell("go mod init github.com/invrainbow/life", world.current_path);
    shell("go mod tidy", world.current_path);
}

void World::init_workspace() {
    resizing_pane = -1;

    panes.init(LIST_FIXED, _countof(_panes), _panes);

#if 0 // RELEASE_BUILD
    Select_File_Opts opts; ptr0(&opts);
    opts.buf = current_path;
    opts.bufsize = _countof(current_path);
    opts.folder = true;
    opts.save = false;
    let_user_select_file(&opts);
#else
    {
        SCOPED_FRAME();

        File f;
        f.init(".idedefaultfolder", FILE_MODE_READ, FILE_OPEN_EXISTING);
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

    project_settings.read(path_join(current_path, ".ideproj"));

    /*
    if (project_settings.build_command[0] == '\0')
        strcpy_safe(project_settings.build_command, _countof(project_settings.build_command), "go build --gcflags=\"all=-N -l\" ");
    */
}

void World::init() {
    ptr0(this);

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
    init_mem(message_queue_mem);
    init_mem(index_log_mem);
    init_mem(search_mem);
#undef init_mem

    // use frame_mem as the default mem
    MEM = &frame_mem;

    chunk0_fridge.init(512);
    chunk1_fridge.init(256);
    chunk2_fridge.init(128);
    chunk3_fridge.init(64);
    chunk4_fridge.init(32);
    chunk5_fridge.init(16);
    chunk6_fridge.init(8);

    init_gohelper_crap();

#if RELEASE_BUILD
    for (bool first = true; !GHCheckLicense(); first = false) {
        if (first)
            tell_user("Please select your license keyfile.", "License key required");
        else
            tell_user("Sorry, that keyfile was invalid. Please select another one.", "License key required");

        char buf[MAX_PATH];

        Select_File_Opts opts; ptr0(&opts);
        opts.buf = buf;
        opts.bufsize = _countof(buf);
        opts.folder = false;
        opts.save = false;

        if (!let_user_select_file(&opts))
            exit(1); // should we be like, "no license key selected"?

        auto executable_dir = our_dirname(get_executable_path());
        if (!copy_file(buf, path_join(executable_dir, ".idelicense"), true)) {
            tell_user("Unable to load license keyfile.", NULL);
            exit(1);
        }
    }
#endif

#if RELEASE_BUILD
    /*
    {
        auto executable_dir = our_dirname(get_executable_path());
        set_run_on_computer_startup("CodePerfect95_Autoupdate", path_join(executable_dir, "autoupdate.exe"));
    }
    */
#endif

    {
        // do we need world_mem anywhere else?
        // i assume we will have other things that "orchestrate" world
        SCOPED_MEM(&world_mem);
        message_queue.init();
    }

    {
        SCOPED_MEM(&index_log_mem);
        wnd_index_log.lines.init();
    }

    message_queue_lock.init();

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

    fill_file_tree();

    error_list.height = 125;
    file_explorer.selection = NULL;

    {
        SCOPED_MEM(&ui_mem);
        ::ui.init();
    }
}

void World::add_event(fn<void(Main_Thread_Message*)> f) {
    SCOPED_LOCK(&message_queue_lock);
    SCOPED_MEM(&message_queue_mem);
    f(message_queue.append());
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

Editor *World::focus_editor(ccstr path) {
    return focus_editor(path, new_cur2(-1, -1));
}

Editor *World::focus_editor(ccstr path, cur2 pos) {
    for (auto&& pane : panes) {
        for (u32 i = 0; i < pane.editors.len; i++) {
            auto &it = pane.editors[i];
            if (are_filepaths_same_file(path, it.filepath)) {
                activate_pane((&pane) - panes.items);
                pane.focus_editor_by_index(i, pos);
                return &it;
            }
        }
    }
    return get_current_pane()->focus_editor(path, pos);
}

void World::activate_pane(u32 idx) {
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
}

void init_goto_file() {
    SCOPED_MEM(&world.goto_file_mem);
    world.goto_file_mem.reset();

    auto &wnd = world.wnd_goto_file;
    ptr0(&wnd);

    wnd.show = true;
    wnd.filepaths = alloc_list<ccstr>();
    wnd.filtered_results = alloc_list<int>();

    fn<void(File_Tree_Node*, ccstr)> fill_files = [&](auto node, auto path) {
        for (auto it = node->children; it != NULL; it = it->next) {
            auto isdir = it->is_directory;

            if (isdir && node->parent == NULL && streq(it->name, ".ide")) return;

            auto relpath = path[0] == '\0' ? our_strcpy(it->name) : path_join(path, it->name);
            if (isdir)
                fill_files(it, relpath);
            else
                wnd.filepaths->append(relpath);
        }
    };

    fill_files(world.file_tree, "");
}

void init_goto_symbol() {
    SCOPED_MEM(&world.goto_symbol_mem);
    world.goto_symbol_mem.reset();

    if (!world.indexer.ready) return;
    if (!world.indexer.lock.try_enter()) return;
    defer { world.indexer.lock.leave(); };

    world.indexer.fill_goto_symbol();
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
                auto err = build->errors.append();

                err->message = our_strcpy(errors[i].text);
                err->valid = errors[i].is_valid;

                if (err->valid) {
                    err->file = our_strcpy(errors[i].filename);
                    err->row = errors[i].line;
                    err->col = errors[i].col;
                    // errors[i].is_vcol;
                }
            }

            build->current_error = -1;
            build->done = true;
            build->started = false;
            build->creating_extmarks = true;

            if (build->errors.len == 0)
                world.error_list.show = false;

            {
                auto &nv = world.nvim;
                auto msgid = nv.start_request_message("nvim_create_namespace", 1);
                nv.save_request(NVIM_REQ_CREATE_EXTMARKS_CREATE_NAMESPACE, msgid, 0);
                nv.writer.write_string(our_sprintf("build-%d", world.build.id));
                nv.end_message();
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
#endif
}

void prompt_delete_all_breakpoints() {
    auto res = ask_user_yes_no(
        "Are you sure you want to delete all breakpoints?",
        NULL
    );

    if (res != ASKUSER_YES) return;
    world.dbg.push_call(DLVC_DELETE_ALL_BREAKPOINTS);
}

void filter_files() {
    auto &wnd = world.wnd_goto_file;

    wnd.filtered_results->len = 0;

    Timer t;
    t.init("filter_files");

    u32 i = 0;
    For (*wnd.filepaths) {
        if (fzy_has_match(wnd.query, it))
            wnd.filtered_results->append(i);
        i++;
    }

    t.log("matching");

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

    t.log("scoring");
}

void filter_symbols() {
    auto &wnd = world.wnd_goto_symbol;

    wnd.filtered_results->len = 0;

    Timer t;
    t.init("filter_symbols");

    u32 i = 0;
    For (*wnd.symbols) {
        if (fzy_has_match(wnd.query, it))
            wnd.filtered_results->append(i);
        i++;
    }

    t.log("matching");

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

    t.log("scoring");
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
                activate_pane((&it) - panes.items);
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

void goto_jump_to_definition_result(Jump_To_Definition_Result *result) {
    world.focus_editor(result->file, result->pos);

    /*
    auto target = world.get_current_editor();
    if (target == NULL || !streq(target->filepath, result->file))
        target = world.focus_editor(result->file);

    if (target == NULL) return;

    auto pos = result->pos;
    if (world.use_nvim) {
        if (target->is_nvim_ready()) {
            if (pos.y == -1) pos = target->offset_to_cur(pos.x);
            target->move_cursor(pos);
        } else {
            target->nvim_data.initial_pos = pos;
            target->nvim_data.need_initial_pos_set = true;
        }
    } else {
        if (pos.y == -1) pos = target->offset_to_cur(pos.x);
        target->move_cursor(pos);
    }
    */
}

void handle_goto_definition() {
    auto editor = world.get_current_editor();
    if (editor == NULL) return;

    SCOPED_MEM(&world.indexer.ui_mem);
    defer { world.indexer.ui_mem.reset(); };

    Jump_To_Definition_Result *result = NULL;

    {
        if (!world.indexer.ready) return; // strictly we can just call try_enter(), but want consistency with UI, which is based on `ready`
        if (!world.indexer.lock.try_enter()) return;
        defer { world.indexer.lock.leave(); };

        result = world.indexer.jump_to_definition(editor->filepath, new_cur2(editor->cur_to_offset(editor->cur), -1));
        if (result == NULL) {
            error("unable to jump to definition");
            return;
        }
    }

    goto_jump_to_definition_result(result);
}

void save_all_unsaved_files() {
    for (auto&& pane : world.panes)
        For (pane.editors)
            if (!it.is_untitled)
                it.handle_save(false);
}
