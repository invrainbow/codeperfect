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
    Mark *mark;
};

struct FT_Node {
    bool is_directory;
    ccstr name;
    i32 num_children;
    i32 depth;
    FT_Node *parent;
    FT_Node *children;
    FT_Node *prev;
    FT_Node *next;
    bool open;
};

enum Main_Thread_Message_Type {
    MTM_NVIM_MESSAGE,
    MTM_GOTO_FILEPOS,

    MTM_FILETREE_DELETE,
    MTM_FILETREE_CREATE,

    MTM_PANIC,
    MTM_TELL_USER,
};

struct Main_Thread_Message {
    Main_Thread_Message_Type type;

    union {
        Nvim_Message nvim_message;
        u32 reload_editor_id;
        struct {
            ccstr goto_file;
            cur2 goto_pos;
        };
        ccstr panic_message;
        struct {
            ccstr tell_user_text;
            ccstr tell_user_title;
        };
        ccstr debugger_stdout_line;
    };
};

struct History_Loc {
    int editor_id;
    cur2 pos;
    Mark *mark; // do we even need pos then?

    void cleanup();
};

struct History {
    History_Loc ring[128];
    int start;
    int top;
    int curr;

    void init() { ptr0(this); }

    void actually_push(int editor_id, cur2 pos);
    void push(int editor_id, cur2 pos);
    void actually_go(History_Loc *it);
    bool go_forward();
    bool go_backward();
    void remove_invalid_marks();
    void save_latest();
    void check_marks(int upper = -1);

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
    // i32 scroll_offset;
    u32 selection;
    int scroll_to;

    bool ready() {
        return done;
    }

    void init() {
        ptr0(this);
        mem.init("build");

        SCOPED_MEM(&mem);

        errors.init();
        scroll_to = -1;
    }

    void cleanup();
};

#define INDEX_LOG_CAP 64 // 512
#define INDEX_LOG_MAXLEN 256

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
    Pool find_references_mem;
    Pool rename_identifier_mem;

    Fridge<Mark> mark_fridge;
    Fridge<Mark_Node> mark_node_fridge;
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

    struct {
        bool recording;
        File f;
    } record_keys;

    Message_Queue<Main_Thread_Message> message_queue;

    Searcher searcher;

    Go_Indexer indexer;

    Font font;

    Nvim nvim;
    bool use_nvim;

    Debugger dbg;

    u32 next_editor_id;

    FT_Node *file_tree;
    u64 next_build_id;

    char go_binary_path[MAX_PATH];
    char delve_path[MAX_PATH];
    char current_path[MAX_PATH];

    Pane _panes[MAX_PANES];
    List<Pane> panes;
    u32 current_pane;

    i32 resizing_pane; // if this value is i, we're resizing the border between i and i+1

    History history;

    bool darkmode;

    Lock global_mark_tree_lock;

    struct Navigation_Dest {
        int editor_id;
        cur2 pos;
    };

    Navigation_Dest _navigation_queue[256];
    List<Navigation_Dest> navigation_queue;

    // bool navigating_to;
    // cur2 navigating_to_pos;
    // int navigating_to_editor;

    void activate_pane(Pane *pane);
    void activate_pane_by_index(u32 idx);
    void init_workspace();

    bool replace_line_numbers_with_bytecounts;
    bool turn_off_framerate_cap;
    bool randomly_move_cursor_around;

    Fs_Watcher fswatch;

    bool auth_update_done;
    u64 auth_update_last_check;

    struct : Wnd {
        bool running;
        char rename_to[256];
        Godecl *decl;
        ccstr filepath;
        Thread_Handle thread;
    } wnd_rename_identifier;

    // collapse into world.find_references?
    struct : Wnd {
    } wnd_find_references;

    struct {
        bool in_progress;
        // ???
    } find_references;

    struct : Wnd {
        // ring buffer
        char buf[INDEX_LOG_CAP][INDEX_LOG_MAXLEN];
        int start;
        int len;
        bool cmd_scroll_to_end;
    } wnd_index_log;

    struct : Wnd {
    } wnd_mouse_pos;

    struct : Wnd {
        int selection;
        bool cmd_scroll_to_end;
    } wnd_debug_output;

    struct : Wnd {
    } wnd_about;

    struct : Wnd {
        Project_Settings tmp;
        int current_debug_profile;
        int current_build_profile;
        int tmp_debug_profile_type;

        bool focus_debug_profiles;
        bool focus_build_profiles;
    } wnd_project_settings;

    struct : Wnd {
        float height;
    } error_list;

    Build build;

    struct : Wnd {
        FT_Node *selection;
        // char buf[256];
        // bool adding_something;
        // bool thing_being_added_is_file;
        FT_Node *last_file_copied;
        FT_Node *last_file_cut;
        FT_Node *scroll_to;
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
        vec2f mouse_pos;
        // vec2f mouse_delta;
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

    struct : Wnd {
        bool replace;
        char find_str[256];
        char replace_str[256];
        bool use_regex;
        bool case_sensitive;
        int focus_textbox;
        int selection;
    } wnd_search_and_replace;

    struct : Wnd {
        bool show_anon_nodes;
        bool show_comments;
        // bool move;
        // cur2 move_to_cur;
    } wnd_editor_tree;

    struct : Wnd {
    } wnd_options;

    struct : Wnd {
    } wnd_editor_toplevels;

    struct : Wnd {
        char name[MAX_PATH];
        char location[MAX_PATH];
        bool location_is_root;
        bool folder;
        FT_Node *dest;
    } wnd_add_file_or_folder;

    struct : Wnd {
        char name[MAX_PATH];
        char location[MAX_PATH];
        FT_Node *target;
    } wnd_rename_file_or_folder;

    struct : Wnd {
        char query[MAX_PATH];
        u32 selection;
        List<ccstr> *filepaths;
        List<int> *filtered_results;
    } wnd_goto_file;

    struct : Wnd {
        char query[MAX_PATH];
        u32 selection;
        List<ccstr> *symbols;
        List<int> *filtered_results;
    } wnd_goto_symbol;

    struct {
        i32 scroll_offset;
        // ???
    } wnd_build_and_debug;

    struct {
        bool focused;
        int current_goroutine;
        int current_frame;
        char new_watch_buf[256];
        Dlv_Var *watch_selection;
        Dlv_Var *locals_selection;
    } wnd_debugger;

    struct : Wnd {
    } wnd_style_editor;

    void init(GLFWwindow *_wnd);
    void start_background_threads();

    Pane* get_current_pane();
    Editor* get_current_editor();
    Editor* find_editor(find_editor_func f);
    Editor* find_editor_by_id(u32 id);
    Editor* find_editor_by_filepath(ccstr filepath);
    void fill_file_tree();

    Editor *focus_editor(ccstr path);
    Editor *focus_editor(ccstr path, cur2 pos);
    Editor* focus_editor_by_id(int editor_id, cur2 pos);

    FT_Node *sort_ft_nodes(FT_Node *nodes);
    void add_ft_node(FT_Node *parent, fn<void(FT_Node* it)> cb);
    int compare_ft_nodes(FT_Node *a, FT_Node *b);
    FT_Node *find_ft_node(ccstr relpath);
    FT_Node *find_or_create_ft_node(ccstr relpath, bool is_directory);
    void delete_ft_node(FT_Node *it);
    ccstr ft_node_to_path(FT_Node *node);
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
void goto_file_and_pos(ccstr file, cur2 pos, Ensure_Cursor_Mode mode = ECM_NONE);
void goto_jump_to_definition_result(Jump_To_Definition_Result *result);
void handle_goto_definition();
void save_all_unsaved_files();
void start_search_job(ccstr query);
void goto_error(int index);
void goto_next_error(int direction);
void reload_file_subtree(ccstr path);
bool kick_off_find_references();
void open_rename_identifier();

extern u64 post_insert_dotrepeat_time;

bool move_autocomplete_cursor(Editor *ed, int direction);
Jump_To_Definition_Result *get_current_definition(ccstr *filepath = NULL);

extern int gargc;
extern char **gargv;


