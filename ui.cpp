#include "ui.hpp"
#include "common.hpp"
#include "world.hpp"
#include "go.hpp"
#include "settings.hpp"

#define _USE_MATH_DEFINES // what the fuck is this lol
#include <math.h>
#include "tree_sitter_crap.hpp"

UI ui;

ccstr image_filenames[] = {
    "images/add.png",
    "images/folder.png",
    "images/add-folder.png",
    "images/refresh.png",
    "images/source-file.png",
};

// true means "should use DRAW_IMAGE_MASK"
bool image_mask_types[] = {
    true,
    false,
    true,
    true,
    true,
};

bool UI::init_sprite_texture() {
    sprite_tex_size = 1024;

    glActiveTexture(GL_TEXTURE0 + TEXTURE_IMAGES);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    {
        SCOPED_FRAME();

        stbrp_context context;
        s32 node_count = 4096 * 2;
        auto nodes = alloc_array(stbrp_node, node_count);

        stbrp_init_target(&context, 4096, 4096, nodes, node_count);
        if (!stbrp_pack_rects(&context, sprite_rects.items, sprite_rects.len))
            return false;

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sprite_tex_size, sprite_tex_size, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

        u32 i = 0;
        For (sprite_rects) {
            int w = 0, h = 0, chans = 0;
            auto data = stbi_load(image_filenames[i], &w, &h, &chans, STBI_rgb_alpha);
            assert(chans == 4);

            if (data == NULL) return false;
            defer { stbi_image_free(data); };

            assert(it.w == w);
            assert(it.h == h);

            glTexSubImage2D(GL_TEXTURE_2D, 0, it.x, it.y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, data);
            i++;
        }
    }
}

#define LINE_HEIGHT 1.2

vec3f rgb_hex(ccstr s) {
    if (s[0] == '#') s++;

    char r[] = { s[0], s[1], '\0' };
    char g[] = { s[2], s[3], '\0' };
    char b[] = { s[4], s[5], '\0' };

    return {
        strtol(r, NULL, 16) / 255.0f,
        strtol(g, NULL, 16) / 255.0f,
        strtol(b, NULL, 16) / 255.0f,
    };
}

vec4f rgba(vec3f color, float alpha) {
    vec4f ret;
    ret.rgb = color;
    ret.a = alpha;
    return ret;
}

vec4f rgba(ccstr hex, float alpha) {
    return rgba(rgb_hex(hex), alpha);
}

const vec3f COLOR_WHITE = rgb_hex("#ffffff");
const vec3f COLOR_RED = rgb_hex("#ff8888");
const vec3f COLOR_LIGHT_BLUE = rgb_hex("#6699dd");
const vec3f COLOR_DARK_RED = rgb_hex("#880000");
const vec3f COLOR_DARK_YELLOW = rgb_hex("#6b6d0a");
const vec3f COLOR_BLACK = rgb_hex("#000000");
const vec3f COLOR_BG = rgb_hex("#181818");
const vec3f COLOR_LIGHT_GREY = rgb_hex("#eeeeee");
const vec3f COLOR_DARK_GREY = rgb_hex("#333333");
const vec3f COLOR_MEDIUM_DARK_GREY = rgb_hex("#585858");
const vec3f COLOR_MEDIUM_GREY = rgb_hex("#888888");
const vec3f COLOR_LIME = rgb_hex("#22ff22");
const vec3f COLOR_GREEN = rgb_hex("#88dd88");
const vec3f COLOR_THEME_1 = rgb_hex("fd3f5c");
const vec3f COLOR_THEME_2 = rgb_hex("fbd19b");
const vec3f COLOR_THEME_3 = rgb_hex("edb891");
const vec3f COLOR_THEME_4 = rgb_hex("eca895");
const vec3f COLOR_THEME_5 = rgb_hex("e8918c");

bool get_type_color(Ast_Node *node, Editor *editor, vec3f *out) {
    switch (node->type) {
    case TS_PACKAGE:
    case TS_IMPORT:
    case TS_CONST:
    case TS_VAR:
    case TS_FUNC:
    case TS_TYPE:
    case TS_STRUCT:
    case TS_INTERFACE:
    case TS_MAP:
    case TS_CHAN:
    case TS_FALLTHROUGH:
    case TS_BREAK:
    case TS_CONTINUE:
    case TS_GOTO:
    case TS_RETURN:
    case TS_GO:
    case TS_DEFER:
    case TS_IF:
    case TS_ELSE:
    case TS_FOR:
    case TS_RANGE:
    case TS_SWITCH:
    case TS_CASE:
    case TS_DEFAULT:
    case TS_SELECT:
    case TS_NEW:
    case TS_MAKE:
        *out = COLOR_THEME_1;
        return true;

    case TS_PLUS:
    case TS_DASH:
    case TS_BANG:
    case TS_SEMI:
    case TS_DOT:
    case TS_ANON_DOT:
    case TS_LPAREN:
    case TS_RPAREN:
    case TS_COMMA:
    case TS_EQ:
    case TS_DOT_DOT_DOT:
    case TS_STAR:
    case TS_LBRACK:
    case TS_RBRACK:
    case TS_LBRACE:
    case TS_RBRACE:
    case TS_CARET:
    case TS_AMP:
    case TS_SLASH:
    case TS_PERCENT:
    case TS_LT_DASH:
    case TS_COLON_EQ:
    case TS_PLUS_PLUS:
    case TS_DASH_DASH:
    case TS_STAR_EQ:
    case TS_SLASH_EQ:
    case TS_PERCENT_EQ:
    case TS_LT_LT_EQ:
    case TS_GT_GT_EQ:
    case TS_AMP_EQ:
    case TS_AMP_CARET_EQ:
    case TS_PLUS_EQ:
    case TS_DASH_EQ:
    case TS_PIPE_EQ:
    case TS_CARET_EQ:
    case TS_COLON:
    case TS_LT_LT:
    case TS_GT_GT:
    case TS_AMP_CARET:
    case TS_PIPE:
    case TS_EQ_EQ:
    case TS_BANG_EQ:
    case TS_LT:
    case TS_LT_EQ:
    case TS_GT:
    case TS_GT_EQ:
    case TS_AMP_AMP:
    case TS_PIPE_PIPE:
        *out = COLOR_MEDIUM_GREY;
        return true;

    case TS_INT_LITERAL:
    case TS_FLOAT_LITERAL:
    case TS_IMAGINARY_LITERAL:
    case TS_RUNE_LITERAL:
    case TS_NIL:
    case TS_TRUE:
    case TS_FALSE:
        *out = COLOR_THEME_2;
        return true;

    case TS_COMMENT:
        *out = COLOR_THEME_3;
        return true;

    case TS_INTERPRETED_STRING_LITERAL:
    case TS_RAW_STRING_LITERAL:
        *out = COLOR_THEME_4;
        return true;

    case TS_IDENTIFIER:
    case TS_FIELD_IDENTIFIER:
    case TS_PACKAGE_IDENTIFIER:
    case TS_TYPE_IDENTIFIER:
        {
            auto len = node->end_byte - node->start_byte;
            if (len >= 16) break;

            ccstr keywords1[] = {
                "package", "import", "const", "var", "func",
                "type", "struct", "interface", "map", "chan",
                "fallthrough", "break", "continue", "goto", "return",
                "go", "defer", "if", "else",
                "for", "range", "switch", "case",
                "default", "select", "new", "make",
            };

            ccstr keywords2[] = {
                "int", "append", "len", "string", "rune", "bool",
                "byte", "copy", "float32", "float64", "error",
            };

            char keyword[16] = {0};
            auto it = editor->iter(node->start);
            for (u32 i = 0; it.pos != node->end; i++)
                keyword[i] = it.next();

            For (keywords1) {
                if (streq(it, keyword)) {
                    *out = COLOR_THEME_1;
                    return true;
                }
            }

            For (keywords2) {
                if (streq(it, keyword)) {
                    *out = COLOR_THEME_5;
                    return true;
                }
            }
        }
        break;
    }

    return false;
}

void UI::init() {
    ptr0(this);
    font = &world.font;
    editor_sizes.init(LIST_FIXED, _countof(_editor_sizes), _editor_sizes);

    sprite_rects.init();

    For (image_filenames) {
        int w = 0, h = 0, comp = 0;
        auto data = stbi_load(it, &w, &h, &comp, STBI_rgb_alpha);
        assert(data != NULL);
        defer { stbi_image_free(data); };

        auto rect = sprite_rects.append();
        rect->w = w;
        rect->h = h;
    }
}

void UI::flush_verts() {
    if (verts.len == 0) return;

    glBufferData(GL_ARRAY_BUFFER, sizeof(Vert) * verts.len, verts.items, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, verts.len);
    verts.len = 0;
}

void UI::start_clip(boxf b) {
    flush_verts();
    glEnable(GL_SCISSOR_TEST);
    glScissor(b.x, world.display_size.y - (b.y + b.h), b.w, b.h);
    clipping = true;
    current_clip = b;
}

void UI::end_clip() {
    flush_verts();
    glDisable(GL_SCISSOR_TEST);
    clipping = false;
}

void UI::draw_triangle(vec2f a, vec2f b, vec2f c, vec2f uva, vec2f uvb, vec2f uvc, vec4f color, Draw_Mode mode, Texture_Id texture) {
    if (verts.len + 3 >= verts.cap)
        flush_verts();

    a.x *= world.display_scale.x;
    a.y *= world.display_scale.y;
    b.x *= world.display_scale.x;
    b.y *= world.display_scale.y;
    c.x *= world.display_scale.x;
    c.y *= world.display_scale.y;

    verts.append({ a.x, a.y, uva.x, uva.y, color, mode, texture });
    verts.append({ b.x, b.y, uvb.x, uvb.y, color, mode, texture });
    verts.append({ c.x, c.y, uvc.x, uvc.y, color, mode, texture });
}

void UI::draw_quad(boxf b, boxf uv, vec4f color, Draw_Mode mode, Texture_Id texture) {
    if (verts.len + 6 >= verts.cap)
        flush_verts();

    draw_triangle(
        {b.x, b.y + b.h},
        {b.x, b.y},
        {b.x + b.w, b.y},
        {uv.x, uv.y + uv.h},
        {uv.x, uv.y},
        {uv.x + uv.w, uv.y},
        color,
        mode,
        texture
    );

    draw_triangle(
        {b.x, b.y + b.h},
        {b.x + b.w, b.y},
        {b.x + b.w, b.y + b.h},
        {uv.x, uv.y + uv.h},
        {uv.x + uv.w, uv.y},
        {uv.x + uv.w, uv.y + uv.h},
        color,
        mode,
        texture
    );
}

void UI::draw_rect(boxf b, vec4f color) {
    draw_quad(b, { 0, 0, 1, 1 }, color, DRAW_SOLID);
}

void UI::draw_rounded_rect(boxf b, vec4f color, float radius, int round_flags) {
    /* A picture is worth a thousand words:
       _________
      |_|     |_|
      |_       _|
      |_|_____|_| */

    boxf edge;
    edge.x = b.x;
    edge.y = b.y + radius;
    edge.w = radius;
    edge.h = b.h - radius * 2;
    draw_rect(edge, color);

    edge.x = b.x + radius;
    edge.y = b.y;
    edge.h = radius;
    edge.w = b.w - radius * 2;
    draw_rect(edge, color);

    edge.x = b.x + b.w - radius;
    edge.y = b.y + radius;
    edge.w = radius;
    edge.h = b.h - radius * 2;
    draw_rect(edge, color);

    edge.x = b.x + radius;
    edge.y = b.y + b.h - radius;
    edge.w = b.w - radius * 2;
    edge.h = radius;
    draw_rect(edge, color);

    boxf center;
    center.x = b.x + radius;
    center.y = b.y + radius;
    center.w = b.w - radius * 2;
    center.h = b.h - radius * 2;
    draw_rect(center, color);

    auto draw_rounded_corner = [&](vec2f center, float start_rad, float end_rad) {
        vec2f zero = {0};

        float increment = (end_rad - start_rad) / max(3, (int)(radius / 5));
        for (float angle = start_rad; angle < end_rad; angle += increment) {
            auto ang1 = angle;
            auto ang2 = angle + increment;

            vec2f v1 = {center.x + radius * cos(ang1), center.y - radius * sin(ang1)};
            vec2f v2 = {center.x + radius * cos(ang2), center.y - radius * sin(ang2)};

            draw_triangle(center, v1, v2, zero, zero, zero, color, DRAW_SOLID);
        }
    };

    auto draw_corner = [&](bool round, vec2f center, float ang_start, float ang_end) {
        if (round) {
            draw_rounded_corner(center, ang_start, ang_end);
            return;
        }

        boxf b;
        b.x = fmin(center.x, center.x + radius * cos(ang_start));
        b.y = fmin(center.y, center.y - radius * sin(ang_start));
        b.w = radius;
        b.h = radius;
        draw_rect(b, color);
    };

    draw_corner(round_flags & ROUND_TR, {b.x + b.w - radius, b.y + radius}, 0, M_PI / 2);
    draw_corner(round_flags & ROUND_TL, {b.x + radius, b.y + radius}, M_PI / 2, M_PI);
    draw_corner(round_flags & ROUND_BL, {b.x + radius, b.y + b.h - radius}, M_PI, M_PI * 3/2);
    draw_corner(round_flags & ROUND_BR, {b.x + b.w - radius, b.y + b.h - radius}, M_PI * 3/2, M_PI * 2);
}

void UI::draw_bordered_rect_outer(boxf b, vec4f color, vec4f border_color, int border_width, float radius) {
    auto b2 = b;
    b2.x -= border_width;
    b2.y -= border_width;
    b2.h += border_width * 2;
    b2.w += border_width * 2;

    if (radius == 0) {
        draw_rect(b2, border_color);
        draw_rect(b, color);
    } else {
        draw_rounded_rect(b2, border_color, radius, ROUND_ALL);
        draw_rounded_rect(b, color, radius, ROUND_ALL);
    }
}

// advances pos forward
void UI::draw_char(vec2f* pos, char ch, vec4f color) {
    stbtt_aligned_quad q;
    stbtt_GetPackedQuad(font->char_info, font->tex_size, font->tex_size, ch - ' ', &pos->x, &pos->y, &q, 0);
    if (q.x1 > q.x0) {
        boxf box = { q.x0, q.y0, q.x1 - q.x0, q.y1 - q.y0 };
        boxf uv = { q.s0, q.t0, q.s1 - q.s0, q.t1 - q.t0 };
        draw_quad(box, uv, color, DRAW_FONT_MASK);
    }
}

vec2f UI::draw_string(vec2f pos, ccstr s, vec4f color) {
    pos.y += font->offset_y;
    for (u32 i = 0, len = strlen(s); i < len; i++)
        draw_char(&pos, s[i], color);
    pos.y -= font->offset_y;
    return pos;
}

float UI::get_text_width(ccstr s) {
    float x = 0, y = 0;
    stbtt_aligned_quad q;
    for (u32 i = 0, len = strlen(s); i < len; i++)
        stbtt_GetPackedQuad(font->char_info, font->tex_size, font->tex_size, s[i] - ' ', &x, &y, &q, 0);
    return x;
}

boxf UI::get_viewport_area() {
    boxf b = {0};
    b.size = world.window_size;
    b.y += world.ui.menubar_height;
    b.h -= world.ui.menubar_height;
    return b;
}

boxf UI::get_sidebar_area() {
    auto viewport_area = get_viewport_area();

    boxf sidebar_area = {0};
    sidebar_area.pos = viewport_area.pos;

    if (world.sidebar.view != SIDEBAR_CLOSED) {
        sidebar_area.h = viewport_area.h;
        sidebar_area.w = world.sidebar.width;
    }

    sidebar_area.h -= get_build_results_area().h;
    return sidebar_area;
}

boxf UI::get_panes_area(boxf *pstatus_area) {
    auto panes_area = get_viewport_area();

    auto sidebar_area = get_sidebar_area();
    panes_area.x += sidebar_area.w;
    panes_area.w -= sidebar_area.w;

    panes_area.h -= get_build_results_area().h;

    boxf status_area;
    status_area.h = font->height + settings.status_padding_y * 2;
    status_area.w = panes_area.w;
    status_area.x = panes_area.x;
    status_area.y = panes_area.y + panes_area.h - status_area.h;

    panes_area.h -= status_area.h;

    if (pstatus_area != NULL)
        *pstatus_area = status_area;
    return panes_area;
}

int UI::get_mouse_flags(boxf area) {
    // how do we handle like overlapping elements later?
    // it can't be that hard, imgui does it

    if (world.ui.mouse_captured_by_imgui)
        return 0;

    auto contains_mouse = [&]() -> bool {
        if (area.contains(world.ui.mouse_pos))
            if (!clipping || current_clip.contains(world.ui.mouse_pos))
                return true;
        return false;
    };

    int ret = 0;
    if (contains_mouse()) {
        ret |= MOUSE_HOVER;
        if (world.ui.mouse_down[GLFW_MOUSE_BUTTON_LEFT])
            ret |= MOUSE_CLICKED;
        if (world.ui.mouse_down[GLFW_MOUSE_BUTTON_RIGHT])
            ret |= MOUSE_RCLICKED;
        if (world.ui.mouse_down[GLFW_MOUSE_BUTTON_MIDDLE])
            ret |= MOUSE_MCLICKED;
    }
    return ret;
}

File_Tree_Node *get_file_tree_node_from_index(int idx) {
    auto stack = alloc_list<File_Tree_Node*>();
    for (auto child = world.file_tree->children; child != NULL; child = child->next)
        stack->append(child);

    for (u32 i = 0; stack->len > 0; i++) {
        auto it = *stack->last();
        if (i == idx) return it;

        stack->len--;

        if (it->is_directory && it->open) {
            SCOPED_FRAME();
            auto children = alloc_list<File_Tree_Node*>();
            for (auto curr = it->children; curr != NULL; curr = curr->next)
                children->append(curr);
            for (i32 i = children->len-1; i >= 0; i--)
                stack->append(children->at(i));
        }
    }

    return NULL;
}

ccstr file_tree_node_to_path(File_Tree_Node *node) {
    auto path = alloc_list<File_Tree_Node*>();
    for (auto curr = node; curr != NULL; curr = curr->parent)
        path->append(curr);
    path->len--; // remove root

    Text_Renderer r;
    r.init();
    for (i32 j = path->len - 1; j >= 0; j--) {
        r.write("%s", path->at(j)->name);
        if (j != 0) r.write("/");
    }
    return r.finish();
}

void UI::draw_everything(GLuint vao, GLuint vbo, GLuint program) {
    {
        // prepare opengl for drawing shit
        glViewport(0, 0, world.display_size.x, world.display_size.y);
        glUseProgram(program);
        glBindVertexArray(vao); // bind my vertex array & buffers
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        verts.init(LIST_FIXED, 6 * 128, alloc_array(Vert, 6 * 128));
    }

    auto& wksp = world.wksp;

    boxf status_area = {0};
    boxf panes_area = get_panes_area(&status_area);
    boxf sidebar_area = get_sidebar_area();

    boxf pane_area;
    pane_area.pos = panes_area.pos;

    if (world.sidebar.view != SIDEBAR_CLOSED) {
        draw_rect(sidebar_area, rgba(COLOR_BG));

        {
            boxf line;
            line.x = sidebar_area.x + sidebar_area.w - 1;
            line.y = sidebar_area.y;
            line.h = sidebar_area.h;
            line.w = 2;
            draw_rect(line, rgba(COLOR_DARK_GREY));
        }

        sidebar_area.x += settings.sidebar_padding_x;
        sidebar_area.w -= settings.sidebar_padding_x * 2;

        switch (world.sidebar.view) {
        case SIDEBAR_FILE_EXPLORER:
            {
                u32 depth = 0;

                boxf buttons_area = sidebar_area;
                buttons_area.h = settings.filetree_button_size + (settings.filetree_button_padding * 2) + (settings.filetree_buttons_area_padding_y * 2);

                // draw buttons area
                {
                    int num_buttons = 3;
                    int button_idx = 0;
                    boxf button_area;

                    button_area.x = buttons_area.x + buttons_area.w - settings.filetree_buttons_area_padding_x - settings.filetree_button_size - settings.filetree_button_padding;
                    button_area.y = buttons_area.y + settings.filetree_buttons_area_padding_y;
                    button_area.w = settings.filetree_button_size + (settings.filetree_button_padding * 2);
                    button_area.h = settings.filetree_button_size + (settings.filetree_button_padding * 2);

                    // call this from right to left
                    auto draw_button = [&](Sprites_Image_Type image_id) -> bool {
                        auto mouse_flags = get_mouse_flags(button_area);
                        if (mouse_flags & MOUSE_HOVER)
                            draw_rounded_rect(button_area, rgba(COLOR_WHITE, 0.2), 2, ROUND_ALL);

                        boxf icon_area = button_area;
                        icon_area.x += settings.filetree_button_padding;
                        icon_area.y += settings.filetree_button_padding;
                        icon_area.w -= settings.filetree_button_padding * 2;
                        icon_area.h -= settings.filetree_button_padding * 2;
                        draw_image(image_id, icon_area);

                        button_area.pos.x -= settings.filetree_button_margin_x;
                        button_area.pos.x -= settings.filetree_button_size;
                        button_area.pos.x -= settings.filetree_button_padding * 2;
                        return (mouse_flags & MOUSE_CLICKED);
                    };

                    if (draw_button(SIMAGE_REFRESH)) {
                        // TODO: probably make this async task
                        fill_file_tree();
                    }

                    auto open_add_file_or_folder = [&](bool folder) {
                        world.wnd_add_file_or_folder.show = true;

                        File_Tree_Node *node = NULL;

                        auto is_root = [&]() {
                            if (world.file_explorer.selection == -1) return true;

                            node = get_file_tree_node_from_index(world.file_explorer.selection);
                            if (node->is_directory) return false;

                            node = node->parent;
                            return (node->parent == NULL);
                        };

                        auto &wnd = world.wnd_add_file_or_folder;

                        wnd.location_is_root = is_root();
                        if (!wnd.location_is_root) {
                            strcpy_safe(
                                wnd.location,
                                _countof(wnd.location),
                                file_tree_node_to_path(node)
                            );
                        }

                        wnd.folder = folder;
                    };

                    if (draw_button(SIMAGE_ADD_FOLDER))
                        open_add_file_or_folder(true);
                    if (draw_button(SIMAGE_ADD))
                        open_add_file_or_folder(false);
                }

                boxf files_area = sidebar_area;
                files_area.h -= buttons_area.h;
                files_area.y += buttons_area.h;

                start_clip(files_area);
                defer { end_clip(); };

                boxf row_area;
                row_area.pos = files_area.pos;
                row_area.y -= world.file_explorer.scroll_offset;
                row_area.y += settings.filetree_item_margin;
                row_area.w = files_area.w;
                row_area.h = font->height;
                row_area.h += settings.filetree_item_padding_y * 2;

                auto stack = alloc_list<File_Tree_Node*>();
                for (auto child = world.file_tree->children; child != NULL; child = child->next)
                    stack->append(child);

                for (u32 i = 0; stack->len > 0 && row_area.y <= files_area.y + files_area.h; i++, row_area.y += row_area.h + settings.filetree_space_between_items) {
                    auto it = *stack->last();
                    stack->len--;

                    if (world.file_explorer.selection == i)
                        draw_rounded_rect(row_area, rgba(COLOR_DARK_GREY), 4, ROUND_ALL);

                    auto is_row_visible = [&]() {
                        auto sb_top = files_area.y;
                        auto sb_bot = files_area.y + files_area.h;

                        auto row_top = row_area.y;
                        auto row_bot = row_area.y + row_area.h;

                        return (
                            sb_top <= row_top && row_top <= sb_bot
                            || sb_top <= row_bot && row_bot <= sb_bot
                        );
                    };

                    if (is_row_visible()) {
                        SCOPED_FRAME();

                        int mouse_flags = get_mouse_flags(row_area);

                        if (mouse_flags & MOUSE_HOVER)
                            draw_rounded_rect(row_area, rgba(COLOR_DARK_GREY), 4, ROUND_ALL);

                        if (mouse_flags & MOUSE_CLICKED) {
                            world.file_explorer.selection = i;

                            if (it->is_directory) {
                                it->open = !it->open;
                            } else {
                                SCOPED_FRAME();
                                auto rel_path = file_tree_node_to_path(it);
                                auto full_path = path_join(world.wksp.path, rel_path);
                                world.get_current_pane()->focus_editor(full_path);
                            }
                        }

                        boxf text_area = row_area;
                        text_area.x += settings.sidebar_item_padding_x + it->depth * 20;
                        text_area.w -= settings.sidebar_item_padding_x * 2;
                        text_area.y += settings.sidebar_item_padding_y;
                        text_area.h -= settings.sidebar_item_padding_y * 2;

                        boxf icon_area;
                        icon_area.pos = text_area.pos;
                        icon_area.w = text_area.h;
                        icon_area.h = text_area.h;

                        text_area.x += (icon_area.w + 5);
                        text_area.w -= (icon_area.w + 5);

                        // draw_image(SIMAGE_FOLDER, icon_area);
                        draw_image(it->is_directory ? SIMAGE_FOLDER : SIMAGE_SOURCE_FILE, icon_area);

                        auto label = (cstr)our_strcpy(it->name);
                        auto avail_chars = (int)(text_area.w / font->width);
                        if (avail_chars < strlen(label))
                            label[avail_chars] = '\0';

                        draw_string(text_area.pos, label, rgba(COLOR_WHITE));
                    }

                    if (it->is_directory && it->open) {
                        SCOPED_FRAME();
                        auto children = alloc_list<File_Tree_Node*>();
                        for (auto curr = it->children; curr != NULL; curr = curr->next)
                            children->append(curr);
                        for (i32 i = children->len-1; i >= 0; i--)
                            stack->append(children->at(i));
                    }
                }

                boxf sep_area = buttons_area;
                sep_area.y += buttons_area.h;
                sep_area.h = 1;
                draw_rect(sep_area, rgba(COLOR_WHITE, 0.2));
            }
            break;
        case SIDEBAR_SEARCH_RESULTS:
            {
                vec2f pos;
                pos.x = 0;
                pos.y = sidebar_area.y - world.search_results.scroll_offset;
                pos.y += font->offset_y;

                For (world.search_results.results) {
                    if (pos.y >= sidebar_area.y) {
                        SCOPED_FRAME();

                        pos.x = 0;

                        boxf line_area;
                        line_area.pos = pos;
                        line_area.y -= font->offset_y;
                        line_area.w = sidebar_area.w;
                        line_area.h = font->height;

                        auto mouse_flags = get_mouse_flags(line_area);

                        if (mouse_flags & MOUSE_HOVER)
                            draw_rect(line_area, rgba(COLOR_DARK_GREY));

                        if (mouse_flags & MOUSE_CLICKED) {
                            SCOPED_FRAME();
                            auto path = path_join(world.wksp.path, it->filename);
                            auto pos = new_cur2(it->match_col, it->row-1);
                            world.get_current_pane()->focus_editor(path, pos);
                        }

                        auto str = our_sprintf("%s:%d:%d ", it->filename, it->row, it->match_col+1);
                        auto len = strlen(str);

                        for (u32 i = 0; i < len && pos.x < sidebar_area.w; i++)
                            draw_char(&pos, str[i], rgba(COLOR_WHITE));

                        len = strlen(it->preview);
                        for (u32 i = 0; i < len && pos.x < sidebar_area.w; i++) {
                            auto color = rgba(COLOR_WHITE);
                            if (it->match_col_in_preview <= i && i < it->match_col_in_preview + it->match_len)
                                color = rgba(COLOR_LIME);
                            draw_char(&pos, it->preview[i], color);
                        }
                    }

                    pos.y += font->height;
                    if (pos.y > sidebar_area.h) break;
                }
            }
            break;
        }
    }

    // Draw panes.
    u32 current_pane = 0;
    for (auto && pane : wksp.panes) {
        auto is_pane_selected = (current_pane == wksp.current_pane);

        pane_area.w = pane.width;
        pane_area.h = panes_area.h;

        boxf tabs_area, editor_area;
        get_tabs_and_editor_area(&pane_area, &tabs_area, &editor_area);

        draw_rect(tabs_area, rgba(is_pane_selected ? COLOR_MEDIUM_GREY : COLOR_DARK_GREY));
        draw_rect(editor_area, rgba(COLOR_BG));

        vec2 tab_padding = { 15, 5 };

        boxf tab;
        tab.pos = tabs_area.pos + new_vec2(5, tabs_area.h - tab_padding.y * 2 - font->height);

        i32 tab_to_remove = -1;

        // draw tabs
        u32 tab_id = 0;
        for (auto&& editor : pane.editors) {
            SCOPED_FRAME();

            bool is_selected = (tab_id == pane.current_editor);

            ccstr label = NULL;

            if (editor.is_untitled) {
                label = "<untitled>";
            } else {
                auto wksp_path = make_path(wksp.path);
                auto file_path = make_path(editor.filepath);

                if (wksp_path->contains(file_path)) {
                    Text_Renderer r;
                    r.init();
                    for (int i = wksp_path->parts->len; i < file_path->parts->len; i++) {
                        r.writestr(file_path->parts->at(i));
                        if (i + 1 < file_path->parts->len)
                            r.writechar(PATH_SEP);
                    }
                    label = r.finish();
                } else {
                    label = file_path->str();
                }

                label = our_sprintf("%s%s", label, editor.buf.dirty ? "*" : "");
            }

            auto text_width = get_text_width(label);

            tab.w = text_width + tab_padding.x * 2;
            tab.h = font->height + tab_padding.y * 2;

            auto mouse_flags = get_mouse_flags(tab);

            vec3f tab_color = COLOR_MEDIUM_DARK_GREY;
            if (is_selected)
                tab_color = COLOR_BG;
            else if (mouse_flags & MOUSE_HOVER)
                tab_color = COLOR_DARK_GREY;

            draw_rounded_rect(tab, rgba(tab_color), 4, ROUND_TL | ROUND_TR);
            draw_string(tab.pos + tab_padding, label, rgba(is_selected ? COLOR_WHITE : COLOR_LIGHT_GREY));

            if (mouse_flags & MOUSE_CLICKED)
                pane.focus_editor_by_index(tab_id);

            if (mouse_flags & MOUSE_MCLICKED)
                tab_to_remove = tab_id;

            tab.pos.x += tab.w + 5;
            tab_id++;
        }

        if (tab_to_remove != -1) {
            // duplicate of code in main.cpp under GLFW_KEY_W handler, refactor
            // if we copy this a few more times
            pane.editors[tab_to_remove].cleanup();
            pane.editors.remove(tab_to_remove);

            if (pane.editors.len == 0)
                pane.current_editor = -1;
            else if (pane.current_editor == tab_to_remove) {
                auto new_idx = pane.current_editor;
                if (new_idx >= pane.editors.len)
                    new_idx = pane.editors.len - 1;
                pane.focus_editor_by_index(new_idx);
            } else if (pane.current_editor > tab_to_remove) {
                pane.current_editor--;
            }
        }

        // draw editor
        if (pane.editors.len > 0) {
            vec2f cur_pos = editor_area.pos + new_vec2f(EDITOR_MARGIN_X, EDITOR_MARGIN_Y);
            cur_pos.y += font->offset_y;

            auto editor = pane.get_current_editor();

            struct Highlight {
                cur2 start;
                cur2 end;
                vec3f color;
            };

            List<Highlight> highlights;
            highlights.init();

            // generate editor highlights
            if (editor->tree != NULL) {
                ts_tree_cursor_reset(&editor->cursor, ts_tree_root_node(editor->tree));

                auto start = new_cur2(0, editor->view.y);
                auto end = new_cur2(0, editor->view.y + editor->view.h);

                walk_ts_cursor(&editor->cursor, false, [&](Ast_Node *node, Ts_Field_Type, int depth) -> Walk_Action {
                    auto node_start = node->start;
                    auto node_end = node->end;

                    if (node_end < start) return WALK_SKIP_CHILDREN;
                    if (node_start > end) return WALK_ABORT;

                    vec3f color = {0};
                    if (get_type_color(node, editor, &color)) {
                        auto hl = highlights.append();
                        hl->start = node_start;
                        hl->end = node_end;
                        hl->color = color;
                    }

                    return WALK_CONTINUE;
                });
            }

            if (world.nvim.waiting_focus_window == editor->id) {
                // TODO
            } else {
                auto &buf = editor->buf;
                auto &view = editor->view;

                vec2f actual_cursor_position = { -1, -1 };
                vec2f actual_parameter_hint_start = { -1, -1 };

                auto draw_cursor = [&]() {
                    actual_cursor_position = cur_pos;    // save position where cursor is drawn for later use
                    boxf b;
                    b.x = cur_pos.x;
                    b.y = cur_pos.y - font->offset_y;
                    b.h = (float)font->height;

                    if (world.nvim.mode == VI_INSERT && !world.nvim.exiting_insert_mode) {
                        b.w = 2;
                    } else {
                        b.w = (float)font->width;
                    }

                    draw_rect(b, rgba(COLOR_LIME));
                };

                List<Client_Breakpoint> breakpoints_for_this_editor;

                {
                    u32 len = 0;
                    For (world.dbg.breakpoints)
                        if (streq(it.file, editor->filepath))
                            len++;

                    alloc_list(&breakpoints_for_this_editor, len);
                    For (world.dbg.breakpoints) {
                        if (streq(it.file, editor->filepath)) {
                            auto p = breakpoints_for_this_editor.append();
                            memcpy(p, &it, sizeof(it));
                        }
                    }
                }

                if (buf.lines.len == 0) draw_cursor();

                auto &hint = editor->parameter_hint;

                int next_hl = (highlights.len > 0 ? 0 : -1);

                auto relative_y = 0;
                for (u32 y = view.y; y < view.y + view.h; y++, relative_y++) {
                    if (y >= buf.lines.len) break;

                    auto line = &buf.lines[y];

                    auto is_stopped_at_this_line = [&]() -> bool {
                        if (world.dbg.state_flag == DBGSTATE_PAUSED)
                            if (streq(world.dbg.state.file_stopped_at, editor->filepath))
                                if (world.dbg.state.line_stopped_at == y + 1)
                                    return true;
                        return false;
                    };

                    boxf line_box = {
                        cur_pos.x,
                        cur_pos.y - font->offset_y,
                        (float)editor_area.w,
                        (float)font->height - 1,
                    };

                    if (is_stopped_at_this_line()) {
                        draw_rect(line_box, rgba(COLOR_DARK_YELLOW));
                    } else {
                        For (breakpoints_for_this_editor) {
                            if (it.line == y + 1) {
                                bool inactive = (it.pending || world.dbg.state_flag == DBGSTATE_INACTIVE);
                                draw_rect(line_box, rgba(COLOR_DARK_RED, inactive ? 0.5 : 1.0));
                                break;
                            }
                        }
                    }

                    auto line_number_width = (int)log10(buf.lines.len) + 1;

                    auto is_cursor_match = [&](cur2 cur, i32 x, i32 y) -> bool {
                        if (cur.y != y) return false;
                    };

                    {
                        cur_pos.x += settings.line_number_margin_left;
                        auto line_number_str = our_sprintf("%*d", line_number_width, y + 1);
                        auto len = strlen(line_number_str);
                        for (u32 i = 0; i < len; i++)
                            draw_char(&cur_pos, line_number_str[i], rgba(COLOR_MEDIUM_DARK_GREY));
                        cur_pos.x += settings.line_number_margin_right;
                    }

                    for (u32 x = view.x, vx = view.x; vx < view.x + view.w; x++) {
                        if (x >= line->len) break;

                        vec3f text_color = COLOR_WHITE;

                        if (next_hl != -1) {
                            auto curr = new_cur2(x, y);

                            while (next_hl != -1 && curr >= highlights[next_hl].end)
                                if (++next_hl >= highlights.len)
                                    next_hl = -1;

                            auto& hl = highlights[next_hl];
                            if (hl.start <= curr && curr < hl.end)
                                text_color = hl.color;
                        }

                        if (editor->cur == new_cur2(x, y)) {
                            draw_cursor();
                            if (world.nvim.mode != VI_INSERT)
                                text_color = COLOR_BLACK;
                        }
                        uchar uch = line->at(x);
                        if (uch == '\t') {
                            auto chars = TAB_SIZE - ((vx - view.x) % TAB_SIZE);
                            cur_pos.x += font->width * chars;
                            vx += chars;
                        } else {
                            draw_char(&cur_pos, (char)uch, rgba(text_color));
                            vx++;
                        }

                        if (hint.gotype != NULL)
                            if (new_cur2(x, y) == hint.start)
                                actual_parameter_hint_start = cur_pos;
                    }

                    if (editor->cur == new_cur2(line->len, y))
                        draw_cursor();

                    cur_pos.x = editor_area.x + EDITOR_MARGIN_X;
                    cur_pos.y += font->height * LINE_HEIGHT;
                }

                do {
                    // we can't draw the autocomplete if we don't know where to draw it
                    // when would this ever happen though?
                    if (actual_cursor_position.x == -1) break;

                    auto &ac = editor->autocomplete;

                    if (ac.ac.results == NULL) break;

                    s32 max_len = 0;
                    s32 num_items = min(ac.filtered_results->len, AUTOCOMPLETE_WINDOW_ITEMS);

                    For(*ac.filtered_results) {
                        auto len = strlen(ac.ac.results->at(it).name);
                        if (len > max_len)
                            max_len = len;
                    }

                    /*
                    OK SO BASICALLY
                    first try to put it in bottom
                    then put it in top
                    if neither fits then shrink it to max(bottom_limit, top_limit) and then put it there
                    */

                    if (num_items > 0) {
                        boxf menu;

                        menu.w = (font->width * max_len) + (settings.autocomplete_item_padding_x * 2) + (settings.autocomplete_menu_padding * 2);
                        menu.h = (font->height * num_items) + (settings.autocomplete_item_padding_y * 2 * num_items) + (settings.autocomplete_menu_padding * 2);

                        // menu.x = min(actual_cursor_position.x - strlen(ac.ac.prefix) * font->width, world.window_size.x - menu.w);
                        // menu.y = min(actual_cursor_position.y - font->offset_y + font->height, world.window_size.y - menu.h);

                        {
                            auto y1 = actual_cursor_position.y - font->offset_y;
                            auto y2 = actual_cursor_position.y - font->offset_y + font->height;

                            if (y2 + menu.h < world.window_size.y) {
                                menu.y = y2;
                            } else if (y1 >= menu.h) {
                                menu.y = y1 - menu.h;
                            } else {
                                auto space_under = world.window_size.y - y2;
                                auto space_above = y1;

                                if (space_under > space_above) {
                                    menu.y = y2;
                                    menu.h = space_under;
                                } else {
                                    menu.y = 0;
                                    menu.h = y1;
                                }
                            }

                            auto x1 = actual_cursor_position.x - strlen(ac.ac.prefix) * font->width;
                            auto x2 = actual_cursor_position.x;

                            if (x1 + menu.w < world.window_size.x) {
                                menu.x = x1;
                            } else if (x2 >= menu.w) {
                                menu.x = x2 - menu.w;
                            } else {
                                auto space_right = world.window_size.x - x1;
                                auto space_left = x2;

                                if (space_right > space_left) {
                                    menu.x = x1;
                                    menu.w = space_right;
                                } else {
                                    menu.x = 0;
                                    menu.w = x2;
                                }
                            }
                        }

                        draw_bordered_rect_outer(menu, rgba(COLOR_BLACK), rgba(COLOR_LIGHT_GREY), 1, 4);

                        auto menu_pos = menu.pos + new_vec2f(settings.autocomplete_menu_padding, settings.autocomplete_menu_padding);

                        for (int i = ac.view; i < ac.view + num_items; i++) {
                            auto idx = ac.filtered_results->at(i);

                            vec3f color = new_vec3f(1.0, 1.0, 1.0);

                            if (i == ac.selection) {
                                boxf b;
                                b.pos = menu_pos;
                                b.h = font->height + (settings.autocomplete_item_padding_y * 2);
                                b.w = menu.w - (settings.autocomplete_menu_padding * 2);
                                draw_rounded_rect(b, rgba(COLOR_DARK_GREY), 4, ROUND_ALL);
                                // color = new_vec3f(0.0, 0.0, 0.0);
                            }

                            {
                                SCOPED_FRAME();
                                auto str = ac.ac.results->at(idx).name;

                                auto pos = menu_pos + new_vec2f(settings.autocomplete_item_padding_x, settings.autocomplete_item_padding_y);
                                draw_string(pos, str, rgba(color));
                            }

                            menu_pos.y += font->height + settings.autocomplete_item_padding_y * 2;
                        }
                    }
                } while (0);

                do {
                    if (actual_parameter_hint_start.x == -1) break;

                    if (hint.gotype == NULL) break;

                    boxf bg;
                    bg.w = font->width * strlen(hint.help_text);
                    bg.h = font->height;
                    bg.x = min(actual_parameter_hint_start.x, world.window_size.x - bg.w);
                    bg.y = min(actual_parameter_hint_start.y - font->offset_y - font->height, world.window_size.y - bg.h);

                    draw_bordered_rect_outer(bg, rgba(COLOR_DARK_GREY), rgba(COLOR_WHITE), 1);
                    draw_string(bg.pos, hint.help_text, rgba(COLOR_WHITE));
                } while (0);
            }
        }

        current_pane++;
        pane_area.x += pane_area.w;
    }

    {
        // Draw pane resizers.

        // TODO: set cursors. For reference:
        // glfwSetCursor(wnd, world.ui.cursors[ImGuiMouseCursor_ResizeEW]);
        // glfwSetCursor(wnd, world.ui.cursors[ImGuiMouseCursor_Arrow]);

        float offset = panes_area.x;

        for (u32 i = 0; i < world.wksp.panes.len - 1; i++) {
            offset += world.wksp.panes[i].width;

            boxf b;
            b.w = 4;
            b.h = panes_area.h;
            b.x = panes_area.x + offset - 2;
            b.y = panes_area.y;

            if (get_mouse_flags(b) & MOUSE_HOVER) {
                draw_rect(b, rgba(COLOR_WHITE));
                if (world.ui.mouse_down[GLFW_MOUSE_BUTTON_LEFT]) {
                    world.wksp.resizing_pane = i;
                } else {
                    world.wksp.resizing_pane = -1;
                }
            } else {
                draw_rect(b, rgba(COLOR_MEDIUM_GREY));
            }
        }
    }

    {
        draw_rect(status_area, rgba("#252525"));

        boxf str_area;
        str_area.pos = status_area.pos;
        str_area.h = status_area.h;

        auto draw_status_piece = [&](ccstr s, vec4f bgcolor, vec4f fgcolor) {
            str_area.w = font->width * strlen(s) + (settings.status_padding_x * 2);
            draw_rect(str_area, bgcolor);

            boxf text_area = str_area;
            text_area.x += settings.status_padding_x;
            text_area.y += settings.status_padding_y;
            text_area.w -= (settings.status_padding_x * 2);
            text_area.h -= (settings.status_padding_y * 2);
            draw_string(text_area.pos, s, fgcolor);

            str_area.x += str_area.w;
        };

        if (world.use_nvim) {
            ccstr mode_str = NULL;

            switch (world.nvim.mode) {
            case VI_NORMAL: mode_str = "NORMAL"; break;
            case VI_VISUAL: mode_str = "VISUAL"; break;
            case VI_INSERT: mode_str = "INSERT"; break;
            case VI_REPLACE: mode_str = "REPLACE"; break;
            case VI_OPERATOR: mode_str = "OPERATOR"; break;
            case VI_CMDLINE: mode_str = "CMDLINE"; break;
            default: mode_str = "UNKNOWN"; break;
            }

            if (mode_str != NULL)
                draw_status_piece(mode_str, rgba("#666666"), rgba("aaaaaa"));
        }

        if (world.indexer.ready)
            draw_status_piece("INDEX READY", rgba("#008800"), rgba("#cceecc"));
        else
            draw_status_piece("INDEXING IN PROGRESS", rgba("#880000"), rgba("#eecccc"));
    }

    auto get_debugger_state_string = [&]() -> ccstr {
        switch (world.dbg.state_flag) {
        case DBGSTATE_PAUSED: return "PAUSED";
        case DBGSTATE_STARTING: return "STARTING";
        case DBGSTATE_RUNNING: return "RUNNING";
        }
        return NULL;
    };

    auto state_str = get_debugger_state_string();
    if (state_str != NULL) {
        boxf b;
        b.w = (font->width * strlen(state_str));
        b.h = font->height;
        b.x = world.display_size.x - 10 - b.w;
        b.y = world.display_size.y - 10 - b.h;

        draw_rect(b, rgba(COLOR_DARK_RED));
        draw_string(b.pos, state_str, rgba(COLOR_WHITE));
    }

    if (world.error_list.show) {
        boxf build_results_area = get_build_results_area();
        draw_rect(build_results_area, rgba(COLOR_DARK_GREY));

        boxf row_area;
        row_area.x = build_results_area.x;
        row_area.y = build_results_area.y;
        row_area.h = font->height + settings.error_list_item_padding_y * 2;
        row_area.w = build_results_area.w;

        if (world.build.done) {
            if (world.build.errors.len == 0) {
                auto pos = row_area.pos;
                pos.x += settings.error_list_item_padding_x;
                pos.y += settings.error_list_item_padding_y;
                draw_string(pos, "Build was successful!", rgba(COLOR_GREEN));
            } else {
                row_area.y -= world.wnd_build_and_debug.scroll_offset;

                start_clip(build_results_area);
                defer { end_clip(); };

                int index = 0;
                For (world.build.errors) {
                    defer {
                        row_area.y += row_area.h;
                        index++;
                    };

                    if (index == world.build.current_error) {
                        if (row_area.y + row_area.h > build_results_area.y + build_results_area.h) {
                            auto delta = (row_area.y + row_area.h) - (build_results_area.y + build_results_area.h);
                            world.wnd_build_and_debug.scroll_offset += delta;
                        }
                        if (row_area.y < build_results_area.y) {
                            world.wnd_build_and_debug.scroll_offset -= (build_results_area.y - row_area.y);
                        }
                    }

                    if (row_area.y > build_results_area.y + build_results_area.h) continue;

                    auto mouse_flags = get_mouse_flags(row_area);

                    vec3f text_color = COLOR_WHITE;

                    if (mouse_flags & MOUSE_HOVER)
                        draw_rect(row_area, rgba(COLOR_MEDIUM_GREY));

                    if (index == world.build.current_error) {
                        draw_rect(row_area, rgba(COLOR_LIGHT_GREY));
                        text_color = COLOR_BLACK;
                    }

                    if (mouse_flags & MOUSE_CLICKED) {
                        world.build.current_error = index;
                        go_to_error(index);
                    }

                    if (row_area.y + row_area.h > build_results_area.y) {
                        SCOPED_FRAME();
                        auto s = our_sprintf("%s:%d:%d: %s", it.file, it.row, it.col, it.message);
                        auto pos = row_area.pos;
                        pos.x += settings.error_list_item_padding_x;
                        pos.y += settings.error_list_item_padding_y;
                        draw_string(pos, s, rgba(text_color));
                    }
                }
            }
        } else {
            auto pos = row_area.pos;
            pos.x += settings.error_list_item_padding_x;
            pos.y += settings.error_list_item_padding_y;
            draw_string(pos, "Building...", rgba(COLOR_MEDIUM_GREY));
        }
    }

    // TODO: draw 'search anywhere' window
    flush_verts();

    recalculate_view_sizes();
}

void UI::draw_image(Sprites_Image_Type image_id, boxf b) {
    auto rect = &sprite_rects[image_id];

    boxf uv;
    uv.x = rect->x / sprite_tex_size;
    uv.y = rect->y / sprite_tex_size;
    uv.w = rect->w / sprite_tex_size;
    uv.h = rect->h / sprite_tex_size;

    if (image_mask_types[image_id])
        draw_quad(b, uv, {1.0, 1.0, 1.0, 1.0}, DRAW_IMAGE_MASK, TEXTURE_IMAGES);
    else
        draw_quad(b, uv, {0}, DRAW_IMAGE, TEXTURE_IMAGES);
}

boxf UI::get_build_results_area() {
    boxf b;
    b.x = 0;
    b.y = world.window_size.y;
    b.w = world.window_size.x;
    b.h = 0;

    if (world.error_list.show) {
        b.y -= world.error_list.height;
        b.h += world.error_list.height;
    }

    return b;
}

void UI::get_tabs_and_editor_area(boxf* pane_area, boxf* ptabs_area, boxf* peditor_area) {
    boxf tabs_area, editor_area;

    tabs_area.pos = pane_area->pos;
    tabs_area.w = pane_area->w;
    tabs_area.h = 30; // ???

    editor_area.pos = pane_area->pos;
    editor_area.y += tabs_area.h;
    editor_area.w = pane_area->w;
    editor_area.h = pane_area->h - tabs_area.h;

    if (ptabs_area != NULL)
        memcpy(ptabs_area, &tabs_area, sizeof(boxf));
    if (peditor_area != NULL)
        memcpy(peditor_area, &editor_area, sizeof(boxf));
}

void UI::recalculate_view_sizes(bool force) {
    boxf panes_area = get_panes_area();
    auto new_sizes = alloc_list<vec2f>();

    float total = 0;
    For (world.wksp.panes) total += it.width;

    boxf pane_area;
    pane_area.pos = {0, 0};
    pane_area.h = panes_area.h;

    For (world.wksp.panes) {
        it.width = it.width / total * panes_area.w;
        pane_area.w = it.width;

        int line_number_width = 0;
        auto editor = it.get_current_editor();
        if (editor != NULL)
            line_number_width = (int)log10(editor->buf.lines.len) + 1;

        boxf editor_area;
        get_tabs_and_editor_area(&pane_area, NULL, &editor_area);
        editor_area.w -= ((line_number_width * font->width) + settings.line_number_margin_left + settings.line_number_margin_right);
        new_sizes->append(editor_area.size);

        pane_area.x += pane_area.w;
    }

    auto changed = [&]() -> bool {
        if (new_sizes->len != editor_sizes.len) return true;

        for (u32 i = 0; i < new_sizes->len; i++) {
            auto a = new_sizes->at(i);
            auto b = editor_sizes[i];

            if (a.x != b.x) return true;
            if (a.y != b.y) return true;
        }
        return false;
    };

    if (!changed() && !force) return;

    editor_sizes.len = 0;
    For (*new_sizes) editor_sizes.append(&it);

    for (u32 i = 0; i < world.wksp.panes.len; i++) {
        for (auto&& editor : world.wksp.panes[i].editors) {
            editor.view.w = (i32)((editor_sizes[i].x - EDITOR_MARGIN_X) / world.font.width);
            editor.view.h = (i32)((editor_sizes[i].y - EDITOR_MARGIN_Y) / world.font.height / LINE_HEIGHT);

            if (!editor.nvim_data.is_resizing)
                if (!world.nvim.resize_editor(&editor))
                    editor.nvim_data.need_initial_resize = true;
        }
    }
}

