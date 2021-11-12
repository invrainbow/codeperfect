#pragma once

#include <math.h>
#include "stb_truetype.h"
#include "stb_image.h"
#include "stb_rect_pack.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "common.hpp"
#include "editor.hpp"
#include "list.hpp"
#include "debugger.hpp"
#include <GL/glew.h>

enum Texture_Id {
    TEXTURE_FONT,
    TEXTURE_FONT_IMGUI,
    TEXTURE_IMAGES,
    __TEXTURE_COUNT__,
};

struct Font {
    stbtt_packedchar char_info['~' - ' ' + 1 + 1];

    i32 tex_size;
    i32 offset_y;
    i32 height;
    float width;

    bool init(u8* font_data, u32 font_size, int texture_id);
};

struct Vert {
    float x;
    float y;
    float u;
    float v;
    vec4f color;
    i32 mode;
    i32 texture_id;
};

#define ROUND_TL (1 << 0)
#define ROUND_BL (1 << 1)
#define ROUND_TR (1 << 2)
#define ROUND_BR (1 << 3)
#define ROUND_ALL (ROUND_TL | ROUND_BL | ROUND_TR | ROUND_BR)

// these names are shit lol
enum Draw_Mode {
    DRAW_SOLID = 0,         // draw solid color
    DRAW_FONT_MASK = 1,     // draw color using texture(...).r as mask
    DRAW_IMAGE = 2,         // draw a texture directly
    DRAW_IMAGE_MASK = 3,    // draw color using texture(...).a as mask
};

enum Sprites_Image_Type {
    SIMAGE_ADD,
    SIMAGE_FOLDER,
    SIMAGE_ADD_FOLDER,
    SIMAGE_REFRESH,
    SIMAGE_SOURCE_FILE,
};

struct Image_To_Draw {
    Sprites_Image_Type image_id;
    boxf box;
    boxf uv;
};

enum {
    MOUSE_NOTHING = 0,
    MOUSE_HOVER = 1 << 0,
    MOUSE_CLICKED = 1 << 1,
    MOUSE_MCLICKED = 1 << 2,
    MOUSE_RCLICKED = 1 << 3,
    // support double click somehow?
};

enum {
    KEYMOD_NONE = 0,
    KEYMOD_CMD = 1 << 0,
    KEYMOD_SHIFT = 1 << 1,
    KEYMOD_ALT = 1 << 2,
    KEYMOD_CTRL = 1 << 3,
};

#if OS_MAC
#   define KEYMOD_PRIMARY KEYMOD_CMD
#   define KEYMOD_TEXT KEYMOD_ALT
#else
#   define KEYMOD_PRIMARY KEYMOD_CTRL
#   define KEYMOD_TEXT KEYMOD_CTRL
#endif

enum Dbg_Index_Type {
    INDEX_NONE,
    INDEX_ARRAY,
    INDEX_MAP,
};

struct Draw_Debugger_Var_Args {
    Dlv_Var *var;
    Dbg_Index_Type index_type;
    union {
        int index;
        Dlv_Var *key;
    };
    Dlv_Watch *watch;
    bool some_watch_being_edited;
    bool is_child;
    int indent;
    int watch_index;
    int locals_index;
};

enum Focus_Keyboard_Cond {
    FKC_APPEARING = 1 << 0,
    FKC_FOCUSING = 1 << 1,
};

struct Wnd {
    bool show;
    bool focused;
    bool first_open_focus_twice_done;

    // "commands"
    bool cmd_focus;
};

struct UI {
    Font* font;
    List<Vert> verts;

    vec2f _editor_sizes[MAX_PANES];
    List<vec2f> editor_sizes;

    List<stbrp_rect> sprite_rects;
    float sprite_tex_size;

    bool clipping;
    boxf current_clip;
    boxf panes_area;

    ImGuiID dock_main_id;
    ImGuiID dock_sidebar_id;
    ImGuiID dock_bottom_id;
    ImGuiID dock_bottom_right_id;
    bool dock_initialized;

    // for drawing
    vec2f actual_cursor_positions[16];
    vec2f actual_parameter_hint_start;

    struct {
        int id;
        int id_last_frame;
        u64 start_time;
        ImGuiMouseCursor cursor;
        bool ready;
    } hover;

    bool dbg_editing_new_watch;

    void draw_debugger();
    ccstr var_value_as_string(Dlv_Var *var);
    void draw_debugger_var(Draw_Debugger_Var_Args *args);

    void init();
    void flush_verts();
    void draw_triangle(vec2f a, vec2f b, vec2f c, vec2f uva, vec2f uvb, vec2f uvc, vec4f color, Draw_Mode mode, Texture_Id texture = TEXTURE_FONT);
    void draw_quad(boxf b, boxf uv, vec4f color, Draw_Mode mode, Texture_Id texture = TEXTURE_FONT);
    void draw_rect(boxf b, vec4f color);
    void draw_rounded_rect(boxf b, vec4f color, float radius, int round_flags);
    void draw_bordered_rect_outer(boxf b, vec4f color, vec4f border_color, int border_width, float radius = 0);
    void draw_char(vec2f* pos, uchar ch, vec4f color);
    vec2f draw_string(vec2f pos, ccstr s, vec4f color);
    float get_text_width(ccstr s);
    boxf get_build_results_area();
    boxf get_panes_area();
    boxf get_status_area();
    void draw_everything();
    void end_frame();
    void get_tabs_and_editor_area(boxf* pane_area, boxf* ptabs_area, boxf* peditor_area, bool has_tabs);
    void recalculate_view_sizes(bool force = false);
    i32 get_current_resize_area(boxf* out);
    int get_mouse_flags(boxf area);
    List<vec2f> *get_pane_editor_sizes();
    void draw_image(Sprites_Image_Type image_id, boxf b);
    // void draw_image_masked(Sprites_Image_Type image_id, boxf b, vec4f color);
    void flush_images();
    void start_clip(boxf b);
    void end_clip();
    void render_ts_cursor(TSTreeCursor *curr, cur2 open_cur);
    void render_godecl(Godecl *decl);
    void render_gotype(Gotype *gotype, ccstr field = NULL);
    bool test_hover(boxf area, int id, ImGuiMouseCursor cursor = ImGuiMouseCursor_Arrow);
    void open_project_settings();

    void imgui_small_newline();
    bool imgui_input_text_full(ccstr label, char *buf, int count, int flags = 0);
    bool imgui_special_key_pressed(int key);
    bool imgui_key_pressed(int key);
    void imgui_with_disabled(bool disable, fn<void()> f);
    bool imgui_is_window_focusing(bool *b);
    u32 imgui_get_keymods();

    void imgui_push_mono_font();
    void imgui_push_ui_font();
    void imgui_pop_font();

    void focus_keyboard(Wnd *wnd, int cond = FKC_APPEARING | FKC_FOCUSING);

    void help_marker(ccstr text);
};

extern UI ui;

vec3f rgb_hex(ccstr s);
vec4f rgba(vec3f color, float alpha = 1.0);
vec4f rgba(ccstr hex, float alpha = 1.0);

enum {
    HOVERID_PANE_RESIZERS = 1000,
    HOVERID_TABS = 2000,
};

#define AUTOCOMPLETE_TRUNCATE_LENGTH 40

struct Global_Colors {
    vec3f autocomplete_background;
    vec3f autocomplete_border;
    vec3f autocomplete_selection;
    vec3f background;
    vec3f breakpoint_active;
    vec3f breakpoint_current;
    vec3f breakpoint_inactive;
    vec3f breakpoint_other;
    vec3f builtin;
    vec3f comment;
    vec3f cursor;
    vec3f cursor_foreground;
    vec3f foreground;
    vec3f green;
    vec3f keyword;
    vec3f muted;
    vec3f number_literal;
    vec3f pane_active;
    vec3f pane_inactive;
    vec3f pane_resizer;
    vec3f pane_resizer_hover;
    vec3f punctuation;
    vec3f search_background;
    vec3f search_foreground;
    vec3f string_literal;
    vec3f tab;
    vec3f tab_hovered;
    vec3f tab_selected;
    vec3f type;
    vec3f visual_background;
    vec3f visual_foreground;
    vec3f visual_highlight;
    vec3f white;
    vec3f white_muted;

    // added in round 2
    vec3f status_area_background;
    vec3f command_background;
    vec3f command_foreground;
    vec3f status_mode_background;
    vec3f status_mode_foreground;
    vec3f status_debugger_paused_background;
    vec3f status_debugger_starting_background;
    vec3f status_debugger_running_background;
    vec3f status_index_ready_background;
    vec3f status_index_ready_foreground;
    vec3f status_index_indexing_background;
    vec3f status_index_indexing_foreground;
};

extern Global_Colors global_colors;

void init_global_colors();
ccstr format_key(int mods, ccstr key);
