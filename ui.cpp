#include "imgui.h"
#include "imgui_internal.h"
#include "ui.hpp"
#include "common.hpp"
#include "world.hpp"
#include "go.hpp"
#include "unicode.hpp"
#include "settings.hpp"
#include "IconsFontAwesome5.h"
#include "IconsMaterialDesign.h"

#define _USE_MATH_DEFINES // what the fuck is this lol
#include <math.h>
#include "tree_sitter_crap.hpp"

#include <GLFW/glfw3.h>
#include <inttypes.h>

void open_ft_node(FT_Node *it);

UI ui;

namespace ImGui {
    bool OurBeginPopupContextItem(const char* str_id = NULL, ImGuiPopupFlags popup_flags = 1) {
        ImGuiWindow* window = GImGui->CurrentWindow;
        if (window->SkipItems)
            return false;
        ImGuiID id = str_id ? window->GetID(str_id) : window->DC.LastItemId; // If user hasn't passed an ID, we can use the LastItemID. Using LastItemID as a Popup ID won't conflict!
        IM_ASSERT(id != 0);                                                  // You cannot pass a NULL str_id if the last item has no identifier (e.g. a Text() item)
        int mouse_button = (popup_flags & ImGuiPopupFlags_MouseButtonMask_);
        if (IsMouseReleased(mouse_button) && IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup))
            OpenPopupEx(id, popup_flags);
        return BeginPopupEx(id, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings);
    }
}

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

    return max(4, (int)log10(maxval) + 1);
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
const vec3f COLOR_RED = rgb_hex("#ff5555");
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
    switch (node->type()) {
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
            auto len = node->end_byte() - node->start_byte();
            if (len >= 16) break;

            ccstr keywords[] = {
                "package", "import", "const", "var", "func",
                "type", "struct", "interface",
                "fallthrough", "break", "continue", "goto", "return",
                "go", "defer", "if", "else",
                "for", "range", "switch", "case",
                "default", "select", "new", "make", "iota",
            };

            ccstr builtins[] = {
                "map", "chan", // technically keywords, but look better here

                "append", "cap", "close", "complex", "copy", "delete", "imag",
                "len", "make", "new", "panic", "real", "recover", "bool",
                "byte", "complex128", "complex64", "error", "float32",
                "float64", "int", "int16", "int32", "int64", "int8", "rune",
                "string", "uint", "uint16", "uint32", "uint64", "uint8",
                "uintptr",
            };

            char token[16] = {0};
            auto it = editor->iter(node->start());
            for (u32 i = 0; i < _countof(token) && it.pos != node->end(); i++)
                token[i] = it.next();

            For (keywords) {
                if (streq(it, token)) {
                    *out = COLOR_THEME_1;
                    return true;
                }
            }

            For (builtins) {
                if (streq(it, token)) {
                    *out = COLOR_THEME_5;
                    return true;
                }
            }
        }
        break;
    }

    return false;
}

bool UI::imgui_is_window_focusing(bool *b) {
    auto old_focus = *b;
    *b = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
    return !old_focus && *b;
}

void UI::render_godecl(Godecl *decl) {
    auto flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (ImGui::TreeNodeEx(decl, flags, "%s", godecl_type_str(decl->type))) {
        ImGui::Text("decl_start: %s", format_cur(decl->decl_start));
        ImGui::Text("spec_start: %s", format_cur(decl->spec_start));
        ImGui::Text("name_start: %s", format_cur(decl->name_start));
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
            ImGui::Text("pos: %s", format_cur(gotype->id_pos));
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
                        ImGui::Text("tag: %s", it->tag);
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
            ImGui::Text("pos: %s", format_cur(gotype->lazy_id_pos));
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


void UI::render_ts_cursor(TSTreeCursor *curr, cur2 open_cur) {
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
        if (node->anon() && !world.wnd_editor_tree.show_anon_nodes)
            return WALK_SKIP_CHILDREN;

        if (node->type() == TS_COMMENT && !world.wnd_editor_tree.show_comments)
            return WALK_SKIP_CHILDREN;

        // auto changed = ts_node_has_changes(node->node);

        pop(depth);
        last_depth = depth;

        auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

        if (node->child_count() == 0)
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;

        auto type_str = ts_ast_type_str(node->type());
        if (type_str == NULL)
            type_str = "(unknown)";
        else
            type_str += strlen("TS_");

        if (node->anon())
            ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(128, 128, 128));
        if (node->type() == TS_COMMENT)
            ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(100, 130, 100));

        if (open_cur.x != -1) {
            bool open = node->start() <= open_cur && open_cur < node->end();
            ImGui::SetNextItemOpen(open, ImGuiCond_Always);
        }

        auto field_type_str = ts_field_type_str(field_type);
        if (field_type_str == NULL)
            last_open = ImGui::TreeNodeEx(
                node->id(),
                flags,
                "%s, start = %s, end = %s",
                type_str,
                format_cur(node->start()),
                format_cur(node->end())
            );
        else
            last_open = ImGui::TreeNodeEx(
                node->id(),
                flags,
                "(%s) %s, start = %s, end = %s",
                field_type_str + strlen("TSF_"),
                type_str,
                format_cur(node->start()),
                format_cur(node->end())
            );

        if (node->anon())
            ImGui::PopStyleColor();
        if (node->type() == TS_COMMENT)
            ImGui::PopStyleColor();

        if (ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered(ImGuiHoveredFlags_None)) {
            auto editor = world.get_current_editor();
            if (editor != NULL)
                editor->move_cursor(node->start());
        }

        return last_open ? WALK_CONTINUE : WALK_SKIP_CHILDREN;
    });

    pop(0);
}


void UI::init() {
    ptr0(this);
    font = &world.font;
    editor_sizes.init(LIST_FIXED, _countof(_editor_sizes), _editor_sizes);

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

    boxf bs;
    memcpy(&bs, &b, sizeof(boxf));
    bs.x *= world.display_scale.x;
    bs.y *= world.display_scale.y;
    bs.w *= world.display_scale.x;
    bs.h *= world.display_scale.y;
    glScissor(bs.x, world.display_size.y - (bs.y + bs.h), bs.w, bs.h);

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
        vec2f zeroval; ptr0(&zeroval);

        float increment = (end_rad - start_rad) / max(3, (int)(radius / 5));
        for (float angle = start_rad; angle < end_rad; angle += increment) {
            auto ang1 = angle;
            auto ang2 = angle + increment;

            vec2f v1 = {center.x + radius * cos(ang1), center.y - radius * sin(ang1)};
            vec2f v2 = {center.x + radius * cos(ang2), center.y - radius * sin(ang2)};

            draw_triangle(center, v1, v2, zeroval, zeroval, zeroval, color, DRAW_SOLID);
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
void UI::draw_char(vec2f* pos, uchar ch, vec4f color) {
    stbtt_aligned_quad q;
    stbtt_GetPackedQuad(
        font->char_info, font->tex_size, font->tex_size,
        ch == 0xfffd ? _countof(font->char_info) - 1 : ch - ' ',
        &pos->x, &pos->y, &q, 0
    );

    if (q.x1 > q.x0) {
        boxf box = { q.x0, q.y0, q.x1 - q.x0, q.y1 - q.y0 };
        boxf uv = { q.s0, q.t0, q.s1 - q.s0, q.t1 - q.t0 };
        draw_quad(box, uv, color, DRAW_FONT_MASK);
    }

    /*
        boxf b;
        b.pos = *pos;
        b.w = font->width;
        b.h = font->height;
        b.y -= font->offset_y;
        draw_rect(b, color);
        pos->x += font->width;
    */
}

vec2f UI::draw_string(vec2f pos, ccstr s, vec4f color) {
    pos.y += font->offset_y;

    Cstr_To_Ustr conv;
    conv.init();

    for (u32 i = 0, len = strlen(s); i < len; i++) {
        bool found;
        auto uch = conv.feed(s[i], &found);
        if (found) {
            if (uch < 0x7f)
                draw_char(&pos, uch, color);
            else
                draw_char(&pos, '?', color);
        }
    }

    pos.y -= font->offset_y;
    return pos;
}

float UI::get_text_width(ccstr s) {
    float x = 0, y = 0;
    stbtt_aligned_quad q;
    Cstr_To_Ustr conv;
    bool found;

    conv.init();
    for (u32 i = 0, len = strlen(s); i < len; i++) {
        auto uch = conv.feed(s[i], &found);
        if (found) {
            if (uch > 0x7f) uch = '?';
            stbtt_GetPackedQuad(font->char_info, font->tex_size, font->tex_size, uch - ' ', &x, &y, &q, 0);
        }
    }

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

        if (ImGui::IsMouseClicked(0))
            ret |= MOUSE_CLICKED;
        if (ImGui::IsMouseClicked(1))
            ret |= MOUSE_RCLICKED;
        if (ImGui::IsMouseClicked(2))
            ret |= MOUSE_MCLICKED;
    }
    return ret;
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
                        our_sprintf("##newwatch%x", (iptr)(void*)watch),
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

                // if (leaf) ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());

                open = ImGui::TreeNodeEx(var, tree_flags, "%s", var_name) && !leaf;

                // if (leaf) ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());

                for (int i = 0; i < args->indent; i++)
                    ImGui::Unindent();
            }
        }

        if (var->incomplete()) {
            for (int i = 0; i < args->indent; i++)
                ImGui::Indent();
            ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());

            ImGui::PushFont(world.ui.im_font_ui);
            bool clicked = ImGui::SmallButton("Load more...");
            ImGui::PopFont();

            if (clicked) {
                world.dbg.push_call(DLVC_VAR_LOAD_MORE, [&](Dlv_Call *it) {
                    it->var_load_more.state_id = world.dbg.state_id;
                    it->var_load_more.var = var;
                });
            }

            for (int i = 0; i < args->indent; i++)
                ImGui::Unindent();
            ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());
        }

        if (watch == NULL || watch->fresh) {
            ImGui::TableNextColumn();

            ccstr value_label = NULL;
            ccstr underlying_value = NULL;

            auto muted = (watch != NULL && watch->state == DBGWATCH_ERROR);
            if (muted) {
                value_label ="<unable to read>";
                underlying_value = value_label;
            } else {
                value_label = var_value_as_string(var);
                if (var->kind == GO_KIND_STRING) {
                    auto len = strlen(value_label) - 2;
                    auto buf = alloc_array(char, len+1);
                    strncpy(buf, value_label+1, len);
                    buf[len] = '\0';
                    underlying_value = buf;
                } else {
                    underlying_value = value_label;
                }
            }

            ImGuiStyle &style = ImGui::GetStyle();

            if (muted) ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
            ImGui::TextWrapped("%s", value_label);
            if (muted) ImGui::PopStyleColor();

            if (ImGui::OurBeginPopupContextItem("dbg_copyvalue")) {
                if (ImGui::Selectable("Copy")) {
                    glfwSetClipboardString(world.window, underlying_value);
                }
                ImGui::EndPopup();
            }

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

        if (open && (watch == NULL || (watch->fresh && watch->state != DBGWATCH_ERROR))) {
            if (var->children != NULL) {
                if (var->kind == GO_KIND_MAP) {
                    for (int k = 0; k < var->children->len; k += 2) {
                        Render_Args a;
                        a.watch = watch;
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
                        a.watch = watch;
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
            return our_sprintf("\"%s%s\"", var->value, var->incomplete() ? "..." : "");

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

            if (world.dbg.state_flag == DLV_STATE_PAUSED && !world.dbg.exiting) {
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
            }

            ImGui::PopFont();
            ImGui::End();
        }

        {
            ImGui::SetNextWindowDockID(ui->dock_bottom_right_id, ImGuiCond_Once);

            {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                ImGui::Begin("Local Variables");
                ImGui::PopStyleVar();
            }

            ImGui::PushFont(world.ui.im_font_mono);

            if (world.dbg.state_flag == DLV_STATE_PAUSED) {
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
                                Render_Args a; ptr0(&a);
                                a.var = &it;
                                a.is_child = false;
                                a.watch = NULL;
                                a.index = INDEX_NONE;
                                render_var(&a);
                            }
                        }

                        if (frame->args != NULL) {
                            For (*frame->args) {
                                Render_Args a; ptr0(&a);
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
            }

            ImGui::PopFont();
            ImGui::End();
        }

        {
            ImGui::SetNextWindowDockID(ui->dock_bottom_right_id, ImGuiCond_Once);

            {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                ImGui::Begin("Watches");
                ImGui::PopStyleVar();
            }

            ImGui::PushFont(world.ui.im_font_mono);

            if (world.dbg.state_flag == DLV_STATE_PAUSED) {
                auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable;
                if (ImGui::BeginTable("vars", 3, flags)) {
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_NoHide);
                    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_NoHide);
                    ImGui::TableHeadersRow();

                    for (int k = 0; k < world.dbg.watches.len; k++) {
                        auto &it = world.dbg.watches[k];
                        if (it.deleted) continue;

                        Render_Args a; ptr0(&a);
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

void open_rename(FT_Node *target) {
    auto &wnd = world.wnd_rename_file_or_folder;

    wnd.show = true;
    wnd.target = target;

    strcpy_safe(wnd.location, _countof(wnd.location), world.ft_node_to_path(target));
    strcpy_safe(wnd.name, _countof(wnd.name), target->name);
}

void open_add_file_or_folder(bool folder, FT_Node *dest = NULL) {
    if (dest == NULL) dest = world.file_explorer.selection;

    FT_Node *node = NULL;
    auto &wnd = world.wnd_add_file_or_folder;

    auto is_root = [&]() {
        node = dest;

        if (node == NULL) return true;
        if (node->is_directory) return false;

        node = node->parent;
        return (node->parent == NULL);
    };

    wnd.location_is_root = is_root();
    if (!wnd.location_is_root)
        strcpy_safe(wnd.location, _countof(wnd.location), world.ft_node_to_path(node));

    wnd.dest = node;
    wnd.folder = folder;
    wnd.show = true;
    wnd.name[0] = '\0';
}

void UI::imgui_small_newline() {
    ImGui::Dummy(ImVec2(0.0f, ImGui::GetFrameHeightWithSpacing() * 1/4));
}

bool UI::imgui_input_text_full(ccstr label, char *buf, int count, int flags) {
    ImGui::PushItemWidth(-1);
    {
        ImGui::PushFont(world.ui.im_font_ui);
        ImGui::Text("%s", label);
        ImGui::PopFont();
    }
    auto ret = ImGui::InputText(our_sprintf("###%s", label), buf, count, flags);
    ImGui::PopItemWidth();

    return ret;
}

#define imgui_input_text_full_fixbuf(x, y) imgui_input_text_full(x, y, _countof(y))

void UI::open_project_settings() {
    auto &wnd = world.wnd_project_settings;
    if (wnd.show) return;

    ptr0(&wnd.tmp);
    wnd.tmp.copy(&project_settings);
    wnd.show = true;
}

void UI::imgui_with_disabled(bool disable, fn<void()> f) {
    if (disable) {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }

    f();

    if (disable) {
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
    }
}

bool UI::imgui_special_key_pressed(int key) {
    return imgui_key_pressed(ImGui::GetKeyIndex(key));
}

bool UI::imgui_key_pressed(int key) {
    return ImGui::IsKeyPressed(tolower(key)) || ImGui::IsKeyPressed(toupper(key));
}

u32 UI::imgui_get_keymods() {
    auto &io = ImGui::GetIO();

    u32 ret = 0;
    if (io.KeySuper) ret |= OUR_MOD_CMD;
    if (io.KeyCtrl) ret |= OUR_MOD_CTRL;
    if (io.KeyShift) ret |= OUR_MOD_SHIFT;
    if (io.KeyAlt) ret |= OUR_MOD_ALT;
    return ret;
}

void open_ft_node(FT_Node *it) {
    SCOPED_FRAME();
    auto rel_path = world.ft_node_to_path(it);
    auto full_path = path_join(world.current_path, rel_path);
    if (world.focus_editor(full_path) != NULL)
        ImGui::SetWindowFocus(NULL);
}

void UI::draw_everything() {
    verts.len = 0;

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

        auto window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar
            | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2, 2));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            /**/
            ImGui::Begin("main_dockspace", NULL, window_flags);
            /**/
            ImGui::PopStyleVar(3);
        }

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
    }

    if (ImGui::BeginMainMenuBar()) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(7, 5));

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
            if (ImGui::MenuItem("Search...", "Ctrl+Shift+F")) {
                world.wnd_search_and_replace.show = true;
                world.wnd_search_and_replace.replace = false;
            }
            if (ImGui::MenuItem("Search and Replace...", "Ctrl+Shift+H")) {
                world.wnd_search_and_replace.show = true;
                world.wnd_search_and_replace.replace = true;
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
                if (!world.wnd_goto_file.show) {
                    world.wnd_goto_file.show = true;
                    init_goto_file();
                }
            }
            if (ImGui::MenuItem("Go to Symbol...", "Ctrl+P")) {
                if (!world.wnd_goto_symbol.show) {
                    world.wnd_goto_symbol.show = true;
                    init_goto_symbol();
                }
            }
            if (ImGui::MenuItem("Go to Next Item", "Alt+]")) {
                goto_next_error(1);
            }
            if (ImGui::MenuItem("Go to Previous Item", "Alt+[")) {
                goto_next_error(-1);
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

            if (ImGui::MenuItem("Project Settings...")) {
                open_project_settings();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Build")) {
            if (ImGui::MenuItem("Build", "Ctrl+Shift+B")) {
                world.error_list.show = true;
                save_all_unsaved_files();
                kick_off_build();
            }

            ImGui::Separator();

            if (ImGui::BeginMenu("Select Active Build Profile..."))  {
                for (int i = 0; i < project_settings.build_profiles_len; i++) {
                    auto &it = project_settings.build_profiles[i];
                    if (ImGui::MenuItem(it.label, NULL, project_settings.active_build_profile == i, true)) {
                        project_settings.active_build_profile = i;
                        project_settings.write(path_join(world.current_path, ".cp95proj"));
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::MenuItem("Build Profiles...")) {
                open_project_settings();
                world.wnd_project_settings.focus_build_profiles = true;
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Format")) {
            auto editor = world.get_current_editor();

            if (ImGui::MenuItem("Format File", NULL, false, editor != NULL)) {
                if (editor != NULL)
                    editor->format_on_save(GH_FMT_GOIMPORTS);
            }

            if (ImGui::MenuItem("Format File and Organize Imports", NULL, false, editor != NULL)) {
                if (editor != NULL)
                    editor->format_on_save(GH_FMT_GOIMPORTS_WITH_AUTOIMPORT);
            }

            if (ImGui::MenuItem("Format Selection", NULL, false, editor != NULL)) {
                // how do we even get the visual selection?
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Refactor")) {
            auto can_rename_id_under_cursor = [&]() -> bool {
#if 0
                auto editor = world.get_current_editor();
                if (editor == NULL) return false;
                if (!editor->is_go_file) return false;
                if (editor->tree == NULL) return false;

                Parser_It it;
                it.init(&editor->buf);
                auto root_node = new_ast_node(ts_tree_root_node(editor->tree), &it);

                find_nodes_containing_pos(root_node, editor->cur, true, [&](auto it) -> Walk_Action {
                    switch (it->type()) {
                    case TS_TYPE_DECLARATION:
                        break;
                    case TS_PARAMETER_LIST:
                        break;
                    case TS_SHORT_VAR_DECLARATION:
                        break;
                    case TS_CONST_DECLARATION:
                        break;
                    case TS_VAR_DECLARATION:
                        break;
                    case TS_RANGE_CLAUSE:
                        break;
                    default:
                        return WALK_CONTINUE;
                    }

                    if (it->type() == TS_FUNCTION_DECLARATION) {
                        auto name = it->field(TSF_NAME);
                        if (!name->null)
                            ret = str_starts_with(name->string(), "Test");
                    }

                    return WALK_ABORT;
                });
#endif

                return true; // ???
            };

            if (ImGui::MenuItem("Rename...", NULL, false, can_rename_id_under_cursor())) {
                // ???
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Debug")) {
            if (world.dbg.state_flag == DLV_STATE_PAUSED) {
                if (ImGui::MenuItem("Continue", "F5", false)) {
                    world.dbg.push_call(DLVC_CONTINUE_RUNNING);
                }
            } else {
                if (ImGui::MenuItem("Start Debugging", "F5", false, world.dbg.state_flag == DLV_STATE_INACTIVE)) {
                    save_all_unsaved_files();
                    world.dbg.push_call(DLVC_START);
                }
            }

            auto can_debug_test_under_cursor = [&]() -> bool {
                if (world.dbg.state_flag != DLV_STATE_INACTIVE) return false;

                auto editor = world.get_current_editor();
                if (editor == NULL) return false;
                if (!editor->is_go_file) return false;
                if (!str_ends_with(editor->filepath, "_test.go")) return false;
                if (!path_contains_in_subtree(world.current_path, editor->filepath)) return false;
                if (editor->buf.tree == NULL) return false;

                bool ret = false;

                Parser_It it;
                it.init(&editor->buf);
                auto root_node = new_ast_node(ts_tree_root_node(editor->buf.tree), &it);

                find_nodes_containing_pos(root_node, editor->cur, true, [&](auto it) -> Walk_Action {
                    if (it->type() == TS_SOURCE_FILE)
                        return WALK_CONTINUE;

                    if (it->type() == TS_FUNCTION_DECLARATION) {
                        auto name = it->field(TSF_NAME);
                        if (!name->null)
                            ret = str_starts_with(name->string(), "Test");
                    }

                    return WALK_ABORT;
                });

                return ret;
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

            ImGui::Separator();

            if (ImGui::BeginMenu("Select Active Debug Profile..."))  {
                for (int i = 1; i < project_settings.debug_profiles_len; i++) {
                    auto &it = project_settings.debug_profiles[i];
                    if (ImGui::MenuItem(it.label, NULL, project_settings.active_debug_profile == i, true)) {
                        project_settings.active_debug_profile = i;
                        project_settings.write(path_join(world.current_path, ".cp95proj"));
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::MenuItem("Debug Profiles...")) {
                open_project_settings();
                world.wnd_project_settings.focus_debug_profiles = true;
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools")) {
            if (ImGui::MenuItem("Rescan Index")) {
                world.indexer.message_queue.add([&](auto msg) {
                    msg->type = GOMSG_RESCAN_INDEX;
                });
            }

            if (ImGui::MenuItem("Obliterate and Recreate Index")) {
                world.indexer.message_queue.add([&](auto msg) {
                    msg->type = GOMSG_OBLITERATE_AND_RECREATE_INDEX;
                });
            }

            if (io.KeyAlt) {
                ImGui::Separator();
                ImGui::MenuItem("ImGui Demo", NULL, &world.windows_open.im_demo);
                ImGui::MenuItem("ImGui Metrics", NULL, &world.windows_open.im_metrics);
                ImGui::MenuItem("Editor AST Viewer", NULL, &world.wnd_editor_tree.show);
                ImGui::MenuItem("Editor Toplevels Viewer", NULL, &world.wnd_editor_toplevels.show);
                ImGui::MenuItem("Roll Your Own IDE Construction Set", NULL, &world.wnd_style_editor.show);
                ImGui::MenuItem("Mark Edit Viewer", NULL, &world.wnd_mark_edit_viewer.show);
                ImGui::MenuItem("Replace Line Numbers with Bytecounts", NULL, &world.replace_line_numbers_with_bytecounts);
                ImGui::MenuItem("Disable Framerate Cap", NULL, &world.turn_off_framerate_cap);

                if (ImGui::MenuItem("Cleanup unused memory")) {
                    world.indexer.message_queue.add([&](auto msg) {
                        msg->type = GOMSG_CLEANUP_UNUSED_MEMORY;
                    });
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Show message box (OK)")) {
                    tell_user("Sorry, that keyfile was invalid. Please select another one.", "License key required");
                }

                if (ImGui::MenuItem("Show message box (Yes/No)")) {
                    auto res = ask_user_yes_no(
                        "Are you sure you want to delete all breakpoints?",
                        NULL,
                        "Delete",
                        "Don't Delete"
                    );
                    print("%d", res);
                }

                if (ImGui::MenuItem("Show message box (Yes/No/Cancel)")) {
                    auto res = ask_user_yes_no_cancel(
                        "Do you want to save your changes to main.go?",
                        "Your changes will be lost if you don't.",
                        "Save",
                        "Don't Save"
                    );
                    print("%d", res);
                }
            }

            /*
            ImGui::Separator();
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

        ImGui::PopStyleVar(2);
        ImGui::EndMainMenuBar();
    }

    /*
    if (world.wnd_options.show) {
        ImGui::Begin("Options", &world.wnd_options.show, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking);
        ImGui::SliderInt("Scroll offset", &options.scrolloff, 0, 10);
        ImGui::End();
    }
    */

    if (world.wnd_index_log.show) {
        ImGui::SetNextWindowDockID(dock_bottom_id, ImGuiCond_Once);
        ImGui::Begin("Index Log", &world.wnd_index_log.show);

        ImGui::PushFont(world.ui.im_font_mono);

        For (world.wnd_index_log.lines) {
            ImGui::Text("%s", it);
        }

        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);

        ImGui::PopFont();

        ImGui::End();
    }

    if (world.error_list.show) {
        ImGui::SetNextWindowDockID(dock_bottom_id, ImGuiCond_Once);
        ImGui::Begin("Build Results", &world.error_list.show);

        static Build_Error *menu_current_error = NULL;

        if (ImGui::IsWindowAppearing()) {
            ImGui::SetWindowFocus(NULL);
        }

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

                    /*
                    auto flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth;
                    if (i == world.build.current_error)
                        flags |= ImGuiTreeNodeFlags_Selected;

                    ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());
                    ImGui::TreeNodeEx(&it, flags, "%s:%d:%d: %s", it.file, it.row, it.col, it.message);
                    ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());
                    */

                    if (i == world.build.scroll_to) {
                        ImGui::SetScrollHereY();
                        world.build.scroll_to = -1;
                    }

                    auto label = our_sprintf("%s:%d:%d: %s", it.file, it.row, it.col, it.message);
                    auto wrap_width = ImGui::GetContentRegionAvail().x;
                    auto text_size = ImVec2(wrap_width, ImGui::CalcTextSize(label, NULL, false, wrap_width).y);
                    auto pos = ImGui::GetCursorScreenPos();

                    bool clicked = ImGui::Selectable(our_sprintf("##hidden_%d", i), i == world.build.current_error, 0, text_size);
                    ImGui::GetWindowDrawList()->AddText(NULL, 0.0f, pos, ImGui::GetColorU32(ImGuiCol_Text), label, NULL, wrap_width);

                    if (ImGui::OurBeginPopupContextItem()) {
                        if (ImGui::Selectable("Copy")) {
                            glfwSetClipboardString(world.window, label);
                        }
                        ImGui::EndPopup();
                    }

                    /*
                    if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)) {
                        menu_current_error = &it;
                        ImGui::OpenPopup("error list menu");
                    }
                    */

                    if (clicked) {
                        world.build.current_error = i;
                        goto_error(i);
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

    if (world.wnd_rename_file_or_folder.show) {
        auto &wnd = world.wnd_rename_file_or_folder;

        ImGui::SetNextWindowSize(ImVec2(300, -1));
        ImGui::SetNextWindowPos(ImVec2(world.window_size.x/2, world.window_size.y/2), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

        auto label = our_sprintf("Rename %s", wnd.target->is_directory ? "folder" : "file");
        ImGui::Begin(our_sprintf("%s###add_file_or_folder", label), &wnd.show, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking);

        ImGui::Text("Renaming");

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(140, 194, 248)));
        ImGui::PushFont(world.ui.im_font_mono);
        ImGui::Text("%s", wnd.location);
        ImGui::PopStyleColor();
        ImGui::PopFont();

        imgui_small_newline();

        auto is_focusing = imgui_is_window_focusing(&wnd.focused);
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
        } else if (!wnd.first_open_focus_twice_done) {
            wnd.first_open_focus_twice_done = true;
            ImGui::SetKeyboardFocusHere();
        } else if (is_focusing) {
            ImGui::SetKeyboardFocusHere();
        }

        // close the window when we unfocus
        if (!wnd.focused) wnd.show = false;

        ImGui::PushFont(world.ui.im_font_mono);
        bool entered = imgui_input_text_full("Name", wnd.name, _countof(wnd.name), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopFont();

        if (entered) {
            wnd.show = false;

            if (strlen(wnd.name) > 0) {
                auto oldpath = path_join(world.current_path, wnd.location);
                auto newpath = path_join(our_dirname(oldpath), wnd.name);

                // TODO: handle wnd.name having a slash in it
                GHRenameFileOrDirectory((char*)oldpath, (char*)newpath);
                reload_file_subtree(our_dirname(wnd.location));
            }
        }

        ImGui::End();
    }

    if (world.wnd_add_file_or_folder.show) {
        auto &wnd = world.wnd_add_file_or_folder;

        ImGui::SetNextWindowSize(ImVec2(300, -1));
        ImGui::SetNextWindowPos(ImVec2(world.window_size.x/2, world.window_size.y/2), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

        auto label = our_sprintf("Add %s", wnd.folder ? "Folder" : "File");
        ImGui::Begin(our_sprintf("%s###add_file_or_folder", label), &wnd.show, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking);

        ImGui::Text("Destination");

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(140, 194, 248)));
        ImGui::PushFont(world.ui.im_font_mono);

        if (wnd.location_is_root)
            ImGui::Text("(workspace root)");
        else
            ImGui::Text("%s", wnd.location);

        ImGui::PopStyleColor();
        ImGui::PopFont();

        imgui_small_newline();

        auto is_focusing = imgui_is_window_focusing(&wnd.focused);
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
        } else if (!wnd.first_open_focus_twice_done) {
            wnd.first_open_focus_twice_done = true;
            ImGui::SetKeyboardFocusHere();
        } else if (is_focusing) {
            ImGui::SetKeyboardFocusHere();
        }

        // close the window when we unfocus
        if (!wnd.focused) wnd.show = false;

        ImGui::PushFont(world.ui.im_font_mono);
        bool entered = imgui_input_text_full("Name", wnd.name, _countof(wnd.name), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopFont();

        if (entered) {
            wnd.show = false;

            if (strlen(wnd.name) > 0) {
                auto dest = wnd.location_is_root ? world.current_path : path_join(world.current_path, wnd.location);
                auto path = path_join(dest, wnd.name);

                auto ok = wnd.folder ? create_directory(path) : touch_file(path);
                if (ok) {
                    auto dest = wnd.location_is_root ? world.file_tree : wnd.dest;
                    world.add_ft_node(dest, [&](auto child) {
                        child->is_directory = wnd.folder;
                        child->name = our_strcpy(wnd.name);
                    });
                }
            }
        }

        ImGui::End();
    }

    if (world.file_explorer.show) {
        auto &wnd = world.file_explorer;

        auto old_item_spacing = ImGui::GetStyle().ItemSpacing;

        ImGui::SetNextWindowDockID(dock_sidebar_id, ImGuiCond_Once);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("File Explorer", &wnd.show);
        ImGui::PopStyleVar();

        auto is_focusing = imgui_is_window_focusing(&wnd.focused);

        auto imgui_icon_button = [&](ccstr icon) -> bool {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
            auto ret = ImGui::Button(icon);
            ImGui::PopStyleVar();
            return ret;
        };

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15, 0.15, 0.15, 1.0));
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));

            ImGuiStyle &style = ImGui::GetStyle();
            float child_height = ImGui::GetTextLineHeight() + (style.FramePadding.y * 2.0f) + (style.WindowPadding.y * 2.0f);
            ImGui::BeginChild("child2", ImVec2(0, child_height), false, ImGuiWindowFlags_AlwaysUseWindowPadding);

            ImGui::PopStyleVar();

            if (imgui_icon_button(ICON_MD_NOTE_ADD)) {
                open_add_file_or_folder(false);
            }

            ImGui::SameLine(0.0, 4.0f);

            if (imgui_icon_button(ICON_MD_CREATE_NEW_FOLDER)) {
                open_add_file_or_folder(true);
            }

            ImGui::SameLine(0.0, 4.0f);

            if (imgui_icon_button(ICON_MD_REFRESH)) {
                // TODO: probably make this async task
                world.fill_file_tree();
            }

            ImGui::EndChild();
        }
        ImGui::PopStyleColor();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
        ImGui::BeginChild("child3", ImVec2(0,0), true);
        ImGui::PopStyleVar();
        {
            SCOPED_FRAME();

            fn<void(FT_Node*)> draw = [&](auto it) {
                for (u32 j = 0; j < it->depth; j++) ImGui::Indent();

                ccstr icon = NULL;
                if (it->is_directory) {
                    icon = ICON_MD_FOLDER;
                } else {
                    if (str_ends_with(it->name, ".go"))
                        icon = ICON_MD_CODE;
                    else
                        icon = ICON_MD_DESCRIPTION;
                }

                {
                    bool mute = !it->is_directory && !str_ends_with(it->name, ".go");
                    ImGuiStyle &style = ImGui::GetStyle();

                    if (wnd.selection != it) {
                        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2, 0.2, 0.2, 1.0));
                        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.2, 0.2, 0.2, 1.0));
                        if (mute)
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5, 0.5, 0.5, 1.0));
                    }
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));

                    ccstr label = NULL;
                    if (it->is_directory)
                        label = our_sprintf("%s %s %s", icon, it->name, it->open ? ICON_MD_EXPAND_MORE : ICON_MD_CHEVRON_RIGHT);
                    else
                        label = our_sprintf("%s %s", icon, it->name);

                    if (it == wnd.scroll_to) {
                        ImGui::SetScrollHereY();
                        wnd.scroll_to = NULL;
                    }

                    ImGui::PushID(it);
                    ImGui::Selectable(label, wnd.selection == it, ImGuiSelectableFlags_AllowDoubleClick);
                    ImGui::PopID();

                    ImGui::PopStyleVar();
                    if (wnd.selection != it) {
                        ImGui::PopStyleColor(2);
                        if (mute)
                            ImGui::PopStyleColor();
                    }
                }

                if (ImGui::OurBeginPopupContextItem(NULL)) {
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, old_item_spacing);

                    // wnd.selection = it;

                    if (!it->is_directory) {
                        if (ImGui::Selectable("Open")) {
                            open_ft_node(it);
                        }
                    }

                    if (ImGui::Selectable("Rename")) {
                        open_rename(it);
                    }

                    if (ImGui::Selectable("Delete")) {
                        world.delete_ft_node(it);
                    }

                    ImGui::Separator();

                    if (ImGui::Selectable("Add new file...")) {
                        open_add_file_or_folder(false, it);
                    }

                    if (ImGui::Selectable("Add new folder...")) {
                        open_add_file_or_folder(true, it);
                    }

                    /*
                    if (ImGui::Selectable("Cut")) {
                        wnd.last_file_cut = it;
                        wnd.last_file_copied = NULL;
                    }

                    if (ImGui::Selectable("Copy")) {
                        wnd.last_file_copied = it;
                        wnd.last_file_cut = NULL;
                    }

                    if (ImGui::Selectable("Paste")) {
                        FT_Node *src = wnd.last_file_copied;
                        bool cut = false;
                        if (src == NULL) {
                            src = wnd.last_file_cut;
                            cut = true;
                        }

                        if (src != NULL) {
                            auto dest = it;
                            if (!dest->is_directory) dest = dest->parent;

                            ccstr srcpath = ft_node_to_path(src);
                            ccstr destpath = NULL;

                            // if we're copying to the same place
                            if (src->parent == dest)
                                destpath = path_join(ft_node_to_path(dest), our_sprintf("copy of %s", src->name));
                            else
                                destpath = path_join(ft_node_to_path(dest), src->name);

                            srcpath = path_join(world.current_path, srcpath);
                            destpath = path_join(world.current_path, destpath);

                            if (src->is_directory) {

                                // ???
                                ///
                            } else {
                                if (cut)
                                    move_file_atomically(sc)
                                else
                                    copy_file(srcpath, destpath, true);
                            }
                        }
                    }
                    */

                    ImGui::Separator();

                    if (ImGui::Selectable("Copy relative path")) {
                        SCOPED_FRAME();
                        auto rel_path = world.ft_node_to_path(it);
                        glfwSetClipboardString(world.window, rel_path);
                    }

                    if (ImGui::Selectable("Copy absolute path")) {
                        SCOPED_FRAME();
                        auto rel_path = world.ft_node_to_path(it);
                        auto full_path = path_join(world.current_path, rel_path);
                        glfwSetClipboardString(world.window, full_path);
                    }

                    ImGui::PopStyleVar();
                    ImGui::EndPopup();
                }

                for (u32 j = 0; j < it->depth; j++) ImGui::Unindent();

                if (ImGui::IsItemClicked(0) || ImGui::IsItemClicked(1)) {
                    wnd.selection = it;
                }

                if (ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered(ImGuiHoveredFlags_None)) {
                    if (it->is_directory)
                        it->open ^= 1;
                    else
                        open_ft_node(it);
                }

                if (it->is_directory && it->open)
                    for (auto child = it->children; child != NULL; child = child->next)
                        draw(child);
            };

            if (ImGui::BeginPopup("file_explorer_rename_file")) {
                ImGui::Text("shit goes here");
                ImGui::EndPopup();
            }

            for (auto child = world.file_tree->children; child != NULL; child = child->next)
                draw(child);
        }
        ImGui::EndChild();
        // ImGui::PopStyleVar();

        if (wnd.focused) {
            auto mods = imgui_get_keymods();
            switch (mods) {
            case OUR_MOD_NONE:
                if (imgui_special_key_pressed(ImGuiKey_DownArrow) || imgui_key_pressed('j')) {
                    auto getnext = [&]() -> FT_Node * {
                        auto curr = wnd.selection;
                        if (curr == NULL) return world.file_tree->children;

                        if (curr->children != NULL && curr->open)
                            return curr->children;
                        if (curr->next != NULL)
                            return curr->next;

                        while (curr->parent != NULL) {
                            curr = curr->parent;
                            if (curr->next != NULL)
                                return curr->next;
                        }

                        return NULL;
                    };

                    auto next = getnext();
                    if (next != NULL)
                        wnd.selection = next;
                }
                if (imgui_special_key_pressed(ImGuiKey_LeftArrow) || imgui_key_pressed('h')) {
                    auto curr = wnd.selection;
                    if (curr != NULL)
                        if (curr->is_directory)
                            curr->open = false;
                }
                if (imgui_special_key_pressed(ImGuiKey_RightArrow) || imgui_key_pressed('l')) {
                    auto curr = wnd.selection;
                    if (curr != NULL)
                        if (curr->is_directory)
                            curr->open = true;
                }
                if (imgui_special_key_pressed(ImGuiKey_UpArrow) || imgui_key_pressed('k')) {
                    auto curr = wnd.selection;
                    if (curr != NULL) {
                        if (curr->prev != NULL) {
                            curr = curr->prev;
                            // as long as curr has children, keep grabbing the last child
                            while (curr->is_directory && curr->open && curr->children != NULL) {
                                curr = curr->children;
                                while (curr->next != NULL)
                                    curr = curr->next;
                            }
                        } else {
                            curr = curr->parent;
                            if (curr->parent == NULL) // if we're at the root
                                curr = NULL; // don't set selection to root
                        }
                    }

                    if (curr != NULL)
                        wnd.selection = curr;
                }
                break;
            case OUR_MOD_PRIMARY:
                if (imgui_special_key_pressed(ImGuiKey_Delete) || imgui_special_key_pressed(ImGuiKey_Backspace)) {
                    auto curr = wnd.selection;
                    if (curr != NULL) world.delete_ft_node(curr);
                }
                if (imgui_special_key_pressed(ImGuiKey_Enter)) {
                    auto curr = wnd.selection;
                    if (curr != NULL) open_ft_node(curr);
                }
                break;
            }
        }

        wnd.scroll_to = NULL;

        ImGui::End();
        ImGui::PopStyleVar();
    }

    if (world.wnd_project_settings.show) {
        auto &wnd = world.wnd_project_settings;

        ImGui::Begin("Project Settings", &wnd.show, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking);

        auto &tmp = wnd.tmp;

        if (ImGui::BeginTabBar("MyTabBar", ImGuiTabBarFlags_None)) {
            int flags = ImGuiTabItemFlags_None;
            if (wnd.focus_debug_profiles) {
                flags |= ImGuiTabItemFlags_SetSelected;
                wnd.focus_debug_profiles = false;
            }

            if (ImGui::BeginTabItem("Debug Profiles", NULL, flags)) {
                {
                    ImGui::BeginChild("left pane", ImVec2(200, 300), true);

                    for (int i = 0; i < tmp.debug_profiles_len; i++) {
                        auto &it = tmp.debug_profiles[i];
                        if (ImGui::Selectable(it.label, wnd.current_debug_profile == i))
                            wnd.current_debug_profile = i;
                    }

                    ImGui::EndChild();
                }

                ImGui::SameLine();

                {
                    ImGui::BeginChild("right pane", ImVec2(400, 300));

                    auto &dp = tmp.debug_profiles[wnd.current_debug_profile];

                    if (dp.is_builtin) {
                        if (dp.type == DEBUG_TEST_CURRENT_FUNCTION) {
                            ImGuiStyle &style = ImGui::GetStyle();
                            ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
                            ImGui::TextWrapped("This is a built-in debug profile, used for the Debug Test Under Cursor command. It can't be changed, except to add command-line arguments.");
                            ImGui::PopStyleColor();
                            imgui_small_newline();
                        }
                    }

                    imgui_with_disabled(dp.is_builtin, [&]() {
                        imgui_input_text_full_fixbuf("Name", dp.label);
                    });

                    imgui_small_newline();

                    const char* labels[] = {
                        "Test Package",
                        "Test Function Under Cursor",
                        "Run Package",
                        "Run Binary",
                    };

                    imgui_with_disabled(dp.is_builtin, [&]() {
                        ImGui::Text("Type");
                        ImGui::PushItemWidth(-1);
                        ImGui::Combo("##dp_type", (int*)&dp.type, labels, _countof(labels));
                        ImGui::PopItemWidth();
                    });

                    imgui_small_newline();

                    switch (dp.type) {
                    case DEBUG_TEST_PACKAGE:
                        ImGui::Checkbox("Use package of current file", &dp.test_package.use_current_package);

                        imgui_small_newline();

                        imgui_with_disabled(dp.test_package.use_current_package, [&]() {
                            imgui_input_text_full_fixbuf("Package path", dp.test_package.package_path);
                        });

                        imgui_small_newline();
                        break;

                    case DEBUG_TEST_CURRENT_FUNCTION:
                        break;

                    case DEBUG_RUN_PACKAGE:
                        ImGui::Checkbox("Use package of current file", &dp.run_package.use_current_package);
                        imgui_small_newline();

                        imgui_with_disabled(dp.run_package.use_current_package, [&]() {
                            imgui_input_text_full_fixbuf("Package path", dp.run_package.package_path);
                        });

                        imgui_small_newline();
                        break;

                    case DEBUG_RUN_BINARY:
                        imgui_input_text_full_fixbuf("Binary path", dp.run_binary.binary_path);
                        imgui_small_newline();
                        break;
                    }

                    imgui_input_text_full_fixbuf("Additional arguments", dp.args);

                    ImGui::EndChild();
                }

                ImGui::EndTabItem();
            }

            flags = ImGuiTabItemFlags_None;
            if (wnd.focus_build_profiles) {
                flags |= ImGuiTabItemFlags_SetSelected;
                wnd.focus_build_profiles = false;
            }

            if (ImGui::BeginTabItem("Build Profiles", NULL, flags)) {
                {
                    ImGui::BeginChild("left pane", ImVec2(200, 300), true);

                    for (int i = 0; i < tmp.build_profiles_len; i++) {
                        auto &it = tmp.build_profiles[i];
                        if (ImGui::Selectable(it.label, wnd.current_build_profile == i))
                            wnd.current_build_profile = i;
                    }

                    ImGui::EndChild();
                }

                ImGui::SameLine();

                {
                    ImGui::BeginChild("right pane", ImVec2(400, 300));

                    auto &dp = tmp.build_profiles[wnd.current_build_profile];

                    imgui_input_text_full_fixbuf("Name", dp.label);
                    imgui_small_newline();
                    imgui_input_text_full_fixbuf("Build command", dp.cmd);

                    ImGui::EndChild();
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::Separator();

        {
            ImGuiStyle &style = ImGui::GetStyle();

            float button1_w = ImGui::CalcTextSize("Save").x + style.FramePadding.x * 2.f;
            float button2_w = ImGui::CalcTextSize("Cancel").x + style.FramePadding.x * 2.f;
            float width_needed = button1_w + style.ItemSpacing.x + button2_w;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - width_needed);

            if (ImGui::Button("Save")) {
                project_settings.copy(&wnd.tmp);
                project_settings.write(path_join(world.current_path, ".cp95proj"));
                world.wnd_project_settings.show = false;
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel")) {
                world.wnd_project_settings.show = false;
            }
        }

        ImGui::End();
    }

    if (world.windows_open.im_demo)
        ImGui::ShowDemoWindow(&world.windows_open.im_demo);

    if (world.windows_open.im_metrics)
        ImGui::ShowMetricsWindow(&world.windows_open.im_metrics);

    if (world.wnd_goto_file.show) {
        auto& wnd = world.wnd_goto_file;

        auto go_up = [&]() {
            if (wnd.filtered_results->len == 0) return;
            if (wnd.selection == 0)
                wnd.selection = min(wnd.filtered_results->len, settings.goto_file_max_results) - 1;
            else
                wnd.selection--;
        };

        auto go_down = [&]() {
            if (wnd.filtered_results->len == 0) return;
            wnd.selection++;
            wnd.selection %= min(wnd.filtered_results->len, settings.goto_file_max_results);
        };

        ImGui::SetNextWindowSize(ImVec2(500, -1));
        ImGui::SetNextWindowPos(ImVec2(world.window_size.x/2, 150), ImGuiCond_Always, ImVec2(0.5f, 0));

        ImGui::Begin("Go To File", &world.wnd_goto_file.show, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking);

        auto is_focusing = imgui_is_window_focusing(&wnd.focused);

        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
        } else if (!wnd.first_open_focus_twice_done) {
            wnd.first_open_focus_twice_done = true;
            ImGui::SetKeyboardFocusHere();
        } else if (is_focusing) {
            ImGui::SetKeyboardFocusHere();
        }

        // close the window when we unfocus
        if (!wnd.focused) wnd.show = false;

        if (imgui_input_text_full("Search for file:", wnd.query, _countof(wnd.query), ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (wnd.filtered_results->len > 0) {
                auto relpath = wnd.filepaths->at(wnd.filtered_results->at(wnd.selection));
                auto filepath = path_join(world.current_path, relpath);
                world.focus_editor(filepath);
            }
            world.wnd_goto_file.show = false;
            ImGui::SetWindowFocus(NULL);
        }

        if (wnd.focused) {
            auto mods = imgui_get_keymods();
            switch (mods) {
            case OUR_MOD_NONE:
                if (imgui_special_key_pressed(ImGuiKey_DownArrow)) go_down();
                if (imgui_special_key_pressed(ImGuiKey_UpArrow)) go_up();
                if (imgui_special_key_pressed(ImGuiKey_Escape)) wnd.show = false;
                break;
            case OUR_MOD_PRIMARY:
                if (imgui_key_pressed('j')) go_down();
                if (imgui_key_pressed('k')) go_up();
                break;
            }
        }

        if (ImGui::IsItemEdited()) {
            if (strlen(wnd.query) >= 2)
                filter_files();
            else
                wnd.filtered_results->len = 0; // maybe use logic from filter_files
        }

        {
            ImGui::PushFont(world.ui.im_font_mono);
            defer { ImGui::PopFont(); };

            for (u32 i = 0; i < wnd.filtered_results->len && i < settings.goto_file_max_results; i++) {
                if (i == 0) imgui_small_newline();

                auto it = wnd.filepaths->at(wnd.filtered_results->at(i));
                if (i == wnd.selection)
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "%s", it);
                else
                    ImGui::Text("%s", it);
            }
        }

        ImGui::End();
    }

    if (world.wnd_goto_symbol.show) {
        auto& wnd = world.wnd_goto_symbol;

        auto go_up = [&]() {
            if (wnd.filtered_results->len == 0) return;
            if (wnd.selection == 0)
                wnd.selection = min(wnd.filtered_results->len, settings.goto_symbol_max_results) - 1;
            else
                wnd.selection--;
        };

        auto go_down = [&]() {
            if (wnd.filtered_results->len == 0) return;
            wnd.selection++;
            wnd.selection %= min(wnd.filtered_results->len, settings.goto_file_max_results);
        };

        ImGui::Begin("Go To Symbol", &world.wnd_goto_symbol.show, ImGuiWindowFlags_AlwaysAutoResize);

        auto mods = imgui_get_keymods();
        switch (mods) {
        case OUR_MOD_NONE:
            if (imgui_special_key_pressed(ImGuiKey_DownArrow)) go_down();
            if (imgui_special_key_pressed(ImGuiKey_UpArrow)) go_up();
            if (imgui_special_key_pressed(ImGuiKey_Escape)) wnd.show = false;
            break;
        case OUR_MOD_PRIMARY:
            if (imgui_key_pressed('j')) go_down();
            if (imgui_key_pressed('k')) go_up();
            break;
        }

        auto is_focusing = imgui_is_window_focusing(&wnd.focused);

        ImGui::Text("Search for symbol:");

        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
        } else if (!wnd.first_open_focus_twice_done) {
            wnd.first_open_focus_twice_done = true;
            ImGui::SetKeyboardFocusHere();
        } else if (is_focusing) {
            ImGui::SetKeyboardFocusHere();
        }

        if (ImGui::InputText("##search_for_symbol", wnd.query, _countof(wnd.query), ImGuiInputTextFlags_EnterReturnsTrue)) {
            wnd.show = false;
            ImGui::SetWindowFocus(NULL);

            do {
                if (wnd.filtered_results->len == 0) break;
                if (!world.indexer.ready) break;

                if (!world.indexer.lock.try_enter()) break;
                defer { world.indexer.lock.leave(); };

                auto symbol = wnd.symbols->at(wnd.filtered_results->at(wnd.selection));
                auto result = world.indexer.jump_to_symbol(symbol);
                if (result == NULL) break;

                goto_jump_to_definition_result(result);
            } while (0);
        }

        if (ImGui::IsItemEdited()) {
            if (strlen(wnd.query) >= 2)
                filter_symbols();
            else
                wnd.filtered_results->len = 0;
        }

        {
            ImGui::PushFont(world.ui.im_font_mono);
            defer { ImGui::PopFont(); };

            for (u32 i = 0; i < wnd.filtered_results->len && i < settings.goto_file_max_results; i++) {
                auto it = wnd.symbols->at(wnd.filtered_results->at(i));
                if (i == wnd.selection)
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "%s", it);
                else
                    ImGui::Text("%s", it);
            }
        }

        ImGui::End();
    }

    // Don't show the debugger UI if we're still starting up, because we're
    // still building, and the build could fail.  Might regret this, can always
    // change later.
    if (world.dbg.state_flag != DLV_STATE_INACTIVE && world.dbg.state_flag != DLV_STATE_STARTING) {
        Debugger_UI dui;
        dui.init(this);
        dui.draw();
    }

    if (world.wnd_search_and_replace.show) {
        auto& wnd = world.wnd_search_and_replace;

        ImGui::SetNextWindowDockID(dock_sidebar_id, ImGuiCond_Once);

        ImGui::Begin(
            our_sprintf("%s###search_and_replace", wnd.replace ? "Search and Replace" : "Search"),
            &wnd.show, ImGuiWindowFlags_AlwaysAutoResize
        );

        bool is_focusing = imgui_is_window_focusing(&wnd.focus_bool);

        bool entered = false;

        ImGui::PushFont(world.ui.im_font_mono);
        {
            auto should_focus_textbox = [&]() -> bool {
                if (wnd.focus_textbox == 1) {
                    wnd.focus_textbox = 2;
                    return true;
                }
                if (wnd.focus_textbox == 2) {
                    wnd.focus_textbox = 0;
                    return true;
                }
                return false;
            };

            if (ImGui::IsWindowAppearing() || should_focus_textbox()) {
                ImGui::SetKeyboardFocusHere();
                ImGui::SetScrollHereY();
            }

            if (imgui_input_text_full("Search for", wnd.find_str, _countof(wnd.find_str), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
                entered = true;

            if (wnd.replace)
                if (imgui_input_text_full("Replace with", wnd.replace_str, _countof(wnd.replace_str), ImGuiInputTextFlags_EnterReturnsTrue))
                    entered = true;
        }
        ImGui::PopFont();

        imgui_small_newline();

        if (ImGui::Checkbox("Case-sensitive", &wnd.case_sensitive))
            entered = true;

        ImGui::SameLine();
        if (ImGui::Checkbox("Use regular expression", &wnd.use_regex))
            entered = true;

        imgui_small_newline();

        if (entered) {
            auto &s = world.searcher;

            s.cleanup();
            if (wnd.find_str[0] != '\0') {
                s.init();

                Search_Opts opts; ptr0(&opts);
                opts.case_sensitive = wnd.case_sensitive;
                opts.literal = !wnd.use_regex;

                wnd.selection = -1;
                s.start_search(wnd.find_str, &opts);
            }
        }

        switch (world.searcher.state) {
        case SEARCH_SEARCH_IN_PROGRESS:
            ImGui::Text("Searching...");
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                // TODO
            }
            break;
        case SEARCH_SEARCH_DONE:
            {
                if (wnd.replace)
                    if (ImGui::Button("Perform Replacement"))
                        // TODO: if we have more results than we're showing, warn user about that
                        world.searcher.start_replace(wnd.replace_str);

                int index = 0;
                int result_index = 0;
                bool didnt_finish = false;

                Search_Result *current_result = NULL;
                ccstr current_filepath = NULL;

                For (world.searcher.search_results) {
                    if (index > 400) {
                        didnt_finish = true;
                        break;
                    }

                    ImGui::Text("%s", get_path_relative_to(it.filepath, world.current_path));

                    ImGui::Indent();
                    ImGui::PushFont(world.ui.im_font_mono);

                    auto filepath = it.filepath;

                    For (*it.results) {
                        defer { index++; };

                        // allow up to 100 to finish the results in current file
                        if (index > 500) {
                            didnt_finish = true;
                            break;
                        }

                        auto availwidth = ImGui::GetContentRegionAvail().x;
                        auto text_size = ImVec2(availwidth, ImGui::CalcTextSize("blah").y);
                        auto drawpos = ImGui::GetCursorScreenPos();

                        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(ImColor(60, 60, 60)));

                        bool clicked = ImGui::Selectable(
                            our_sprintf("##search_result_%d", index),
                            index == wnd.selection,
                            ImGuiSelectableFlags_AllowDoubleClick,
                            text_size
                        );

                        if (index == wnd.selection) {
                            current_result = &it;
                            current_filepath = filepath;
                        }

                        ImGui::PopStyleColor();

                        auto draw_text = [&](ccstr s, int len, bool strikethrough = false) {
                            auto text = our_sprintf("%.*s", len, s);
                            auto size = ImGui::CalcTextSize(text);

                            auto drawlist = ImGui::GetWindowDrawList();
                            drawlist->AddText(drawpos, ImGui::GetColorU32(ImGuiCol_Text), text);

                            if (strikethrough) {
                                ImVec2 a = drawpos, b = drawpos;
                                b.y += size.y/2;
                                a.y += size.y/2;
                                b.x += size.x;
                                // ImGui::GetFontSize() / 2
                                drawlist->AddLine(a, b, ImGui::GetColorU32(ImGuiCol_Text), 1.0f);
                            }

                            drawpos.x += ImGui::CalcTextSize(text).x;
                        };

                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(200, 178, 178)));
                        {
                            auto s = our_sprintf("%d:%d ", it.match_start.y+1, it.match_start.x+1);
                            draw_text(s, strlen(s));
                        }
                        ImGui::PopStyleColor();

                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(178, 178, 178)));

                        draw_text(it.preview, it.match_offset_in_preview);

                        if (wnd.replace) {
                            // draw old
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(255, 180, 180)));
                            draw_text(it.match, it.match_len, true);
                            ImGui::PopStyleColor();

                            // draw new
                            auto newtext = world.searcher.get_replacement_text(&it, wnd.replace_str);
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(180, 255, 180)));
                            draw_text(newtext, strlen(newtext));
                            ImGui::PopStyleColor();
                        } else {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(255, 255, 255)));
                            draw_text(it.match, it.match_len);
                            ImGui::PopStyleColor();
                        }

                        draw_text(
                            &it.preview[it.match_offset_in_preview + it.match_len],
                            it.preview_len - it.match_offset_in_preview - it.match_len
                        );

                        ImGui::PopStyleColor();

                        if (clicked) {
                            if (ImGui::IsMouseDoubleClicked(0)) {
                                auto pos = it.match_start;
                                if (it.mark_start != NULL)
                                    pos = it.mark_start->pos();

                                goto_file_and_pos(filepath, pos);
                            } else {
                                wnd.selection = index;
                            }
                        }
                    }

                    ImGui::PopFont();
                    ImGui::Unindent();
                }

                if (wnd.focus_bool && !world.ui.keyboard_captured_by_imgui) {
                    auto mods = imgui_get_keymods();
                    switch (mods) {
                    case OUR_MOD_NONE:
                        if (imgui_special_key_pressed(ImGuiKey_DownArrow) || imgui_key_pressed('j')) {
                            if (wnd.selection < index-1)
                                wnd.selection++;
                        }
                        if (imgui_special_key_pressed(ImGuiKey_UpArrow) || imgui_key_pressed('k')) {
                            if (wnd.selection > 0)
                                wnd.selection--;
                        }
                        if (imgui_special_key_pressed(ImGuiKey_Enter))
                            if (current_result != NULL)
                                goto_file_and_pos(current_filepath, current_result->match_start);
                        break;
                    }
                }

                if (didnt_finish) {
                    ImGui::Text("There were too many results; some are omitted.");
                }
            }
            break;
        case SEARCH_REPLACE_IN_PROGRESS:
            ImGui::Text("Replacing...");
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                // TODO
            }
            break;
        case SEARCH_REPLACE_DONE:
            ImGui::Text("Done!"); // TODO: show # files replaced, i guess, and undo button
            break;
        case SEARCH_NOTHING_HAPPENING:
            break;
        }

        ImGui::End();
    }

    if (world.wnd_mark_edit_viewer.show) {
        ImGui::Begin("Mark Edit Viewer", &world.wnd_mark_edit_viewer.show, ImGuiWindowFlags_AlwaysAutoResize);

        auto editor = world.get_current_editor();
        if (editor != NULL) {
            For (editor->buf.mark_tree.edits) {
                ImGui::Text("start = %s, oldend = %s, newend = %s", 
                            format_cur(it.start),
                            format_cur(it.old_end),
                            format_cur(it.new_end));
            }
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

        auto tree = editor->buf.tree;
        if (tree == NULL) break;

        if (world.wnd_editor_tree.show) {
            auto &wnd = world.wnd_editor_tree;

            ImGui::Begin("AST", &wnd.show, 0);

            ImGui::Checkbox("show anon?", &wnd.show_anon_nodes);
            ImGui::SameLine();
            ImGui::Checkbox("show comments?", &wnd.show_comments);
            ImGui::SameLine();

            cur2 open_cur = new_cur2(-1, -1);
            if (ImGui::Button("go to cursor"))
                open_cur = editor->cur;

            ts_tree_cursor_reset(&editor->buf.cursor, ts_tree_root_node(tree));
            render_ts_cursor(&editor->buf.cursor, open_cur);

            ImGui::End();
        }

        if (world.wnd_editor_toplevels.show) {
            ImGui::Begin("Toplevels", &world.wnd_editor_toplevels.show, 0);

            List<Godecl> decls;
            decls.init();

            Parser_It it; ptr0(&it);
            it.init(&editor->buf);

            Ast_Node node; ptr0(&node);
            node.init(ts_tree_root_node(tree), &it);

            FOR_NODE_CHILDREN (&node) {
                switch (it->type()) {
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

    boxf pane_area;
    pane_area.pos = panes_area.pos;

    int editor_index;

    For (actual_cursor_positions) {
        it.x = -1;
        it.y = -1;
    }
    actual_parameter_hint_start.x = -1;
    actual_parameter_hint_start.y = -1;

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
        boxf current_tab;

        start_clip(tabs_area); // will become problem once we have popups on tabs

        // draw tabs
        u32 tab_id = 0;
        for (auto&& editor : pane.editors) {
            defer { editor_index++; };

            SCOPED_FRAME();

            bool is_selected = (tab_id == pane.current_editor);

            ccstr label = "<untitled>";
            if (!editor.is_untitled) {
                auto &ind = world.indexer;
                if (ind.goroot != NULL && path_contains_in_subtree(ind.goroot, editor.filepath)) {
                    label = get_path_relative_to(editor.filepath, ind.goroot);
                    label = our_sprintf("$GOROOT/%s", label);
                } else if (ind.gomodcache != NULL && path_contains_in_subtree(ind.gomodcache, editor.filepath)) {
                    label = get_path_relative_to(editor.filepath, ind.gomodcache);
                    label = our_sprintf("$GOMODCACHE/%s", label);
                } else {
                    label = get_path_relative_to(editor.filepath, world.current_path);
                }
            }

            if (editor.buf.dirty)
                label = our_sprintf("%s*", label);

            auto text_width = get_text_width(label);

            tab.w = text_width + tab_padding.x * 2;
            tab.h = font->height + tab_padding.y * 2;

            // Now `tab` is filled out, and I can do my logic to make sure it's visible on screen
            if (tab_id == pane.current_editor) {
                current_tab = tab;

                auto margin = tab_id == 0 ? 5 : settings.tabs_offset;
                if (tab.x < tabs_area.x + margin) {
                    pane.tabs_offset -= (tabs_area.x + margin - tab.x);
                }

                margin = (tab_id == pane.editors.len - 1) ? 5 : settings.tabs_offset;
                if (tab.x + tab.w > tabs_area.x + tabs_area.w - margin)
                    // if it would make the other constraint fail, don't do it
                    if (!(tab.x - ((tab.x + tab.w) - (tabs_area.x + tabs_area.w - margin)) < tabs_area.x + margin)) {
                        pane.tabs_offset += ((tab.x + tab.w) - (tabs_area.x + tabs_area.w - margin));
                    }
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

        end_clip(); // tabs_area

        // if tabs are off screen, see if we can move them back
        if (pane.tabs_offset > 0) {
            auto margin = (pane.current_editor == pane.editors.len - 1) ? 5 : settings.tabs_offset;
            auto space_avail = min(
                relu_sub(tabs_area.x + tabs_area.w, (current_tab.x + current_tab.w + margin)),
                pane.tabs_offset
            );
            /*
            auto space_avail = relu_sub(
                tabs_area.x + tabs_area.w,
                (current_tab.x + current_tab.w + margin)
            );
            */
            pane.tabs_offset -= space_avail;
        }

        if (tab_to_remove != -1) {
            // duplicate of code in main.cpp under GLFW_KEY_W handler, refactor
            // if we copy this a few more times
            pane.editors[tab_to_remove].cleanup();
            pane.editors.remove(tab_to_remove);

            if (pane.editors.len == 0)
                pane.set_current_editor(-1);
            else if (pane.current_editor == tab_to_remove) {
                auto new_idx = pane.current_editor;
                if (new_idx >= pane.editors.len)
                    new_idx = pane.editors.len - 1;
                pane.focus_editor_by_index(new_idx);
            } else if (pane.current_editor > tab_to_remove) {
                pane.set_current_editor(pane.current_editor - 1);
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
            if (editor->buf.tree != NULL) {
                ts_tree_cursor_reset(&editor->buf.cursor, ts_tree_root_node(editor->buf.tree));

                auto start = new_cur2(0, editor->view.y);
                auto end = new_cur2(0, editor->view.y + editor->view.h);

                walk_ts_cursor(&editor->buf.cursor, false, [&](Ast_Node *node, Ts_Field_Type, int depth) -> Walk_Action {
                    auto node_start = node->start();
                    auto node_end = node->end();

                    if (node_end < start) return WALK_SKIP_CHILDREN;
                    if (node_start > end) return WALK_ABORT;
                    // if (node->child_count() != 0) return WALK_CONTINUE;

                    vec3f color; ptr0(&color);
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

                auto draw_cursor = [&](int chars) {
                    if (current_pane != world.current_pane) return;

                    actual_cursor_positions[current_pane] = cur_pos;    // save position where cursor is drawn for later use
                    bool is_insert_cursor = (world.nvim.mode == VI_INSERT && is_pane_selected && !world.nvim.exiting_insert_mode);

                    auto pos = cur_pos;
                    pos.y -= font->offset_y;

                    boxf b;
                    b.pos = pos;
                    b.h = (float)font->height;
                    b.w = is_insert_cursor ? 2 : ((float)font->width * chars);

                    auto py = font->height * (settings.line_height - 1.0) / 2;
                    b.y -= py;
                    b.h += py * 2;

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
                        if (are_filepaths_equal(it.file, editor->filepath)) {
                            auto p = breakpoints_for_this_editor.append();
                            memcpy(p, &it, sizeof(it));
                        }
                    }
                }

                if (buf.lines.len == 0) draw_cursor(1);

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
                                return inactive ? BREAKPOINT_INACTIVE : BREAKPOINT_ACTIVE;
                            }
                        }

                        return BREAKPOINT_NONE;
                    };

                    boxf line_box = {
                        cur_pos.x,
                        cur_pos.y - font->offset_y,
                        (float)editor_area.w,
                        (float)font->height,
                    };

                    auto py = font->height * (settings.line_height - 1.0) / 2;
                    line_box.y -= py;
                    line_box.h += py * 2;

                    auto bptype = find_breakpoint_stopped_at_this_line();
                    if (bptype == BREAKPOINT_CURRENT_GOROUTINE)
                        draw_rect(line_box, rgba(COLOR_DARK_YELLOW));
                    else if (bptype == BREAKPOINT_OTHER_GOROUTINE)
                        draw_rect(line_box, rgba(COLOR_DARKER_YELLOW));
                    else if (bptype == BREAKPOINT_ACTIVE)
                        draw_rect(line_box, rgba(COLOR_RED, 0.5));
                    else if (bptype == BREAKPOINT_INACTIVE)
                        draw_rect(line_box, rgba(COLOR_RED, 0.3));

                    auto line_number_width = get_line_number_width(editor);

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

                    Grapheme_Clusterer gc;
                    gc.init();

                    int cp_idx = 0;
                    gc.feed(line->at(cp_idx)); // feed first character for GB1

                    // jump {view.x} clusters
                    for (int i = 0; i < view.x && cp_idx < line->len; i++) {
                        cp_idx++;
                        while (cp_idx < line->len && !gc.feed(line->at(cp_idx)))
                            cp_idx++;
                    }

                    for (u32 x = view.x, vx = view.x; vx < view.x + view.w; x++) {
                        if (cp_idx >= line->len) break;

                        auto curr_cp_idx = cp_idx;

                        int curr_cp = line->at(cp_idx++);
                        int grapheme_cpsize = 1;

                        // grab another cluster
                        while (cp_idx < line->len && !gc.feed(line->at(cp_idx))) {
                            cp_idx++;
                            grapheme_cpsize++;
                        }

                        int glyph_width = 0;
                        if (grapheme_cpsize == 1 && curr_cp == '\t')
                            glyph_width = options.tabsize - ((vx - view.x) % options.tabsize);
                        else
                            glyph_width = our_wcwidth(curr_cp);

                        if (glyph_width == -1) glyph_width = 1;

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

                        if (editor->cur == new_cur2((u32)curr_cp_idx, (u32)y)) {
                            draw_cursor(glyph_width);
                            if ((world.nvim.mode != VI_INSERT || world.nvim.exiting_insert_mode) && current_pane == world.current_pane)
                                text_color = COLOR_BLACK;
                        } else if (world.nvim.mode != VI_INSERT) {
                            auto topline = editor->nvim_data.grid_topline;
                            if (topline <= y && y < topline + NVIM_DEFAULT_HEIGHT) {
                                for (int i = 0; i < glyph_width && vx+i < _countof(editor->highlights[y - topline]); i++) {
                                    auto draw_highlight = [&](vec4f color) {
                                        boxf b;
                                        b.pos = cur_pos;
                                        b.x += font->width * i;
                                        b.y -= font->offset_y;
                                        b.w = font->width;
                                        b.h = font->height;

                                        auto py = font->height * (settings.line_height - 1.0) / 2;
                                        b.y -= py;
                                        b.h += py * 2;

                                        draw_rect(b, color);
                                    };

                                    auto hl = editor->highlights[y - topline][vx + i];
                                    switch (hl) {
                                    case HL_INCSEARCH:
                                        draw_highlight(rgba("#553333"));
                                        break;
                                    case HL_SEARCH:
                                        draw_highlight(rgba("#994444"));
                                        break;
                                    case HL_VISUAL:
                                        draw_highlight(rgba("#335533"));
                                        break;
                                    }
                                }
                            }
                        }

                        if (hint.gotype != NULL)
                            if (new_cur2(x, y) == hint.start)
                                actual_parameter_hint_start = cur_pos;

                        uchar uch = curr_cp;
                        if (uch == '\t') {
                            auto chars = options.tabsize - ((vx - view.x) % options.tabsize);
                            cur_pos.x += font->width * chars;
                            vx += chars;
                        } else if (grapheme_cpsize > 1 || uch > 0x7f) {
                            auto pos = cur_pos;
                            pos.x += (font->width * glyph_width) / 2 - (font->width / 2);
                            draw_char(&pos, 0xfffd, rgba(text_color));

                            cur_pos.x += font->width * glyph_width;
                            vx += glyph_width;
                        } else {
                            draw_char(&cur_pos, uch, rgba(text_color));
                            vx++;
                        }
                    }

                    if (editor->cur == new_cur2(line->len, y))
                        draw_cursor(1);

                    cur_pos.x = editor_area.x + settings.editor_margin_x;
                    cur_pos.y += font->height * settings.line_height;
                }
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
                if (world.ui.mouse_down[GLFW_MOUSE_BUTTON_LEFT])
                    if (world.resizing_pane == -1)
                        world.resizing_pane = i;
            } else if (world.resizing_pane == i) {
                draw_rect(b, rgba(COLOR_WHITE));
            } else {
                draw_rect(b, rgba(COLOR_DARK_GREY));
            }
        }

        if (!world.ui.mouse_down[GLFW_MOUSE_BUTTON_LEFT])
            world.resizing_pane = -1;
    }

    {
        draw_rect(status_area, rgba("#252525"));

        float status_area_left = status_area.x;
        float status_area_right = status_area.x + status_area.w;

        enum { LEFT = 0, RIGHT = 1 };

        auto get_status_piece_rect = [&](int dir, ccstr s) -> boxf {
            boxf ret; ptr0(&ret);
            ret.y = status_area.y;
            ret.h = status_area.h;
            ret.w = font->width * strlen(s) + (settings.status_padding_x * 2);

            if (dir == RIGHT)
                ret.x = status_area_right - ret.w;
            else
                ret.x = status_area_left;

            return ret;
        };

        // returns mouse flags
        auto draw_status_piece = [&](int dir, ccstr s, vec4f bgcolor, vec4f fgcolor) -> int {
            auto rect = get_status_piece_rect(dir, s);
            draw_rect(rect, bgcolor);

            if (dir == RIGHT)
                status_area_right -= rect.w;
            else
                status_area_left += rect.w;

            boxf text_area = rect;
            text_area.x += settings.status_padding_x;
            text_area.y += settings.status_padding_y;
            text_area.w -= (settings.status_padding_x * 2);
            text_area.h -= (settings.status_padding_y * 2);
            draw_string(text_area.pos, s, fgcolor);

            return get_mouse_flags(rect);
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
            } else if (world.get_current_editor() != NULL) {
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

        int index_mouse_flags = 0;
        if (world.indexer.ready) {
            auto mouse_flags = get_mouse_flags(get_status_piece_rect(RIGHT, "INDEX READY"));
            auto opacity = mouse_flags & MOUSE_HOVER ? 1.0 : 0.8;
            index_mouse_flags = draw_status_piece(RIGHT, "INDEX READY", rgba("#008800", opacity), rgba("#cceecc", opacity));
        } else {
            auto mouse_flags = get_mouse_flags(get_status_piece_rect(RIGHT, "INDEXING..."));
            auto opacity = mouse_flags & MOUSE_HOVER ? 1.0 : 0.8;
            index_mouse_flags = draw_status_piece(RIGHT, "INDEXING...", rgba("#880000", opacity), rgba("#eecccc", opacity));
        }

        if (index_mouse_flags & MOUSE_CLICKED) {
            world.wnd_index_log.show ^= 1;
        }

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

    // now go back and draw things that go on top, like autocomplete and param hints
    glViewport(0, 0, world.display_size.x, world.display_size.y);
    glUseProgram(world.ui.program);
    glBindVertexArray(world.ui.vao); // bind my vertex array & buffers
    glBindBuffer(GL_ARRAY_BUFFER, world.ui.vbo);
    glDisable(GL_SCISSOR_TEST);

    for (u32 current_pane = 0; current_pane < world.panes.len; current_pane++) {
        auto &pane = world.panes[current_pane];
        if (pane.editors.len == 0) continue;

        auto editor = pane.get_current_editor();
        do {
            auto actual_cursor_position = actual_cursor_positions[current_pane];
            if (actual_cursor_position.x == -1) break;

            auto &ac = editor->autocomplete;

            if (ac.ac.results == NULL) break;

            s32 max_len = 0;
            s32 num_items = min(ac.filtered_results->len, AUTOCOMPLETE_WINDOW_ITEMS);

            For (*ac.filtered_results) {
                auto len = strlen(ac.ac.results->at(it).name);
                if (len > max_len)
                    max_len = len;
            }

            int max_desc_len = 40;

            max_len += max_desc_len + 5; // leave space for hints

            /*
            OK SO BASICALLY
            first try to put it in bottom
            then put it in top
            if neither fits then shrink it to max(bottom_limit, top_limit) and then put it there
            */

            if (num_items > 0) {
                boxf menu;
                // float preview_width = settings.autocomplete_preview_width_in_chars * font->width;

                menu.w = (
                    // options
                    (font->width * max_len)
                    + (settings.autocomplete_item_padding_x * 2)
                    + (settings.autocomplete_menu_padding * 2)

                    // preview
                    // + preview_width
                    // + (settings.autocomplete_menu_padding * 2)
                );

                menu.h = (
                    (font->height * num_items)
                    + (settings.autocomplete_item_padding_y * 2 * num_items)
                    + (settings.autocomplete_menu_padding * 2)
                );

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

                    if (menu.w > world.window_size.x - 4) // small margin
                        menu.w = world.window_size.x - 4;

                    auto x1 = actual_cursor_position.x - strlen(ac.ac.prefix) * font->width;
                    if (x1 + menu.w + 4 > world.window_size.x) // margin of 4
                        x1 = world.window_size.x - menu.w - 4;
                    menu.x = x1;
                }

                draw_bordered_rect_outer(menu, rgba(COLOR_BLACK), rgba(COLOR_DARK_GREY), 1, 4);

                // --- draw the actual stuff inside the menu

                // boxf preview_area = menu;
                // preview_area.w = preview_width + (settings.autocomplete_menu_padding * 2);
                // preview_area.x += (menu.w - preview_area.w);

                boxf items_area = menu;
                items_area.w = menu.w; // - preview_area.w;

                float menu_padding = settings.autocomplete_menu_padding;
                auto items_pos = items_area.pos + new_vec2f(menu_padding, menu_padding);

                {
                    boxf sep;
                    sep.w = 1;
                    sep.h = items_area.h;
                    sep.x = items_area.x + items_area.w - 1;
                    sep.y = items_area.y;
                    draw_rect(sep, rgba(COLOR_DARK_GREY));
                }

                for (int i = ac.view; i < ac.view + num_items; i++) {
                    auto idx = ac.filtered_results->at(i);

                    vec3f color = COLOR_WHITE;

                    if (i == ac.selection) {
                        boxf b;
                        b.pos = items_pos;
                        b.h = font->height + (settings.autocomplete_item_padding_y * 2);
                        b.w = items_area.w - (settings.autocomplete_menu_padding * 2);
                        draw_rounded_rect(b, rgba(COLOR_DARK_GREY), 4, ROUND_ALL);
                    }

                    auto &result = ac.ac.results->at(idx);

                    float text_end = 0;

                    {
                        SCOPED_FRAME();

                        auto actual_color = color;
                        if (result.type == ACR_POSTFIX)
                            actual_color = new_vec3f(1.0, 0.8, 0.8);
                        else if (result.type == ACR_KEYWORD)
                            actual_color = new_vec3f(1.0, 1.0, 0.8);
                        else if (result.type == ACR_DECLARATION && result.declaration_is_builtin)
                            actual_color = new_vec3f(0.8, 1.0, 0.8);
                        else if (result.type == ACR_DECLARATION && result.declaration_is_struct_literal_field)
                            actual_color = new_vec3f(1.0, 1.0, 0.8);

                        // add icon based on type
                        // show a bit more helpful info (like inline signature for funcs)
                        // show extended info on a panel to the right

                        auto str = (cstr)our_strcpy(result.name);
                        auto pos = items_pos + new_vec2f(settings.autocomplete_item_padding_x, settings.autocomplete_item_padding_y);

                        auto avail_width = items_area.w - settings.autocomplete_item_padding_x * 2;
                        if (strlen(str) * font->width > avail_width)
                            str[(int)(avail_width / font->width)] = '\0';

                        draw_string(pos, str, rgba(actual_color));
                        text_end = pos.x + strlen(str) * font->width;
                    }

                    {
                        SCOPED_FRAME();

                        auto render_type = [&](Gotype *gotype) -> ccstr {
                            if (gotype == NULL) return "";

                            Type_Renderer rend;
                            rend.init();
                            rend.write_type(gotype, false);
                            return rend.finish();
                        };

                        auto render_description = [&]() -> ccstr {
                            switch (result.type) {
                            case ACR_DECLARATION:
                                {
                                    auto decl = result.declaration_godecl;
                                    switch (decl->type) {
                                    case GODECL_IMPORT:
                                        // is this even possible here?
                                        // handle either way
                                        return our_sprintf("\"%s\"", decl->import_path);
                                    case GODECL_TYPE:
                                    case GODECL_VAR:
                                    case GODECL_CONST:
                                    case GODECL_SHORTVAR:
                                    case GODECL_FUNC:
                                    case GODECL_FIELD:
                                        return render_type(result.declaration_evaluated_gotype);
                                    }
                                }
                                break;
                            case ACR_KEYWORD:
                                if (streq(result.name, "package")) return "Declare package";
                                if (streq(result.name, "import")) return "Declare import";
                                if (streq(result.name, "const")) return "Declare constant";
                                if (streq(result.name, "var")) return "Declare variable";
                                if (streq(result.name, "func")) return "Declare function";
                                if (streq(result.name, "type")) return "Declare type";
                                if (streq(result.name, "struct")) return "Declare struct type";
                                if (streq(result.name, "interface")) return "Declare interface type";
                                if (streq(result.name, "map")) return "Declare map type";
                                if (streq(result.name, "chan")) return "Declare channel type";
                                if (streq(result.name, "fallthrough")) return "Fall through to next case";
                                if (streq(result.name, "break")) return "Break out";
                                if (streq(result.name, "continue")) return "Continue to next iteration";
                                if (streq(result.name, "goto")) return "Go to label";
                                if (streq(result.name, "return")) return "Return from function";
                                if (streq(result.name, "go")) return "Spawn goroutine";
                                if (streq(result.name, "defer")) return "Defer statement to end of function";
                                if (streq(result.name, "if")) return "Begin branching conditional";
                                if (streq(result.name, "else")) return "Specify else clause";
                                if (streq(result.name, "for")) return "Declare loop";
                                if (streq(result.name, "range")) return "Iterate over collection";
                                if (streq(result.name, "switch")) return "Match expression against values";
                                if (streq(result.name, "case")) return "Declare case";
                                if (streq(result.name, "default")) return "Declare the default case";
                                if (streq(result.name, "select")) return "Wait for a number of channels";
                                if (streq(result.name, "new")) return "Allocate new instance of type";
                                if (streq(result.name, "make")) return "Make new instance of type";
                                if (streq(result.name, "iota")) return "Counter in variable declaration";
                            case ACR_POSTFIX:
                                switch (result.postfix_operation) {
                                case PFC_ASSIGNAPPEND: return "append and assign";
                                case PFC_APPEND: return "append";
                                case PFC_LEN: return "len";
                                case PFC_CAP: return "cap";
                                case PFC_FOR: return "iterate over collection";
                                case PFC_FORKEY: return "iterate over keys";
                                case PFC_FORVALUE: return "iterate over values";
                                case PFC_NIL: return "nil";
                                case PFC_NOTNIL: return "not nil";
                                case PFC_NOT: return "not";
                                case PFC_EMPTY: return "empty";
                                case PFC_IFEMPTY: return "check if empty";
                                case PFC_IF: return "check if true";
                                case PFC_IFNOT: return "check if false";
                                case PFC_IFNIL: return "check if nil";
                                case PFC_IFNOTNIL: return "check if not nil";
                                case PFC_CHECK: return "check returned error";
                                case PFC_DEFSTRUCT: return "define a struct";
                                case PFC_DEFINTERFACE: return "define an interface";
                                case PFC_SWITCH: return "switch on expression";
                                }
                                break;
                            case ACR_IMPORT:
                                return our_sprintf("\"%s\"", result.import_path);
                            }
                            return "";
                        };

                        auto desc = render_description();
                        auto desclen = strlen(desc);

                        auto pos = items_pos + new_vec2f(settings.autocomplete_item_padding_x, settings.autocomplete_item_padding_y);
                        pos.x += (items_area.w - (settings.autocomplete_item_padding_x*2) - (settings.autocomplete_menu_padding*2));
                        pos.x -= font->width * desclen;

                        auto desclimit = desclen;
                        while (pos.x < text_end + 10) {
                            pos.x += font->width;
                            desclimit--;
                        }

                        if (desclen > desclimit)
                            desc = our_sprintf("%.*s...", desclimit-3, desc);

                        draw_string(pos, desc, rgba(new_vec3f(0.5, 0.5, 0.5)));
                    }

                    items_pos.y += font->height + settings.autocomplete_item_padding_y * 2;
                }

                // auto preview_padding = settings.autocomplete_preview_padding;
                // auto preview_pos = preview_area.pos + new_vec2f(preview_padding, preview_padding);

                /*
                // is this ever a thing?
                if (ac.selection != -1) {
                    auto idx = ac.filtered_results->at(ac.selection);
                    auto &result = ac.ac.results->at(idx);

                    auto color = rgba(COLOR_WHITE);

                    preview_drawable_area = preview_area;
                    preview_drawable_area.x += preview_padding;
                    preview_drawable_area.y += preview_padding;
                    preview_drawable_area.w -= preview_padding * 2;
                    preview_drawable_area.h -= preview_padding * 2;

                    switch (result.type) {
                    case ACR_POSTFIX:
                        draw_string(preview_pos, "postfix", color);
                        break;
                    case ACR_KEYWORD:
                        draw_string(preview_pos, "keyword", color);
                        break;
                    case ACR_DECLARATION:
                        `type` result.name result.declaration_godecl->gotype

                        comment here
                        draw_string(preview_pos, "declaration", color);
                        break;
                    case ACR_IMPORT:
                        draw_string(preview_pos, "import", color);
                        break;
                    }
                }
                */
            }
        } while (0);

        do {
            if (actual_parameter_hint_start.x == -1) break;

            auto &hint = editor->parameter_hint;
            if (hint.gotype == NULL) break;

            struct Token_Change {
                int token;
                int index;
            };

            List<Token_Change> token_changes;

            ccstr help_text = NULL;

            {
                token_changes.init();

                Type_Renderer rend;
                rend.init();

                auto add_token_change = [&](int token) {
                    auto c = token_changes.append();
                    c->token = token;
                    c->index = rend.chars.len;
                };

                {
                    auto t = hint.gotype;
                    auto params = t->func_sig.params;
                    auto result = t->func_sig.result;

                    add_token_change(hint.current_param == -1 ? HINT_CURRENT_PARAM : HINT_NOT_CURRENT_PARAM);

                    // write params
                    rend.write("(");
                    for (u32 i = 0; i < params->len; i++) {
                        auto &it = params->at(i);

                        if (i == hint.current_param)
                            add_token_change(HINT_CURRENT_PARAM);

                        add_token_change(HINT_NAME);
                        rend.write("%s ", it.name);
                        add_token_change(HINT_TYPE);
                        rend.write_type(it.gotype);
                        add_token_change(HINT_NORMAL);

                        if (i == hint.current_param)
                            add_token_change(HINT_NOT_CURRENT_PARAM);

                        if (i < params->len - 1)
                            rend.write(", ");
                    }
                    rend.write(")");

                    // write result
                    if (result != NULL && result->len > 0) {
                        rend.write(" ");
                        if (result->len == 1 && is_goident_empty(result->at(0).name)) {
                            add_token_change(HINT_TYPE);
                            rend.write_type(result->at(0).gotype);
                        } else {
                            rend.write("(");
                            for (u32 i = 0; i < result->len; i++) {
                                if (!is_goident_empty(result->at(i).name)) {
                                    add_token_change(HINT_NAME);
                                    rend.write("%s ", result->at(i).name);
                                }
                                add_token_change(HINT_TYPE);
                                rend.write_type(result->at(i).gotype);
                                add_token_change(HINT_NORMAL);
                                if (i < result->len - 1)
                                    rend.write(", ");
                            }
                            rend.write(")");
                        }
                    }
                }

                help_text = rend.finish();
            }

            boxf bg;
            bg.w = font->width * strlen(help_text) + settings.parameter_hint_padding_x * 2;
            bg.h = font->height + settings.parameter_hint_padding_y * 2;
            bg.x = min(actual_parameter_hint_start.x, world.window_size.x - bg.w);
            bg.y = min(actual_parameter_hint_start.y - font->offset_y - bg.h - settings.parameter_hint_margin_y, world.window_size.y - bg.h);

            draw_bordered_rect_outer(bg, rgba(COLOR_DARK_GREY), rgba(COLOR_WHITE), 1, 4);

            auto text_pos = bg.pos;
            text_pos.x += settings.parameter_hint_padding_x;
            text_pos.y += settings.parameter_hint_padding_y;

            text_pos.y += font->offset_y;

            {
                u32 len = strlen(help_text);
                vec3f color = COLOR_MEDIUM_DARK_GREY;
                float opacity = 1.0;

                int j = 0;

                for (u32 i = 0; i < len; i++) {
                    while (j < token_changes.len && i == token_changes[j].index) {
                        switch (token_changes[j].token) {
                        case HINT_CURRENT_PARAM: opacity = 1.0; break;
                        case HINT_NOT_CURRENT_PARAM: opacity = 0.4; break;
                        case HINT_NAME: color = COLOR_WHITE; break;
                        case HINT_TYPE: color = COLOR_THEME_5; break;
                        case HINT_NORMAL: color = COLOR_MEDIUM_DARK_GREY; break;
                        }

                        j++;
                    }
                    draw_char(&text_pos, help_text[i], rgba(color, opacity));
                }
            }
        } while (0);
    }

    flush_verts();

    recalculate_view_sizes();

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
            editor.ensure_cursor_on_screen();
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
