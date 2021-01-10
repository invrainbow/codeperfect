#pragma once

#include "common.hpp"
#include "editor.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "ui.hpp"
#include "os.hpp"
#include "go.hpp"
#include "debugger.hpp"
#include "nvim.hpp"
#include "utils.hpp"
#include "imgui.h"

typedef fn<bool(Editor* e)> find_editor_func;

struct File_Explorer_Entry {
    ccstr name;
    bool open;
    i32 depth;
    i32 num_children;
    File_Explorer_Entry* parent;
};

struct Nvim_Hl_Def {
    u32 id;
    HlType type;
};

struct Grid_Window_Pair {
    u32 grid;
    u32 win;
};

struct Search_Result {
    u32 match_col;
    u32 match_col_in_preview;
    u32 match_len;
    ccstr preview;
    u32 row;
    u32 results_in_row;
    ccstr filename;
};

enum Sidebar_View {
    SIDEBAR_CLOSED,
    SIDEBAR_FILE_EXPLORER,
    SIDEBAR_SEARCH_RESULTS,
};

// TODO: define init/cleanup routines for find and replace, and for build

struct Build_Error {
    ccstr file;
    u32 row;
    u32 col;
    ccstr message;
};

struct World {
    Stack frame_mem;
    Stack parser_mem;
    Stack ast_viewer_mem;
    Stack autocomplete_mem;
    Stack parameter_hint_mem;
    Stack open_file_mem;
    Stack debugger_mem;
    Stack scratch_mem;
    Stack nvim_mem;
    Stack nvim_loop_mem;
    Stack build_index_mem;
    /**/
    Arena file_explorer_arena;
    Arena build_arena;
    Arena gomod_parser_arena;
    Arena build_index_arena;

    struct {
        Nvim_Hl_Def _hl_defs[128];
        List<Nvim_Hl_Def> hl_defs;
        Grid_Window_Pair _grid_to_window[128];
        List<Grid_Window_Pair> grid_to_window;
        bool is_ui_attached;
        u32 waiting_focus_window;
    } nvim_data;

    // Stack *current_mem;

    Pool<Chunk0> chunk0_pool;
    Pool<Chunk1> chunk1_pool;
    Pool<Chunk2> chunk2_pool;
    Pool<Chunk3> chunk3_pool;
    Pool<Chunk4> chunk4_pool;
    Pool<Chunk5> chunk5_pool;
    Pool<Chunk6> chunk6_pool;

    Nvim nvim;
    GLFWwindow* window;
    vec2 window_size;
    vec2 display_size;
    vec2f display_scale;
    Font font;
    bool use_nvim;

    u32 next_editor_id;

    struct {
        char build_command[MAX_PATH];
    } settings;

    struct {
        bool show;
        float height;
    } error_list;

    struct {
        Sidebar_View view;
        float width;
    } sidebar;

    struct {
        Arena arena;
        List<Search_Result*> results;
        i32 scroll_offset;

        void init() {
            arena.init();
            results.init(LIST_MALLOC, 128);
        }

        void cleanup() {
            arena.cleanup();
            results.cleanup();
        }
    } search_results;

    struct {
        Arena arena;
        List<Build_Error*> errors;
        i32 scroll_offset;
        u32 selection;

        void init() {
            arena.init();
            errors.init(LIST_MALLOC, 128);
        }

        void cleanup() {
            arena.cleanup();
            errors.cleanup();
        }
    } build_errors;

    struct {
        List<File_Explorer_Entry*> files;
        i32 scroll_offset;
    } file_explorer;

    // in the future, multiple workspaces?
    Workspace wksp;

    struct {
        union {
            struct {
                bool flag_list_files : 1;
                bool flag_search_and_replace : 1;
                bool flag_search : 1;
                bool flag_build;
            };
            u64 mask;
        };

        struct {
            Process proc;
        } list_files;

        struct {
            Process proc;
            bool signal_done;
        } search_and_replace;

        struct {
            Process proc;
        } search;

        struct {
            Process proc;
            bool signal_done;
        } build;
    } jobs;

    struct {
        GLint program;
        GLint im_program;
        cur2 mouse_pos;
        cur2f mouse_delta;
        double scroll_buffer;
        bool mouse_buttons_pressed[3];
        GLFWcursor* cursors[ImGuiMouseCursor_COUNT];
    } ui;

    struct Windows_Open {
        bool open_file;
        bool im_demo;
        bool ast_viewer;
        bool search_and_replace;
        bool build_and_debug;

        bool is_any_open() {
            Windows_Open zero = { 0 };
            return (memcmp(this, &zero, sizeof(*this)) != 0);
        }
    } windows_open;

    struct Popups_Open {
        bool debugger_add_watch;

        bool is_any_open() {
            Popups_Open zero = { 0 };
            return (memcmp(this, &zero, sizeof(*this)) != 0);
        }
    } popups_open;

    struct {
        bool searching;
        char query[MAX_PATH];
        u32 selection;
        ccstr _files[100];
        List<ccstr> files;
    } wnd_open_file;

    struct {
        char find_str[256];
        char replace_str[256];
        bool use_regex;
        bool case_sensitive;
    } wnd_search_and_replace;

    struct {
        // ???
    } wnd_build_and_debug;

    struct {
    } wnd_im_demo;

    struct {
        Ast* ast;
    } wnd_ast_viewer;

    struct {
        i32 current_location;
    } wnd_debugger;

    struct {
        // the debugger itself
        Debugger debugger;

        // internals
        Thread_Handle thread;
        Lock lock;

        In_Memory_Queue<Dbg_Call> call_queue;

        // HANDLE pipe_w;
        // HANDLE pipe_r;

        // client-side list of all breakpoints
        Client_Breakpoint _breakpoints[1024];
        List<Client_Breakpoint> breakpoints;

        // client-side list of all watches
        Dbg_Watch _watches[128];
        List<Dbg_Watch> watches;

        // debugger state
        DbgState state_flag;
        struct {
            ccstr file_stopped_at;
            u32 line_stopped_at;
            List<Dbg_Location>* stackframe;
        } state;

        bool send_dbgcall(Dbg_Call* call) {
            return call_queue.push(call);
        }
    } dbg;

    void init();
    Pane* get_current_pane();
    Editor* get_current_editor();
    Editor* find_editor(find_editor_func f);
};

extern World world;
extern thread_local Stack* MEM;
extern thread_local Arena* ARENA_MEM;

uchar* alloc_chunk(s32 needed, s32* new_size);
void free_chunk(uchar* buf, s32 cap);

struct Frame {
    Stack* stack;
    Arena* arena;

    union {
        struct {
            s32 sp;
        } old_stack_data;

        struct {
            Arena_Page* curr;
            s32 ptr;
        } old_arena_data;
    };

    // void* starting_pointer;

    Frame() {
        stack = MEM;
        arena = ARENA_MEM;

        if (arena != NULL) {
            old_arena_data.curr = arena->curr;
            old_arena_data.ptr = arena->curr->ptr;
            // starting_pointer = (void*)((char*)(&arena->curr) + arena->curr->ptr);
        } else if (stack != NULL) {
            old_stack_data.sp = stack->sp;
            // starting_pointer = &stack->buf[stack->sp];
        }
    }

    void restore() {
        if (arena != NULL) {
            // TODO: this is what we need to fix lmao, i think maybe we just don't put new pages before curr.
            arena->rewind_to(old_arena_data.curr);
            old_arena_data.curr->ptr = old_arena_data.ptr;
        } else if (stack != NULL) {
            stack->sp = old_stack_data.sp;
        }
    }
};

struct Scoped_Frame {
    Frame frame;
    ~Scoped_Frame() {
        frame.restore();
    }
};

#define SCOPED_FRAME() Scoped_Frame GENSYM(SCOPED_FRAME)

struct Scoped_Mem {
    Stack* old_stack;
    Arena* old_arena;

    Scoped_Mem(Stack* stack) {
        old_stack = MEM;
        old_arena = ARENA_MEM;
        MEM = stack;
        ARENA_MEM = NULL;
    }

    ~Scoped_Mem() {
        MEM = old_stack;
        ARENA_MEM = old_arena;
    }
};

struct Scoped_Arena {
    Arena* old;
    Scoped_Arena(Arena* arena) { old = ARENA_MEM; ARENA_MEM = arena; }
    ~Scoped_Arena() { ARENA_MEM = old; }
};

struct Scoped_Use_Malloc {
    Stack* old_mem;
    Arena* old_arena;

    Scoped_Use_Malloc() {
        old_mem = MEM;
        old_arena = ARENA_MEM;
        MEM = NULL;
        ARENA_MEM = NULL;
    }

    ~Scoped_Use_Malloc() {
        MEM = old_mem;
        ARENA_MEM = old_arena;
    }
};

#define SCOPED_MEM(x) Scoped_Mem GENSYM(SCOPED_MEM)(x)
#define SCOPED_ARENA(x) Scoped_Arena GENSYM(SCOPED_ARENA)(x)
#define SCOPED_USE_MALLOC() Scoped_Use_Malloc GENSYM(SCOPED_ARENA)

void* _alloc_memory(s32 size, bool zero);

#define alloc_memory(n) _alloc_memory(n, true)
#define alloc_array(T, n) (T *)alloc_memory(sizeof(T) * (n))
#define alloc_object(T) alloc_array(T, 1)

template <typename T>
void alloc_list(List<T>* list, s32 len) {
    list->init(LIST_FIXED, len, alloc_array(T, len));
}

template <typename T>
List<T>* alloc_list(s32 len) {
    auto ret = alloc_object(List<T>);
    alloc_list(ret, len);
    return ret;
}

#define TAB_SIZE 2 // TODO

struct Arena_Alloc_String {
    s32 oldptr;

    char* start(u32 to_reserve) {
        ARENA_MEM->ensure_can_alloc(to_reserve);
        oldptr = ARENA_MEM->curr->ptr;
        return (char*)ARENA_MEM->alloc(0);
    }

    bool push(char ch) {
        if (!ARENA_MEM->can_alloc(1))
            return false;
        *(char*)ARENA_MEM->alloc(1) = ch;
        return true;
    }

    bool done() { return push('\0'); }
    void revert() { ARENA_MEM->curr->ptr = oldptr; }
    void truncate() { ARENA_MEM->curr->ptr--; push('\0'); }
};

