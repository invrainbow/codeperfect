#pragma once

#include "list.hpp"
#include "buffer.hpp"
#include "go.hpp"
#include "os.hpp"

const int AUTOCOMPLETE_WINDOW_ITEMS = 10;

#define MAX_BREAKPOINTS 128

struct Client_Parameter_Hint {
  List<ccstr>* params;
  cur2 start;
  u32 current_param;
};

enum EditorNvimStatus {
  ENS_BUF_PENDING,
  ENS_WIN_PENDING,
  ENS_SET_LINES_PENDING,
};

enum ViMode {
  VIMODE_NONE,
  VIMODE_NORMAL,
  VIMODE_VISUAL,
  VIMODE_INSERT,
  VIMODE_REPLACE,
};

enum HlType {
  HL_NONE,
  HL_COMMENT,
  HL_STATEMENT,
  HL_TYPE,
  HL_CONSTANT,
  HL_VISUAL,
};

ccstr hl_type_str(HlType type);

struct Hl_Token {
  HlType type : 8;
  // u32 col;
  // u32 len;
};

struct Pane;

struct Editor {
  u32 id;
  Buffer buf;
  box view;
  cur2 cur;
  Pane* pane;
  char filepath[MAX_PATH];
  bool is_untitled;
  Arena arena;

  Lock highlights_lock;
  Hl_Token* highlights_buf;
  List<List<Hl_Token>>* highlights;

  struct {
    bool is_buf_attached;
    u32 buf_id;
    FILE* file_handle;
    u32 win_id;

    ViMode vimode;
    bool is_resizing;
    bool need_initial_resize;
    bool need_initial_pos_set;
    cur2 initial_pos;
  } nvim_data;

  void init();
  void cleanup();

  bool is_nvim_ready();

  // u32 _breakpoints[MAX_BREAKPOINTS];
  // List<u32> breakpoints;

  struct {
    Autocomplete ac;
    List<int>* filtered_results;
    char prefix[256];
    u32 selection;
    u32 view;
  } autocomplete;

  Client_Parameter_Hint parameter_hint;

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
  // void process_key_after_nvim_finishes(u32 key);
  void on_type();

  void trigger_autocomplete(bool triggered_by_dot);
  void filter_autocomplete_results(Autocomplete* ac);
  Ast* parse_autocomplete_id(Autocomplete* ac);
  void trigger_parameter_hint(bool triggered_by_paren);
};

struct Pane {
  List<Editor> editors;
  u32 current_editor;
  float width;

  void init();
  void cleanup();
  Editor* focus_editor(ccstr path);
  Editor* focus_editor_by_index(u32 index);
  Editor* get_current_editor();
  Editor* open_empty_editor();
};

struct Workspace {
  char path[MAX_PATH];
  char go_import_path[MAX_PATH];

  Pane _panes[4];
  List<Pane> panes;
  u32 current_pane;

  i32 resizing_pane; // if this value is i, we're resizing the border between i and i+1

  bool gomod_exists;
  Gomod_Info gomod_info;

  void init();
  bool parse_gomod_file(ccstr path);
  void activate_pane(u32 idx);
  Pane* get_current_pane();
};
