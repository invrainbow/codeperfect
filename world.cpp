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

bool is_ignored_by_git(ccstr path, bool isdir) {
    auto git_repo = world.git_repo;
    if (git_repo == NULL) return false;

    SCOPED_FRAME();

    // get path relative to repo root
    auto relpath = get_path_relative_to(path, git_repository_workdir(git_repo));
    if (relpath == NULL) return false;

    // if it's a directory, libgit2 requires a slash to be at the end
    if (isdir)
        if (!is_sep(relpath[strlen(relpath)-1]))
            relpath = our_sprintf("%s/", relpath, PATH_SEP);

    // libgit2 requires forward slashes
    relpath = normalize_path_sep(relpath, '/');

    // get rid of "./" at beginning, it breaks libgit2
    if (str_starts_with(relpath, "./")) relpath += 2;

    int ignored = 0;
    if (git_ignore_path_is_ignored(&ignored, git_repo, relpath) == 0)
        return (bool)ignored;
    return false;
}

void World::fill_file_tree() {
    SCOPED_MEM(&file_tree_mem);
    file_tree_mem.reset();

    file_tree = alloc_object(File_Tree_Node);
    file_tree->is_directory = true;
    file_tree->depth = -1;

    u32 depth = 0;

    fn<void(ccstr, File_Tree_Node*)> recur = [&](ccstr path, File_Tree_Node *parent) {
        list_directory(path, [&](Dir_Entry *ent) {
            auto fullpath = path_join(path, ent->name);
            if (is_ignored_by_git(fullpath, ent->type & FILE_TYPE_DIRECTORY))
                return;

            if (streq(ent->name, ".ideproj")) return;
            if (str_ends_with(ent->name, ".exe")) return;

            auto file = alloc_object(File_Tree_Node);
            file->name = our_strcpy(ent->name);
            file->is_directory = (ent->type & FILE_TYPE_DIRECTORY);
            file->num_children = 0;
            file->parent = parent;
            file->children = NULL;
            file->depth = depth;
            file->open = false;
            file->next = parent->children;

            parent->children = file;
            parent->num_children++;

            if (file->is_directory) {
                depth++;
                recur(fullpath, file);
                depth--;
            }
        });
    };

    recur(current_path, file_tree);
}

bool copy_file(ccstr src, ccstr dest) {
    auto ef = read_entire_file(src);
    if (ef == NULL) return false;
    defer { free_entire_file(ef); };

    File f;
    if (f.init(dest, FILE_MODE_WRITE, FILE_CREATE_NEW) != FILE_RESULT_SUCCESS)
        return false;
    defer { f.cleanup(); };

    s32 ret = 0;
    return f.write((char*)ef->data, ef->len, &ret) && (ret == ef->len);
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

#if 0
    strcpy_safe(current_path, _countof(current_path), normalize_path_sep("c:/users/brandon/ide/payments"));
#else
    Select_File_Opts opts = {0};
    opts.buf = current_path;
    opts.bufsize = _countof(current_path);
    opts.folder = true;
    opts.save = false;
    let_user_select_file(&opts);
#endif

    project_settings.read(path_join(current_path, ".ideproj"));

    /*
    if (project_settings.build_command[0] == '\0')
        strcpy_safe(project_settings.build_command, _countof(project_settings.build_command), "go build --gcflags=\"all=-N -l\" ");
    */

    git_buf root = {0};
    if (git_repository_discover(&root, current_path, 0, NULL) == 0) {
        git_repository_open(&git_repo, root.ptr);
        git_buf_free(&root);
    }
}

bool check_license_key() {
    SCOPED_FRAME();

    Process proc;
    proc.init();
    // proc.dir = path_join(our_dirname(get_executable_path()), "helpers");
    proc.run("license_check.exe");
    defer { proc.cleanup(); };

    while (proc.status() == PROCESS_WAITING)
        continue;

    return (proc.exit_code == EXIT_SUCCESS);
}

void World::init() {
    ptr0(this);

#define init_mem(x) x.init(#x)
    init_mem(world_mem);
    init_mem(frame_mem);
    init_mem(file_tree_mem);
    init_mem(autocomplete_mem);
    init_mem(parameter_hint_mem);
    init_mem(open_file_mem);
    init_mem(scratch_mem);
    init_mem(build_index_mem);
    init_mem(ui_mem);
    init_mem(message_queue_mem);
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

#if 1
    for (bool first = true; !check_license_key(); first = false) {
        if (first)
            tell_user("Please select your license keyfile.", "License key required");
        else
            tell_user("Sorry, that keyfile was invalid. Please select another one.", "License key required");

        char buf[MAX_PATH];

        Select_File_Opts opts = {0};
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

    {
        // do we need world_mem anywhere else?
        // i assume we will have other things that "orchestrate" world
        SCOPED_MEM(&world_mem);
        message_queue.init();
    }

    message_queue_lock.init();

    git_libgit2_init();
    fzy_init();

    // prepare_workspace();
    // build helper
    // shell("go build helper.go", "w:/helper");

    use_nvim = true;

    init_workspace();
    indexer.init();
    nvim.init();
    dbg.init();

    fill_file_tree();

    error_list.height = 125;
    file_explorer.selection = -1;

    windows_open.search_and_replace = false;
    windows_open.build_and_debug = false;
    windows_open.im_metrics = false;

    jumplist.init();

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

void init_open_file() {
    SCOPED_MEM(&world.open_file_mem);
    world.open_file_mem.reset();

    auto wnd = &world.wnd_open_file;
    ptr0(wnd);

    wnd->show = true;
    wnd->filepaths = alloc_list<ccstr>();
    wnd->filtered_results = alloc_list<int>();

    fn<void(ccstr)> fill_files = [&](ccstr path) {
        auto thispath = path_join(world.current_path, path);
        list_directory(thispath, [&](Dir_Entry *entry) {
            bool isdir = (entry->type == DIRENT_DIR);

            auto fullpath = path_join(thispath, entry->name);
            if (is_ignored_by_git(fullpath, isdir)) return;

            if (isdir && streq(path, "") && streq(entry->name, ".ide")) return;

            auto relpath = path[0] == '\0' ? our_strcpy(entry->name) : path_join(path, entry->name);
            if (isdir)
                fill_files(relpath);
            else
                wnd->filepaths->append(relpath);
        });
    };

    fill_files("");
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

        {
            SCOPED_LOCK(&indexer.gohelper_static.lock);

            // TODO: do this
            indexer.gohelper_static.run("start_build", build_profile->cmd, NULL);
            if (indexer.gohelper_static.returned_error) {
                build->done = true;
                build->build_itself_had_error = true;
                return;
            }
        }

        for (;; sleep_milliseconds(100)) {
            SCOPED_LOCK(&indexer.gohelper_static.lock);

            auto resp = indexer.gohelper_static.run("get_build_status", NULL);
            if (indexer.gohelper_static.returned_error) {
                build->done = true;
                build->started = false;
                build->build_itself_had_error = true;
                return;
            }

            if (!streq(resp, "done")) continue;

            auto len = indexer.gohelper_static.readint();

            for (u32 i = 0; i < len; i++) {
                auto err = build->errors.append();

                err->message = indexer.gohelper_static.readline();
                err->valid = (bool)indexer.gohelper_static.readint();

                if (err->valid) {
                    err->file = indexer.gohelper_static.readline();
                    err->row = indexer.gohelper_static.readint();
                    err->col = indexer.gohelper_static.readint();
                    auto is_vcol = indexer.gohelper_static.readint();
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
    auto wnd = &world.wnd_open_file;

    wnd->filtered_results->len = 0;

    u32 i = 0;
    For (*wnd->filepaths) {
        if (fzy_has_match(wnd->query, it))
            wnd->filtered_results->append(i);
        i++;
    }

    wnd->filtered_results->sort([&](int *ia, int *ib) {
        auto a = fzy_match(wnd->query, wnd->filepaths->at(*ia));
        auto b = fzy_match(wnd->query, wnd->filepaths->at(*ib));
        return a < b ? 1 : (a > b ? -1 : 0);  // reverse
    });
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

void Jumplist::add(int editor_id, cur2 pos, bool bypass_duplicate_check) {
    if (disable) return;

    auto editor = world.find_editor_by_id(editor_id);
    if (editor == NULL) return;

    if (!bypass_duplicate_check)
        if (editor->is_current_editor() && editor->cur == pos)
            return;

    // print("jumplist add: %s %s", editor->filepath, format_pos(pos));

    if (empty) {
        empty = false;
    } else {
        if (inc(p) == start)
            start = inc(start);
        end = p = inc(p);
    }

    buf[p].editor_id = editor_id;
    buf[p].pos = pos;
}

bool is_build_debug_free() {
    if (world.build.started) return false;
    if (world.dbg.state_flag != DLV_STATE_INACTIVE) return false;

    return true;
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

    auto target = editor;
    if (!streq(editor->filepath, result->file))
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
}
