#pragma once

#include <git2.h>
#include "list.hpp"
#include "buffer.hpp"
#include "go.hpp"
#include "os.hpp"
#include "tree_sitter_crap.hpp"

const int AUTOCOMPLETE_WINDOW_ITEMS = 10;

#define MAX_BREAKPOINTS 128

struct Client_Parameter_Hint {
    Gotype *gotype; // save this here in case we need it later
    ccstr help_text;
    cur2 start;
    // u32 current_param;
    bool closed;
};

enum Vi_Mode {
    VI_NONE,
    VI_NORMAL,
    VI_VISUAL,
    VI_INSERT,
    VI_REPLACE,
    VI_UNKNOWN,
};

enum Editor_Msg_Type {
    EMSG_NVIM_EDIT,
    EMSG_RELOAD,
};

struct Editor_Msg {
    Editor_Msg_Type type;

    struct {
        u32 firstline;
        u32 lastline;
        List<uchar*> *lines;
        List<s32> *line_lengths;
    } nvim_edit;
};

struct Pane;

struct Editor {
    u32 id;
    Pool mem;
    Pane* pane;

    Buffer buf;
    Lock buf_lock;

    box view;
    cur2 cur;

    char filepath[MAX_PATH];
    bool is_untitled;

    TSParser *parser;
    TSTree *tree;
    TSTreeCursor cursor;
    char tsinput_buffer[128];
    TSInputEdit curr_change;

    Pool msg_mem;
    Lock msg_lock;
    List<Editor_Msg> msg_queue;

    // is this file "dirty" from the perspective of the index?
    bool index_dirty;
    bool is_go_file;

    struct {
        bool is_buf_attached;
        u32 buf_id;
        u32 win_id;

        bool got_initial_cur;
        bool got_initial_lines;

        Vi_Mode mode;
        bool is_resizing;
        bool need_initial_resize;
        bool need_initial_pos_set;
        cur2 initial_pos;
    } nvim_data;

    struct {
        cur2 start;
        cur2 backspaced_to;
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

    void process_msg(Editor_Msg *msg);
    void update_tree();
    void raw_move_cursor(cur2 c);
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

    void trigger_autocomplete(bool triggered_by_dot);
    void filter_autocomplete_results(Autocomplete* ac);
    void trigger_parameter_hint(bool triggered_by_paren);

    void type_char(char ch);
    void type_char_in_insert_mode(char ch);
    void update_autocomplete();
    void update_parameter_hint();

    void start_change();
    void end_change();

    void apply_edits(List<TSInputEdit> *edits);
    void reload_file();
};

struct Pane {
    List<Editor> editors;
    u32 current_editor;
    float width;

    void init();
    void cleanup();
    Editor* focus_editor(ccstr path);
    Editor* focus_editor_by_index(u32 index);
    Editor* focus_editor(ccstr path, cur2 pos);
    Editor* focus_editor_by_index(u32 index, cur2 pos);
    Editor* get_current_editor();
    Editor* open_empty_editor();
};

#define MAX_PANES 4

struct Workspace {
    char path[MAX_PATH];

    Pane _panes[MAX_PANES];
    List<Pane> panes;
    u32 current_pane;

    i32 resizing_pane; // if this value is i, we're resizing the border between i and i+1

    git_repository *git_repo;

    void init();
    void activate_pane(u32 idx);
    Pane* get_current_pane();
};

bool check_file_dimensions(ccstr path);
