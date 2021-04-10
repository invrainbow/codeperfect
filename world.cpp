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
    relpath = normalize_path_sep(relpath, '/');

    // get rid of "./" at beginning, it breaks libgit2
    if (str_starts_with(relpath, "./")) relpath += 2;

    int ignored = 0;
    if (git_ignore_path_is_ignored(&ignored, git_repo, relpath) == 0)
        return (bool)ignored;
    return false;
}

/*
void fill_file_tree(ccstr path) {
    SCOPED_MEM(&world.file_tree_mem);
    world.file_tree_mem.reset();

    auto &files = world.file_tree;
    files.init();

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
*/

void fill_file_tree(ccstr path) {
    SCOPED_MEM(&world.file_tree_mem);
    world.file_tree_mem.reset();

    world.file_tree = alloc_object(File_Tree_Node);
    world.file_tree->is_directory = true;
    world.file_tree->depth = -1;

    u32 depth = 0;

    fn<void(ccstr, File_Tree_Node*)> recur = [&](ccstr path, File_Tree_Node *parent) {
        list_directory(path, [&](Dir_Entry *ent) {
            auto fullpath = path_join(path, ent->name);
            if (is_ignored_by_git(fullpath, ent->type & FILE_TYPE_DIRECTORY))
                return;

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

    recur(path, world.file_tree);
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
}

void prepare_workspace() {
    auto p = [&](ccstr f) {
        return path_join(TEST_PATH, f);
    };

    if (!copy_file(p("main.go.bak"), p("main.go")))
        panic("failed to copy main.go.bak");

    delete_rm_rf(p("db.tmp"));
    delete_rm_rf(p("go.mod"));
    delete_rm_rf(p("go.sum"));

    shell("go mod init github.com/invrainbow/life", TEST_PATH);
    shell("go mod tidy", TEST_PATH);
}

void World::init() {
    ptr0(this);

    git_libgit2_init();
    fzy_init();

#define init_mem(x) x.init(#x)
    init_mem(frame_mem);
    init_mem(file_tree_mem);
    init_mem(autocomplete_mem);
    init_mem(parameter_hint_mem);
    init_mem(open_file_mem);
    init_mem(scratch_mem);
    init_mem(build_index_mem);
    init_mem(ui_mem);
#undef init_mem

    MEM = &frame_mem;

    chunk0_fridge.init(512);
    chunk1_fridge.init(256);
    chunk2_fridge.init(128);
    chunk3_fridge.init(64);
    chunk4_fridge.init(32);
    chunk5_fridge.init(16);
    chunk6_fridge.init(8);

    // prepare_workspace();

    use_nvim = true;

    wnd_debugger.current_location = -1;

    wksp.init();
    indexer.init();
    nvim.init();
    dbg.init();

    fill_file_tree(wksp.path);

    sidebar.width = 300;
    error_list.height = 150;
    file_explorer.selection = -1;

    windows_open.search_and_replace = false;
    windows_open.build_and_debug = false;
    windows_open.im_metrics = false;

    // TODO: allow user to enter this command himself
    strcpy_safe(world.settings.build_command, _countof(world.settings.build_command), "go build helper.go");

    {
        SCOPED_MEM(&ui_mem);
        ::ui.init();
    }
}

void World::start_background_threads() {
    indexer.start_background_thread();
    nvim.start_running();
    dbg.start_loop();
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

