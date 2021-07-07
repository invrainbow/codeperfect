#pragma once

#include <math.h>
#include "stb_truetype.h"
#include "stb_image.h"
#include "stb_rect_pack.h"
#include "imgui.h"
#include "common.hpp"
#include "editor.hpp"
#include "list.hpp"
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

    bool init(u8* font_data, u32 font_size, int texture_id) {
        height = font_size;
        tex_size = (i32)pow(2.0f, (i32)log2(sqrt((float)height * height * 128)) + 1);

        u8* atlas_data = (u8*)our_malloc(tex_size * tex_size);
        if (atlas_data == NULL)
            return false;
        defer { our_free(atlas_data); };

        stbtt_pack_context context;
        if (!stbtt_PackBegin(&context, atlas_data, tex_size, tex_size, 0, 1, NULL)) {
            error("stbtt_PackBegin failed");
            return false;
        }

        // TODO: what does this do?
        stbtt_PackSetOversampling(&context, 2, 2);

        if (!stbtt_PackFontRange(&context, font_data, 0, (float)height, ' ', '~' - ' ' + 1, char_info)) {
            error("stbtt_PackFontRange failed");
            return false;
        }

        if (!stbtt_PackFontRange(&context, font_data, 0, (float)height, 0xfffd, 1, &char_info[_countof(char_info) - 1])) {
            error("stbtt_PackFontRange failed");
            return false;
        }

        stbtt_PackEnd(&context);

        glActiveTexture(GL_TEXTURE0 + texture_id);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tex_size, tex_size, 0, GL_RED, GL_UNSIGNED_BYTE, atlas_data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbtt_fontinfo font;
        i32 unscaled_advance, junk;
        float scale;

        stbtt_InitFont(&font, font_data, 0);
        stbtt_GetCodepointHMetrics(&font, 'A', &unscaled_advance, &junk);
        stbtt_GetFontVMetrics(&font, &offset_y, NULL, NULL);

        scale = stbtt_ScaleForPixelHeight(&font, (float)height);
        width = unscaled_advance * scale;
        offset_y = (int)((float)offset_y * scale);

        return true;
    }
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
    OUR_MOD_NONE = 0,
    OUR_MOD_CMD = 1 << 0,
    OUR_MOD_SHIFT = 1 << 1,
    OUR_MOD_ALT = 1 << 2,
    OUR_MOD_CTRL = 1 << 3,
};

#if OS_MAC
#define OUR_MOD_PRIMARY OUR_MOD_CMD
#else
#define OUR_MOD_PRIMARY OUR_MOD_CTRL
#endif

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

    struct {
        int id;
        int id_last_frame;
        u64 start_time;
        ImGuiMouseCursor cursor;
        bool ready;
    } hover;

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
    void render_ts_cursor(TSTreeCursor *curr);
    void render_godecl(Godecl *decl);
    void render_gotype(Gotype *gotype, ccstr field = NULL);
    bool test_hover(boxf area, int id, ImGuiMouseCursor cursor = ImGuiMouseCursor_Arrow);
    void open_project_settings();

    void imgui_small_newline();
    bool imgui_input_text_full(ccstr label, char *buf, int count, int flags = 0);
    bool imgui_input_special_key_pressed(int key);
    bool imgui_input_key_pressed(int key);
    void imgui_with_disabled(bool disable, fn<void()> f);
    bool imgui_is_window_focusing(bool *b);
    u32 imgui_get_keymods();
};

extern UI ui;

vec3f rgb_hex(ccstr s);
vec4f rgba(vec3f color, float alpha = 1.0);
vec4f rgba(ccstr hex, float alpha = 1.0);

extern const vec3f COLOR_WHITE;
extern const vec3f COLOR_DARK_RED;
extern const vec3f COLOR_DARK_YELLOW;
extern const vec3f COLOR_BLACK;
extern const vec3f COLOR_LIGHT_GREY;
extern const vec3f COLOR_DARK_GREY;
extern const vec3f COLOR_MEDIUM_DARK_GREY;
extern const vec3f COLOR_MEDIUM_GREY;
extern const vec3f COLOR_LIME;
extern const vec3f COLOR_BG;

extern const vec3f COLOR_THEME_1;
extern const vec3f COLOR_THEME_2;
extern const vec3f COLOR_THEME_3;
extern const vec3f COLOR_THEME_4;
extern const vec3f COLOR_THEME_5;

enum {
    HOVERID_PANE_RESIZERS = 1000,
    HOVERID_TABS = 2000,
};
