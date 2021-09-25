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
#define KEYMOD_PRIMARY KEYMOD_CMD
#else
#define KEYMOD_PRIMARY KEYMOD_CTRL
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




};

extern UI ui;

vec3f rgb_hex(ccstr s);
vec4f rgba(vec3f color, float alpha = 1.0);
vec4f rgba(ccstr hex, float alpha = 1.0);

extern const vec3f COLOR_JBLOW_BG;
extern const vec3f COLOR_JBLOW_FG;
extern const vec3f COLOR_JBLOW_GREEN;
extern const vec3f COLOR_JBLOW_BLUE_STRING;
extern const vec3f COLOR_JBLOW_BLUE_NUMBER;
extern const vec3f COLOR_JBLOW_CYAN;
extern const vec3f COLOR_JBLOW_WHITE;
extern const vec3f COLOR_JBLOW_YELLOW;
extern const vec3f COLOR_JBLOW_SEARCH;
extern const vec3f COLOR_JBLOW_VISUAL;

extern const vec3f COLOR_WHITE;
extern const vec3f COLOR_DARK_RED;
extern const vec3f COLOR_DARK_YELLOW;
extern const vec3f COLOR_BLACK;
extern const vec3f COLOR_LIGHT_GREY;
extern const vec3f COLOR_DARK_GREY;
extern const vec3f COLOR_MEDIUM_DARK_GREY;
extern const vec3f COLOR_MEDIUM_GREY;
extern const vec3f COLOR_LIME;

enum {
    HOVERID_PANE_RESIZERS = 1000,
    HOVERID_TABS = 2000,
};

#define AUTOCOMPLETE_TRUNCATE_LENGTH 40
