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
#include "fzy_match.h"

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

    MTM_RELOAD_EDITOR,
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

    ccstr build_profile_name;
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

/*
enum {
    DISCARD_UNSAVED = 0,
    SAVE_UNSAVED = 1,
};
*/

enum Command {
    CMD_NEW_FILE,
    CMD_SAVE_FILE,
    CMD_SAVE_ALL,
    CMD_EXIT,
    CMD_SEARCH,
    CMD_SEARCH_AND_REPLACE,
    CMD_FILE_EXPLORER,
    CMD_ERROR_LIST,
    CMD_GO_TO_FILE,
    CMD_GO_TO_SYMBOL,
    CMD_GO_TO_NEXT_ERROR,
    CMD_GO_TO_PREVIOUS_ERROR,
    CMD_GO_TO_DEFINITION,
    CMD_FIND_REFERENCES,
    CMD_FORMAT_FILE,
    CMD_FORMAT_FILE_AND_ORGANIZE_IMPORTS,
    CMD_FORMAT_SELECTION,
    CMD_RENAME,
    CMD_ADD_NEW_FILE,
    CMD_ADD_NEW_FOLDER,
    CMD_PROJECT_SETTINGS,
    CMD_BUILD,
    CMD_BUILD_RESULTS,
    CMD_BUILD_PROFILES,
    CMD_CONTINUE,
    CMD_START_DEBUGGING,
    CMD_DEBUG_TEST_UNDER_CURSOR,
    CMD_BREAK_ALL,
    CMD_STOP_DEBUGGING,
    CMD_STEP_OVER,
    CMD_STEP_INTO,
    CMD_STEP_OUT,
    CMD_RUN_TO_CURSOR,
    CMD_TOGGLE_BREAKPOINT,
    CMD_DELETE_ALL_BREAKPOINTS,
    CMD_DEBUG_OUTPUT,
    CMD_DEBUG_PROFILES,
    CMD_RESCAN_INDEX,
    CMD_OBLITERATE_AND_RECREATE_INDEX,
    CMD_OPTIONS,
    CMD_ABOUT,
    CMD_UNDO,
    CMD_REDO,
    CMD_GENERATE_IMPLEMENTATION,
    CMD_FIND_IMPLEMENTATIONS,
    CMD_FIND_INTERFACES,
    CMD_DOCUMENTATION,
    CMD_VIEW_CALLER_HIERARCHY,
    CMD_VIEW_CALLEE_HIERARCHY,
    CMD_BUY_LICENSE,
    CMD_ENTER_LICENSE,

    _CMD_COUNT_,
};

struct Command_Info {
    int mods;
    int key;
    ccstr name;
};

extern Command_Info command_info_table[_CMD_COUNT_];

enum Auth_State {
    AUTH_NOTHING,
    AUTH_TRIAL,
    AUTH_REGISTERED,
};

enum GH_Auth_Status {
    GH_AUTH_WAITING,
    GH_AUTH_OK,
    GH_AUTH_INTERNETERROR,
    GH_AUTH_UNKNOWNERROR,
    GH_AUTH_BADCREDS,
};

struct Auth_To_Disk {
    Auth_State state;
    u64 grace_period_start;

    union {
        struct {
            u64 trial_start;
        };

        struct {
            char reg_email[256];
            int reg_email_len;
            char reg_license[256];
            int reg_license_len;
        };
    };
};

struct Auth_Extras {
    // ???
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
    Pool find_references_mem;
    Pool rename_identifier_mem;
    Pool run_command_mem;
    // TODO: should i start scoping these to their respective Wnd_*?
    Pool generate_implementation_mem;
    Pool find_implementations_mem;
    Pool find_interfaces_mem;
    Pool caller_hierarchy_mem;
    Pool callee_hierarchy_mem;

    Fridge<Mark> mark_fridge;
    Fridge<Mark_Node> mark_node_fridge;
    Fridge<Change> change_fridge;
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
    bool use_nvim_this_time;

    Auth_To_Disk auth;
    Auth_Extras auth_extras;
    bool auth_error;
    GH_Auth_Status auth_status;
    char authed_email[256];

    bool cmd_unfocus_all_windows;

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

    bool flag_defocus_imgui;

    struct Navigation_Dest {
        int editor_id;
        cur2 pos;
    };

    Navigation_Dest _navigation_queue[256];
    List<Navigation_Dest> navigation_queue;

    // bool navigating_to;
    // cur2 navigating_to_pos;
    // int navigating_to_editor;

    bool replace_line_numbers_with_bytecounts;
    bool turn_off_framerate_cap;
    bool randomly_move_cursor_around;

    Fs_Watcher fswatch;

    bool auth_update_done;
    u64 auth_update_last_check;

    struct Wnd_Enter_License : Wnd {
        char email[256];
        char license[256];
    } wnd_enter_license;

    struct Wnd_Hover_Info : Wnd {
        // ...
    } wnd_hover_info;

    struct Wnd_Rename_Identifier : Wnd {
        Pool thread_mem;
        bool running;
        bool too_late_to_cancel = false;
        char rename_to[256];
        Goresult *declres;
        Thread_Handle thread;
    } wnd_rename_identifier;

// #ifdef DEBUG_MODE
    struct Wnd_History : Wnd {} wnd_history;
// #endif

    struct Wnd_Generate_Implementation : Wnd {
        bool fill_running;
        Thread_Handle fill_thread;
        Pool fill_thread_pool;
        u64 fill_time_started_ms;

        u64 file_hash_on_open;
        Goresult *declres;
        List<Go_Symbol> *symbols;

        int selection;
        List<int> *filtered_results;
        char query[256];
        List<ccstr> errors;

        // if true, the user selected the interface,
        // and now needs to select the type to add methods on
        //
        // if false, the user selected the type,
        // and now needs to select the interface whose methods to add
        bool selected_interface;
    } wnd_generate_implementation;

    struct Wnd_Find_References : Wnd {
        Pool thread_mem;
        bool done;
        Goresult *declres;
        Thread_Handle thread;
        List<Find_References_File> *results;
        ccstr current_import_path;
    } wnd_find_references;

    struct Wnd_Find_Interfaces : Wnd {
        Pool thread_mem;
        bool search_everywhere;
        bool done;
        Goresult *declres;
        Thread_Handle thread;
        bool include_empty;
        List<Find_Decl*> *results;
        ccstr current_import_path;
    } wnd_find_interfaces;

    struct Wnd_Find_Implementations : Wnd {
        Pool thread_mem;
        bool search_everywhere;
        bool done;
        Goresult *declres;
        Thread_Handle thread;
        List<Find_Decl*> *results;
        ccstr current_import_path;
    } wnd_find_implementations;

    struct Wnd_Caller_Hierarchy : Wnd {
        Pool thread_mem;
        bool done;
        Goresult *declres;
        // TODO: when is it time to abstract out all this create new thread,
        // kill, etc logic? like we're now repeating it for find references,
        // find interfaces, find implementations, call hierarchy, etc...
        Thread_Handle thread;
        List<Call_Hier_Node> *results;
        ccstr current_import_path;
        bool show_tests_and_benchmarks;
    } wnd_caller_hierarchy;

    struct Wnd_Callee_Hierarchy : Wnd {
        Pool thread_mem;
        bool done;
        Goresult *declres;
        Thread_Handle thread;
        List<Call_Hier_Node> *results;
        ccstr current_import_path;
    } wnd_callee_hierarchy;

    struct Wnd_Index_Log : Wnd {
        // ring buffer
        char buf[INDEX_LOG_CAP][INDEX_LOG_MAXLEN];
        int start;
        int len;
        bool cmd_scroll_to_end;
    } wnd_index_log;

    struct Wnd_Mouse_Pos : Wnd {
    } wnd_mouse_pos;

    struct Wnd_Debug_Output : Wnd {
        int selection;
        bool cmd_scroll_to_end;
    } wnd_debug_output;

    struct Wnd_About : Wnd {
    } wnd_about;

    struct Wnd_Project_Settings : Wnd {
        Project_Settings tmp;
        int current_debug_profile;
        int current_build_profile;
        int tmp_debug_profile_type;

        bool focus_general_settings;
        bool focus_debug_profiles;
        bool focus_build_profiles;
    } wnd_project_settings;

    struct Error_List : Wnd {
        float height;
    } error_list;

    Build build;

    struct File_Explorer : Wnd {
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
        bool mouse_just_released[ImGuiMouseButton_COUNT];
        GLFWcursor* cursors[ImGuiMouseCursor_COUNT];
        GLuint textures[__TEXTURE_COUNT__];
        bool mouse_captured_by_imgui;
        bool keyboard_captured_by_imgui;
    } ui;

    struct Windows_Open {
        bool im_demo;
        bool im_metrics;
    } windows_open;

    struct Wnd_Search_And_Replace : Wnd {
        bool replace;
        char find_str[256];
        char replace_str[256];
        bool use_regex;
        bool case_sensitive;
        int focus_textbox;
        int selection;
    } wnd_search_and_replace;

    struct Wnd_Editor_Tree : Wnd {
        bool show_anon_nodes;
        bool show_comments;
        // bool move;
        // cur2 move_to_cur;
    } wnd_editor_tree;

    struct Wnd_Options : Wnd {
        Options tmp;
        bool something_that_needs_restart_was_changed;
    } wnd_options;

    struct Wnd_Editor_Toplevels : Wnd {
    } wnd_editor_toplevels;

    struct Wnd_Add_File_Or_Folder : Wnd {
        char name[MAX_PATH];
        char location[MAX_PATH];
        bool location_is_root;
        bool folder;
        // FT_Node *dest;
    } wnd_add_file_or_folder;

    struct Wnd_Rename_File_or_Folder : Wnd {
        char name[MAX_PATH];
        char location[MAX_PATH];
        FT_Node *target;
    } wnd_rename_file_or_folder;

    struct Wnd_Goto_File : Wnd {
        char query[MAX_PATH];
        u32 selection;
        List<ccstr> *filepaths;
        List<int> *filtered_results;
    } wnd_goto_file;

    struct Wnd_Command : Wnd {
        char query[256];
        u32 selection;
        List<Command> *actions;
        List<int> *filtered_results; // directly holds casted commands
    } wnd_command;

    struct Wnd_Goto_Symbol : Wnd {
        bool fill_running;
        Thread_Handle fill_thread;
        Pool fill_thread_pool;
        u64 fill_time_started_ms;

        char query[MAX_PATH];
        ccstr current_import_path;
        bool current_file_only;
        u32 selection;
        List<Go_Symbol> *symbols;
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

    struct Wnd_Style_Editor : Wnd {
    } wnd_style_editor;

    void init(GLFWwindow *_wnd);
    void start_background_threads();
};

extern World world;

void activate_pane(Pane *pane);
void activate_pane_by_index(u32 idx);

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
void delete_ft_node(FT_Node *it, bool delete_on_disk = true);
ccstr ft_node_to_path(FT_Node *node);

bool is_ignored_by_git(ccstr path, bool isdir);

void init_goto_file();
void filter_files();
void filter_symbols();

void kick_off_build(Build_Profile *build_profile = NULL);
void prompt_delete_all_breakpoints();
void run_proc_the_normal_way(Process* proc, ccstr cmd);
void* get_native_window_handle();
bool is_build_debug_free();
void goto_file_and_pos(ccstr file, cur2 pos, Ensure_Cursor_Mode mode = ECM_NONE);
void goto_jump_to_definition_result(Jump_To_Definition_Result *result);
void handle_goto_definition(cur2 pos = {-1, -1});
void save_all_unsaved_files();
void start_search_job(ccstr query);
void goto_error(int index);
void goto_next_error(int direction);
void reload_file_subtree(ccstr path);
void open_rename_identifier();
void kick_off_rename_identifier();
void cancel_rename_identifier();
void cancel_caller_hierarchy();
void cancel_callee_hierarchy();
void cancel_find_references();
void cancel_find_interfaces();
void cancel_find_implementations();
bool exclude_from_file_tree(ccstr path);

extern u64 post_insert_dotrepeat_time;

bool move_autocomplete_cursor(Editor *ed, int direction);
Jump_To_Definition_Result *get_current_definition(ccstr *filepath = NULL, bool display_error = false, cur2 pos = {-1, -1});

extern int gargc;
extern char **gargv;

ccstr get_command_name(Command action);
bool is_command_enabled(Command action);
void init_command_info_table();
void handle_command(Command action, bool from_menu);
void open_add_file_or_folder(bool folder, FT_Node *dest = NULL);
void do_generate_implementation();
bool has_unsaved_files();

void fuzzy_sort_filtered_results(ccstr query, List<int> *list, int total_results, fn<ccstr(int)> get_name);
void do_find_interfaces();
void do_find_implementations();

void read_auth();
void write_auth();
