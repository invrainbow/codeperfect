#include "world.hpp"

World world;
thread_local Stack* MEM;
thread_local Arena* ARENA_MEM;

run_before_main { world.init(); };

uchar* alloc_chunk(s32 needed, s32* new_size) {
    ChunkSize sizes[] = { CHUNK0, CHUNK1, CHUNK2, CHUNK3, CHUNK4, CHUNK5, CHUNK6 };

    for (auto size : sizes) {
        if (size < needed) continue;

        *new_size = size;
        switch (size) {
            case CHUNK0: return (uchar*)world.chunk0_pool.alloc();
            case CHUNK1: return (uchar*)world.chunk1_pool.alloc();
            case CHUNK2: return (uchar*)world.chunk2_pool.alloc();
            case CHUNK3: return (uchar*)world.chunk3_pool.alloc();
            case CHUNK4: return (uchar*)world.chunk4_pool.alloc();
            case CHUNK5: return (uchar*)world.chunk5_pool.alloc();
            case CHUNK6: return (uchar*)world.chunk6_pool.alloc();
        }
    }

    return NULL;
}

void free_chunk(uchar* buf, s32 cap) {
    switch (cap) {
        case CHUNK0: world.chunk0_pool.free((Chunk0*)buf); break;
        case CHUNK1: world.chunk1_pool.free((Chunk1*)buf); break;
        case CHUNK2: world.chunk2_pool.free((Chunk2*)buf); break;
        case CHUNK3: world.chunk3_pool.free((Chunk3*)buf); break;
        case CHUNK4: world.chunk4_pool.free((Chunk4*)buf); break;
        case CHUNK5: world.chunk5_pool.free((Chunk5*)buf); break;
        case CHUNK6: world.chunk6_pool.free((Chunk6*)buf); break;
    }
}

void fill_file_tree(ccstr path) {
    SCOPED_ARENA(&world.file_explorer_arena);

    world.file_explorer_arena.cleanup();
    world.file_explorer_arena.init();

    auto& out = world.file_explorer.files;
    out.cleanup();
    out.init(LIST_MALLOC, 256);

    fn<void(ccstr path, File_Explorer_Entry *parent, i32 depth)> recur;

    recur = [&](ccstr path, File_Explorer_Entry* parent, i32 depth) {
        list_directory(path, [&](Dir_Entry *ent) {
            if (parent != NULL) parent->num_children++;

            File_Explorer_Entry *file;
            {
                SCOPED_ARENA(&world.file_explorer_arena);
                file = alloc_object(File_Explorer_Entry);
                file->name = our_strcpy(ent->name);
            }
            file->parent = parent;
            file->depth = depth;
            file->open = false;
            out.append(file);

            if (ent->type & FILE_TYPE_DIRECTORY) {
                file->num_children = 0;
                {
                    SCOPED_MEM(&world.scratch_mem);
                    SCOPED_FRAME();
                    auto new_path = path_join(path, ent->name);
                    recur(new_path, file, depth + 1);
                }
            } else {
                file->num_children = -1;
            }
        });
    };

    recur(path, NULL, 0);
}

void World::init() {
    ptr0(this);

    MEM = &frame_mem;
    ARENA_MEM = NULL;

    use_nvim = true;

#define init_mem(x, size) x.init(size, #x)

    init_mem(frame_mem, 1024 * 1024);
    init_mem(parser_mem, 100 * 1024 * 1024);
    init_mem(ast_viewer_mem, 1024 * 64);
    init_mem(autocomplete_mem, 1024 * 64);
    init_mem(parameter_hint_mem, 1024 * 64);
    init_mem(open_file_mem, 1024 * 1024);
    init_mem(scratch_mem, 1024 * 1024);
    init_mem(debugger_mem, 1024 * 1024);
    init_mem(nvim_mem, 1024 * 1024);
    init_mem(nvim_loop_mem, 1024 * 1024);
    init_mem(build_index_mem, 1024 * 1024);

#undef init_mem

    chunk0_pool.init_managed(1024);
    chunk1_pool.init_managed(512);
    chunk2_pool.init_managed(256);
    chunk3_pool.init_managed(128);
    chunk4_pool.init_managed(64);
    chunk5_pool.init_managed(32);
    chunk6_pool.init_managed(16);

    dbg.breakpoints.init(LIST_FIXED, _countof(dbg._breakpoints), dbg._breakpoints);
    dbg.watches.init(LIST_FIXED, _countof(dbg._watches), dbg._watches);

    wksp.init();

    wnd_open_file.files.init(LIST_FIXED, _countof(wnd_open_file._files), wnd_open_file._files);

    wnd_debugger.current_location = -1;

    nvim_data.hl_defs.init(LIST_FIXED, _countof(nvim_data._hl_defs), nvim_data._hl_defs);
    nvim_data.grid_to_window.init(LIST_FIXED, _countof(nvim_data._grid_to_window), nvim_data._grid_to_window);

    nvim.init();
    nvim.start_running();

    fill_file_tree(wksp.path);

    sidebar.width = 300;
    error_list.height = 150;

    windows_open.search_and_replace = false;
    windows_open.build_and_debug = false;

    // TODO: bounds checking once we allow user to enter this command himself
    strcpy_safe(world.settings.build_command, _countof(world.settings.build_command), "go build gotest.go"); 
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

void* _alloc_memory(s32 size, bool zero) {
    if (ARENA_MEM != NULL)
        return ARENA_MEM->alloc(size, zero);

    if (MEM != NULL)
        return MEM->alloc(size, zero);

    auto ret = malloc(size);
    if (zero) mem0(ret, size);
    return ret;
}
