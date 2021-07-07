#pragma once

#include <math.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "common.hpp"
#include "editor.hpp"
#include "ui.hpp"
#include "os.hpp"
#include "go.hpp"
#include "debugger.hpp"
#include "nvim.hpp"
#include "utils.hpp"
#include "imgui.h"
#include "mem.hpp"
#include "settings.hpp"
#include "search.hpp"

#define RELEASE_BUILD 0

typedef fn<bool(Editor* e)> find_editor_func;

// TODO: define init/cleanup routines for find and replace, and for build

struct Build_Error {
    ccstr message;
    bool valid;
    ccstr file;
    u32 row;
    u32 col;
    u64 nvim_extmark;
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

enum Main_Thread_Message_Type {
    MTM_NVIM_MESSAGE,
    MTM_RELOAD_EDITOR,
    MTM_GOTO_FILEPOS,
};

struct Main_Thread_Message {
    Main_Thread_Message_Type type;

    union {
        Nvim_Message nvim_message;
        u32 reload_editor_id;
        struct {
            ccstr file;
            cur2 pos;
        } goto_filepos;
    };
};

struct History_Loc {
    int editor_id;
    cur2 pos;
};

struct History {
    History_Loc ring[128];
    int start;
    int top;
    int curr;
    bool navigating_in_progress;

    void init() { ptr0(this); }

    void push(int editor_id, cur2 pos, bool force = false);
    void actually_go(History_Loc *it);
    bool go_forward();
    bool go_backward();
    void remove_editor_from_history(int editor_id);

    int inc(int i) { return i == _countof(ring) - 1 ? 0 : i + 1; }
    int dec(int i) { return i == 0 ? _countof(ring) - 1 : i - 1; }
};

struct Build {
    Pool mem;

    u64 id;
    bool done;
    bool started;
    List<Build_Error> errors;
    u64 nvim_namespace_id;
    int current_error;
    bool build_itself_had_error;
    Thread_Handle thread;
    i32 scroll_offset;
    u32 selection;
    bool creating_extmarks;

    bool ready() {
        return done && !creating_extmarks;
    }

    void init() {
        ptr0(this);
        mem.init("build");

        SCOPED_MEM(&mem);

        errors.init();
    }

    void cleanup() {
        if (thread != NULL) {
            kill_thread(thread);
            close_thread_handle(thread);
            // TODO: delete nvim namespace and extmarks
        }
        mem.cleanup();
    }
};

struct World {
    Pool world_mem;
    Pool frame_mem;
    Pool autocomplete_mem;
    Pool parameter_hint_mem;
    Pool goto_file_mem;
    Pool goto_symbol_mem;
    Pool scratch_mem;
    Pool file_tree_mem;
    Pool build_mem;
    Pool build_index_mem;
    Pool ui_mem;
    Pool message_queue_mem;
    Pool index_log_mem;
    Pool search_mem;

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

    Lock message_queue_lock;
    List<Main_Thread_Message> message_queue;

    Searcher searcher;

    Go_Indexer indexer;

    Font font;

    Nvim nvim;
    bool use_nvim;

    Debugger dbg;

    u32 next_editor_id;

    File_Tree_Node *file_tree;
    u64 next_build_id;

    char current_path[MAX_PATH];

    Pane _panes[MAX_PANES];
    List<Pane> panes;
    u32 current_pane;

    i32 resizing_pane; // if this value is i, we're resizing the border between i and i+1

    History history;

    void activate_pane(u32 idx);
    void init_workspace();

    bool replace_line_numbers_with_bytecounts;
    bool turn_off_framerate_cap;

    struct {
        bool show;
        List<ccstr> lines;
    } wnd_index_log;

    struct {
        bool show;
        Project_Settings tmp;
        int current_debug_profile;
        int current_build_profile;
        int tmp_debug_profile_type;

        bool focus_debug_profiles;
        bool focus_build_profiles;
    } wnd_project_settings;

    struct {
        bool show;
        float height;
    } error_list;

    struct {
        bool show;
        // what else?
    } search_results;

    Build build;

    struct {
        bool show;
        File_Tree_Node *selection;
        // char buf[256];
        // bool adding_something;
        // bool thing_being_added_is_file;
    } file_explorer;

    struct {
        GLuint vao;
        GLuint vbo;
        GLuint im_vao;
        GLuint im_vbo;
        GLuint im_vebo;
        GLuint fbo;

        GLint program;
        GLint im_program;
        cur2 mouse_pos;
        cur2f mouse_delta;
        ImFont *im_font_mono;
        ImFont *im_font_ui;
        double scroll_buffer;
        bool mouse_down[ImGuiMouseButton_COUNT];
        bool mouse_just_pressed[ImGuiMouseButton_COUNT];
        GLFWcursor* cursors[ImGuiMouseCursor_COUNT];
        GLuint textures[__TEXTURE_COUNT__];
        bool mouse_captured_by_imgui;
        bool keyboard_captured_by_imgui;
    } ui;

    struct Windows_Open {
        bool im_demo;
        bool im_metrics;
    } windows_open;

    struct {
        bool show;
        bool replace;
        char find_str[256];
        char replace_str[256];
        bool use_regex;
        bool case_sensitive;

        bool focus_bool;

    } wnd_search_and_replace;

    struct {
        bool show;
    } wnd_editor_tree;

    struct {
        bool show;
    } wnd_options;

    struct {
        bool show;
    } wnd_editor_toplevels;

    struct {
        bool show;
        char name[MAX_PATH];
        char location[MAX_PATH];
        bool location_is_root;
        bool folder;
    } wnd_add_file_or_folder;

    struct {
        bool show;
        bool focused;
        bool first_open_focus_twice_done;

        char query[MAX_PATH];
        u32 selection;
        List<ccstr> *filepaths;
        List<int> *filtered_results;
    } wnd_goto_file;

    struct {
        bool show;

        bool focused;
        bool first_open_focus_twice_done;

        char query[MAX_PATH];
        u32 selection;
        List<ccstr> *symbols;
        List<int> *filtered_results;
    } wnd_goto_symbol;

    struct {
        bool show_anon_nodes;
    } wnd_ast_vis;

    struct {
        i32 scroll_offset;
        // ???
    } wnd_build_and_debug;

    struct {
    } wnd_im_demo;

    struct {
        bool focused;
        int current_goroutine;
        int current_frame;
        char new_watch_buf[256];
    } wnd_debugger;

    struct {
        bool show;
    } wnd_style_editor;

    void init();
    void start_background_threads();
    Pane* get_current_pane();
    Editor* get_current_editor();
    Editor* find_editor(find_editor_func f);
    void add_event(fn<void(Main_Thread_Message*)> f);
    Editor* find_editor_by_id(u32 id);
    void fill_file_tree();

    Editor *focus_editor(ccstr path);
    Editor *focus_editor(ccstr path, cur2 pos);
    Editor* focus_editor_by_id(int editor_id, cur2 pos);
};

extern World world;

bool is_ignored_by_git(ccstr path, bool isdir);

void init_goto_file();
void init_goto_symbol();
void filter_files();
void filter_symbols();

void kick_off_build(Build_Profile *build_profile = NULL);
void prompt_delete_all_breakpoints();
void run_proc_the_normal_way(Process* proc, ccstr cmd);
void* get_native_window_handle();
bool is_build_debug_free();
void goto_jump_to_definition_result(Jump_To_Definition_Result *result);
void handle_goto_definition();
void save_all_unsaved_files();
void start_search_job(ccstr query);
