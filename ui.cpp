#include "imgui.h"
#include "imgui_internal.h"
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

int get_line_number_width(Editor *editor) {
    auto &buf = editor->buf;

    u32 maxval = 0;
    if (world.replace_line_numbers_with_bytecounts) {
        For (buf.bytecounts)
            if (it > maxval)
                maxval = it;
    } else {
        maxval = buf.lines.len;
    }

    return (int)log10(maxval) + 1;
}

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

ImVec4 to_imcolor(vec4f color) {
    return (ImVec4)ImColor(color.r, color.g, color.b, color.a);
}

const vec3f COLOR_WHITE = rgb_hex("#ffffff");
const vec3f COLOR_RED = rgb_hex("#ff8888");
const vec3f COLOR_LIGHT_BLUE = rgb_hex("#6699dd");
const vec3f COLOR_DARK_RED = rgb_hex("#880000");
const vec3f COLOR_DARK_YELLOW = rgb_hex("#6b6d0a");
const vec3f COLOR_DARKER_YELLOW = rgb_hex("#2b2d00");
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
                "default", "select", "new", "make", "iota",
            };

            ccstr keywords2[] = {
                "append", "cap", "close", "complex", "copy", "delete", "imag",
                "len", "make", "new", "panic", "real", "recover", "bool",
                "byte", "complex128", "complex64", "error", "float32",
                "float64", "int", "int16", "int32", "int64", "int8", "rune",
                "string", "uint", "uint16", "uint32", "uint64", "uint8",
                "uintptr",
            };

            char keyword[16] = {0};
            auto it = editor->iter(node->start);
            for (u32 i = 0; i < _countof(keyword) && it.pos != node->end; i++)
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

void UI::render_godecl(Godecl *decl) {
    auto flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (ImGui::TreeNodeEx(decl, flags, "%s", godecl_type_str(decl->type))) {
        ImGui::Text("decl_start: %s", format_pos(decl->decl_start));
        ImGui::Text("spec_start: %s", format_pos(decl->spec_start));
        ImGui::Text("name_start: %s", format_pos(decl->name_start));
        ImGui::Text("name: %s", decl->name);

        switch (decl->type) {
        case GODECL_IMPORT:
            ImGui::Text("import_path: %s", decl->import_path);
            break;
        case GODECL_VAR:
        case GODECL_CONST:
        case GODECL_TYPE:
        case GODECL_FUNC:
        case GODECL_FIELD:
        case GODECL_SHORTVAR:
            render_gotype(decl->gotype);
            break;
        }
        ImGui::TreePop();
    }
}

void UI::render_gotype(Gotype *gotype, ccstr field) {
    if (gotype == NULL) return;

    auto flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    bool is_open = false;

    if (field == NULL)
        is_open = ImGui::TreeNodeEx(gotype, flags, "%s", gotype_type_str(gotype->type));
    else
        is_open = ImGui::TreeNodeEx(gotype, flags, "%s: %s", field, gotype_type_str(gotype->type));

    if (is_open) {
        switch (gotype->type) {
        case GOTYPE_ID:
            ImGui::Text("name: %s", gotype->id_name);
            ImGui::Text("pos: %s", format_pos(gotype->id_pos));
            break;
        case GOTYPE_SEL:
            ImGui::Text("package: %s", gotype->sel_name);
            ImGui::Text("sel: %s", gotype->sel_sel);
            break;
        case GOTYPE_MAP:
            render_gotype(gotype->map_key, "key");
            render_gotype(gotype->map_value, "value");
            break;
        case GOTYPE_STRUCT:
        case GOTYPE_INTERFACE:
            {
                auto specs = gotype->type == GOTYPE_STRUCT ? gotype->struct_specs : gotype->interface_specs;
                for (u32 i = 0; i < specs->len; i++) {
                    auto it = &specs->items[i];
                    if (ImGui::TreeNodeEx(it, flags, "spec %d", i)) {
                        ImGui::Text("is_embedded: %d", it->is_embedded);
                        ImGui::Text("tag: %s", it->tag);
                        if (it->is_embedded)
                            render_gotype(it->embedded_type);
                        else
                            render_godecl(it->field);
                        ImGui::TreePop();
                    }
                }
            }
            break;
        case GOTYPE_POINTER: render_gotype(gotype->pointer_base, "base"); break;
        case GOTYPE_SLICE: render_gotype(gotype->slice_base, "base"); break;
        case GOTYPE_ARRAY: render_gotype(gotype->array_base, "base"); break;
        case GOTYPE_LAZY_INDEX: render_gotype(gotype->lazy_index_base, "base"); break;
        case GOTYPE_LAZY_CALL: render_gotype(gotype->lazy_call_base, "base"); break;
        case GOTYPE_LAZY_DEREFERENCE: render_gotype(gotype->lazy_dereference_base, "base"); break;
        case GOTYPE_LAZY_REFERENCE: render_gotype(gotype->lazy_reference_base, "base"); break;
        case GOTYPE_LAZY_ARROW: render_gotype(gotype->lazy_arrow_base, "base"); break;
        case GOTYPE_VARIADIC: render_gotype(gotype->variadic_base, "base"); break;
        case GOTYPE_ASSERTION: render_gotype(gotype->assertion_base, "base"); break;

        case GOTYPE_CHAN:
            render_gotype(gotype->chan_base, "base"); break;
            ImGui::Text("direction: %d", gotype->chan_direction);
            break;

        case GOTYPE_FUNC:
            if (gotype->func_sig.params == NULL) {
                ImGui::Text("params: NULL");
            } else if (ImGui::TreeNodeEx(&gotype->func_sig.params, flags, "params:")) {
                For (*gotype->func_sig.params)
                    render_godecl(&it);
                ImGui::TreePop();
            }

            if (gotype->func_sig.result == NULL) {
                ImGui::Text("result: NULL");
            } else if (ImGui::TreeNodeEx(&gotype->func_sig.result, flags, "result:")) {
                For (*gotype->func_sig.result)
                    render_godecl(&it);
                ImGui::TreePop();
            }

            render_gotype(gotype->func_recv);
            break;

        case GOTYPE_MULTI:
            For (*gotype->multi_types) render_gotype(it);
            break;

        case GOTYPE_RANGE:
            render_gotype(gotype->range_base, "base");
            ImGui::Text("type: %d", gotype->range_type);
            break;

        case GOTYPE_LAZY_ID:
            ImGui::Text("name: %s", gotype->lazy_id_name);
            ImGui::Text("pos: %s", format_pos(gotype->lazy_id_pos));
            break;

        case GOTYPE_LAZY_SEL:
            render_gotype(gotype->lazy_sel_base, "base");
            ImGui::Text("sel: %s", gotype->lazy_sel_sel);
            break;

        case GOTYPE_LAZY_ONE_OF_MULTI:
            render_gotype(gotype->lazy_one_of_multi_base, "base");
            ImGui::Text("index: %d", gotype->lazy_one_of_multi_index);
            break;
        }
        ImGui::TreePop();
    }
}


void UI::render_ts_cursor(TSTreeCursor *curr) {
    int last_depth = 0;
    bool last_open = false;

    auto pop = [&](int new_depth) {
        if (new_depth > last_depth) return;

        if (last_open)
            ImGui::TreePop();
        for (i32 i = 0; i < last_depth - new_depth; i++)
            ImGui::TreePop();
    };

    walk_ts_cursor(curr, false, [&](Ast_Node *node, Ts_Field_Type field_type, int depth) -> Walk_Action {
        if (node->anon && !world.wnd_ast_vis.show_anon_nodes)
            return WALK_SKIP_CHILDREN;

        // auto changed = ts_node_has_changes(node->node);

        pop(depth);
        last_depth = depth;

        auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

        if (node->child_count == 0)
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;

        auto type_str = ts_ast_type_str(node->type);
        if (type_str == NULL)
            type_str = "(unknown)";
        else
            type_str += strlen("TS_");

        if (node->anon)
            ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(128, 128, 128));

        auto field_type_str = ts_field_type_str(field_type);
        if (field_type_str == NULL)
            last_open = ImGui::TreeNodeEx(
                node->id,
                flags,
                "%s, start = %s, end = %s",
                type_str,
                format_pos(node->start),
                format_pos(node->end)
            );
        else
            last_open = ImGui::TreeNodeEx(
                node->id,
                flags,
                "(%s) %s, start = %s, end = %s",
                field_type_str + strlen("TSF_"),
                type_str,
                format_pos(node->start),
                format_pos(node->end)
            );

        if (node->anon)
            ImGui::PopStyleColor();

        if (ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered(ImGuiHoveredFlags_None)) {
            auto editor = world.get_current_editor();
            if (editor != NULL)
                editor->move_cursor(node->start);
        }

        return last_open ? WALK_CONTINUE : WALK_SKIP_CHILDREN;
    });

    pop(0);
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

    // make sure panes_area is nonzero, so that panes can be initialized
    panes_area.w = 1;
    panes_area.h = 1;
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

boxf UI::get_status_area() {
    boxf b;
    b.w = world.window_size.x;
    b.h = font->height + settings.status_padding_y * 2;
    b.x = 0;
    b.y = world.window_size.y - b.h;
    return b;
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

struct Debugger_UI {
    enum Index_Type {
        INDEX_NONE,
        INDEX_ARRAY,
        INDEX_MAP,
    };

    struct Render_Args {
        Dlv_Var *var;
        Index_Type index_type;
        union {
            int index;
            Dlv_Var *key;
        };
        Dlv_Watch *watch;
        bool is_child;
        int indent;
        int watch_index;
    };

    UI *ui;

    void init(UI *_ui) {
        ui = _ui;
    }

    void render_var(Render_Args *args) {
        SCOPED_FRAME();

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        bool open = false;
        auto var = args->var;
        auto watch = args->watch;

        {
            SCOPED_FRAME();

            int tree_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            bool leaf = true;

            if (var != NULL) {
                switch (var->kind) {
                case GO_KIND_ARRAY:
                case GO_KIND_CHAN: // ???
                case GO_KIND_FUNC: // ???
                case GO_KIND_INTERFACE:
                case GO_KIND_MAP:
                case GO_KIND_PTR:
                case GO_KIND_SLICE:
                case GO_KIND_STRUCT:
                case GO_KIND_UNSAFEPOINTER: // ???
                    leaf = false;
                    break;
                case GO_KIND_STRING:
                    if (var->incomplete())
                        leaf = false;
                    break;
                }
            }

            if (leaf)
                tree_flags |= ImGuiTreeNodeFlags_Leaf;

            if (watch != NULL && !args->is_child) {
                if (watch->editing) {
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (watch->edit_first_frame) {
                        watch->edit_first_frame = false;
                        ImGui::SetKeyboardFocusHere();
                    }
                    bool changed = ImGui::InputText(
                        our_sprintf("##newwatch%x", (int)(void*)watch),
                        watch->expr_tmp,
                        _countof(watch->expr_tmp),
                        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll
                    );
                    ImGui::PopStyleColor();

                    if (changed || ImGui::IsItemDeactivated()) {
                        if (watch->expr_tmp[0] != '\0') {
                            world.dbg.push_call(DLVC_EDIT_WATCH, [&](auto it) {
                                it->edit_watch.expression = our_strcpy(watch->expr_tmp);
                                it->edit_watch.watch_idx = args->watch_index;
                            });
                        } else {
                            world.dbg.push_call(DLVC_DELETE_WATCH, [&](auto it) {
                                it->delete_watch.watch_idx = args->watch_index;
                            });
                        }
                    }
                } else {
                    for (int i = 0; i < args->indent; i++)
                        ImGui::Indent();

                    // if (leaf) ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());

                    open = ImGui::TreeNodeEx(var, tree_flags, "%s", watch->expr) && !leaf;

                    // if (leaf) ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());

                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered(ImGuiHoveredFlags_None)) {
                        watch->editing = true;
                        watch->open_before_editing = open;
                        watch->edit_first_frame = true;
                    }

                    for (int i = 0; i < args->indent; i++)
                        ImGui::Unindent();
                }
            } else {
                ccstr var_name = NULL;
                switch (args->index_type) {
                case INDEX_NONE:
                    var_name = var->name;
                    if (var->is_shadowed)
                        var_name = our_sprintf("(%s)", var_name);
                    break;
                case INDEX_ARRAY:
                    var_name = our_sprintf("[%d]", args->index);
                    break;
                case INDEX_MAP:
                    var_name = our_sprintf("[%s]", var_value_as_string(args->key));
                    break;
                }

                for (int i = 0; i < args->indent; i++)
                    ImGui::Indent();

                if (leaf) ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());

                open = ImGui::TreeNodeEx(var, tree_flags, "%s", var_name) && !leaf;

                if (leaf) ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());

                for (int i = 0; i < args->indent; i++)
                    ImGui::Unindent();
            }
        }

        if (watch == NULL || watch->fresh) {
            ImGui::TableNextColumn();
            if (watch != NULL && watch->state == DBGWATCH_ERROR)
                ImGui::Text("<unable to read>");
            else
                ImGui::TextWrapped("%s", var_value_as_string(var));
            ImGui::TableNextColumn();
            if (watch == NULL || watch->state != DBGWATCH_ERROR) {
                if (var->kind_name == NULL || var->kind_name[0] == '\0') {
                    switch (var->kind) {
                    case GO_KIND_BOOL: ImGui::TextWrapped("bool"); break;
                    case GO_KIND_INT: ImGui::TextWrapped("int"); break;
                    case GO_KIND_INT8: ImGui::TextWrapped("int8"); break;
                    case GO_KIND_INT16: ImGui::TextWrapped("int16"); break;
                    case GO_KIND_INT32: ImGui::TextWrapped("int32"); break;
                    case GO_KIND_INT64: ImGui::TextWrapped("int64"); break;
                    case GO_KIND_UINT: ImGui::TextWrapped("uint"); break;
                    case GO_KIND_UINT8: ImGui::TextWrapped("uint8"); break;
                    case GO_KIND_UINT16: ImGui::TextWrapped("uint16"); break;
                    case GO_KIND_UINT32: ImGui::TextWrapped("uint32"); break;
                    case GO_KIND_UINT64: ImGui::TextWrapped("uint64"); break;
                    case GO_KIND_UINTPTR: ImGui::TextWrapped("uintptr"); break;
                    case GO_KIND_FLOAT32: ImGui::TextWrapped("float32"); break;
                    case GO_KIND_FLOAT64: ImGui::TextWrapped("float64"); break;
                    case GO_KIND_COMPLEX64: ImGui::TextWrapped("complex64"); break;
                    case GO_KIND_COMPLEX128: ImGui::TextWrapped("complex128"); break;
                    case GO_KIND_ARRAY: ImGui::TextWrapped("<array>"); break;
                    case GO_KIND_CHAN: ImGui::TextWrapped("<chan>"); break;
                    case GO_KIND_FUNC: ImGui::TextWrapped("<func>"); break;
                    case GO_KIND_INTERFACE: ImGui::TextWrapped("<interface>"); break;
                    case GO_KIND_MAP: ImGui::TextWrapped("<map>"); break;
                    case GO_KIND_PTR: ImGui::TextWrapped("<pointer>"); break;
                    case GO_KIND_SLICE: ImGui::TextWrapped("<slice>"); break;
                    case GO_KIND_STRING: ImGui::TextWrapped("string"); break;
                    case GO_KIND_STRUCT: ImGui::TextWrapped("<struct>"); break;
                    case GO_KIND_UNSAFEPOINTER: ImGui::TextWrapped("unsafe.Pointer"); break;
                    default: ImGui::TextWrapped("<unknown>"); break;
                    }
                } else {
                    ImGui::TextWrapped("%s", var->kind_name);
                }
            }
        } else {
            ImGui::TableNextColumn();
            if (watch != NULL && !watch->fresh) {
                // TODO: grey out
                ImGui::TextWrapped("Reading...");
            }
            ImGui::TableNextColumn();
        }

        if (open) {
            if (args->watch == NULL || args->watch->fresh) {
                if (var->children != NULL) {
                    if (var->kind == GO_KIND_MAP) {
                        for (int k = 0; k < var->children->len; k += 2) {
                            Render_Args a;
                            a.watch = args->watch;
                            a.watch_index = args->watch_index;
                            a.is_child = true;
                            a.var = &var->children->at(k+1);
                            a.index_type = INDEX_MAP;
                            a.key = &var->children->at(k);
                            a.indent = args->indent + 1;
                            render_var(&a);
                        }
                    } else {
                        bool isarr = (var->kind == GO_KIND_ARRAY || var->kind == GO_KIND_SLICE);
                        for (int k = 0; k < var->children->len; k++) {
                            Render_Args a;
                            a.watch = args->watch;
                            a.watch_index = args->watch_index;
                            a.is_child = true;
                            a.var = &var->children->at(k);
                            if (isarr) {
                                a.index_type = INDEX_ARRAY;
                                a.index = k;
                            } else {
                                a.index_type = INDEX_NONE;
                            }
                            a.indent = args->indent + 1;
                            render_var(&a);
                        }
                    }
                }

                if (var->incomplete()) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    if (ImGui::SmallButton("Load more...")) {
                        world.dbg.push_call(DLVC_VAR_LOAD_MORE, [&](Dlv_Call *it) {
                            it->var_load_more.state_id = world.dbg.state_id;
                            it->var_load_more.var = var;
                        });
                    }

                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    // TODO: render "load more" button
                }
            }
        }
    }

    ccstr var_value_as_string(Dlv_Var *var) {
        if (var->unreadable_description != NULL)
            return our_sprintf("<unreadable: %s>", var->unreadable_description);

        switch (var->kind) {
        case GO_KIND_INVALID: // i don't think this should even happen
            return "<invalid>";

        case GO_KIND_ARRAY:
        case GO_KIND_SLICE:
            return our_sprintf("0x%" PRIx64 " (Len = %d, Cap = %d)", var->address, var->len, var->cap);

        case GO_KIND_STRUCT:
        case GO_KIND_INTERFACE:
            return our_sprintf("0x%" PRIx64, var->address);

        case GO_KIND_MAP:
            return our_sprintf("0x%" PRIx64 " (Len = %d)", var->address, var->len);

        case GO_KIND_STRING:
            {
                Text_Renderer r;
                r.init();

                for (int i = 0, len = strlen(var->value); i < len; i++) {
                    auto ch = var->value[i];
                    switch (ch) {
                      case '\"': r.writestr("\\\""); break;
                      case '\'': r.writestr("\\\'"); break;
                      case '\\': r.writestr("\\\\"); break;
                      case '\a': r.writestr("\\a"); break;
                      case '\b': r.writestr("\\b"); break;
                      case '\n': r.writestr("\\n"); break;
                      case '\t': r.writestr("\\t"); break;
                      default:
                        if (iscntrl(ch))
                            r.write("\\%03o", ch);
                        else
                            r.writechar(ch);
                        break;
                    }
                }

                return our_sprintf("\"%s%s\"", r.finish(), var->incomplete() ? "..." : "");
            }

        case GO_KIND_UNSAFEPOINTER:
        case GO_KIND_CHAN:
        case GO_KIND_FUNC:
        case GO_KIND_PTR:
            return our_sprintf("0x%" PRIx64, var->address);

        default:
            return var->value;
        }
    }

    void draw() {
        world.wnd_debugger.focused = ImGui::IsWindowFocused();

        auto &dbg = world.dbg;
        auto &state = dbg.state;
        auto &wnd = world.wnd_debugger;

        {
            ImGui::SetNextWindowDockID(ui->dock_bottom_id, ImGuiCond_Once);
            ImGui::Begin("Call Stack");
            ImGui::PushFont(world.ui.im_font_mono);
            for (int i = 0; i < state.goroutines.len; i++) {
                auto &goroutine = state.goroutines[i];

                int tree_flags = ImGuiTreeNodeFlags_OpenOnArrow;

                bool is_current = (state.current_goroutine_id == goroutine.id);
                if (is_current) {
                    tree_flags |= ImGuiTreeNodeFlags_Bullet;
                    ImGui::SetNextItemOpen(true);
                    ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(255, 100, 100));
                }

                auto open = ImGui::TreeNodeEx(
                    (void*)(uptr)i,
                    tree_flags,
                    "%s (%s)",
                    goroutine.curr_func_name,
                    goroutine.breakpoint_hit ? "BREAKPOINT HIT" : "PAUSED"
                );

                if (is_current)
                    ImGui::PopStyleColor();

                if (ImGui::IsItemClicked()) {
                    world.dbg.push_call(DLVC_SET_CURRENT_GOROUTINE, [&](auto call) {
                        call->set_current_goroutine.goroutine_id = goroutine.id;
                    });
                }

                if (open) {
                    if (goroutine.fresh) {
                        for (int j = 0; j < goroutine.frames->len; j++) {
                            auto &frame = goroutine.frames->items[j];

                            int tree_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                            if (state.current_goroutine_id == goroutine.id && state.current_frame == j)
                                tree_flags |= ImGuiTreeNodeFlags_Selected;

                            ImGui::TreeNodeEx(&frame, tree_flags, "%s (%s:%d)", frame.func_name, our_basename(frame.filepath), frame.lineno);
                            if (ImGui::IsItemClicked()) {
                                world.dbg.push_call(DLVC_SET_CURRENT_FRAME, [&](auto call) {
                                    call->set_current_frame.goroutine_id = goroutine.id;
                                    call->set_current_frame.frame = j;
                                });
                            }
                        }
                    } else {
                        ImGui::Text("Loading...");
                    }

                    ImGui::TreePop();
                }
            }
            ImGui::PopFont();
            ImGui::End();
        }

        {
            ImGui::SetNextWindowDockID(ui->dock_bottom_right_id, ImGuiCond_Once);
            ImGui::Begin("Local Variables");
            ImGui::PushFont(world.ui.im_font_mono);

            auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable;
            if (ImGui::BeginTable("vars", 3, flags)) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_NoHide);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_NoHide);
                ImGui::TableHeadersRow();

                bool loading = false;
                bool done = false;
                Dlv_Frame *frame = NULL;

                do {
                    if (dbg.state_flag != DLV_STATE_PAUSED) break;
                    if (state.current_goroutine_id == -1 || state.current_frame == -1) break;

                    auto goroutine = state.goroutines.find([&](auto it) { return it->id == state.current_goroutine_id; });
                    if (goroutine == NULL) break;

                    loading = true;

                    if (!goroutine->fresh) break;
                    if (state.current_frame >= goroutine->frames->len) break;

                    frame = &goroutine->frames->items[state.current_frame];
                    if (!frame->fresh) {
                        frame = NULL;
                        break;
                    }

                    if (frame->locals != NULL) {
                        For (*frame->locals) {
                            Render_Args a = {0};
                            a.var = &it;
                            a.is_child = false;
                            a.watch = NULL;
                            a.index = INDEX_NONE;
                            render_var(&a);
                        }
                    }

                    if (frame->args != NULL) {
                        For (*frame->args) {
                            Render_Args a = {0};
                            a.var = &it;
                            a.is_child = false;
                            a.watch = NULL;
                            a.index = INDEX_NONE;
                            render_var(&a);
                        }
                    }
                } while (0);

                ImGui::EndTable();

                if (frame == NULL && loading)
                    ImGui::Text("Loading...");
                if (frame != NULL)
                    if ((frame->locals == NULL || frame->locals->len == 0) && (frame->args == NULL || frame->args->len == 0))
                        ImGui::Text("No variables to show here.");
            }

            ImGui::PopFont();
            ImGui::End();
        }

        {
            ImGui::SetNextWindowDockID(ui->dock_bottom_right_id, ImGuiCond_Once);
            ImGui::Begin("Watches");
            ImGui::PushFont(world.ui.im_font_mono);

            auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable;
            if (ImGui::BeginTable("vars", 3, flags)) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_NoHide);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_NoHide);
                ImGui::TableHeadersRow();

                for (int k = 0; k < world.dbg.watches.len; k++) {
                    auto &it = world.dbg.watches[k];
                    if (it.deleted) continue;

                    Render_Args a = {0};
                    a.var = &it.value;
                    a.is_child = false;
                    a.watch = &it;
                    a.index = INDEX_NONE;
                    a.watch_index = k;
                    render_var(&a);
                }

                {
                    // render an extra row for adding new watches

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); // name

                    ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    bool changed = ImGui::InputText(
                        "##newwatch",
                        world.wnd_debugger.new_watch_buf,
                        _countof(world.wnd_debugger.new_watch_buf),
                        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll
                    );
                    ImGui::PopStyleColor();
                    if (changed || ImGui::IsItemDeactivated())
                        if (world.wnd_debugger.new_watch_buf[0] != '\0') {
                            dbg.push_call(DLVC_CREATE_WATCH, [&](auto it) {
                                it->create_watch.expression = our_strcpy(world.wnd_debugger.new_watch_buf);
                            });
                            world.wnd_debugger.new_watch_buf[0] = '\0';
                        }

                    ImGui::TableNextColumn(); // value
                    ImGui::TableNextColumn(); // type
                }

                ImGui::EndTable();
            }

            ImGui::PopFont();
            ImGui::End();
        }

        /*
        {
            ImGui::SetNextWindowDockID(ui->dock_bottom_right_id, ImGuiCond_Once);
            ImGui::Begin("Global Variables");
            ImGui::PushFont(world.ui.im_font_mono);
            ImGui::Text("@Incomplete: global vars go here");
            ImGui::PopFont();
            ImGui::End();
        }
        */
    }
};

void open_add_file_or_folder(bool folder) {
    File_Tree_Node *node = NULL;
    auto &wnd = world.wnd_add_file_or_folder;

    auto is_root = [&]() {
        if (world.file_explorer.selection == -1) return true;

        node = get_file_tree_node_from_index(world.file_explorer.selection);
        if (node->is_directory) return false;

        node = node->parent;
        return (node->parent == NULL);
    };

    wnd.location_is_root = is_root();
    if (!wnd.location_is_root)
        strcpy_safe(wnd.location, _countof(wnd.location), file_tree_node_to_path(node));

    wnd.folder = folder;
    wnd.show = true;
}

void UI::draw_everything() {
    hover.id_last_frame = hover.id;
    hover.id = 0;
    hover.cursor = ImGuiMouseCursor_Arrow;

    ImGuiIO& io = ImGui::GetIO();

    // start rendering imgui
    ImGui::NewFrame();

    // draw the main dockspace
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();

        auto dock_size = viewport->WorkSize;
        dock_size.y -= get_status_area().h;

        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(dock_size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::SetNextWindowBgAlpha(0.0f);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2, 2));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        auto window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar
            | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        ImGui::Begin("main_dockspace", NULL, window_flags);

        ImGuiID dockspace_id = ImGui::GetID("main_dockspace");

        // set up dock layout
        if (!dock_initialized) {
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, dock_size);

            dock_main_id = dockspace_id;
            dock_sidebar_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, NULL, &dock_main_id);
            dock_bottom_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.20f, NULL, &dock_main_id);
            dock_bottom_right_id = ImGui::DockBuilderSplitNode(dock_bottom_id, ImGuiDir_Right, 0.66f, NULL, &dock_bottom_id);

            /*
            ImGui::DockBuilderDockWindow("Call Stack", dock_bottom_id);
            ImGui::DockBuilderDockWindow("Build Results", dock_bottom_id);

            ImGui::DockBuilderDockWindow("Watches", dock_bottom_right_id);
            ImGui::DockBuilderDockWindow("Local Variables", dock_bottom_right_id);
            ImGui::DockBuilderDockWindow("Global Variables", dock_bottom_right_id);

            ImGui::DockBuilderDockWindow("File Explorer", dock_sidebar_id);
            ImGui::DockBuilderDockWindow("Search Results", dock_sidebar_id);
            */

            ImGui::DockBuilderFinish(dockspace_id);
            dock_initialized = true;
        }

        auto dock_flags = ImGuiDockNodeFlags_NoDockingInCentralNode | ImGuiDockNodeFlags_PassthruCentralNode;
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dock_flags);

        {
            // get panes_area
            auto node = ImGui::DockBuilderGetCentralNode(dockspace_id);
            if (node != NULL) {
                panes_area.x = node->Pos.x;
                panes_area.y = node->Pos.y;
                panes_area.w = node->Size.x;
                panes_area.h = node->Size.y;
            } else {
                panes_area.w = 1;
                panes_area.h = 1;
            }
        }

        ImGui::End();

        ImGui::PopStyleVar(3);
    }

    // draw menubar first, so we can get its height (which we need for UI)
    if (ImGui::BeginMainMenuBar()) {
        world.ui.menubar_height = ImGui::GetWindowSize().y;

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New File", "Ctrl+N")) {
                world.get_current_pane()->open_empty_editor();
            }

            {
                auto editor = world.get_current_editor();
                bool clicked = false;

                if (editor != NULL) {
                    if (editor->is_untitled)
                        clicked = ImGui::MenuItem("Save untitled file...###save_file", "Ctrl+S");
                    else
                        clicked = ImGui::MenuItem(our_sprintf("Save %s...###save_file", our_basename(editor->filepath)), "Ctrl+S");
                } else {
                    ImGui::MenuItem("Save file...###save_file", "Ctrl+S", false, false);
                }

                if (clicked && editor != NULL)
                    editor->handle_save();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                glfwSetWindowShouldClose(world.window, true);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Find Everywhere...", "Ctrl+Shift+F")) {
                // TODO
            }
            if (ImGui::MenuItem("Find and Replace Everywhere...", "Ctrl+Shift+H")) {
                // TODO
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("File Explorer", "Ctrl+Shift+E", &world.file_explorer.show);
            ImGui::MenuItem("Error List", NULL, &world.error_list.show);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Go")) {
            if (ImGui::MenuItem("Go to File...", "Ctrl+P")) {
                if (!world.wnd_open_file.show) {
                    world.wnd_open_file.show = true;
                    init_open_file();
                }
            }
            if (ImGui::MenuItem("Go to Next Item", "Alt+]")) {
                go_to_next_error(1);
            }
            if (ImGui::MenuItem("Go to Previous Item", "Alt+[")) {
                go_to_next_error(-1);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Project")) {
            if (ImGui::MenuItem("Add New File...")) {
                open_add_file_or_folder(false);
            }

            if (ImGui::MenuItem("Add New Folder...")) {
                open_add_file_or_folder(true);
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Build", "Ctrl+Shift+B")) {
                kick_off_build();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Project Settings...")) {
                auto &wnd = world.wnd_project_settings;
                ptr0(&wnd.tmp);
                wnd.tmp.copy(&project_settings);
                wnd.show = true;
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Debug")) {
            auto can_run = [&]() {
                switch (world.dbg.state_flag) {
                case DLV_STATE_PAUSED:
                case DLV_STATE_INACTIVE:
                    return true;
                }
                return false;
            };

            if (ImGui::MenuItem("Start Debugging", "F5", false, can_run())) {
                switch (world.dbg.state_flag) {
                case DLV_STATE_PAUSED:
                    world.dbg.push_call(DLVC_CONTINUE_RUNNING);
                    break;

                case DLV_STATE_INACTIVE:
                    world.dbg.push_call(DLVC_START);
                    break;
                }
            }

            auto can_debug_test_under_cursor = [&]() -> bool {
                auto editor = world.get_current_editor();
                if (editor == NULL) return false;

                if (!editor->is_go_file) return false;
                if (editor->tree == NULL) return false;
                return true;
            };

            if (ImGui::MenuItem("Debug Test Under Cursor", "F6", false, can_debug_test_under_cursor())) {
                // TODO
            }

            if (ImGui::MenuItem("Break All", NULL, false, world.dbg.state_flag == DLV_STATE_RUNNING)) {
                world.dbg.push_call(DLVC_BREAK_ALL);
            }

            if (ImGui::MenuItem("Stop Debugging", "Shift+F5", false, world.dbg.state_flag != DLV_STATE_INACTIVE)) {
                world.dbg.push_call(DLVC_STOP);
            }

            if (ImGui::MenuItem("Step Over", "F10", false, world.dbg.state_flag == DLV_STATE_PAUSED)) {
                world.dbg.push_call(DLVC_STEP_OVER);
            }

            if (ImGui::MenuItem("Step Into", "F11", false, world.dbg.state_flag == DLV_STATE_PAUSED)) {
                world.dbg.push_call(DLVC_STEP_INTO);
            }

            if (ImGui::MenuItem("Step Out", "Shift+F11", false, world.dbg.state_flag == DLV_STATE_PAUSED)) {
                world.dbg.push_call(DLVC_STEP_OUT);
            }

            /*
            if (ImGui::MenuItem("Run to Cursor", "Shift+F10", false, world.dbg.state_flag == DLV_STATE_PAUSED)) {
                // TODO
            }
            */

            ImGui::Separator();

            if (ImGui::MenuItem("Toggle Breakpoint", "F9")) {
                auto editor = world.get_current_editor();
                if (editor != NULL) {
                    world.dbg.push_call(DLVC_TOGGLE_BREAKPOINT, [&](auto call) {
                        call->toggle_breakpoint.filename = our_strcpy(editor->filepath);
                        call->toggle_breakpoint.lineno = editor->cur.y + 1;
                    });
                }
            }

            if (ImGui::MenuItem("Delete All Breakpoints", "Shift+F9")) {
                prompt_delete_all_breakpoints();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools")) {
            if (io.KeyAlt) {
                if (ImGui::BeginMenu("Developer")) {
                    ImGui::MenuItem("ImGui demo", NULL, &world.windows_open.im_demo);
                    ImGui::MenuItem("ImGui metrics", NULL, &world.windows_open.im_metrics);
                    ImGui::MenuItem("Editor AST viewer", NULL, &world.wnd_editor_tree.show);
                    ImGui::MenuItem("Editor toplevels viewer", NULL, &world.wnd_editor_toplevels.show);
                    ImGui::MenuItem("Roll Your Own IDE Construction Set", NULL, &world.wnd_style_editor.show);
                    ImGui::MenuItem("Replace line numbers with bytecounts", NULL, &world.replace_line_numbers_with_bytecounts);
                    ImGui::EndMenu();
                }
                ImGui::Separator();
            }

            if (ImGui::MenuItem("Reload go.mod")) {
                atomic_set_flag(
                    &world.indexer.flag_lock,
                    &world.indexer.flag_handle_gomod_changed
                );
            }

            if (ImGui::MenuItem("Re-index everything")) {
                atomic_set_flag(
                    &world.indexer.flag_lock,
                    &world.indexer.flag_reindex_everything
                );
            }

            ImGui::Separator();

            /*
            if (ImGui::MenuItem("Options...")) {
                if (world.wnd_options.show) {
                    ImGui::Begin("Options");
                    ImGui::SetWindowFocus();
                    ImGui::End();
                } else {
                    world.wnd_options.show = true;
                }
            }
            */

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    /*
    if (world.wnd_options.show) {
        ImGui::Begin("Options", &world.wnd_options.show, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking);

        ImGui::SliderInt("Scroll offset", &options.scrolloff, 0, 10);

        ImGui::End();
    }
    */

    if (world.error_list.show) {
        ImGui::SetNextWindowDockID(dock_bottom_id, ImGuiCond_Once);
        ImGui::Begin("Build Results", &world.error_list.show);

        if (world.build.ready()) {
            if (world.build.errors.len == 0) {
                ImGui::TextColored(to_imcolor(rgba(COLOR_GREEN)), "Build was successful!");
            } else {
                ImGui::PushFont(world.ui.im_font_mono);

                for (int i = 0; i < world.build.errors.len; i++) {
                    auto &it = world.build.errors[i];

                    if (!it.valid) {
                        ImGui::TextColored(to_imcolor(rgba(COLOR_MEDIUM_GREY)), "%s", it.message);
                        continue;
                    }

                    auto flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth;
                    if (i == world.build.current_error)
                        flags |= ImGuiTreeNodeFlags_Selected;

                    ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());
                    ImGui::TreeNodeEx(&it, flags, "%s:%d:%d: %s", it.file, it.row, it.col, it.message);
                    ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());

                    if (ImGui::IsItemClicked()) {
                        world.build.current_error = i;
                        go_to_error(i);
                    }
                }

                ImGui::PopFont();
            }
        } else if (world.build.started) {
            ImGui::Text("Building...");
        } else {
            ImGui::Text("No build in progress.");
        }

        ImGui::End();
    }

    if (world.search_results.show) {
        ImGui::SetNextWindowDockID(dock_sidebar_id, ImGuiCond_Always);
        ImGui::Begin("Search Results", &world.search_results.show);

        For (world.search_results.results) {
            SCOPED_FRAME();

            auto flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanFullWidth;
            ImGui::TreeNodeEx(it, flags, "%s:%d:%d %s", it->filename, it->row, it->match_col+1, it->preview);
            if (ImGui::IsItemClicked()) {
                SCOPED_FRAME();
                auto path = path_join(world.current_path, it->filename);
                auto pos = new_cur2(it->match_col, it->row-1);
                world.focus_editor(path, pos);
            }

            // use following to highlight match
            /*
            len = strlen(it->preview);
            for (u32 i = 0; i < len && pos.x < sidebar_area.w; i++) {
                auto color = rgba(COLOR_WHITE);
                if (it->match_col_in_preview <= i && i < it->match_col_in_preview + it->match_len)
                    color = rgba(COLOR_LIME);
                draw_char(&pos, it->preview[i], color);
            }
            */
        }

        ImGui::End();
    }

    if (world.wnd_add_file_or_folder.show) {
        auto &wnd = world.wnd_add_file_or_folder;

        auto label = our_sprintf(
            "Add %s to %s",
            wnd.folder ? "folder" : "file",
            wnd.location_is_root ? "workspace root" : wnd.location
        );

        ImGui::SetNextWindowSize(ImVec2(450, -1));
        ImGui::Begin(label, &world.wnd_add_file_or_folder.show, ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::Text("Name:");

        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();

        ImGui::InputText("##add_file", wnd.name, IM_ARRAYSIZE(wnd.name));

        if (ImGui::Button("Add")) {
            world.wnd_add_file_or_folder.show = false;

            if (strlen(wnd.name) > 0) {
                auto dest = wnd.location_is_root ? world.current_path : path_join(world.current_path, wnd.location);
                auto path = path_join(dest, wnd.name);

                if (wnd.folder) {
                    CreateDirectoryA(path, NULL);
                } else {
                    // need to share, or else we have race condition
                    // with fsevent handler in
                    // Go_Indexer::background_thread() trying to read
                    auto h = CreateFileA(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
                    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
                }

                world.fill_file_tree();
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::End();
    }


    if (world.file_explorer.show) {
        ImGui::SetNextWindowDockID(dock_sidebar_id, ImGuiCond_Once);
        ImGui::Begin("File Explorer", &world.file_explorer.show);

        if (ImGui::Button("Add file")) {
            open_add_file_or_folder(false);
        }

        ImGui::SameLine();

        if (ImGui::Button("Add folder")) {
            open_add_file_or_folder(true);
        }

        ImGui::SameLine();

        if (ImGui::Button("Refresh")) {
            // TODO: probably make this async task
            world.fill_file_tree();
        }

        ImGui::Separator();

        // draw files area
        {
            auto stack = alloc_list<File_Tree_Node*>();
            for (auto child = world.file_tree->children; child != NULL; child = child->next)
                stack->append(child);

            for (u32 i = 0; stack->len > 0; i++) {
                auto it = *stack->last();
                stack->len--;

                auto flags = ImGuiTreeNodeFlags_DefaultOpen
                    | ImGuiTreeNodeFlags_OpenOnArrow
                    | ImGuiTreeNodeFlags_OpenOnDoubleClick
                    // | ImGuiTreeNodeFlags_SpanAvailWidth
                    | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                if (!it->is_directory)
                    flags |= ImGuiTreeNodeFlags_Leaf; // | ImGuiTreeNodeFlags_Bullet;
                if (world.file_explorer.selection == i)
                    flags |= ImGuiTreeNodeFlags_Selected;

                // it->is_directory

                for (u32 j = 0; j < it->depth; j++) ImGui::Indent();

                bool open = false;
                if (ImGui::TreeNodeEx(it, flags, "%s%s", it->name, it->is_directory ? "/" : ""))
                    if (it->is_directory)
                        open = true;

                for (u32 j = 0; j < it->depth; j++) ImGui::Unindent();

                if (ImGui::IsItemClicked()) {
                    world.file_explorer.selection = i;
                    if (!it->is_directory) {
                        SCOPED_FRAME();
                        auto rel_path = file_tree_node_to_path(it);
                        auto full_path = path_join(world.current_path, rel_path);
                        world.focus_editor(full_path);
                    }
                }

                if (it->is_directory && open) {
                    SCOPED_FRAME();
                    auto children = alloc_list<File_Tree_Node*>();
                    for (auto curr = it->children; curr != NULL; curr = curr->next)
                        children->append(curr);
                    for (i32 i = children->len-1; i >= 0; i--)
                        stack->append(children->at(i));
                }
            }
        }

        ImGui::End();
    }

    if (world.wnd_project_settings.show) {
        auto &wnd = world.wnd_project_settings;

        ImGui::Begin("Project Settings", &wnd.show, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking);

        ImGui::Text("Build command");
        ImGui::InputText("##build_command", wnd.tmp.build_command, _countof(wnd.tmp.build_command));

        ImGui::Text("Debug binary path");
        ImGui::InputText("##debug_binary_path", wnd.tmp.debug_binary_path, _countof(wnd.tmp.debug_binary_path));

        ImGui::NewLine();

        if (ImGui::Button("Save")) {
            project_settings.copy(&wnd.tmp);
            project_settings.write(path_join(world.current_path, ".ideproj"));
            world.wnd_project_settings.show = false;
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel")) {
            world.wnd_project_settings.show = false;
        }

        ImGui::End();
    }

    if (world.windows_open.im_demo)
        ImGui::ShowDemoWindow(&world.windows_open.im_demo);

    if (world.windows_open.im_metrics)
        ImGui::ShowMetricsWindow(&world.windows_open.im_metrics);

    if (world.wnd_open_file.show) {
        auto& wnd = world.wnd_open_file;
        ImGui::Begin("Open File", &world.wnd_open_file.show, ImGuiWindowFlags_AlwaysAutoResize);

        wnd.focused = ImGui::IsWindowFocused();

        ImGui::Text("Search for file:");

        ImGui::PushFont(world.ui.im_font_mono);

        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
        } else if (!wnd.first_open_focus_twice_done) {
            wnd.first_open_focus_twice_done = true;
            ImGui::SetKeyboardFocusHere();
        }

        ImGui::InputText("##search_for_file", wnd.query, _countof(wnd.query));

        if (ImGui::IsItemEdited())
            filter_files();

        for (u32 i = 0; i < wnd.filtered_results->len; i++) {
            auto it = wnd.filepaths->at(wnd.filtered_results->at(i));
            if (i == wnd.selection)
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "%s", it);
            else
                ImGui::Text("%s", it);
        }

        ImGui::PopFont();

        ImGui::End();
    }

    if (world.dbg.state_flag != DLV_STATE_INACTIVE) {
        Debugger_UI dui;
        dui.init(this);
        dui.draw();
    }

    if (world.windows_open.search_and_replace) {
        auto& wnd = world.wnd_search_and_replace;

        ImGui::Begin("Search and Replace", &world.windows_open.search_and_replace, ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::Text("Search for:");
        ImGui::InputText("##find", wnd.find_str, _countof(wnd.find_str));
        ImGui::Text("Replace with:");
        ImGui::InputText("##replace", wnd.replace_str, _countof(wnd.replace_str));

        ImGui::Separator();

        ImGui::Checkbox("Case-sensitive", &wnd.case_sensitive);
        ImGui::SameLine();
        ImGui::Checkbox("Use regular expression", &wnd.use_regex);

        ImGui::Separator();

        if (ImGui::Button("Search for All")) {
            world.search_results.mem.cleanup();
            run_proc_the_normal_way(
                &world.jobs.search.proc,
                our_sprintf("ag --ackmate %s %s \"%s\"", wnd.use_regex ? "" : "-Q", wnd.case_sensitive ? "-s" : "", wnd.find_str)
            );
            world.jobs.flag_search = true;
        }

        ImGui::SameLine();

        if (ImGui::Button("Replace All")) {
            // TODO: some kind of permanent settings system
            // so we can save things like "don't show this popup again"
            ImGui::OpenPopup("Really replace?");
        }

        if (world.jobs.search_and_replace.signal_done) {
            world.jobs.search_and_replace.signal_done = false;
            ImGui::OpenPopup("Done");
        }

        ImGui::SetNextWindowSize(ImVec2(450, -1));
        if (ImGui::BeginPopupModal("Really replace?", NULL, ImGuiWindowFlags_NoResize)) {
            ImGui::TextWrapped("Pressing this button will automatically, irreversibly perform the text replacement you requested.");
            ImGui::TextWrapped("If you make a mistake, we trust your version control system will save you.");
            ImGui::TextWrapped("To preview your changes before replacing, click Find All first.");
            ImGui::TextWrapped("We won't show this warning again.");
            ImGui::NewLine();
            if (ImGui::Button("Ok, replace all")) {
                run_proc_the_normal_way(
                    &world.jobs.search_and_replace.proc,
                    our_sprintf("ag -g \"\" | xargs sed -i \"s/%s/%s/g\"", wnd.find_str, wnd.replace_str)
                );
                world.jobs.flag_search_and_replace = true;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::SetNextWindowSize(ImVec2(450, -1));
        if (ImGui::BeginPopupModal("Done", NULL, ImGuiWindowFlags_NoResize)) {
            ImGui::TextWrapped("Replace operation has completed.");
            ImGui::NewLine();
            if (ImGui::Button("Ok")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::End();
    }

    if (world.wnd_style_editor.show) {
        ImGui::Begin("Style Editor", &world.wnd_style_editor.show, ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::SliderFloat("status_padding_x", &settings.status_padding_x, 0.0, 20.0f, "%.0f");
        ImGui::SliderFloat("status_padding_y", &settings.status_padding_y, 0.0, 20.0f, "%.0f");
        ImGui::SliderFloat("line_number_margin_left", &settings.line_number_margin_left, 0.0, 20.0f, "%.0f");
        ImGui::SliderFloat("line_number_margin_right", &settings.line_number_margin_right, 0.0, 20.0f, "%.0f");
        ImGui::SliderFloat("autocomplete_menu_padding", &settings.autocomplete_menu_padding, 0.0, 20.0f, "%.0f");
        ImGui::SliderFloat("autocomplete_item_padding_x", &settings.autocomplete_item_padding_x, 0.0, 20.0f, "%.0f");
        ImGui::SliderFloat("autocomplete_item_padding_y", &settings.autocomplete_item_padding_y, 0.0, 20.0f, "%.0f");
        ImGui::SliderFloat("tabs_offset", &settings.tabs_offset, 0.0, 20.0f, "%.0f");

        ImGui::End();
    }

    do {
        auto editor = world.get_current_editor();
        if (editor == NULL) break;

        auto tree = editor->tree;
        if (tree == NULL) break;

        if (world.wnd_editor_tree.show) {
            ImGui::Begin("AST", &world.wnd_editor_tree.show, 0);
            ImGui::Checkbox("show anon?", &world.wnd_ast_vis.show_anon_nodes);
            ts_tree_cursor_reset(&editor->cursor, ts_tree_root_node(tree));
            render_ts_cursor(&editor->cursor);
            ImGui::End();
        }

        if (world.wnd_editor_toplevels.show) {
            ImGui::Begin("Toplevels", &world.wnd_editor_toplevels.show, 0);

            List<Godecl> decls;
            decls.init();

            Parser_It it = {0};
            it.init(&editor->buf);

            Ast_Node node = {0};
            node.init(ts_tree_root_node(tree), &it);

            FOR_NODE_CHILDREN (&node) {
                switch (it->type) {
                case TS_VAR_DECLARATION:
                case TS_CONST_DECLARATION:
                case TS_FUNCTION_DECLARATION:
                case TS_METHOD_DECLARATION:
                case TS_TYPE_DECLARATION:
                case TS_SHORT_VAR_DECLARATION:
                    decls.len = 0;
                    world.indexer.node_to_decls(it, &decls, NULL);
                    For (decls) render_godecl(&it);
                    break;
                }
            }

            ImGui::End();
        }
    } while (0);

    world.ui.mouse_captured_by_imgui = io.WantCaptureMouse;
    world.ui.keyboard_captured_by_imgui = io.WantCaptureKeyboard;

    ImGui::Render();

    {
        // prepare opengl for drawing shit
        glViewport(0, 0, world.display_size.x, world.display_size.y);
        glUseProgram(world.ui.program);
        glBindVertexArray(world.ui.vao); // bind my vertex array & buffers
        glBindBuffer(GL_ARRAY_BUFFER, world.ui.vbo);
        verts.init(LIST_FIXED, 6 * 128, alloc_array(Vert, 6 * 128));
    }

    boxf status_area = get_status_area();
    // boxf panes_area = get_panes_area();

    boxf pane_area;
    pane_area.pos = panes_area.pos;

    int editor_index;

    // Draw panes.
    draw_rect(panes_area, rgba(COLOR_BG));
    for (u32 current_pane = 0; current_pane < world.panes.len; current_pane++) {
        auto &pane = world.panes[current_pane];
        auto is_pane_selected = (current_pane == world.current_pane);

        pane_area.w = pane.width;
        pane_area.h = panes_area.h;

        boxf tabs_area, editor_area;
        get_tabs_and_editor_area(&pane_area, &tabs_area, &editor_area, pane.editors.len > 0);

        if (pane.editors.len > 0)
            draw_rect(tabs_area, rgba(is_pane_selected ? COLOR_MEDIUM_GREY : COLOR_DARK_GREY));
        draw_rect(editor_area, rgba(COLOR_BG));

        vec2 tab_padding = { 15, 5 };

        boxf tab;
        tab.pos = tabs_area.pos + new_vec2(5, tabs_area.h - tab_padding.y * 2 - font->height);
        tab.x -= pane.tabs_offset;

        i32 tab_to_remove = -1;

        // draw tabs
        u32 tab_id = 0;
        for (auto&& editor : pane.editors) {
            defer { editor_index++; };

            SCOPED_FRAME();

            bool is_selected = (tab_id == pane.current_editor);

            ccstr label = NULL;

            if (editor.is_untitled) {
                label = "<untitled>";
            } else {
                auto wksp_path = make_path(world.current_path);
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
            }

            label = our_sprintf("%s%s", label, editor.buf.dirty ? "*" : "");

            auto text_width = get_text_width(label);

            tab.w = text_width + tab_padding.x * 2;
            tab.h = font->height + tab_padding.y * 2;

            // Now `tab` is filled out, and I can do my logic to make sure it's visible on screen
            if (tab_id == pane.current_editor) {
                auto margin = tab_id == 0 ? 5 : settings.tabs_offset;
                if (tab.x < tabs_area.x + margin)
                    pane.tabs_offset -= (tabs_area.x + margin - tab.x);

                margin = (tab_id == pane.editors.len - 1) ? 5 : settings.tabs_offset;
                if (tab.x + tab.w > tabs_area.x + tabs_area.w - margin)
                    pane.tabs_offset += ((tab.x + tab.w) - (tabs_area.x + tabs_area.w - margin));
            }

            auto is_hovered = test_hover(tab, HOVERID_TABS + editor_index);

            vec3f tab_color = COLOR_MEDIUM_DARK_GREY;
            if (is_selected)
                tab_color = COLOR_BG;
            else if (is_hovered)
                tab_color = COLOR_DARK_GREY;

            draw_rounded_rect(tab, rgba(tab_color), 4, ROUND_TL | ROUND_TR);
            draw_string(tab.pos + tab_padding, label, rgba(is_selected ? COLOR_WHITE : COLOR_LIGHT_GREY));

            auto mouse_flags = get_mouse_flags(tab);
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
            vec2f cur_pos = editor_area.pos + new_vec2f(settings.editor_margin_x, settings.editor_margin_y);
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
                    // if (node->child_count != 0) return WALK_CONTINUE;

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

            if (editor->is_nvim_ready()) {
                auto &buf = editor->buf;
                auto &view = editor->view;

                vec2f actual_cursor_position = { -1, -1 };
                vec2f actual_parameter_hint_start = { -1, -1 };

                auto draw_background = [&](bool insert_cursor, vec4f color) {
                    boxf b;
                    b.x = cur_pos.x;
                    b.y = cur_pos.y - font->offset_y;
                    b.h = (float)font->height;
                    if (insert_cursor)
                        b.w = 2;
                    else
                        b.w = (float)font->width;
                    draw_rect(b, color);
                };

                auto draw_cursor = [&]() {
                    actual_cursor_position = cur_pos;    // save position where cursor is drawn for later use
                    bool insert_cursor = (world.nvim.mode == VI_INSERT && is_pane_selected && !world.nvim.exiting_insert_mode);

                    if (current_pane == world.current_pane)
                        draw_background(insert_cursor, rgba(COLOR_LIME));
                    else
                        draw_background(insert_cursor, rgba(COLOR_LIME, 0.3));
                };

                List<Client_Breakpoint> breakpoints_for_this_editor;

                {
                    u32 len = 0;
                    For (world.dbg.breakpoints)
                        if (streq(it.file, editor->filepath))
                            len++;

                    alloc_list(&breakpoints_for_this_editor, len);
                    For (world.dbg.breakpoints) {
                        if (are_filepaths_equal(it.file, editor->filepath)) {
                            auto p = breakpoints_for_this_editor.append();
                            memcpy(p, &it, sizeof(it));
                        }
                    }
                }

                if (buf.lines.len == 0) draw_cursor();

                auto &hint = editor->parameter_hint;

                int next_hl = (highlights.len > 0 ? 0 : -1);

                auto goroutines_hit = alloc_list<Dlv_Goroutine*>();
                u32 current_goroutine_id = 0;
                Dlv_Goroutine *current_goroutine = NULL;
                Dlv_Frame *current_frame = NULL;
                bool is_current_goroutine_on_current_file = false;

                if (world.dbg.state_flag == DLV_STATE_PAUSED) {
                    current_goroutine_id = world.dbg.state.current_goroutine_id;
                    For (world.dbg.state.goroutines) {
                        if (it.id == current_goroutine_id) {
                            current_goroutine = &it;
                            if (current_goroutine->fresh) {
                                current_frame = &current_goroutine->frames->at(world.dbg.state.current_frame);
                                if (are_filepaths_equal(editor->filepath, current_frame->filepath))
                                    is_current_goroutine_on_current_file = true;
                            } else {
                                if (are_filepaths_equal(editor->filepath, current_goroutine->curr_file))
                                    is_current_goroutine_on_current_file = true;
                            }
                        } else if (it.breakpoint_hit) {
                            if (are_filepaths_equal(editor->filepath, editor->filepath))
                                goroutines_hit->append(&it);
                        }
                    }
                }

                auto relative_y = 0;
                for (u32 y = view.y; y < view.y + view.h; y++, relative_y++) {
                    if (y >= buf.lines.len) break;

                    auto line = &buf.lines[y];

                    enum {
                        BREAKPOINT_NONE,
                        BREAKPOINT_CURRENT_GOROUTINE,
                        BREAKPOINT_OTHER_GOROUTINE,
                        BREAKPOINT_ACTIVE,
                        BREAKPOINT_INACTIVE,
                    };

                    auto find_breakpoint_stopped_at_this_line = [&]() -> int {
                        if (world.dbg.state_flag == DLV_STATE_PAUSED) {
                            if (is_current_goroutine_on_current_file) {
                                if (current_frame != NULL) {
                                    if (current_frame->lineno == y + 1)
                                        return BREAKPOINT_CURRENT_GOROUTINE;
                                } else if (current_goroutine->curr_line == y + 1)
                                    return BREAKPOINT_CURRENT_GOROUTINE;
                            }

                            For (*goroutines_hit)
                                if (it->curr_line == y + 1)
                                    return BREAKPOINT_OTHER_GOROUTINE;
                        }

                        For (breakpoints_for_this_editor) {
                            if (it.line == y + 1) {
                                bool inactive = (it.pending || world.dbg.state_flag == DLV_STATE_INACTIVE);
                                return inactive ?  BREAKPOINT_ACTIVE : BREAKPOINT_INACTIVE;
                            }
                        }

                        return BREAKPOINT_NONE;
                    };

                    boxf line_box = {
                        cur_pos.x,
                        cur_pos.y - font->offset_y,
                        (float)editor_area.w,
                        (float)font->height - 1,
                    };

                    auto bptype = find_breakpoint_stopped_at_this_line();
                    if (bptype == BREAKPOINT_CURRENT_GOROUTINE)
                        draw_rect(line_box, rgba(COLOR_DARK_YELLOW));
                    else if (bptype == BREAKPOINT_OTHER_GOROUTINE)
                        draw_rect(line_box, rgba(COLOR_DARKER_YELLOW));
                    else if (bptype == BREAKPOINT_ACTIVE)
                        draw_rect(line_box, rgba(COLOR_DARK_RED, 1.0));
                    else if (bptype == BREAKPOINT_INACTIVE)
                        draw_rect(line_box, rgba(COLOR_DARK_RED, 0.5));

                    auto line_number_width = get_line_number_width(editor);

                    auto is_cursor_match = [&](cur2 cur, i32 x, i32 y) -> bool {
                        if (cur.y != y) return false;
                    };

                    {
                        cur_pos.x += settings.line_number_margin_left;
                        ccstr line_number_str = NULL;
                        if (world.replace_line_numbers_with_bytecounts)
                            line_number_str = our_sprintf("%*d", line_number_width, buf.bytecounts[y]);
                        else
                            line_number_str = our_sprintf("%*d", line_number_width, y + 1);
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
                            if (world.nvim.mode != VI_INSERT && current_pane == world.current_pane)
                                text_color = COLOR_BLACK;
                        } else if (world.nvim.mode != VI_INSERT) {
                            auto topline = editor->nvim_data.grid_topline;
                            if (topline <= y && y < topline + NVIM_DEFAULT_HEIGHT) {
                                auto hl = editor->highlights[y - topline][vx];
                                switch (hl) {
                                case HL_INCSEARCH:
                                    draw_background(false, rgba("#553333"));
                                    break;
                                case HL_SEARCH:
                                    draw_background(false, rgba("#994444"));
                                    break;
                                case HL_VISUAL:
                                    draw_background(false, rgba("#335533"));
                                    break;
                                }
                            }
                        }

                        uchar uch = line->at(x);
                        if (uch == '\t') {
                            auto chars = options.tabsize - ((vx - view.x) % options.tabsize);
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

                    cur_pos.x = editor_area.x + settings.editor_margin_x;
                    cur_pos.y += font->height * settings.line_height;
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
                            auto y1 = actual_cursor_position.y - font->offset_y - settings.autocomplete_menu_margin_y;
                            auto y2 = actual_cursor_position.y - font->offset_y + (font->height * settings.line_height) + settings.autocomplete_menu_margin_y;

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
                    bg.w = font->width * strlen(hint.help_text) + settings.parameter_hint_padding_x * 2;
                    bg.h = font->height + settings.parameter_hint_padding_y * 2;
                    bg.x = min(actual_parameter_hint_start.x, world.window_size.x - bg.w);
                    bg.y = min(actual_parameter_hint_start.y - font->offset_y - bg.h - settings.parameter_hint_margin_y, world.window_size.y - bg.h);

                    draw_bordered_rect_outer(bg, rgba(COLOR_DARK_GREY), rgba(COLOR_WHITE), 1, 4);

                    auto text_pos = bg.pos;
                    text_pos.x += settings.parameter_hint_padding_x;
                    text_pos.y += settings.parameter_hint_padding_y;

                    draw_string(text_pos, hint.help_text, rgba(COLOR_WHITE));
                } while (0);
            }
        }

        pane_area.x += pane_area.w;
    }

    {
        // Draw pane resizers.

        float offset = 0;

        for (u32 i = 0; i < world.panes.len - 1; i++) {
            offset += world.panes[i].width;

            boxf b;
            b.w = 2;
            b.h = panes_area.h;
            b.x = panes_area.x + offset - 1;
            b.y = panes_area.y;

            boxf hitbox = b;
            hitbox.x -= 4;
            hitbox.w += 8;

            if (test_hover(hitbox, HOVERID_PANE_RESIZERS + i, ImGuiMouseCursor_ResizeEW)) {
                draw_rect(b, rgba(COLOR_WHITE));
                world.resizing_pane = world.ui.mouse_down[GLFW_MOUSE_BUTTON_LEFT] ? i : -1;
            } else {
                draw_rect(b, rgba(COLOR_DARK_GREY));
            }
        }
    }

    {
        draw_rect(status_area, rgba("#252525"));

        boxf str_area_l;
        str_area_l.pos = status_area.pos;
        str_area_l.h = status_area.h;

        boxf str_area_r;
        str_area_r.pos = status_area.pos;
        str_area_r.x += status_area.w;
        str_area_r.h = status_area.h;

        enum {
            LEFT = 0,
            RIGHT = 1,
        };

        auto draw_status_piece = [&](int dir, ccstr s, vec4f bgcolor, vec4f fgcolor) {
            auto str_area = dir == RIGHT ? &str_area_r : &str_area_l;

            str_area->w = font->width * strlen(s) + (settings.status_padding_x * 2);
            if (dir == RIGHT)
                str_area->x -= str_area->w;

            draw_rect(*str_area, bgcolor);

            boxf text_area = *str_area;
            text_area.x += settings.status_padding_x;
            text_area.y += settings.status_padding_y;
            text_area.w -= (settings.status_padding_x * 2);
            text_area.h -= (settings.status_padding_y * 2);
            draw_string(text_area.pos, s, fgcolor);

            if (dir == LEFT)
                str_area->x += str_area->w;
        };

        if (world.use_nvim) {
            auto should_show_cmd = [&]() -> bool {
                auto &nv = world.nvim;
                if (nv.mode != VI_CMDLINE) return false;
                if (nv.cmdline.content.len > 0) return true;
                if (nv.cmdline.firstc.len > 0) return true;
                if (nv.cmdline.prompt.len > 0) return true;
                return false;
            };

            if (should_show_cmd()) {
                auto &cmd = world.nvim.cmdline;

                auto get_title = [&]() -> ccstr {
                    if (cmd.prompt.len > 1)
                        return cmd.prompt.items;
                    if (streq(cmd.firstc.items, "/"))
                        return "Forward search: ";
                    if (streq(cmd.firstc.items, "?"))
                        return "Backward search: ";
                    if (streq(cmd.firstc.items, ":"))
                        return "Command: ";
                    return cmd.firstc.items;
                };

                auto command = our_sprintf("%s%s", get_title(), cmd.content.items);
                draw_status_piece(LEFT, command, rgba("#888833"), rgba("#cccc88"));
            } else {
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
                draw_status_piece(LEFT, mode_str, rgba("#666666"), rgba("aaaaaa"));
            }
        }

        switch (world.dbg.state_flag) {
        case DLV_STATE_PAUSED:
            draw_status_piece(LEFT, "PAUSED", rgba("#800000"), rgba(COLOR_WHITE));
            break;
        case DLV_STATE_STARTING:
            draw_status_piece(LEFT, "STARTING", rgba("#888822"), rgba(COLOR_WHITE));
            break;
        case DLV_STATE_RUNNING:
            draw_status_piece(LEFT, "RUNNING", rgba("#008000"), rgba(COLOR_WHITE));
            break;
        }

        if (world.indexer.ready)
            draw_status_piece(RIGHT, "INDEX READY", rgba("#008800"), rgba("#cceecc"));
        else
            draw_status_piece(RIGHT, "INDEXING IN PROGRESS", rgba("#880000"), rgba("#eecccc"));

        auto curr_editor = world.get_current_editor();
        if (curr_editor != NULL) {
            auto cur = curr_editor->cur;
            draw_status_piece(RIGHT, our_sprintf("%d,%d", cur.y+1, cur.x+1), rgba(COLOR_WHITE, 0.0), rgba("#aaaaaa"));
        }
    }
}

void UI::end_frame() {
    flush_verts();

    ImGui::EndFrame();

    {
        // draw imgui buffers
        ImDrawData* draw_data = ImGui::GetDrawData();
        draw_data->ScaleClipRects(ImVec2(world.display_scale.x, world.display_scale.y));

        glViewport(0, 0, world.display_size.x, world.display_size.y);
        glUseProgram(world.ui.im_program);
        glBindVertexArray(world.ui.im_vao);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glEnable(GL_SCISSOR_TEST);

        for (i32 i = 0; i < draw_data->CmdListsCount; i++) {
            const ImDrawList* cmd_list = draw_data->CmdLists[i];
            const ImDrawIdx* offset = 0;

            glBindBuffer(GL_ARRAY_BUFFER, world.ui.im_vbo);
            glBufferData(GL_ARRAY_BUFFER, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert), cmd_list->VtxBuffer.Data, GL_STREAM_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, world.ui.im_vebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx), cmd_list->IdxBuffer.Data, GL_STREAM_DRAW);

            i32 elem_size = sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;

            for (i32 j = 0; j < cmd_list->CmdBuffer.Size; j++) {
                const ImDrawCmd* cmd = &cmd_list->CmdBuffer[j];
                glScissor(cmd->ClipRect.x, (world.display_size.y - cmd->ClipRect.w), (cmd->ClipRect.z - cmd->ClipRect.x), (cmd->ClipRect.w - cmd->ClipRect.y));
                glDrawElements(GL_TRIANGLES, cmd->ElemCount, elem_size, offset);
                offset += cmd->ElemCount;
            }
        }
    }

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

void UI::get_tabs_and_editor_area(boxf* pane_area, boxf* ptabs_area, boxf* peditor_area, bool has_tabs) {
    boxf tabs_area, editor_area;

    if (has_tabs) {
        tabs_area.pos = pane_area->pos;
        tabs_area.w = pane_area->w;
        tabs_area.h = 30; // ???
    }

    editor_area.pos = pane_area->pos;
    if (has_tabs)
        editor_area.y += tabs_area.h;
    editor_area.w = pane_area->w;
    editor_area.h = pane_area->h;
    if (has_tabs)
        editor_area.h -= tabs_area.h;

    if (has_tabs)
        if (ptabs_area != NULL)
            memcpy(ptabs_area, &tabs_area, sizeof(boxf));
    if (peditor_area != NULL)
        memcpy(peditor_area, &editor_area, sizeof(boxf));
}

void UI::recalculate_view_sizes(bool force) {
    // boxf panes_area = get_panes_area();
    auto new_sizes = alloc_list<vec2f>();

    float total = 0;
    For (world.panes) total += it.width;

    boxf pane_area;
    pane_area.pos = {0, 0};
    pane_area.h = panes_area.h;

    For (world.panes) {
        it.width = it.width / total * panes_area.w;
        pane_area.w = it.width;

        int line_number_width = 0;
        auto editor = it.get_current_editor();
        if (editor != NULL)
            line_number_width = get_line_number_width(editor);

        boxf editor_area;
        get_tabs_and_editor_area(&pane_area, NULL, &editor_area, it.editors.len > 0);
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

    for (u32 i = 0; i < world.panes.len; i++) {
        for (auto&& editor : world.panes[i].editors) {
            editor.view.w = (i32)((editor_sizes[i].x - settings.editor_margin_x) / world.font.width);
            editor.view.h = (i32)((editor_sizes[i].y - settings.editor_margin_y) / world.font.height / settings.line_height);
        }
    }
}

const u64 WINDOWS_RESIZE_FROM_EDGES_FEEDBACK_TIMER = 40000000;

bool UI::test_hover(boxf area, int id, ImGuiMouseCursor cursor) {
    if (!(get_mouse_flags(area) & MOUSE_HOVER))
        return false;

    hover.id = id;
    hover.cursor = cursor;

    auto now = current_time_in_nanoseconds();

    if (id != hover.id_last_frame) {
        hover.start_time = now;
        hover.ready = false;
    }

    if (now - hover.start_time > WINDOWS_RESIZE_FROM_EDGES_FEEDBACK_TIMER)
        hover.ready = true;

    return hover.ready;
}
