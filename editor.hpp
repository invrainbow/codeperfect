#pragma once

#include "list.hpp"
#include "buffer.hpp"
#include "go.hpp"
#include "os.hpp"
#include "tree_sitter_crap.hpp"

const int AUTOCOMPLETE_WINDOW_ITEMS = 10;

#define MAX_BREAKPOINTS 128

enum {
    HINT_NAME,
    HINT_TYPE,
    HINT_NORMAL,
    HINT_CURRENT_PARAM,
    HINT_NOT_CURRENT_PARAM,
};

enum Vim_Mode {
    VI_NONE,
    VI_NORMAL,
    VI_VISUAL,
    VI_INSERT,
    VI_REPLACE,
    VI_OPERATOR,
    VI_CMDLINE,
    VI_UNKNOWN,
};

struct Client_Parameter_Hint {
    Gotype *gotype; // save this here in case we need it later
    int current_param;

    // ccstr help_text;

    cur2 start;
    bool closed;
};

struct Postfix_Info {
    bool in_progress;
    List<cur2> insert_positions;
    cur2 _insert_positions[16];
    int current_insert_position;

    void start() {
        ptr0(this);

        in_progress = true;
        insert_positions.init(LIST_FIXED, _countof(_insert_positions), _insert_positions);
    }
};

struct Pane;

enum Ensure_Cursor_Mode {
    ECM_NONE,
    ECM_GOTO_DEF,
};

struct Move_Cursor_Opts {
    // NOTE: Honestly, this is not a great name. The point isn't whether it's a
    // user movement, but whether we should reset location-pegged things like
    // autocomplete and appending keystrokes to the most recent change in history.
    bool is_user_movement;
};

Move_Cursor_Opts *default_move_cursor_opts();

struct Vim_Command_Input {
    bool is_key;
    union {
        uchar ch;
        struct {
            int key;
            int mods;
        };
    };
};

enum Vim_Parse_Status {
    VIM_PARSE_DONE,
    VIM_PARSE_WAIT,
    VIM_PARSE_DISCARD,
};

struct Vim_Command {
    int o_count;
    List<Vim_Command_Input> *op;
    int m_count;
    List<Vim_Command_Input> *motion;

    void init() {
        ptr0(this);
        op = new_list(Vim_Command_Input);
        motion = new_list(Vim_Command_Input);
    }

    Vim_Command *copy();
};

enum Vim_Dotrepeat_Command_Type {
    VDC_COMMAND,
    VDC_VISUAL_MOVE,
    VDC_INSERT_TEXT,
};

struct Vim_Dotrepeat_Command {
    Vim_Dotrepeat_Command_Type type;
    union {
        Vim_Command command;
        cur2 visual_move_distance;
        struct {
            List<uchar> *chars;
            int backspaced_graphemes;
        } insert_text;
    };

    Vim_Dotrepeat_Command *copy();
};

struct Vim_Dotrepeat_Input {
    bool filled;
    List<Vim_Dotrepeat_Command> *commands;

    Vim_Dotrepeat_Input* copy();
};

enum Motion_Type {
    MOTION_LINE,
    MOTION_CHAR_INCL,
    MOTION_CHAR_EXCL,
    MOTION_OBJ,
    MOTION_OBJ_INNER,
};

struct Motion_Result {
    Motion_Type type;
    cur2 dest; // anything else?
    cur2 object_start; // for MOTION_OBJECT and MOTION_OBJECT_INNER
};

struct Motion_Range {
    cur2 start;
    cur2 end;
    bool is_line;
    // stuff for is_line
    bool at_end;
    int y1;
    int y2;
};

enum Selection_Type {
    SEL_CHAR,
    SEL_LINE,
    SEL_BLOCK,
    SEL_NONE = -1, // sentinel
};

struct Selection_Range {
    cur2 start;
    cur2 end;
};

struct Selection {
    Selection_Type type;
    List<Selection_Range> *ranges;
};

enum Find_Matching_Brace_Result {
    FMB_ERROR,
    FMB_OK,
    FMB_AST_NOT_FOUND,
    FMB_MATCH_NOT_FOUND,
};

// zii
struct Type_Char_Opts {
    bool replace_mode;
    // basically, is this triggered in code for the purpose of getting type_char logic, but we don't actually want the user effects like triggering autocomplete etc
    bool automated;
};

struct Vim_Macro {
    bool active;
    List<Vim_Command_Input> *inputs;
    Pool mem;
};

enum Vim_Macro_State {
    MACRO_IDLE,
    MACRO_RUNNING,
    MACRO_RECORDING,
};

struct Editor {
    u32 id;
    Pool mem;
    Pane* pane;

    Buffer *buf;

    box view;
    cur2 cur;
    int savedvx;

    char filepath[MAX_PATH];
    bool is_untitled;
    bool file_was_deleted;

    // only used when !world.use_nvim
    bool selecting;
    cur2 select_start;

    bool mouse_selecting;
    u64 mouse_drag_last_time_ms;
    i64 mouse_drag_accum;
    double scroll_leftover;

    u64 flash_cursor_error_start_time;

    void flash_cursor_error();

    struct {
        bool on;
        u64 time_start_milli;
        cur2 start;
        cur2 end;
    } highlight_snippet_state;

    bool double_clicked_selection;

    // TSInputEdit curr_change;

    // is this file "dirty" from the perspective of the index?
    Parse_Lang lang;
    u64 disable_file_watcher_until;

    bool saving;

    cur2 go_here_after_escape;

    List<Postfix_Info> postfix_stack;

    // need to reset this when backspacing past it and when accepting an auto
    cur2 last_closed_autocomplete;

    bool ui_rect_set;
    boxf ui_rect;

    struct Insert_Change {
        cur2 start; // line
        cur2 end;
        List<Line> lines;
    };

    struct {
        Autocomplete ac;
        List<int>* filtered_results;
        u32 selection;
        u32 view;
    } autocomplete;

    struct {
        Pool mem;
        bool on;
        Ast_Node *node;
        List<Ast_Node*> *siblings;
        int tree_version;
    } ast_navigation;

    struct {
        Pool mem;
        List<Vim_Command_Input> *command_buffer;
        int hidden_vx;

        // Right now the mode is stored in world, but the mode-specific
        // shit is stored here in editor. This is going to break things if
        // the user somehow switches editors without changing back to
        // normal mode. That seems really stupid.

        // can't use union because shit in each element needs to be init'd
        // separately

        cur2 insert_start;
        Vim_Command insert_command;

        cur2 visual_start;
        Selection_Type visual_type;

        // okay, for now replace is just going to work on codepoints just like nvim lol
        cur2 replace_start;
        cur2 replace_end;
        List<uchar> replace_old_chars;

        // for dotrepeat in insert/replace mode
        int edit_backspaced_graphemes;

        struct {
            bool inserted;
            int buf_version;
            cur2 start;
            cur2 end;
        } inserted_indent;

        Mark *local_marks[26];
        Mark *global_marks[36]; // A-Z and 0-9
    } vim;

    Client_Parameter_Hint parameter_hint;

    void init();
    void cleanup();

    bool is_unsaved() { return is_modifiable() && (file_was_deleted || buf->dirty); }
    bool is_modifiable();

    void move_cursor(cur2 c, Move_Cursor_Opts *opts = NULL);
    void reset_state();
    bool load_file(ccstr new_filepath);
    i32 cur_to_offset(cur2 c);
    i32 cur_to_offset();
    cur2 offset_to_cur(i32 offset);
    int get_indent_of_line(int y);
    Buffer_It iter();
    Buffer_It iter(cur2 _cur);
    void perform_autocomplete(AC_Result *result);

    void trigger_autocomplete(bool triggered_by_dot, bool triggered_by_typing_ident, uchar typed_ident_char = 0);
    void filter_autocomplete_results(Autocomplete* ac);
    void trigger_parameter_hint();
    void trigger_escape();

    void type_char(uchar ch, Type_Char_Opts *opts = NULL);
    void update_autocomplete(bool triggered_by_ident);
    void update_parameter_hint();

    void start_change();
    void end_change();

    void apply_edits(List<TSInputEdit> *edits);
    void reload_file(bool because_of_file_watcher = false);
    bool handle_escape();
    bool optimize_imports();
    void format_on_save(bool fix_imports);
    void handle_save(bool about_to_close = false);
    bool is_current_editor();
    void backspace_in_insert_mode();
    void backspace_in_replace_mode();
    void ensure_cursor_on_screen();
    void ensure_cursor_on_screen_by_moving_view(Ensure_Cursor_Mode mode = ECM_NONE);
    void insert_text_in_insert_mode(ccstr s);
    ccstr get_autoindent(int for_y);
    void add_change_in_insert_mode(cur2 start, cur2 old_end, cur2 new_end);
    bool cur_is_inside_comment_or_string();
    bool ask_user_about_unsaved_changes();
    void delete_selection();
    void toggle_comment(int ystart, int yend);
    void highlight_snippet(cur2 start, cur2 end);
    void update_selected_ast_node(Ast_Node *node);
    void update_ast_navigate(fn<Ast_Node*(Ast_Node*)> cb);
    void ast_navigate_in();
    void ast_navigate_out();
    void ast_navigate_prev();
    void ast_navigate_next();

    Find_Matching_Brace_Result find_matching_brace_with_ast(uchar ch, cur2 pos, cur2 *out);
    cur2 find_matching_brace_with_text(uchar ch, cur2 pos);
    cur2 find_matching_brace(uchar ch, cur2 pos);

    // === vim stuff ===

    Vim_Parse_Status vim_parse_command(Vim_Command *out);
    bool vim_handle_char(u32 ch);
    bool vim_handle_key(int key, int mods);
    bool vim_handle_input(Vim_Command_Input *input);

    Motion_Result* vim_eval_motion(Vim_Command *cmd);
    bool vim_exec_command(Vim_Command *cmd, bool *can_dotrepeat);
    int first_nonspace_cp(int y);
    cur2 open_newline(int y);
    cur2 handle_alt_move(bool back, bool backspace);
    Selection* get_selection(Selection_Type override_type = SEL_NONE);
    cur2 delete_selection(Selection *sel);
    ccstr get_selection_text(Selection *selection);
    void vim_handle_visual_mode_key(Selection_Type type);
    cur2 vim_delete_selection(Selection *selection);
    cur2 vim_delete_range(cur2 start, cur2 end);
    void vim_delete_lines(int y1, int y2);
    void vim_save_inserted_indent(cur2 start, cur2 end);
    void vim_enter_insert_mode(Vim_Command *cmd, fn<void()> prep);
    void vim_enter_replace_mode();
    void vim_return_to_normal_mode(bool from_dotrepeat = false);
    void vim_return_to_normal_mode_user_input();
    void vim_yank_text(ccstr text);
    void vim_handle_visual_S(Vim_Command *cmd);
    cur2 vim_handle_J(Vim_Command *cmd, bool add_spaces);
    ccstr vim_paste_text();
    cur2 vim_handle_text_transform_command(char command, Motion_Result *motion_result);
    Motion_Range* vim_process_motion(Motion_Result *motion_result);
    void indent_block(int y1, int y2, int indents);
    void vim_transform_text(uchar command, cur2 a, cur2 b);
    void vim_dotrepeat_commit();
    Vim_Macro *vim_get_macro(char macro);
    void vim_execute_macro_little_bit(u64 deadline);

    void handle_type_enter();
    void handle_type_backspace(int mods);
};

void vim_copy_command(Vim_Command *dest, Vim_Command *src);

Parse_Lang determine_lang(ccstr filepath);

struct Pane {
    List<Editor> editors;
    u32 current_editor;
    double width;
    float tabs_offset;

    bool scrollbar_dragging;
    float scrollbar_drag_offset;
    float scrollbar_drag_start;

    void init();
    void cleanup();
    Editor* focus_editor(ccstr path);
    Editor* focus_editor_by_index(u32 index);
    Editor* focus_editor(ccstr path, cur2 pos, bool pos_in_byte_format = false);
    Editor* focus_editor_by_index(u32 index, cur2 pos, bool pos_in_byte_format = false);
    Editor* get_current_editor();
    Editor* open_empty_editor();
    void set_current_editor(u32 idx);
};

#define MAX_PANES 4

bool check_file_dimensions(ccstr path);

bool is_goident_empty(ccstr name);
