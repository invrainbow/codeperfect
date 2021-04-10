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
#include "mem.hpp"

typedef fn<bool(Editor* e)> find_editor_func;

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

struct File_Tree_Node {
    bool is_directory;
    ccstr name;
    i32 num_children;
    i32 depth;
    File_Tree_Node *parent;
    File_Tree_Node *children;
    File_Tree_Node *next;
    bool open;
};

struct World {
    Pool frame_mem;
    Pool autocomplete_mem;
    Pool parameter_hint_mem;
    Pool open_file_mem;
    Pool debugger_mem;
    Pool scratch_mem;
    Pool file_tree_mem;
    Pool build_mem;
    Pool build_index_mem;
    Pool ui_mem;

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

    Go_Indexer indexer;

    Font font;

    Nvim nvim;
    bool use_nvim;

    Debugger dbg;

    u32 next_editor_id;

    File_Tree_Node *file_tree;

    Workspace wksp; // in the future, multiple workspaces?

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
            {
                SCOPED_MEM(&pool);
                results.init();
            }
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
            {
                SCOPED_MEM(&pool);
                errors.init();
            }
        }

        void cleanup() {
            pool.cleanup();
        }
    } build_errors;

    struct {
        i32 scroll_offset;
        i32 selection;
        // char buf[256];
        // bool adding_something;
        // bool thing_being_added_is_file;
    } file_explorer;

    struct {
        union {
            struct {
                bool flag_search_and_replace : 1;
                bool flag_search : 1;
                bool flag_build;
            };
            u64 mask;
        };

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
        bool mouse_down[ImGuiMouseButton_COUNT];
        bool mouse_just_pressed[ImGuiMouseButton_COUNT];
        GLFWcursor* cursors[ImGuiMouseCursor_COUNT];
        GLuint textures[__TEXTURE_COUNT__];
    } ui;

    struct Windows_Open {
        bool open_file;
        bool im_demo;
        bool im_metrics;
        bool search_and_replace;
        bool build_and_debug;
        bool is_any_open() { return !iszero(this); }
    } windows_open;

    struct Popups_Open {
        bool debugger_add_watch;
        bool is_any_open() { return !iszero(this); }
    } popups_open;

    struct Dialog_Open {
        bool add_file_or_folder;
        bool is_any_open() { return !iszero(this); }
    } dialogs_open;

    struct {
        char name[MAX_PATH];
        char location[MAX_PATH];
        bool location_is_root;
        bool folder;
    } wnd_add_file_or_folder;

    struct {
        char query[MAX_PATH];
        u32 selection;
        List<ccstr> *filepaths;
        List<int> *filtered_results;
    } wnd_open_file;

    struct {
        bool show_anon_nodes;
    } wnd_ast_vis;

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
        i32 current_location;
    } wnd_debugger;

    void init();
    void start_background_threads();
    Pane* get_current_pane();
    Editor* get_current_editor();
    Editor* find_editor(find_editor_func f);
};

extern World world;

#define TAB_SIZE 2 // TODO

bool is_ignored_by_git(ccstr path, bool isdir);

void fill_file_tree(ccstr path);
