#pragma once

#include "list.hpp"
#include "buffer.hpp"
#include "go.hpp"
#include "os.hpp"
#include "tree_sitter_crap.hpp"

#define NVIM_DEFAULT_WIDTH 200
#define NVIM_DEFAULT_HEIGHT 500

const int AUTOCOMPLETE_WINDOW_ITEMS = 10;

#define MAX_BREAKPOINTS 128

enum {
    GH_FMT_GOFMT = 0,
    GH_FMT_GOIMPORTS = 1,
    GH_FMT_GOIMPORTS_WITH_AUTOIMPORT = 2,
};

enum {
    HINT_NAME,
    HINT_TYPE,
    HINT_NORMAL,

    HINT_CURRENT_PARAM,
    HINT_NOT_CURRENT_PARAM,
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

struct Editor {
    u32 id;
    Pool mem;
    Pane* pane;

    Buffer buf;

    box view;
    cur2 cur;

    char filepath[MAX_PATH];
    bool is_untitled;

    // TSInputEdit curr_change;

    // is this file "dirty" from the perspective of the index?
    bool is_go_file;
    u64 disable_file_watcher_until;

    bool saving;
    Process goimports_proc;
    char highlights[NVIM_DEFAULT_HEIGHT][NVIM_DEFAULT_WIDTH];

    cur2 go_here_after_escape;

    List<Postfix_Info> postfix_stack;

    // need to reset this when backspacing past it and when accepting an auto
    cur2 last_closed_autocomplete;

    struct {
        bool is_buf_attached;
        u32 buf_id;
        u32 win_id;

        bool got_initial_cur;
        bool got_initial_lines;
        bool need_initial_pos_set;
        cur2 initial_pos;

        bool waiting_for_move_cursor;
        cur2 move_cursor_to;
        int grid_topline;

        cur2 post_insert_original_cur;

        bool is_navigating;
        cur2 navigating_to_pos;
        int navigating_to_editor;

        int changedtick;
    } nvim_data;

    struct Insert_Change {
        cur2 start; // line
        cur2 end;
        List<Line> lines;
    };

    struct {
        Pool mem;

        // current change
        cur2 start;
        cur2 old_end;
        u32 deleted_graphemes;

        List<Insert_Change> other_changes;
        u32 skip_changedticks_until;
    } nvim_insert;

    void init();
    void cleanup();

    bool is_nvim_ready();

    struct {
        Autocomplete ac;
        List<int>* filtered_results;
        u32 selection;
        u32 view;
    } autocomplete;

    Client_Parameter_Hint parameter_hint;

    void update_tree();
    void raw_move_cursor(cur2 c, bool dont_add_to_history = false);
    void move_cursor(cur2 c);
    void reset_state();
    bool load_file(ccstr new_filepath);
    bool save_file();
    i32 cur_to_offset(cur2 c);
    i32 cur_to_offset();
    cur2 offset_to_cur(i32 offset);
    int get_indent_of_line(int y);
    Buffer_It iter();
    Buffer_It iter(cur2 _cur);
    void perform_autocomplete(AC_Result *result);

    void trigger_autocomplete(bool triggered_by_dot, bool triggered_by_typing_ident, char typed_ident_char = 0);
    void filter_autocomplete_results(Autocomplete* ac);
    void trigger_parameter_hint();

    void type_char(char ch);
    void type_char_in_insert_mode(char ch);
    void update_autocomplete(bool triggered_by_ident);
    void update_parameter_hint();

    void start_change();
    void end_change();

    void apply_edits(List<TSInputEdit> *edits);
    void reload_file(bool because_of_file_watcher = false);
    void update_lines(int firstline, int lastline, List<uchar*> *lines, List<s32> *line_lengths);
    bool trigger_escape(cur2 go_here_after = {-1, -1});
    void format_on_save(int fmt_type, bool write_to_nvim = true);
    void handle_save(bool about_to_close = false);
    bool is_current_editor();
    void backspace_in_insert_mode(int graphemes_to_erase, int codepoints_to_erase);
    void ensure_cursor_on_screen();
    void insert_text_in_insert_mode(ccstr s);
    ccstr get_autoindent(int for_y);
    void add_change_in_insert_mode(cur2 start, cur2 old_end, cur2 new_end);
    bool cur_is_inside_comment_or_string();
};

struct Pane {
    List<Editor> editors;
    u32 current_editor;
    double width;
    float tabs_offset;

    void init();
    void cleanup();
    Editor* focus_editor(ccstr path);
    Editor* focus_editor_by_index(u32 index);
    Editor* focus_editor(ccstr path, cur2 pos);
    Editor* focus_editor_by_index(u32 index, cur2 pos);
    Editor* get_current_editor();
    Editor* open_empty_editor();
    void set_current_editor(u32 idx);
};

#define MAX_PANES 4

bool check_file_dimensions(ccstr path);

struct Type_Renderer : public Text_Renderer {
    void write_type(Gotype *t, bool parameter_hint_root = false);
};

bool is_goident_empty(ccstr name);
