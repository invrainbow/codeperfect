#pragma once

#include <math.h>
#include "stb_truetype.h"
#include "common.hpp"
#include "editor.hpp"
#include "list.hpp"
#include <GL/glew.h>

struct Font {
    stbtt_packedchar char_info['~' - ' ' + 1];

    GLuint texid;
    i32 tex_size;
    i32 offset_y;
    i32 height;
    float width;

    bool init(u8* font_data, u32 font_size) {
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

        stbtt_PackEnd(&context);

        glGenTextures(1, &texid);
        glBindTexture(GL_TEXTURE_2D, texid);
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
    i32 solid;
};

struct Im_Vert {
    float x;
    float y;
    float u;
    float v;
    vec3f color;
    i32 solid;
};

u32 advance_subtree_in_file_explorer(u32 i);

const auto EDITOR_MARGIN_X = 5.0;
const auto EDITOR_MARGIN_Y = 5.0;

#define ROUND_TL (1 << 0)
#define ROUND_BL (1 << 1)
#define ROUND_TR (1 << 2)
#define ROUND_BR (1 << 3)
#define ROUND_ALL (ROUND_TL | ROUND_BL | ROUND_TR | ROUND_BR)

struct UI {
    Font* font;
    List<Vert> verts;

    vec2f _editor_sizes[MAX_PANES];
    List<vec2f> editor_sizes;

    void init();
    void flush_verts();
    void draw_triangle(vec2f a, vec2f b, vec2f c, vec2f uva, vec2f uvb, vec2f uvc, vec4f color, bool solid);
    void draw_quad(boxf b, boxf uv, vec4f color, bool solid);
    void draw_rect(boxf b, vec4f color);
    void draw_rounded_rect(boxf b, vec4f color, float radius, int round_flags);
    void draw_bordered_rect_outer(boxf b, vec4f color, vec4f border_color, int border_width);
    void draw_char(vec2f* pos, char ch, vec4f color);
    vec2f draw_string(vec2f pos, ccstr s, vec4f color);
    float get_text_width(ccstr s);
    boxf get_sidebar_area();
    boxf get_build_results_area();
    boxf get_panes_area();
    void draw_everything(GLuint vao, GLuint vbo, GLuint program);
    void get_tabs_and_editor_area(boxf* pane_area, boxf* ptabs_area, boxf* peditor_area);
    void recalculate_view_sizes(bool force = false);
    i32 get_current_resize_area(boxf* out);
    bool was_area_clicked(boxf area);
    List<vec2f> *get_pane_editor_sizes();
};

extern UI ui;

vec3f rgb_hex(ccstr s);
vec4f rgba(vec3f color, float alpha = 1.0);

extern const vec3f COLOR_WHITE;
extern const vec3f COLOR_DARK_RED;
extern const vec3f COLOR_DARK_YELLOW;
extern const vec3f COLOR_BLACK;
extern const vec3f COLOR_LIGHT_GREY;
extern const vec3f COLOR_DARK_GREY;
extern const vec3f COLOR_MEDIUM_DARK_GREY;
extern const vec3f COLOR_MEDIUM_GREY;
extern const vec3f COLOR_LIME;

extern const vec3f COLOR_THEME_1;
extern const vec3f COLOR_THEME_2;
extern const vec3f COLOR_THEME_3;
extern const vec3f COLOR_THEME_4;
extern const vec3f COLOR_THEME_5;
