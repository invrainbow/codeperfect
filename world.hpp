#pragma once

#include <math.h>
#include "glcrap.hpp"
#include "common.hpp"
#include "editor.hpp"
#include "ui.hpp"
#include "os.hpp"
#include "go.hpp"
#include "dbg.hpp"
#include "utils.hpp"
#include "imgui.h"
#include "mem.hpp"
#include "settings.hpp"
#include "search.hpp"
#include "fzy_match.h"
#include "win.hpp"
#include "jblow_tests.hpp"

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

enum Fps_Limit {
    FPS_30,
    FPS_60,
    FPS_120,
};

struct FT_Node {
    bool is_directory : 1;
    bool open : 1;
    ccstr name;
    i16 num_children;
    u8 depth;
    FT_Node *parent;
    FT_Node *children;
    FT_Node *prev;
    FT_Node *next;
};

enum Main_Thread_Message_Type {
    MTM_GOTO_FILEPOS,
    /**/
    MTM_FILETREE_DELETE,
    MTM_FILETREE_CREATE,
    /**/
    MTM_TELL_USER,
    /**/
    MTM_RELOAD_EDITOR,
    MTM_EXIT,
    MTM_FOCUS_APP_DEBUGGER,
    /**/
    MTM_WRITE_LAST_FOLDER,
    /**/
    // for tests
    MTM_TEST_MOVE_CURSOR,
    MTM_RESET_AFTER_DEFOCUS,
};

struct Main_Thread_Message {
    Main_Thread_Message_Type type;

    union {
        int focus_app_debugger_pid;
        u32 reload_editor_id;
        struct {
            ccstr goto_file;
            cur2 goto_pos;
        };
        struct {
            ccstr tell_user_text;
            ccstr tell_user_title;
        };
        struct {
            ccstr exit_message;
            int exit_code;
        };
        ccstr debugger_stdout_line;
        cur2 test_move_cursor;
        List<Mark*> *search_marks;
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
    bool go_forward(int count = 1);
    bool go_backward(int count = 1);
    void remove_entries_for_editor(int editor_id);
    void save_latest();
    void check_marks(int upper = -1);

    int inc(int i) { return i == _countof(ring) - 1 ? 0 : i + 1; }
    int dec(int i) { return !i ? _countof(ring) - 1 : i - 1; }
};

struct Build {
    Pool mem;

    ccstr build_profile_name;
    u64 id;
    bool done;
    bool started;
    List<Build_Error> errors;
    int current_error;
    bool build_itself_had_error;
    Thread_Handle thread;
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
    CMD_FIND_NEXT,
    CMD_FIND_PREVIOUS,
    CMD_FIND_CLEAR,
    CMD_GO_TO_NEXT_SEARCH_RESULT,
    CMD_GO_TO_PREVIOUS_SEARCH_RESULT,
    CMD_FIND_REFERENCES,
    CMD_FORMAT_FILE,
    CMD_ORGANIZE_IMPORTS,
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
    CMD_UNDO,
    CMD_REDO,
    CMD_CUT,
    CMD_COPY,
    CMD_PASTE,
    CMD_SELECT_ALL,
    CMD_GENERATE_IMPLEMENTATION,
    CMD_FIND_IMPLEMENTATIONS,
    CMD_FIND_INTERFACES,
    CMD_DOCUMENTATION,
    CMD_VIEW_CALLER_HIERARCHY,
    CMD_VIEW_CALLEE_HIERARCHY,
    CMD_REPLACE,
    CMD_FIND,
    CMD_GENERATE_FUNCTION,
    CMD_TOGGLE_COMMENT,
    CMD_BUILD_PROFILE_1,
    CMD_BUILD_PROFILE_2,
    CMD_BUILD_PROFILE_3,
    CMD_BUILD_PROFILE_4,
    CMD_BUILD_PROFILE_5,
    CMD_BUILD_PROFILE_6,
    CMD_BUILD_PROFILE_7,
    CMD_BUILD_PROFILE_8,
    CMD_BUILD_PROFILE_9,
    CMD_BUILD_PROFILE_10,
    CMD_BUILD_PROFILE_11,
    CMD_BUILD_PROFILE_12,
    CMD_BUILD_PROFILE_13,
    CMD_BUILD_PROFILE_14,
    CMD_BUILD_PROFILE_15,
    CMD_BUILD_PROFILE_16,
    CMD_DEBUG_PROFILE_1,
    CMD_DEBUG_PROFILE_2,
    CMD_DEBUG_PROFILE_3,
    CMD_DEBUG_PROFILE_4,
    CMD_DEBUG_PROFILE_5,
    CMD_DEBUG_PROFILE_6,
    CMD_DEBUG_PROFILE_7,
    CMD_DEBUG_PROFILE_8,
    CMD_DEBUG_PROFILE_9,
    CMD_DEBUG_PROFILE_10,
    CMD_DEBUG_PROFILE_11,
    CMD_DEBUG_PROFILE_12,
    CMD_DEBUG_PROFILE_13,
    CMD_DEBUG_PROFILE_14,
    CMD_DEBUG_PROFILE_15,
    CMD_DEBUG_PROFILE_16,
    CMD_ADD_JSON_TAG,
    CMD_ADD_YAML_TAG,
    CMD_ADD_XML_TAG,
    CMD_ADD_ALL_JSON_TAGS,
    CMD_ADD_ALL_YAML_TAGS,
    CMD_ADD_ALL_XML_TAGS,
    CMD_REMOVE_TAG,
    CMD_REMOVE_ALL_TAGS,
    CMD_GO_BACK,
    CMD_GO_FORWARD,
    CMD_AST_NAVIGATION,
    CMD_COMMAND_PALETTE,
    CMD_OPEN_FILE_MANUALLY,
    CMD_CLOSE_EDITOR,
    CMD_CLOSE_ALL_EDITORS,
    CMD_OPEN_LAST_CLOSED_EDITOR,
    CMD_OPEN_FOLDER,
    CMD_ZOOM_IN,
    CMD_ZOOM_OUT,
    CMD_ZOOM_ORIGINAL,
    CMD_GO_TO_NEXT_EDITOR,
    CMD_GO_TO_PREVIOUS_EDITOR,
    /**/
    _CMD_COUNT_,
    CMD_INVALID = -1,
};

struct Command_Shortcut {
    int key;
    int mods;
    Command_Shortcut *next;
};

struct Command_Info {
    ccstr name;
    bool allow_shortcut_when_imgui_focused;

    Command_Shortcut *shortcuts;

    bool has_shortcut(int mods, int key) {
        for (auto it = shortcuts; it; it = it->next)
            if (it->key == key && it->mods == mods)
                return true;
        return false;
    }
};

extern Command_Info command_info_table[_CMD_COUNT_];

struct Last_Closed {
    char filepath[MAX_PATH];
    cur2 pos;
};

struct Drawn_Quad {
    boxf b;
    vec4f color;
    Draw_Mode mode;
    Texture_Id texture;
    ccstr backtrace;
};

enum Goto_Symbol_State {
    GOTO_SYMBOL_READY,
    GOTO_SYMBOL_RUNNING,
    GOTO_SYMBOL_WAITING,
    GOTO_SYMBOL_ERROR,
};

struct Search_Marks_File {
    ccstr filepath;
    List<Mark*> *mark_starts;
    List<Mark*> *mark_ends;
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
    Pool project_settings_mem;
    Pool fst_mem;
    Pool search_marks_mem;

    Pool workspace_mem_1;
    Pool workspace_mem_2;
    bool which_workspace_mem;

    Fridge<Mark> mark_fridge;
    Fridge<Avl_Node> avl_node_fridge;
    Fridge<Treap> treap_fridge;
    Fridge<Change> change_fridge;
    Fridge<Chunk0> chunk0_fridge;
    Fridge<Chunk1> chunk1_fridge;
    Fridge<Chunk2> chunk2_fridge;
    Fridge<Chunk3> chunk3_fridge;
    Fridge<Chunk4> chunk4_fridge;
    Fridge<Chunk5> chunk5_fridge;
    Fridge<Chunk6> chunk6_fridge;
    Fridge<Chunk7> chunk7_fridge;

    char configdir[MAX_PATH];

    bool time_type_char;
    bool test_running;
    char test_name[256];

    Jblow_Tests jblow_tests;

    List<int> *konami;
    List<Last_Closed> *last_closed;

    struct Frameskip {
        u64 timestamp;
        u64 ms_over;
    };

    Go_Workspace *workspace;

    List<Frameskip> frameskips;

    char gh_version[16];
    u64 frame_index;

    Window* window;

    vec2 window_size;
    vec2 display_size;
    vec2 frame_size;
    vec2f display_scale;

    float zoom_level;

    float get_zoom_scale() {
        auto zl = options.zoom_level;
        if (!zl) zl = 100;
        return sqrt(zl / 100.0f);
    }

    vec2f get_display_scale() {
        auto ret = display_scale;

        auto zs = get_zoom_scale();
        ret.x *= zs;
        ret.y *= zs;
        return ret;
    }

    int xdpi;
    int ydpi;

    bool cmd_unfocus_all_windows;

    bool dont_push_history;

    Message_Queue<Main_Thread_Message> message_queue;
    Searcher searcher;

    List<Search_Marks_File> *search_marks;
    int search_marks_state_id;

    Go_Indexer indexer;

    struct {
        // is vim enabled
        bool on;

        // this variable is going to be marked "private" because we have a
        // fuckload of logic centered around the vim mode, and it is crucial we
        // set it using the vim_*() methods below and in Editor instead of setting it
        // directly
        Vim_Mode _mode;

        Pool mem;

        struct {
            Pool mem_finished;
            Pool mem_working;
            Vim_Dotrepeat_Input input_finished;
            Vim_Dotrepeat_Input input_working;
        } dotrepeat;

        Vim_Macro macros[26+10+1]; // A-Z0-9"
        Vim_Macro_State macro_state;
        struct {
            char macro;
            int runs;      // how many runs
            int run_idx;   // which run we're on
            int input_idx; // which input we're on current run
            char last;
        } macro_run;
        struct {
            char macro;
            char last;
        } macro_record;

        List<char> yank_register;
        bool yank_register_filled;
    } vim;

    Vim_Mode vim_mode() { return vim._mode; }

    // so we can track where we're setting
    void vim_set_mode(Vim_Mode new_mode, bool calling_from_vim_return_to_normal_mode = false) {
        if (vim_mode() == VI_INSERT && new_mode == VI_NORMAL)
            cp_assert(calling_from_vim_return_to_normal_mode);
        vim._mode = new_mode;
    }

    struct Debugger dbg;

    struct Wnd_Mem_Viewer : Wnd {
        bool only_show_significant_pools;
    } wnd_mem_viewer;

    u32 next_editor_id;

    FT_Node *file_tree;
    bool file_tree_busy;
    Thread_Handle file_tree_fill_thread;

    u64 next_build_id;

    char go_binary_path[MAX_PATH];
    char current_path[MAX_PATH];

    // lol this was back before we had dynamic arrays and world_mem
    Pane _panes[MAX_PANES];
    List<Pane> panes;
    u32 current_pane;

    i32 resizing_pane; // if this value is i, we're resizing the border between i and i+1

    History history;

    bool darkmode;

    Lock global_mark_tree_lock;
    Lock build_lock;

    bool flag_defocus_imgui;
    Timer fst;

    struct Navigation_Dest {
        int editor_id;
        cur2 pos;
    };

    Navigation_Dest _navigation_queue[256];
    List<Navigation_Dest> navigation_queue;

    // bool navigating_to;
    // cur2 navigating_to_pos;
    // int navigating_to_editor;

    bool turn_off_framerate_cap;
    bool show_frame_index;
    bool trace_next_frame;
    bool show_frameskips;
    bool dont_prompt_on_close_unsaved_tab;

    Fs_Watcher fswatch;

    // i guess this should go in wnd_run_command, but i don't want it to get cleared out if
    // i ptr0 the whole wnd, and i also don't want to have to worry about not doing that
    Command last_manually_run_command;

    struct {
        cur2 pos;
        cur2 token_start;
        cur2 token_end;
        ccstr token;
    } editor_context_menu;

    // only used for cmds, not show
    struct Wnd_Local_Variables : Wnd {} wnd_local_variables;
    struct Wnd_Watches : Wnd {} wnd_watches;
    struct Wnd_Call_Stack : Wnd {} wnd_call_stack;

    struct Wnd_Local_Search : Wnd {
        char query[256];
        char permanent_query[256];
        char replace_str[256];
        bool replace;
        bool case_sensitive;
        bool use_regex;
        bool opened_from_vim;
    } wnd_local_search;

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

// #ifdef DEBUG_BUILD
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
        Go_Workspace *workspace;
        int current_file;
        int current_result;

        int scroll_to_file;
        int scroll_to_result;

    } wnd_find_references;

    struct Wnd_Find_Interfaces : Wnd {
        Pool thread_mem;
        bool search_everywhere;
        bool done;
        Goresult *declres;
        Thread_Handle thread;
        bool include_empty;
        List<Find_Decl*> *results;
        Go_Workspace *workspace;
        int selection;
        int scroll_to;
    } wnd_find_interfaces;

    struct Wnd_Find_Implementations : Wnd {
        Pool thread_mem;
        bool search_everywhere;
        bool done;
        Goresult *declres;
        Thread_Handle thread;
        List<Find_Decl*> *results;
        Go_Workspace *workspace;
        int selection;
        int scroll_to;
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
        Go_Workspace *workspace;
        bool show_tests_benches;
    } wnd_caller_hierarchy;

    struct Wnd_Callee_Hierarchy : Wnd {
        Pool thread_mem;
        bool done;
        Goresult *declres;
        Thread_Handle thread;
        List<Call_Hier_Node> *results;
        Go_Workspace *workspace;
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

    struct Wnd_Project_Settings : Wnd {
        Pool pool;

        Project_Settings *tmp;
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
        // char buf[256];
        // bool adding_something;
        // bool thing_being_added_is_file;

        // these all need to be invalidated - do so whenever we add a new one
        FT_Node *selection;
        FT_Node *last_file_copied;
        FT_Node *last_file_cut;
        FT_Node *scroll_to;
        FT_Node *dragging_source;
        FT_Node *dragging_dest;

        FT_Node *hovered;
        u64 hovered_start_milli;
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
        // seems like so far when writing imgui code i've been assuming one global window in whole app
        // but when writing window/opengl context code i've been assuming possibility of multiple windows
        // unify this at some point
        bool mouse_down[ImGuiMouseButton_COUNT];
        bool mouse_just_pressed[ImGuiMouseButton_COUNT];
        bool mouse_just_released[ImGuiMouseButton_COUNT];
        Cursor cursors[ImGuiMouseCursor_COUNT];
        bool cursors_ready[ImGuiMouseCursor_COUNT];
        GLuint textures[__TEXTURE_COUNT__];
        bool mouse_captured_by_imgui;
        bool keyboard_captured_by_imgui;
        bool input_captured_by_imgui;
    } ui;

    struct Windows_Open {
        bool im_demo;
        bool im_metrics;
    } windows_open;

    struct Wnd_Search_And_Replace : Wnd {
        Pool mem;
        bool replace;
        char find_str[256];
        char replace_str[256];
        bool use_regex;
        bool search_go_files_only;
        bool case_sensitive;
        int sel_file;
        int sel_result;
        int scroll_file;
        int scroll_result;
        bool *files_open;
        bool *set_file_open;
        bool *set_file_close;
        bool cmd_focus_replace_textbox;
    } wnd_search_and_replace;

    struct Wnd_Tree_Viewer : Wnd {
        // ast viewer
        bool show_anon_nodes;
        bool show_comments;
        // bool move;
        // cur2 move_to_cur;

        // gofile
        Pool pool;
        Go_File *gofile;
        ccstr filepath;
    } wnd_tree_viewer;

    struct Wnd_Options : Wnd {
        Options tmp;
        bool something_that_needs_restart_was_changed;
    } wnd_options;

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
        Goto_Symbol_State state;

        Thread_Handle fill_thread;
        Pool fill_thread_pool;
        u64 fill_time_started_ms;

        char query[MAX_PATH];
        Go_Workspace *workspace;
        bool current_file_only;
        u32 selection;
        List<Go_Symbol> *symbols;
        List<int> *filtered_results;
    } wnd_goto_symbol;

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

    struct Wnd_Poor_Mans_Gpu_Debugger : Wnd {
        bool tracking;
        Pool mem;
        List<Drawn_Quad> *logs;
        int selected_quad;
    } wnd_poor_mans_gpu_debugger;

    void init();
    void start_background_threads();
};

extern World world;

void activate_pane(Pane *pane);
void activate_pane_by_index(u32 idx);

Pane* get_current_pane();
Editor* get_current_editor();
Editor* find_editor(find_editor_func f);
Editor* find_editor_by_id(u32 id);
Editor* find_editor_by_filepath(ccstr filepath); // this is fairly expensive -- it does a stat lookup, not just string based comparison
void fill_file_tree();

Editor *focus_editor(ccstr path);
Editor *focus_editor(ccstr path, cur2 pos, bool pos_in_byte_format = false);
Editor* focus_editor_by_id(int editor_id, cur2 pos, bool pos_in_byte_format = false);

FT_Node *sort_ft_nodes(FT_Node *nodes);
void add_ft_node(FT_Node *parent, fn<void(FT_Node* it)> cb);
int compare_ft_nodes(FT_Node *a, FT_Node *b);
FT_Node *find_ft_node(ccstr relpath);
FT_Node *find_or_create_ft_node(ccstr relpath, bool is_directory);
void delete_ft_node(FT_Node *it, bool delete_on_disk = true);
ccstr ft_node_to_path(FT_Node *node);

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
Editor* goto_file_and_pos(ccstr file, cur2 pos, bool pos_in_byte_format = false, Ensure_Cursor_Mode mode = ECM_NONE);
void goto_jump_to_definition_result(Jump_To_Definition_Result *result);
void handle_goto_definition(cur2 pos = {-1, -1});
void save_all_unsaved_files();
void start_search_job(ccstr query);
void goto_error(int index);
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

bool move_autocomplete_cursor(Editor *editor, int direction);
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
void do_generate_function();

bool write_project_settings();
void handle_window_focus(bool focus);

void fstlog(ccstr fmt, ...);

void recalc_display_size();

void set_zoom_level(int level);

List<Editor*> *get_all_editors();
void reset_everything_when_switching_editors(Editor *old_editor);

void open_current_file_search(bool replace, bool from_vim);
void move_search_result(bool forward, int count);

bool close_editor(Pane *pane, int editor_index);
bool close_pane(int idx);

bool initiate_rename_identifier(cur2 pos);
void initiate_find_references(cur2 pos);

bool is_imgui_hogging_keyboard();
void create_search_marks_for_editor(Searcher_Result_File *file, Editor *editor);
cur2 get_search_mark_pos(ccstr filepath, int index, bool start);
