#pragma once

#include "common.hpp"
#include "editor.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <git2.h>
#include "ui.hpp"
#include "os.hpp"
#include "go.hpp"
#include "debugger.hpp"
#include "nvim.hpp"
#include "utils.hpp"
#include "imgui.h"

typedef fn<bool(Editor* e)> find_editor_func;

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

struct File_Tree_Entry {
    ccstr name;
    i32 num_children;
    i32 depth;
    i32 parent;

    struct {
        bool open;
    } state;
};

struct World {
    Pool frame_mem;
    Pool ast_viewer_mem;
    Pool autocomplete_mem;
    Pool parameter_hint_mem;
    Pool open_file_mem;
    Pool debugger_mem;
    Pool scratch_mem;
    Pool nvim_mem;
    Pool nvim_loop_mem;
    Pool file_tree_mem;
    Pool build_mem;
    Pool gomod_parser_mem;
    Pool build_index_mem;

    Fridge<Chunk0> chunk0_fridge;
    Fridge<Chunk1> chunk1_fridge;
    Fridge<Chunk2> chunk2_fridge;
    Fridge<Chunk3> chunk3_fridge;
    Fridge<Chunk4> chunk4_fridge;
    Fridge<Chunk5> chunk5_fridge;
    Fridge<Chunk6> chunk6_fridge;

    GLFWwindow* window;
    vec2 window_size;
    vec2 display_size;
    vec2f display_scale;

    Go_Index index;

    Font font;

    Nvim nvim;
    bool use_nvim;

    u32 next_editor_id;

    List<File_Tree_Entry> file_tree;

    struct {
        Nvim_Hl_Def _hl_defs[128];
        List<Nvim_Hl_Def> hl_defs;
        Grid_Window_Pair _grid_to_window[128];
        List<Grid_Window_Pair> grid_to_window;
        bool is_ui_attached;
        u32 waiting_focus_window;
    } nvim_data;

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
        Pool pool;
        List<Search_Result*> results;
        i32 scroll_offset;

        void init() {
            pool.init("search_results");
            results.init(LIST_MALLOC, 128);
        }

        void cleanup() {
            pool.cleanup();
            results.cleanup();
        }
    } search_results;

    struct {
        Pool pool;
        List<Build_Error*> errors;
        i32 scroll_offset;
        u32 selection;

        void init() {
            pool.init("build_errors");
            errors.init(LIST_MALLOC, 128);
        }

        void cleanup() {
            pool.cleanup();
            errors.cleanup();
        }
    } build_errors;

    struct {
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

    void init(bool test);
    Pane* get_current_pane();
    Editor* get_current_editor();
    Editor* find_editor(find_editor_func f);
};

extern World world;
extern thread_local Pool *MEM;

uchar* alloc_chunk(s32 needed, s32* new_size);
void free_chunk(uchar* buf, s32 cap);

struct Frame {
    Pool *pool;
    Pool_Block *block;
    s32 pos;

    Frame() {
        pool = MEM;
        block = pool->curr;
        pos = pool->sp;
    }

    void restore() {
        pool->restore(block, pos);
    }
};

struct Scoped_Frame {
    Frame frame;
    ~Scoped_Frame() { frame.restore(); }
};

#define SCOPED_FRAME() Scoped_Frame GENSYM(SCOPED_FRAME)

struct Scoped_Mem {
    Pool* old;
    Scoped_Mem(Pool *pool) { old = MEM; MEM = pool; }
    ~Scoped_Mem() { MEM = old; }
};

#define SCOPED_MEM(x) Scoped_Mem GENSYM(SCOPED_MEM)(x)

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

template <typename T>
List<T>* alloc_list() {
    auto ret = alloc_object(List<T>);
    ret->init(LIST_POOL, 32);
    return ret;
}

#define TAB_SIZE 2 // TODO

// what was even the point of this again?
// why couldn't we just allocate the string?
struct String_Allocator {
    s32 oldpos;

    char* start(u32 to_reserve) {
        MEM->ensure_enough(to_reserve);
        oldpos = MEM->sp;
        return (char*)MEM->alloc(0);
    }

    bool push(char ch) {
        if (!MEM->can_alloc(1))
            return false;
        *(char*)MEM->alloc(1) = ch;
        return true;
    }

    bool done() { return push('\0'); }
    void revert() { MEM->sp = oldpos; }
    void truncate() { MEM->sp--; push('\0'); }
};

