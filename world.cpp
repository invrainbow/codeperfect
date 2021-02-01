#include "world.hpp"
#include "ui.hpp"
#include "fzy_match.h"

World world;

bool is_ignored_by_git(ccstr path, bool isdir) {
    auto git_repo = world.wksp.git_repo;
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
    relpath = (ccstr)normalize_path_separator((cstr)relpath, '/');

    // get rid of "./" at beginning, it breaks libgit2
    if (str_starts_with(relpath, "./")) relpath += 2;

    int ignored = 0;
    if (git_ignore_path_is_ignored(&ignored, git_repo, relpath) == 0)
        return (bool)ignored;
    return false;
}

void fill_file_tree(ccstr path) {
    SCOPED_MEM(&world.file_tree_mem);
    world.file_tree_mem.reset();

    auto &files = world.file_tree;
    files.init(LIST_POOL, 32);

    u32 depth = 0;
    fn<void(ccstr, i32)> recur = [&](ccstr path, i32 parent) {
        list_directory(path, [&](Dir_Entry *ent) {
            auto fullpath = path_join(path, ent->name);
            if (is_ignored_by_git(fullpath, ent->type & FILE_TYPE_DIRECTORY))
                return;

            if (parent != -1) files[parent].num_children++;

            auto file_idx = files.len;
            auto file = files.append();
            file->name = our_strcpy(ent->name);
            file->depth = depth;
            file->parent = parent;
            file->state.open = false;

            if (ent->type & FILE_TYPE_DIRECTORY) {
                file->num_children = 0;
                depth++;
                recur(fullpath, file_idx);
                depth--;
            } else {
                file->num_children = -1;
            }
        });
    };

    recur(path, -1);
}

void World::init(bool test) {
    ptr0(this);

    git_libgit2_init();
    fzy_init();

    MEM = &frame_mem;

#define init_mem(x) x.init(#x)
    init_mem(frame_mem);
    init_mem(file_tree_mem);
    init_mem(ast_viewer_mem);
    init_mem(autocomplete_mem);
    init_mem(parameter_hint_mem);
    init_mem(open_file_mem);
    init_mem(scratch_mem);
    init_mem(debugger_mem);
    init_mem(nvim_mem);
    init_mem(nvim_loop_mem);
    init_mem(build_index_mem);
#undef init_mem

    chunk0_fridge.init_managed(1024);
    chunk1_fridge.init_managed(512);
    chunk2_fridge.init_managed(256);
    chunk3_fridge.init_managed(128);
    chunk4_fridge.init_managed(64);
    chunk5_fridge.init_managed(32);
    chunk6_fridge.init_managed(16);

    dbg.breakpoints.init(LIST_FIXED, _countof(dbg._breakpoints), dbg._breakpoints);
    dbg.watches.init(LIST_FIXED, _countof(dbg._watches), dbg._watches);
    wnd_debugger.current_location = -1;

    wksp.init();

    if (!test) {
        index.init();
        index.run_threads();
    }

    use_nvim = true;
    nvim_data.grid_to_window.init(LIST_FIXED, _countof(nvim_data._grid_to_window), nvim_data._grid_to_window);

    if (!test) {
        SCOPED_MEM(&nvim_mem);
        nvim.init();
        nvim.start_running();
    }

    fill_file_tree(wksp.path);

    sidebar.width = 300;
    error_list.height = 150;

    windows_open.search_and_replace = false;
    windows_open.build_and_debug = false;

    // TODO: allow user to enter this command himself
    strcpy_safe(world.settings.build_command, _countof(world.settings.build_command), "go build main.go");

    if (!test) ::ui.init();
}

Pane* World::get_current_pane() {
    return wksp.get_current_pane();
}

Editor* World::get_current_editor() {
    auto pane = wksp.get_current_pane();
    if (pane == NULL) return NULL;

    return pane->get_current_editor();
}

Editor* World::find_editor(find_editor_func f) {
    for (auto&& pane : wksp.panes)
        For (pane.editors)
            if (f(&it))
                return &it;
    return NULL;
}

