#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include "ui.hpp"
#include "common.hpp"
#include "world.hpp"
#include "go.hpp"
#include "unicode.hpp"
#include "settings.hpp"
#include "icons.h"
#include "fzy_match.h"

#define _USE_MATH_DEFINES // what the fuck is this lol
#include <math.h>
#include "tree_sitter_crap.hpp"
#include "cpcolors.hpp"

#include <GLFW/glfw3.h>
#include <inttypes.h>

void open_ft_node(FT_Node *it);

UI ui;
Global_Colors global_colors;

ccstr get_menu_command_key(Command cmd);
bool menu_command(Command cmd, bool selected = false);

void init_global_colors() {
#if 0 // def DEBUG_MODE
    if (check_path("/Users/brandon/.cpcolors") == CPR_FILE) {
        File f;
        f.init_read("/Users/brandon/.cpcolors");
        f.read((char*)&global_colors, sizeof(global_colors));
        f.cleanup();
        return;
    }
#endif

    memcpy(&global_colors, _cpcolors, min(sizeof(global_colors), _cpcolors_len));
}

ccstr format_key(int mods, ccstr key, bool icon) {
    List<ccstr> parts; parts.init();

#ifndef OS_MAC
    icon = false;
#endif

    if (mods & KEYMOD_CMD)   parts.append(icon ? ICON_MD_KEYBOARD_COMMAND_KEY : "Cmd");
    if (mods & KEYMOD_SHIFT) parts.append(icon ? ICON_MD_ARROW_UPWARD : "Shift");
#if OS_MAC
    if (mods & KEYMOD_ALT)   parts.append(icon ? ICON_MD_KEYBOARD_OPTION_KEY : "Option");
#else
    if (mods & KEYMOD_ALT)   parts.append("Alt");
#endif
    if (mods & KEYMOD_CTRL)  parts.append(icon ? ICON_MD_KEYBOARD_CONTROL_KEY : "Ctrl");

    Text_Renderer rend; rend.init();
    For (parts) {
        if (icon)
            rend.write("%s", it);
        else
            rend.write("%s + ", it);
    }
    rend.write("%s", key);
    return rend.finish();
}

ccstr format_key(int mods, int key) {
    auto keystr = "TODO"; // TODO: convert key to string
    return format_key(mods, keystr);
}

bool Font::init(u8* font_data, u32 font_size, int texture_id) {
    height = font_size;
    tex_size = (i32)pow(2.0f, (i32)log2(sqrt((float)height * height * 8 * 8 * 128)) + 1);

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
    stbtt_PackSetOversampling(&context, 8, 8);

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

    bool OurBeginPopupContextWindow(const char* str_id, ImGuiPopupFlags popup_flags = 1) {
        ImGuiWindow* window = GImGui->CurrentWindow;
        if (!str_id)
            str_id = "window_context";
        ImGuiID id = window->GetID(str_id);
        int mouse_button = (popup_flags & ImGuiPopupFlags_MouseButtonMask_);
        if (IsMouseReleased(mouse_button) && IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup))
            if (!(popup_flags & ImGuiPopupFlags_NoOpenOverItems) || !IsAnyItemHovered())
                OpenPopupEx(id, popup_flags);
        return BeginPopupEx(id, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings);
    }
}

ccstr get_key_name(int key) {
    switch (key) {
    case GLFW_KEY_F1: return "F1";
    case GLFW_KEY_F2: return "F2";
    case GLFW_KEY_F3: return "F3";
    case GLFW_KEY_F4: return "F4";
    case GLFW_KEY_F5: return "F5";
    case GLFW_KEY_F6: return "F6";
    case GLFW_KEY_F7: return "F7";
    case GLFW_KEY_F8: return "F8";
    case GLFW_KEY_F9: return "F9";
    case GLFW_KEY_F10: return "F10";
    case GLFW_KEY_F11: return "F11";
    case GLFW_KEY_F12: return "F12";
    case GLFW_KEY_TAB: return "Tab";
    case GLFW_KEY_ENTER: return "Enter";
    }
    return glfwGetKeyName(key, 0);
}

ccstr get_menu_command_key(Command cmd) {
    auto info = command_info_table[cmd];
    if (info.key == 0) return NULL;

    auto keyname = get_key_name(info.key);
    if (keyname == NULL) return NULL;

    auto s = alloc_list<char>();
    for (int i = 0, len = strlen(keyname); i < len; i++) {
        auto it = keyname[i];
        if (i == 0) it = toupper(it);
        s->append(it);
    }
    s->append('\0');
    return format_key(info.mods, s->items, true);
}

bool menu_command(Command cmd, bool selected) {
    bool clicked = ImGui::MenuItem(
        get_command_name(cmd),
        get_menu_command_key(cmd),
        selected,
        is_command_enabled(cmd)
    );

    if (clicked) handle_command(cmd, true);
    return clicked;
}

int get_line_number_width(Editor *editor) {
    auto buf = editor->buf;

    u32 maxval = 0;
    if (world.replace_line_numbers_with_bytecounts) {
        For (buf->bytecounts)
            if (it > maxval)
                maxval = it;
    } else {
        maxval = buf->lines.len;
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

vec3f color_darken(vec3f color, float amount) {
    vec3f ret;
    ret.r = color.r * (1 - amount);
    ret.g = color.g * (1 - amount);
    ret.b = color.b * (1 - amount);
    return ret;
}

vec4f rgba(ccstr hex, float alpha) {
    return rgba(rgb_hex(hex), alpha);
}

ImVec4 to_imcolor(vec4f color) {
    return (ImVec4)ImColor(color.r, color.g, color.b, color.a);
}

ImVec4 to_imcolor(vec3f color) {
    return (ImVec4)ImColor(color.r, color.g, color.b, 1.0);
}

bool get_type_color(Ast_Node *node, Editor *editor, vec4f *out) {
    switch (node->type()) {
    case TS_PACKAGE:
    case TS_IMPORT:
    case TS_CONST:
    case TS_VAR:
    case TS_FUNC:
    case TS_TYPE:
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
        *out = rgba(global_colors.keyword);
        return true;

    case TS_STRUCT:
    case TS_INTERFACE:
    case TS_MAP:
    case TS_CHAN:
        *out = rgba(global_colors.type);
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
        *out = rgba(global_colors.punctuation, 0.75);
        return true;

    case TS_INT_LITERAL:
    case TS_FLOAT_LITERAL:
    case TS_IMAGINARY_LITERAL:
    case TS_RUNE_LITERAL:
    case TS_NIL:
    case TS_TRUE:
    case TS_FALSE:
        *out = rgba(global_colors.number_literal);
        return true;

    case TS_COMMENT:
        *out = rgba(global_colors.comment);
        return true;

    case TS_INTERPRETED_STRING_LITERAL:
    case TS_RAW_STRING_LITERAL:
        *out = rgba(global_colors.string_literal);
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

            ccstr builtin_types[] = {
                // technically keywords, but look better here
                "map", "chan", "bool", "byte", "complex128", "complex64",
                "error", "float32", "float64", "int", "int16", "int32",
                "int64", "int8", "rune", "string", "uint", "uint16", "uint32",
                "uint64", "uint8", "uintptr",
            };

            ccstr builtin_others[] = {
                "append", "cap", "close", "complex", "copy", "delete", "imag",
                "len", "make", "new", "panic", "real", "recover",
            };

            char token[16] = {0};
            auto it = editor->iter(node->start());
            for (u32 i = 0; i < _countof(token) && it.pos != node->end(); i++)
                token[i] = it.next();

            For (keywords) {
                if (streq(it, token)) {
                    *out = rgba(global_colors.keyword);
                    return true;
                }
            }

            For (builtin_types) {
                if (streq(it, token)) {
                    *out = rgba(global_colors.type);
                    return true;
                }
            }

            For (builtin_others) {
                if (streq(it, token)) {
                    *out = rgba(global_colors.builtin);
                    return true;
                }
            }
        }
        break;
    }

    return false;
}

Pretty_Menu *UI::pretty_menu_start(ImVec2 padding) {
    auto ret = alloc_object(Pretty_Menu);
    ret->drawlist = ImGui::GetWindowDrawList();
    ret->padding = padding;
    return ret;
}

void UI::pretty_menu_text(Pretty_Menu *pm, ccstr text, ImU32 color) {
    if (color == PM_DEFAULT_COLOR)
        color = pm->text_color;

    pm->drawlist->AddText(pm->pos, color, text);
    pm->pos.x += ImGui::CalcTextSize(text).x;
}

void UI::pretty_menu_item(Pretty_Menu *pm, bool selected) {
    auto h = ImGui::CalcTextSize("Some Text").y;
    auto w = ImGui::GetContentRegionAvail().x;

    auto pad = pm->padding;
    auto tl = ImGui::GetCursorScreenPos();
    auto br = tl + ImVec2(w, h);
    br.y += (pad.y * 2);

    pm->tl = tl;
    pm->br = br;
    pm->text_tl = tl + pad;
    pm->text_br = br - pad;

    ImGui::Dummy(ImVec2(0.0, pm->br.y - pm->tl.y));
    if (selected) {
        pm->text_color = IM_COL32(0, 0, 0, 255);
        pm->drawlist->AddRectFilled(pm->tl, pm->br, IM_COL32(255, 255, 255, 255), 4);
    } else {
        pm->text_color = ImGui::GetColorU32(ImGuiCol_Text);
    }

    pm->pos = pm->text_tl;
}

void UI::begin_window(ccstr title, Wnd *wnd, int flags, bool noclose) {
    ImGui::Begin(title, noclose ? NULL : &wnd->show, flags);
    init_window(wnd);
}

void UI::init_window(Wnd *wnd) {
    // https://github.com/ocornut/imgui/issues/4293#issuecomment-914322632
    bool might_be_focusing = (!wnd->focused_prev && wnd->focused);
    wnd->focused_prev = wnd->focused;
    wnd->focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
    wnd->focusing = might_be_focusing && wnd->focused;

    wnd->appearing = ImGui::IsWindowAppearing();

    auto checkflag = [](bool *b) {
        auto ret = *b;
        *b = false;
        return ret;
    };

    if (checkflag(&wnd->cmd_focus)) {
        ImGui::SetWindowFocus();
    }

    if (checkflag(&wnd->cmd_make_visible_but_dont_focus)) {
        ImGui::SetWindowFocus();
        ImGui::SetWindowFocus(NULL);
    }
}

void UI::begin_centered_window(ccstr title, Wnd *wnd, int flags, int width, bool noclose) {
    if (width != -1) {
        ImGui::SetNextWindowSize(ImVec2(width, -1));
    } else {
        flags |= ImGuiWindowFlags_AlwaysAutoResize;
    }
    flags |= ImGuiWindowFlags_NoDocking;

    ImGui::SetNextWindowPos(ImVec2(world.window_size.x/2, 150), ImGuiCond_Always, ImVec2(0.5f, 0));
    begin_window(title, wnd, flags, noclose);
}

void UI::help_marker(fn<void()> cb) {
    ImGui::TextDisabled(ICON_MD_HELP_OUTLINE);
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 20.0f);
        cb();
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void UI::help_marker(ccstr text) {
    help_marker([&]() {
        ImGui::TextWrapped(text);
    });
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
        case GODECL_PARAM:
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

        if (ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered(0)) {
            auto editor = get_current_editor();
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
        if (ImGui::IsMouseClicked(0)) ret |= MOUSE_CLICKED;
        if (ImGui::IsMouseClicked(1)) ret |= MOUSE_RCLICKED;
        if (ImGui::IsMouseClicked(2)) ret |= MOUSE_MCLICKED;

        if (ImGui::IsMouseDoubleClicked(0)) ret |= MOUSE_DBLCLICKED;
        if (ImGui::IsMouseDoubleClicked(1)) ret |= MOUSE_RDBLCLICKED;
        if (ImGui::IsMouseDoubleClicked(2)) ret |= MOUSE_MDBLCLICKED;
    }
    return ret;
}

void UI::draw_debugger_var(Draw_Debugger_Var_Args *args) {
    SCOPED_FRAME();

    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    bool open = false;
    auto var = args->var;
    auto watch = args->watch;

    Dlv_Var** selection = NULL;
    if (watch != NULL)
        selection = &world.wnd_debugger.watch_selection;
    else
        selection = &world.wnd_debugger.locals_selection;

    {
        SCOPED_FRAME();

        int tree_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (*selection == var)
            tree_flags |= ImGuiTreeNodeFlags_Selected;
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

        ccstr final_var_name = NULL;

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

                final_var_name = watch->expr;
                open = ImGui::TreeNodeEx(var, tree_flags, "%s", watch->expr) && !leaf;

                // if (leaf) ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());

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
            final_var_name = var_name;

            for (int i = 0; i < args->indent; i++)
                ImGui::Indent();

            // if (leaf) ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());

            open = ImGui::TreeNodeEx(var, tree_flags, "%s", var_name) && !leaf;

            // if (leaf) ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());

            for (int i = 0; i < args->indent; i++)
                ImGui::Unindent();
        }

        if (final_var_name != NULL) {
            if (ImGui::OurBeginPopupContextItem(our_sprintf("dbg_copyvalue_%lld", (uptr)var))) {
                if (ImGui::Selectable("Copy Name")) {
                    glfwSetClipboardString(world.window, final_var_name);
                }
                ImGui::EndPopup();
            }
        }

        // Assumes the last thing drawn was the TreeNode.

        if (*selection == var) {
            if (watch != NULL) {
                auto delete_that_fucker = [&]() -> bool {
                    if (args->some_watch_being_edited) return false;
                    if (dbg_editing_new_watch) return false;

                    if (imgui_get_keymods() == KEYMOD_NONE) {
                        if (imgui_special_key_pressed(ImGuiKey_Backspace))
                            return true;
                        if (imgui_special_key_pressed(ImGuiKey_Delete))
                            return true;
                    }

                    return false;
                };

                if (delete_that_fucker()) {
                    auto old_len = world.dbg.watches.len;

                    world.dbg.push_call(DLVC_DELETE_WATCH, [&](auto it) {
                        it->delete_watch.watch_idx = args->watch_index;
                    });

                    if (watch != NULL) {
                        auto next = args->watch_index;
                        if (next >= old_len-1)
                            next--;
                        if (next < old_len-1)
                            *selection = &world.dbg.watches[next].value;
                    } else {
                        do {
                            auto &state = world.dbg.state;
                            auto goroutine = state.goroutines.find([&](auto it) { return it->id == state.current_goroutine_id; });
                            if (goroutine == NULL)
                                break;
                            if (state.current_frame >= goroutine->frames->len)
                                break;
                            auto frame = &goroutine->frames->items[state.current_frame];

                            auto next = args->locals_index;
                            if (next >= frame->locals->len + frame->args->len)
                                next--;
                            if (next >= frame->locals->len + frame->args->len)
                                break;

                            if (next < frame->locals->len) {
                                *selection = &frame->locals->at(next);
                            } else {
                                next -= frame->locals->len;
                                *selection = &frame->args->at(next);
                            }
                        } while (0);
                    }
                }
            }

            if (imgui_get_keymods() == KEYMOD_PRIMARY && imgui_key_pressed('c')) {
                // ???
            }
        }

        if (watch != NULL) {
            if (!args->is_child) {
                if (ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered(0)) {
                    watch->editing = true;
                    watch->open_before_editing = open;
                    watch->edit_first_frame = true;
                }
            }
        }

        if (watch == NULL || !watch->editing) {
            if (ImGui::IsItemClicked()) {
                *selection = var;
            }
        }
    }

    if (var->incomplete()) {
        for (int i = 0; i < args->indent; i++)
            ImGui::Indent();
        ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());

        imgui_push_ui_font();
        bool clicked = ImGui::SmallButton("Load more...");
        imgui_pop_font();

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

        bool copy = false;

        if (*selection == var)
            if (imgui_get_keymods() == KEYMOD_PRIMARY)
                if (imgui_key_pressed('c'))
                    copy = true;

        if (ImGui::OurBeginPopupContextItem(our_sprintf("dbg_copyvalue_%lld", (uptr)var))) {
            if (ImGui::Selectable("Copy Value"))
                copy = true;
            ImGui::EndPopup();
        }

        if (copy) glfwSetClipboardString(world.window, underlying_value);

        ImGui::TableNextColumn();
        if (watch == NULL || watch->state != DBGWATCH_ERROR) {
            ccstr type_name = NULL;
            if (var->type == NULL || var->type[0] == '\0') {
                switch (var->kind) {
                case GO_KIND_BOOL: type_name = "bool"; break;
                case GO_KIND_INT: type_name = "int"; break;
                case GO_KIND_INT8: type_name = "int8"; break;
                case GO_KIND_INT16: type_name = "int16"; break;
                case GO_KIND_INT32: type_name = "int32"; break;
                case GO_KIND_INT64: type_name = "int64"; break;
                case GO_KIND_UINT: type_name = "uint"; break;
                case GO_KIND_UINT8: type_name = "uint8"; break;
                case GO_KIND_UINT16: type_name = "uint16"; break;
                case GO_KIND_UINT32: type_name = "uint32"; break;
                case GO_KIND_UINT64: type_name = "uint64"; break;
                case GO_KIND_UINTPTR: type_name = "uintptr"; break;
                case GO_KIND_FLOAT32: type_name = "float32"; break;
                case GO_KIND_FLOAT64: type_name = "float64"; break;
                case GO_KIND_COMPLEX64: type_name = "complex64"; break;
                case GO_KIND_COMPLEX128: type_name = "complex128"; break;
                case GO_KIND_ARRAY: type_name = "<array>"; break;
                case GO_KIND_CHAN: type_name = "<chan>"; break;
                case GO_KIND_FUNC: type_name = "<func>"; break;
                case GO_KIND_INTERFACE: type_name = "<interface>"; break;
                case GO_KIND_MAP: type_name = "<map>"; break;
                case GO_KIND_PTR: type_name = "<pointer>"; break;
                case GO_KIND_SLICE: type_name = "<slice>"; break;
                case GO_KIND_STRING: type_name = "string"; break;
                case GO_KIND_STRUCT: type_name = "<struct>"; break;
                case GO_KIND_UNSAFEPOINTER: type_name = "unsafe.Pointer"; break;
                default: type_name = "<unknown>"; break;
                }
            } else {
                type_name = var->type;
            }

            if (type_name != NULL) {
                ImGui::TextWrapped("%s", type_name);
                if (ImGui::OurBeginPopupContextItem(our_sprintf("dbg_copyvalue_%lld", (uptr)var))) {
                    if (ImGui::Selectable("Copy Type")) {
                        glfwSetClipboardString(world.window, type_name);
                    }
                    ImGui::EndPopup();
                }
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
                    Draw_Debugger_Var_Args a;
                    a.watch = watch;
                    a.some_watch_being_edited = args->some_watch_being_edited;
                    a.watch_index = args->watch_index;
                    a.is_child = true;
                    a.var = &var->children->at(k+1);
                    a.index_type = INDEX_MAP;
                    a.key = &var->children->at(k);
                    a.indent = args->indent + 1;
                    draw_debugger_var(&a);
                }
            } else {
                bool isarr = (var->kind == GO_KIND_ARRAY || var->kind == GO_KIND_SLICE);
                for (int k = 0; k < var->children->len; k++) {
                    Draw_Debugger_Var_Args a;
                    a.watch = watch;
                    a.some_watch_being_edited = args->some_watch_being_edited;
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
                    draw_debugger_var(&a);
                }
            }
        }
    }
}

ccstr UI::var_value_as_string(Dlv_Var *var) {
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

void UI::draw_debugger() {
    world.wnd_debugger.focused = ImGui::IsWindowFocused();

    auto &dbg = world.dbg;
    auto &state = dbg.state;
    auto &wnd = world.wnd_debugger;

    {
        ImGui::SetNextWindowDockID(dock_bottom_id, ImGuiCond_Once);
        ImGui::Begin("Call Stack");
        imgui_push_mono_font();

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

        imgui_pop_font();
        ImGui::End();
    }

    {
        ImGui::SetNextWindowDockID(dock_bottom_right_id, ImGuiCond_Once);

        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::Begin("Local Variables");
            ImGui::PopStyleVar();
        }

        imgui_push_mono_font();

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

                    int index = 0;

                    if (frame->locals != NULL) {
                        For (*frame->locals) {
                            Draw_Debugger_Var_Args a; ptr0(&a);
                            a.var = &it;
                            a.is_child = false;
                            a.watch = NULL;
                            a.index_type = INDEX_NONE;
                            a.locals_index = index++;
                            draw_debugger_var(&a);
                        }
                    }

                    if (frame->args != NULL) {
                        For (*frame->args) {
                            Draw_Debugger_Var_Args a; ptr0(&a);
                            a.var = &it;
                            a.is_child = false;
                            a.watch = NULL;
                            a.index_type = INDEX_NONE;
                            a.locals_index = index++;
                            draw_debugger_var(&a);
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

        imgui_pop_font();
        ImGui::End();
    }

    {
        ImGui::SetNextWindowDockID(dock_bottom_right_id, ImGuiCond_Once);

        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::Begin("Watches");
            ImGui::PopStyleVar();
        }

        imgui_push_mono_font();

        if (world.dbg.state_flag == DLV_STATE_PAUSED) {
            auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable;
            if (ImGui::BeginTable("vars", 3, flags)) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_NoHide);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_NoHide);
                ImGui::TableHeadersRow();

                bool some_watch_being_edited = false;
                For (world.dbg.watches) {
                    if (it.editing) {
                        some_watch_being_edited = true;
                        break;
                    }
                }

                for (int k = 0; k < world.dbg.watches.len; k++) {
                    auto &it = world.dbg.watches[k];
                    if (it.deleted) continue;

                    Draw_Debugger_Var_Args a; ptr0(&a);
                    a.var = &it.value;
                    a.is_child = false;
                    a.watch = &it;
                    a.some_watch_being_edited = some_watch_being_edited;
                    a.index_type = INDEX_NONE;
                    a.watch_index = k;
                    draw_debugger_var(&a);
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

                    dbg_editing_new_watch = ImGui::IsItemFocused();

                    ImGui::TableNextColumn(); // value
                    ImGui::TableNextColumn(); // type
                }

                ImGui::EndTable();
            }
        }

        imgui_pop_font();
        ImGui::End();
    }

    /*
    {
        ImGui::SetNextWindowDockID(dock_bottom_right_id, ImGuiCond_Once);
        ImGui::Begin("Global Variables");
        imgui_push_mono_font();
        ImGui::Text("@Incomplete: global vars go here");
        imgui_pop_font();
        ImGui::End();
    }
    */
}

void open_rename(FT_Node *target) {
    auto &wnd = world.wnd_rename_file_or_folder;

    wnd.show = true;
    wnd.target = target;

    strcpy_safe_fixed(wnd.location, ft_node_to_path(target));
    strcpy_safe_fixed(wnd.name, target->name);
}

void UI::imgui_small_newline() {
    ImGui::Dummy(ImVec2(0.0f, ImGui::GetFrameHeightWithSpacing() * 1/4));
}

bool UI::imgui_input_text_full(ccstr label, char *buf, int count, int flags) {
    ImGui::PushItemWidth(-1);
    {
        imgui_push_ui_font();
        ImGui::Text("%s", label);
        imgui_pop_font();
    }
    auto ret = ImGui::InputText(our_sprintf("###%s", label), buf, count, flags);
    ImGui::PopItemWidth();

    return ret;
}

#define imgui_input_text_full_fixbuf(x, y) imgui_input_text_full(x, y, _countof(y))

void UI::open_project_settings() {
    auto &wnd = world.wnd_project_settings;
    if (wnd.show) return;

    wnd.focus_general_settings = true;

    {
        wnd.pool.cleanup();
        wnd.pool.init();
        SCOPED_MEM(&wnd.pool);
        wnd.tmp = project_settings.copy();
    }

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
    if (io.KeySuper) ret |= KEYMOD_CMD;
    if (io.KeyCtrl) ret |= KEYMOD_CTRL;
    if (io.KeyShift) ret |= KEYMOD_SHIFT;
    if (io.KeyAlt) ret |= KEYMOD_ALT;
    return ret;
}

void UI::imgui_push_mono_font() {
    ImGui::PushFont(world.ui.im_font_mono);
}

void UI::imgui_push_ui_font() {
    ImGui::PushFont(world.ui.im_font_ui);
}

void UI::imgui_pop_font() {
    ImGui::PopFont();
}

void open_ft_node(FT_Node *it) {
    SCOPED_FRAME();
    auto rel_path = ft_node_to_path(it);
    auto full_path = path_join(world.current_path, rel_path);
    if (focus_editor(full_path) != NULL)
        ImGui::SetWindowFocus(NULL);
}

struct Coarse_Clipper {
    ImVec2 pos;
    ImRect unclipped_rect;
    float last_line_height;
    bool before;

    void init() {
        before = true;
        unclipped_rect = get_window_clip_area();
        pos = ImGui::GetCursorScreenPos();
    }

    ImRect get_window_clip_area() {
        auto g = ImGui::GetCurrentContext();
        auto window = g->CurrentWindow;
        auto ret = window->ClipRect;

        // I don't know and don't care what this does. It's ripped from imgui.cpp
        // and makes my code "just work."
        if (g->NavMoveRequest)
            ret.Add(g->NavScoringRect);
        if (g->NavJustMovedToId && window->NavLastIds[0] == g->NavJustMovedToId)
            ret.Add(ImRect(window->Pos + window->NavRectRel[0].Min, window->Pos + window->NavRectRel[0].Max));

        return ret;
    }

    bool add(float h) {
        if (before) {
            if (pos.y + h >= unclipped_rect.Min.y) {
                ImGui::SetCursorScreenPos(pos);
                before = false;
                return false;
            }
            pos.y += h;
            return true;
        }

        last_line_height = h;

        if (pos.y > unclipped_rect.Max.y) {
            pos.y += h;
            return false;
        }

        return true;
    }

    void finish() {
        auto g = ImGui::GetCurrentContext();
        ImGuiWindow* window = g->CurrentWindow;
        window->DC.CursorPos.y = pos.y;

        window->DC.CursorPos.y = pos.y;
        window->DC.CursorMaxPos.y = ImMax(window->DC.CursorMaxPos.y, pos.y);
        window->DC.CursorPosPrevLine.y = window->DC.CursorPos.y - last_line_height;
        window->DC.PrevLineSize.y = last_line_height;
    }
};

void UI::focus_keyboard(Wnd *wnd, int cond) {
    if (wnd->appearing) {
        if (cond & FKC_APPEARING)
            ImGui::SetKeyboardFocusHere();
    } else if (!wnd->first_open_focus_twice_done) {
        wnd->first_open_focus_twice_done = true;
        if (cond & FKC_APPEARING) {
            ImGui::SetKeyboardFocusHere();
        }
    } else if (wnd->focusing) {
        if (cond & FKC_FOCUSING)
            ImGui::SetKeyboardFocusHere();
    }
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

        /*
        // if the dockspace is focused, means we just closed last docked window
        // but keyboard capture still going to imgui, so we need to SetWindowFocus(NULL)
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootWindow))
            ImGui::SetWindowFocus(NULL);
        */

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
            menu_command(CMD_NEW_FILE);
            menu_command(CMD_SAVE_FILE);
            menu_command(CMD_SAVE_ALL);
            ImGui::Separator();
            menu_command(CMD_EXIT);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            menu_command(CMD_UNDO);
            menu_command(CMD_REDO);
            ImGui::Separator();
            menu_command(CMD_SEARCH);
            menu_command(CMD_SEARCH_AND_REPLACE);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            menu_command(CMD_FILE_EXPLORER, world.file_explorer.show);
            menu_command(CMD_ERROR_LIST, world.error_list.show);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Navigate")) {
            menu_command(CMD_GO_TO_FILE);
            menu_command(CMD_GO_TO_SYMBOL);
            menu_command(CMD_GO_TO_NEXT_ERROR);
            menu_command(CMD_GO_TO_PREVIOUS_ERROR);
            menu_command(CMD_GO_TO_DEFINITION);
            ImGui::Separator();
            menu_command(CMD_FIND_REFERENCES);
            menu_command(CMD_FIND_IMPLEMENTATIONS);
            menu_command(CMD_FIND_INTERFACES);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Format")) {
            menu_command(CMD_FORMAT_FILE);
            menu_command(CMD_FORMAT_FILE_AND_ORGANIZE_IMPORTS);
            // menu_command(CMD_FORMAT_SELECTION);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Refactor")) {
            menu_command(CMD_RENAME);
            menu_command(CMD_GENERATE_IMPLEMENTATION);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Project")) {
            menu_command(CMD_ADD_NEW_FILE);
            menu_command(CMD_ADD_NEW_FOLDER);
            ImGui::Separator();
            menu_command(CMD_PROJECT_SETTINGS);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Build")) {
            menu_command(CMD_BUILD);

            ImGui::Separator();

            if (ImGui::BeginMenu("Windows...")) {
                menu_command(CMD_BUILD_RESULTS, world.error_list.show);
                ImGui::EndMenu();
            }

            ImGui::Separator();

            // TODO: add these as commands
            if (ImGui::BeginMenu("Select Active Build Profile..."))  {
                if (project_settings.build_profiles->len > 0) {
                    for (int i = 0; i < project_settings.build_profiles->len; i++) {
                        auto &it = project_settings.build_profiles->at(i);
                        if (ImGui::MenuItem(it.label, NULL, project_settings.active_build_profile == i, true)) {
                            project_settings.active_build_profile = i;
                            write_project_settings();
                        }
                    }
                } else {
                    ImGui::MenuItem("No profiles to select.", NULL, false, false);
                }
                ImGui::EndMenu();
            }

            menu_command(CMD_BUILD_PROFILES);

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Debug")) {
            if (world.dbg.state_flag == DLV_STATE_PAUSED)
                menu_command(CMD_CONTINUE);
            else
                menu_command(CMD_START_DEBUGGING);

            menu_command(CMD_DEBUG_TEST_UNDER_CURSOR);
            menu_command(CMD_BREAK_ALL);
            menu_command(CMD_STOP_DEBUGGING);
            menu_command(CMD_STEP_OVER);
            menu_command(CMD_STEP_INTO);
            menu_command(CMD_STEP_OUT);
            // menu_command(CMD_RUN_TO_CURSOR); world.dbg.state_flag == DLV_STATE_PAUSED

            ImGui::Separator();

            menu_command(CMD_TOGGLE_BREAKPOINT);
            menu_command(CMD_DELETE_ALL_BREAKPOINTS);

            ImGui::Separator();

            if (ImGui::BeginMenu("Windows..."))  {
                menu_command(CMD_DEBUG_OUTPUT, world.wnd_debug_output.show);
                ImGui::EndMenu();
            }

            ImGui::Separator();

            if (ImGui::BeginMenu("Select Active Debug Profile..."))  {
                // TODO(robust): we should handle the builtins more explicitly, instead of using hardcoded value of 1
                if (project_settings.debug_profiles->len > 1) {
                    for (int i = 1; i < project_settings.debug_profiles->len; i++) {
                        auto &it = project_settings.debug_profiles->at(i);
                        if (ImGui::MenuItem(it.label, NULL, project_settings.active_debug_profile == i, true)) {
                            project_settings.active_debug_profile = i;
                            write_project_settings();
                        }
                    }
                } else {
                    ImGui::MenuItem("No profiles to select.", NULL, false, false);
                }
                ImGui::EndMenu();
            }

            menu_command(CMD_DEBUG_PROFILES);

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools")) {
            // should we allow this even when not ready, so it can be used as an escape hatch if the indexer gets stuck?

            menu_command(CMD_RESCAN_INDEX);
            menu_command(CMD_OBLITERATE_AND_RECREATE_INDEX);
            ImGui::Separator();
            menu_command(CMD_OPTIONS);

#ifdef DEBUG_MODE

            ImGui::Separator();

            if (ImGui::BeginMenu("Debug Tools")) {
                ImGui::MenuItem("ImGui demo", NULL, &world.windows_open.im_demo);
                ImGui::MenuItem("ImGui metrics", NULL, &world.windows_open.im_metrics);
                ImGui::MenuItem("AST viewer", NULL, &world.wnd_editor_tree.show);
                ImGui::MenuItem("History viewer", NULL, &world.wnd_history.show);
                ImGui::MenuItem("Toplevels viewer", NULL, &world.wnd_editor_toplevels.show);
                ImGui::MenuItem("Show mouse position", NULL, &world.wnd_mouse_pos.show);
                ImGui::MenuItem("Style editor", NULL, &world.wnd_style_editor.show);
                ImGui::MenuItem("Replace line numbers with bytecounts", NULL, &world.replace_line_numbers_with_bytecounts);
                ImGui::MenuItem("Randomly move cursor around", NULL, &world.randomly_move_cursor_around);
                ImGui::MenuItem("Disable framerate cap", NULL, &world.turn_off_framerate_cap);
                ImGui::MenuItem("Hover Info", NULL, &world.wnd_hover_info.show);

                if (ImGui::MenuItem("Cleanup unused memory")) {
                    world.indexer.message_queue.add([&](auto msg) {
                        msg->type = GOMSG_CLEANUP_UNUSED_MEMORY;
                    });
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Expire trial")) {
                    if (world.auth.state != AUTH_TRIAL) {
                        tell_user("User is not currently in a trial state.", NULL);
                    } else {
                        world.auth.trial_start = get_unix_time() - 1000 * 60 * 60 * 24 * 14;
                        write_auth();
                    }
                }

                if (ImGui::MenuItem("Start new trial")) {
                    world.auth.state = AUTH_TRIAL;
                    world.auth.trial_start = get_unix_time();
                    write_auth();
                    tell_user(NULL, "Ok, done.");
                }

                ImGui::EndMenu();
            }
#endif

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            menu_command(CMD_ABOUT);
            menu_command(CMD_DOCUMENTATION);

            ImGui::Separator();

            menu_command(CMD_BUY_LICENSE);
            menu_command(CMD_ENTER_LICENSE);

            ImGui::EndMenu();
        }

        ImGui::PopStyleVar(2);
        ImGui::EndMainMenuBar();
    }

    if (world.wnd_options.show) {
        auto &wnd = world.wnd_options;
        auto &tmp = wnd.tmp;

        begin_window("Options", &wnd, ImGuiWindowFlags_AlwaysAutoResize);

        if (wnd.focused) {
            auto mods = imgui_get_keymods();
            switch (mods) {
            case KEYMOD_NONE:
                if (imgui_special_key_pressed(ImGuiKey_Escape)) wnd.show = false;
                break;
            }
        }

        auto &outer_style = ImGui::GetStyle();
        int outer_window_padding = outer_style.WindowPadding.y;

        if (ImGui::BeginTabBar("wnd_options_tab_bar", 0)) {
            if (ImGui::BeginTabItem("Editor Settings", NULL)) {
                auto begin_container_child = [&]() {
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
                    defer { ImGui::PopStyleVar(); };

                    ImGui::BeginChild("container", ImVec2(200, 200), false, ImGuiWindowFlags_AlwaysUseWindowPadding);
                };

                begin_container_child(); {
                    ImGui::PushItemWidth(-1);
                    imgui_push_ui_font();
                    {
                        ImGui::Text("Vim keybindings");
                        ImGui::SameLine();
                        help_marker("Enables Vim keybindings in the editor.\n\nThis requires a restart.");
                        ImGui::Checkbox("Enable", &tmp.enable_vim_mode);
                        if (ImGui::IsItemEdited())
                            wnd.something_that_needs_restart_was_changed = true;

                        imgui_small_newline();

                        ImGui::Text("Scroll offset");
                        ImGui::SameLine();
                        help_marker("The number of lines the editor will keep between your cursor and the top/bottom of the screen.");
                        ImGui::SliderInt("###scroll_offset", &tmp.scrolloff, 0, 10);

                        imgui_small_newline();

                        ImGui::Text("Tab size");
                        ImGui::SliderInt("###tab_size", &tmp.tabsize, 1, 8);
                    }
                    imgui_pop_font();
                    ImGui::PopItemWidth();
                } ImGui::EndChild();

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
                memcpy(&options, &tmp, sizeof(options));

                // write out options
                do {
                    auto configdir = GHGetConfigDir();
                    if (configdir == NULL) break;

                    auto filepath = path_join(configdir, ".options");

                    File f;
                    if (f.init_write(filepath) != FILE_RESULT_OK)
                        break;
                    defer { f.cleanup(); };

                    Serde serde;
                    serde.init(&f);
                    serde.write_type(&options, SERDE_OPTIONS);
                } while (0);

                if (wnd.something_that_needs_restart_was_changed) {
                    tell_user("One of the settings changed requires you to restart CodePerfect.", "Restart needed");
                }

                wnd.show = false;
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel")) {
                wnd.show = false;
            }
        }

        ImGui::End();
    }

    fn<void(Call_Hier_Node*, ccstr, bool)> render_call_hier;
    render_call_hier = [&](auto it, auto current_import_path, auto show_tests_and_benchmarks) {
        auto should_hide = [&](Call_Hier_Node *it) {
            auto decl = it->decl->decl->decl;
            if (world.indexer.get_godecl_recvname(decl) == NULL)
                if (!show_tests_and_benchmarks)
                    if (is_name_special_function(decl->name))
                        return true;
            return false;
        };

        auto fd = it->decl;
        auto res = fd->decl;
        auto ctx = res->ctx;
        auto decl = res->decl;

        if (should_hide(it)) return;

        auto name = decl->name;
        auto recvname = world.indexer.get_godecl_recvname(decl);
        if (recvname != NULL)
            name = our_sprintf("%s.%s", recvname, name);

        auto has_children = [&]() {
            For (*it->children)
                if (!should_hide(&it))
                    return true;
            return false;
        };

        auto flags = 0;
        if (has_children())
            flags = ImGuiTreeNodeFlags_OpenOnArrow;
        else
            flags = ImGuiTreeNodeFlags_Bullet;

        bool open = ImGui::TreeNodeEx(
            (void*)it,
            flags,
            "%s.%s (%s)",
            fd->package_name,
            name,
            get_path_relative_to(ctx->import_path, current_import_path)
        );

        if (ImGui::IsItemClicked()) {
            auto ref = it->ref;
            auto start = ref->is_sel ? ref->x_start : ref->start;
            goto_file_and_pos(fd->filepath, start);
        }

        if (open) {
            For (*it->children)
                render_call_hier(&it, current_import_path, show_tests_and_benchmarks);
            ImGui::TreePop();
        }
    };

    if (world.wnd_callee_hierarchy.show) {
        auto &wnd = world.wnd_callee_hierarchy;

        ImGui::SetNextWindowDockID(dock_sidebar_id, ImGuiCond_Once);
        begin_window(
            our_sprintf("Callee Hierarchy for %s###callee_hierarchy", wnd.declres->decl->name),
            &wnd,
            0,
            !wnd.done
        );

        if (wnd.done) {
            ImGui::Text("Done!");
            For (*wnd.results) render_call_hier(&it, wnd.current_import_path, true);
        } else {
            ImGui::Text("Generating callee hierarchy...");
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                cancel_callee_hierarchy();
                wnd.show = false;
            }
        }

        ImGui::End();
    }

    if (world.wnd_caller_hierarchy.show) {
        auto &wnd = world.wnd_caller_hierarchy;

        ImGui::SetNextWindowDockID(dock_sidebar_id, ImGuiCond_Once);
        begin_window(
            our_sprintf("Caller Hierarchy for %s###caller_hierarchy", wnd.declres->decl->name),
            &wnd,
            0,
            !wnd.done
        );

        if (wnd.done) {
            ImGui::Checkbox("Show tests, examples, and benchmarks", &wnd.show_tests_and_benchmarks);
            For (*wnd.results) render_call_hier(&it, wnd.current_import_path, wnd.show_tests_and_benchmarks);
        } else {
            ImGui::Text("Generating caller hierarchy...");
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                cancel_caller_hierarchy();
                wnd.show = false;
            }
        }

        ImGui::End();
    }

    if (world.wnd_find_interfaces.show) {
        auto &wnd = world.wnd_find_interfaces;

        ImGui::SetNextWindowDockID(dock_sidebar_id, ImGuiCond_Once);

        begin_window("Find Interfaces", &wnd, ImGuiWindowFlags_AlwaysAutoResize, !wnd.done);

        if (wnd.done) {
            ImGui::Checkbox("Show empty interfaces", &wnd.include_empty);
            ImGui::SameLine();
            help_marker("An empty interface{} is always implemented by every type. This checkbox lets you hide these from the results.");

            ImGui::Checkbox("Search everywhere", &wnd.search_everywhere);
            bool search_everywhere_changed = ImGui::IsItemEdited();
            ImGui::SameLine();
            help_marker("By default, Find Interfaces only looks at interfaces inside your workspace. This setting will search everywhere in your dependency tree.");

            defer {
                if (search_everywhere_changed) {
                    if (wnd.search_everywhere)
                        wnd.include_empty = false;
                    do_find_interfaces();
                }
            };

            imgui_small_newline();

            if (wnd.results != NULL && wnd.results->len > 0) {
                imgui_push_mono_font();

                int index = 0;
                For (*wnd.results) {
                    auto is_empty = [&]() {
                        auto gotype = it->decl->decl->gotype;
                        if (gotype == NULL) return false;
                        if (gotype->type != GOTYPE_INTERFACE) return false;
                        return isempty(gotype->interface_specs);
                    };

                    if (!wnd.include_empty && is_empty())
                        continue;

                    // TODO: refactor out custom draw
                    auto availwidth = ImGui::GetContentRegionAvail().x;
                    auto text_size = ImVec2(availwidth, ImGui::CalcTextSize("blah").y);
                    auto drawpos = ImGui::GetCursorScreenPos();
                    auto drawlist = ImGui::GetWindowDrawList();

                    auto draw_selectable = [&]() {
                        auto label = our_sprintf("##find_implementations_result__%d", index++);
                        return ImGui::Selectable(label, false, 0, text_size);
                    };

                    auto clicked = draw_selectable();

                    auto draw_text = [&](ccstr text) {
                        drawlist->AddText(drawpos, ImGui::GetColorU32(ImGuiCol_Text), text);
                        drawpos.x += ImGui::CalcTextSize(text).x;
                    };

                    auto import_path = it->decl->ctx->import_path;

                    if (!path_has_descendant(wnd.current_import_path, import_path)) {
                        ImGui::PushStyleColor(ImGuiCol_Text, to_imcolor(global_colors.muted));
                        defer { ImGui::PopStyleColor(); };

                        draw_text("(ext) ");
                    }

                    draw_text(our_sprintf("%s.%s", it->package_name, it->decl->decl->name));

                    ImGui::PushStyleColor(ImGuiCol_Text, to_imcolor(global_colors.muted));
                    {
                        ccstr path = NULL;

                        ccstr parents[] = {wnd.current_import_path, world.indexer.goroot};
                        For (parents) {
                            if (path_has_descendant(it, import_path)) {
                                path = get_path_relative_to(import_path, it);
                                break;
                            }
                        }

                        if (path == NULL) path = import_path;

                        draw_text(our_sprintf(" (%s)", path));
                    }
                    ImGui::PopStyleColor();

                    // TODO: previews?
                    if (clicked) goto_file_and_pos(it->filepath, it->decl->decl->name_start);
                }
                imgui_pop_font();
            } else {
                ImGui::Text("No interfaces found.");
            }
        } else {
            ImGui::Text("Searching...");
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                cancel_find_interfaces();
                wnd.show = false;
            }
        }

        ImGui::End();
    }

    if (world.wnd_find_implementations.show) {
        auto &wnd = world.wnd_find_implementations;

        ImGui::SetNextWindowDockID(dock_sidebar_id, ImGuiCond_Once);

        begin_window("Find Implementations", &wnd, ImGuiWindowFlags_AlwaysAutoResize, !wnd.done);

        if (wnd.done) {
            ImGui::Checkbox("Search everywhere", &wnd.search_everywhere);
            bool search_everywhere_changed = ImGui::IsItemEdited();
            ImGui::SameLine();
            help_marker("By default, Find Implementations only looks at types inside your workspace. This setting will search everywhere in your dependency tree.");

            defer {
                if (search_everywhere_changed) {
                    do_find_implementations();
                }
            };

            imgui_small_newline();

            if (wnd.results != NULL && wnd.results->len > 0) {
                imgui_push_mono_font();

                int index = 0;
                For (*wnd.results) {
                    // TODO: refactor out custom draw
                    auto availwidth = ImGui::GetContentRegionAvail().x;
                    auto text_size = ImVec2(availwidth, ImGui::CalcTextSize("blah").y);
                    auto drawpos = ImGui::GetCursorScreenPos();
                    auto drawlist = ImGui::GetWindowDrawList();

                    auto draw_selectable = [&]() {
                        auto label = our_sprintf("##find_implementations_result__%d", index++);
                        return ImGui::Selectable(label, false, 0, text_size);
                    };

                    auto clicked = draw_selectable();

                    auto draw_text = [&](ccstr text) {
                        drawlist->AddText(drawpos, ImGui::GetColorU32(ImGuiCol_Text), text);
                        drawpos.x += ImGui::CalcTextSize(text).x;
                    };

                    draw_text(our_sprintf("%s.%s", it->package_name, it->decl->decl->name));

                    ImGui::PushStyleColor(ImGuiCol_Text, to_imcolor(global_colors.muted));
                    draw_text(our_sprintf(" (%s)", get_path_relative_to(it->decl->ctx->import_path, wnd.current_import_path)));
                    ImGui::PopStyleColor();

                    // TODO: previews?
                    if (clicked) goto_file_and_pos(it->filepath, it->decl->decl->name_start);
                }

                imgui_pop_font();
            }
        } else {
            ImGui::Text("Searching...");
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                cancel_find_implementations();
                wnd.show = false;
            }
        }

        ImGui::End();
    }

    if (world.wnd_find_references.show) {
        auto &wnd = world.wnd_find_references;

        ImGui::SetNextWindowDockID(dock_sidebar_id, ImGuiCond_Once);

        begin_window("Find References", &wnd, ImGuiWindowFlags_AlwaysAutoResize, !wnd.done);

        if (wnd.done) {
            imgui_push_mono_font();

            For (*wnd.results) {
                auto filepath = get_path_relative_to(it.filepath, world.current_path);
                For (*it.references) {
                    auto pos = it.is_sel ? it.x_start : it.start;
                    if (ImGui::Selectable(our_sprintf("%s:%s", filepath, format_cur(pos))))
                        goto_file_and_pos(filepath, pos);
                }
            }

            imgui_pop_font();
        } else {
            ImGui::Text("Searching...");
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                cancel_find_references();
                wnd.show = false;
            }
        }

        ImGui::End();
    }

    {
        auto &wnd = world.wnd_generate_implementation;

        if (wnd.show && wnd.fill_running && current_time_milli() - wnd.fill_time_started_ms > 100) {
            begin_centered_window("Generate Implementation...###generate_impelmentation_filling", &wnd, 0, 400);
            ImGui::Text("Loading...");
            ImGui::End();
        }

        if (wnd.show && !wnd.fill_running) {
            auto go_up = [&]() {
                if (wnd.filtered_results->len == 0) return;
                if (wnd.selection == 0)
                    wnd.selection = min(wnd.filtered_results->len, settings.generate_implementation_max_results) - 1;
                else
                    wnd.selection--;
            };

            auto go_down = [&]() {
                if (wnd.filtered_results->len == 0) return;
                wnd.selection++;
                wnd.selection %= min(wnd.filtered_results->len, settings.generate_implementation_max_results);
            };

            begin_centered_window("Generate Implementation###generate_impelmentation_ready", &wnd, 0, 400);

            if (wnd.selected_interface)
                ImGui::TextWrapped("You've selected an interface. Please select a type and we'll add this interface's methods to that type.");
            else
                ImGui::TextWrapped("You've selected a type. Please select an interface and we'll add that interface's methods to this type.");

            auto mods = imgui_get_keymods();
            switch (mods) {
            case KEYMOD_NONE:
                if (imgui_special_key_pressed(ImGuiKey_DownArrow)) go_down();
                if (imgui_special_key_pressed(ImGuiKey_UpArrow)) go_up();
                if (imgui_special_key_pressed(ImGuiKey_Escape)) {
                    wnd.show = false;
                    wnd.filtered_results->len = 0;
                    wnd.selection = 0;
                }
                break;
            }

            focus_keyboard(&wnd);

            if (imgui_input_text_full("", wnd.query, _countof(wnd.query), ImGuiInputTextFlags_EnterReturnsTrue)) {
                do_generate_implementation();
                wnd.show = false;
                ImGui::SetWindowFocus(NULL);
            }

            auto symbol_to_name = [&](auto &it) {
                return our_sprintf("%s.%s", it.pkgname, it.name);
            };

            if (ImGui::IsItemEdited()) {
                wnd.filtered_results->len = 0;
                wnd.selection = 0;

                if (strlen(wnd.query) >= 2) {
                    u32 i = 0;
                    For (*wnd.symbols) {
                        if (fzy_has_match(wnd.query, symbol_to_name(it)))
                            wnd.filtered_results->append(i);
                        if (i++ > 1000)
                            break;
                    }

                    fuzzy_sort_filtered_results(
                        wnd.query,
                        wnd.filtered_results,
                        wnd.symbols->len,
                        [&](auto i) { return symbol_to_name(wnd.symbols->at(i)); }
                    );
                }
            }

            {
                imgui_push_mono_font();
                defer { imgui_pop_font(); };

                for (u32 i = 0; i < wnd.filtered_results->len && i < settings.goto_file_max_results; i++) {
                    auto it = wnd.symbols->at(wnd.filtered_results->at(i));
                    auto text = symbol_to_name(it);

                    if (i == wnd.selection)
                        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "%s", text);
                    else
                        ImGui::Text("%s", text);

                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 1.0, 1.0f, 0.4f), "\"%s\"", it.decl->ctx->import_path);
                }
            }

            ImGui::End();
        }
    }

    if (world.wnd_rename_identifier.show) {
        auto &wnd = world.wnd_rename_identifier;

        auto get_type_str = [&]() -> ccstr {
            switch (wnd.declres->decl->type) {
            // TODO: support import renaming
            case GODECL_TYPE: return "type";
            case GODECL_FUNC: return "function";
            case GODECL_FIELD: return "field";
            case GODECL_PARAM: return "parameter";

            case GODECL_VAR:
            case GODECL_CONST:
            case GODECL_SHORTVAR:
            case GODECL_TYPECASE:
                return "variable";
            }
            return "";
        };

        begin_centered_window(our_sprintf("Rename %s###rename_identifier", get_type_str()), &wnd, 0, 400, wnd.running);

        // if it's running, make sure the window stays focused
        if (wnd.running)
            ImGui::SetWindowFocus();

        /*
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(140, 194, 248)));
        imgui_push_mono_font();
        ImGui::Text("%s", wnd.decl->name);
        ImGui::PopStyleColor();
        imgui_pop_font();

        imgui_small_newline();
        */

        bool submitted = false;

        imgui_push_mono_font();

        focus_keyboard(&wnd);
        if (imgui_input_text_full(our_sprintf("Rename %s to", wnd.declres->decl->name), wnd.rename_to, _countof(wnd.rename_to), ImGuiInputTextFlags_EnterReturnsTrue)) {
            submitted = true;
        }

        imgui_pop_font();

        imgui_small_newline();

        /*
        ImGui::RadioButton("Discard unsaved changes", &wnd.how_to_handle_unsaved_files, DISCARD_UNSAVED);
        ImGui::SameLine();
        ImGui::RadioButton("Save unsaved changes", &wnd.how_to_handle_unsaved_files, SAVE_UNSAVED);

        imgui_small_newline();
        */

        // ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(230, 180, 180)));
        ImGui::TextWrapped("Please note: we don't currently support undo. (You can always just rename it back.)");
        // ImGui::PopStyleColor();

        imgui_small_newline();

        if (wnd.running) {
            if (wnd.too_late_to_cancel) {
                ImGui::Text("Applying changes...");
            } else {
                ImGui::Text("Renaming...");
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    cancel_rename_identifier();
                }
            }
        } else {
            if (ImGui::Button(our_sprintf("Rename", wnd.declres->decl->name)))
                submitted = true;
        }

        /*
        if (wnd.focused) {
            auto mods = imgui_get_keymods();
            switch (mods) {
            case KEYMOD_NONE:
                if (imgui_special_key_pressed(ImGuiKey_Escape))
                    wnd.show = false;
                break;
            }
        }
        */

        // We need to close this window when defocused, because otherwise the
        // user might move the cursor and fuck this up
        if (!wnd.focused) {
            wnd.show = false;
            ImGui::SetWindowFocus(NULL);
        }

        if (submitted && !wnd.running) {
            auto validate = [&]() {
                for (int i = 0, n = strlen(wnd.rename_to); i<n; i++) {
                    if (!isident(wnd.rename_to[i])) {
                        tell_user("Sorry, that's not a valid identifier.", "Error");
                        return false;
                    }
                }
                return true;
            };

            if (validate()) kick_off_rename_identifier();
        }

        ImGui::End();
    }

    if (world.wnd_about.show) {
        auto &wnd = world.wnd_about;

        begin_window("About", &wnd, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking);

        ImGui::Text("CodePerfect 95");

        ImGui::Text("Build %d", world.gh_version);

        if (world.auth.state == AUTH_REGISTERED)
            if (world.auth_status == GH_AUTH_OK)
                ImGui::Text("Registered to %s", world.authed_email);

        ImGui::End();
    }

    if (world.wnd_index_log.show) {
        auto &wnd = world.wnd_index_log;

        ImGui::SetNextWindowDockID(dock_bottom_id, ImGuiCond_Once);
        begin_window("Index Log", &wnd);

        imgui_push_mono_font();

        ImGuiListClipper clipper;
        clipper.Begin(wnd.len);
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                ImGui::Text(wnd.buf[(wnd.start + i) % INDEX_LOG_CAP]);
            }
        }

        if (wnd.cmd_scroll_to_end) {
            wnd.cmd_scroll_to_end = false;
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
        }

        imgui_pop_font();

        ImGui::End();
    }

    if (world.wnd_enter_license.show) {
        auto &wnd = world.wnd_enter_license;
        bool entered = false;

        begin_centered_window("Enter License Key", &wnd, 0, 500);

        if (imgui_input_text_full("Email", wnd.email, _countof(wnd.email), ImGuiInputTextFlags_EnterReturnsTrue))
            entered = true;

        imgui_small_newline();

        if (imgui_input_text_full("License Key", wnd.license, _countof(wnd.license), ImGuiInputTextFlags_EnterReturnsTrue))
            entered = true;

        imgui_small_newline();

        if (ImGui::Button("Enter"))
            entered = true;

        do {
            if (!entered) break;

            wnd.show = false;

            auto &auth = world.auth;
            auto old_state = auth.state;

            auth.state = AUTH_REGISTERED;

            auto email_len = strlen(wnd.email);
            auto license_len = strlen(wnd.license);

            if (email_len + 1 > _countof(auth.reg_email)) {
                tell_user(NULL, "Sorry, that email is too long.");
                break;
            }

            if (license_len + 1 > _countof(auth.reg_license)) {
                tell_user(NULL, "Sorry, that license key is too long.");
                break;
            }

            strcpy_safe_fixed(auth.reg_email, wnd.email);
            strcpy_safe_fixed(auth.reg_license, wnd.license);
            auth.reg_email_len = email_len;
            auth.reg_license_len = license_len;
            write_auth();

            tell_user(NULL, "Your license key was saved. Please restart CodePerfect for it to take effect. Thanks!");
        } while (0);

        ImGui::End();
    }

    if (world.wnd_debug_output.show) {
        auto &wnd = world.wnd_debug_output;

        ImGui::SetNextWindowDockID(dock_bottom_id, ImGuiCond_Once);
        begin_window("Debug Output", &wnd);

        imgui_push_mono_font();

        auto &lines = world.dbg.stdout_lines;

        ImGuiListClipper clipper;
        clipper.Begin(lines.len);
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                auto &it = lines[i];

                ImGui::Text("%s", it);
                if (ImGui::OurBeginPopupContextItem(our_sprintf("##debug_output_hidden_%d", i))) {
                    defer { ImGui::EndPopup(); };

                    if (ImGui::Selectable("Copy")) {
                        glfwSetClipboardString(world.window, it);
                    }

                    if (ImGui::Selectable("Copy All")) {
                        auto output = alloc_list<char>();
                        For (lines) {
                            for (auto p = it; *p != '\0'; p++) {
                                output->append(*p);
                            }
                            output->append('\n');
                        }
                        output->len--; // remove last '\n'
                        output->append('\0');
                        glfwSetClipboardString(world.window, output->items);
                    }

                    if (ImGui::Selectable("Clear")) {
                        // TODO
                        break;
                    }
                }
            }
        }

        if (wnd.cmd_scroll_to_end) {
            wnd.cmd_scroll_to_end = false;
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
        }

        imgui_pop_font();

        ImGui::End();
    }

    if (world.error_list.show) {
        ImGui::SetNextWindowDockID(dock_bottom_id, ImGuiCond_Once);
        begin_window("Build Results", &world.error_list);

        static Build_Error *menu_current_error = NULL;

        if (ImGui::IsWindowAppearing()) {
            ImGui::SetWindowFocus(NULL);
        }

        auto &b = world.build;

        if (b.ready()) {
            if (b.errors.len == 0) {
                ImGui::TextColored(to_imcolor(global_colors.green), "Build \"%s\" was successful!", b.build_profile_name);
            } else {
                ImGui::Text("Building \"%s\"...", b.build_profile_name);

                imgui_push_mono_font();

                for (int i = 0; i < b.errors.len; i++) {
                    auto &it = b.errors[i];

                    if (!it.valid) {
                        ImGui::TextColored(to_imcolor(global_colors.muted), "%s", it.message);
                        continue;
                    }

                    /*
                    auto flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth;
                    if (i == b.current_error)
                        flags |= ImGuiTreeNodeFlags_Selected;

                    ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());
                    ImGui::TreeNodeEx(&it, flags, "%s:%d:%d: %s", it.file, it.row, it.col, it.message);
                    ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());
                    */

                    if (i == b.scroll_to) {
                        ImGui::SetScrollHereY();
                        b.scroll_to = -1;
                    }

                    auto label = our_sprintf("%s:%d:%d: %s", it.file, it.row, it.col, it.message);
                    auto wrap_width = ImGui::GetContentRegionAvail().x;
                    auto text_size = ImVec2(wrap_width, ImGui::CalcTextSize(label, NULL, false, wrap_width).y);
                    auto pos = ImGui::GetCursorScreenPos();

                    bool clicked = ImGui::Selectable(our_sprintf("##hidden_%d", i), i == b.current_error, 0, text_size);
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
                        b.current_error = i;
                        goto_error(i);
                    }
                }

                imgui_pop_font();
            }
        } else if (b.started) {
            ImGui::Text("Building \"%s\"...", b.build_profile_name);
        } else {
            ImGui::Text("No build in progress.");
        }

        ImGui::End();
    }

    if (world.wnd_rename_file_or_folder.show) {
        auto &wnd = world.wnd_rename_file_or_folder;

        auto label = our_sprintf("Rename %s", wnd.target->is_directory ? "folder" : "file");
        begin_centered_window(our_sprintf("%s###add_file_or_folder", label), &wnd, 0, 300);

        ImGui::Text("Renaming");

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(140, 194, 248)));
        imgui_push_mono_font();
        ImGui::Text("%s", wnd.location);
        ImGui::PopStyleColor();
        imgui_pop_font();

        imgui_small_newline();

        focus_keyboard(&wnd);

        // close the window when we unfocus
        if (!wnd.focused) wnd.show = false;

        imgui_push_mono_font();
        bool entered = imgui_input_text_full("Name", wnd.name, _countof(wnd.name), ImGuiInputTextFlags_EnterReturnsTrue);
        imgui_pop_font();

        do {
            if (!entered) break;

            auto error_out = [&](ccstr msg) {
                tell_user(msg, "Error");
                wnd.name[0] = '\0';
            };

            if (strlen(wnd.name) == 0) {
                error_out("Please enter a file name.");
                break;
            }

            bool slash_found = false;
            for (int i = 0, len = strlen(wnd.name); i < len; i++) {
                if (is_sep(wnd.name[i])) {
                    slash_found = true;
                    break;
                }
            }

            if (slash_found) {
                error_out("New file name can't contain slashes.");
                break;
            }

            auto oldpath = path_join(world.current_path, wnd.location);
            auto newpath = path_join(our_dirname(oldpath), wnd.name);

            GHRenameFileOrDirectory((char*)oldpath, (char*)newpath);
            reload_file_subtree(our_dirname(wnd.location));

            wnd.show = false;
        } while (0);

        ImGui::End();
    }

    if (world.wnd_add_file_or_folder.show) {
        auto &wnd = world.wnd_add_file_or_folder;

        auto label = our_sprintf("Add %s", wnd.folder ? "Folder" : "File");
        begin_centered_window(our_sprintf("%s###add_file_or_folder", label), &wnd, 0, 300);

        ImGui::Text("Destination");

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(140, 194, 248)));
        imgui_push_mono_font();

        if (wnd.location_is_root)
            ImGui::Text("(workspace root)");
        else
            ImGui::Text("%s", wnd.location);

        ImGui::PopStyleColor();
        imgui_pop_font();

        imgui_small_newline();

        focus_keyboard(&wnd);

        // close the window when we unfocus
        if (!wnd.focused) wnd.show = false;

        imgui_push_mono_font();
        bool entered = imgui_input_text_full("Name", wnd.name, _countof(wnd.name), ImGuiInputTextFlags_EnterReturnsTrue);
        imgui_pop_font();

        if (entered) {
            wnd.show = false;

            do {
                if (strlen(wnd.name) == 0) break;

                auto dest = wnd.location_is_root ? world.current_path : path_join(world.current_path, wnd.location);
                auto path = path_join(dest, wnd.name);

                auto ok = wnd.folder ? create_directory(path) : touch_file(path);
                if (!ok) break;

                auto destnode = wnd.location_is_root ? world.file_tree : find_ft_node(wnd.location);
                add_ft_node(destnode, [&](auto child) {
                    child->is_directory = wnd.folder;
                    child->name = our_strcpy(wnd.name);
                });

                if (!wnd.folder) {
                    focus_editor(path);
                    ImGui::SetWindowFocus(NULL);
                }
            } while (0);
        }

        ImGui::End();
    }

    if (world.file_explorer.show) {
        auto &wnd = world.file_explorer;

        auto old_item_spacing = ImGui::GetStyle().ItemSpacing;

        ImGui::SetNextWindowDockID(dock_sidebar_id, ImGuiCond_Once);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        begin_window("File Explorer", &wnd);
        ImGui::PopStyleVar();

        auto begin_buttons_child = [&]() {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
            defer { ImGui::PopStyleVar(); };

            auto &style = ImGui::GetStyle();

            auto text_height = ImGui::CalcTextSize(ICON_MD_NOTE_ADD, NULL, true).y;
            float child_height = text_height + (icon_button_padding.y * 2.0f) + (style.WindowPadding.y * 2.0f);
            ImGui::BeginChild("child2", ImVec2(0, child_height), false, ImGuiWindowFlags_AlwaysUseWindowPadding);
        };

        begin_buttons_child(); {
            if (imgui_icon_button(ICON_MD_NOTE_ADD)) {
                open_add_file_or_folder(false);
            }

            ImGui::SameLine(0.0, 6.0f);

            if (imgui_icon_button(ICON_MD_CREATE_NEW_FOLDER)) {
                open_add_file_or_folder(true);
            }

            ImGui::SameLine(0.0, 6.0f);

            if (imgui_icon_button(ICON_MD_REFRESH)) {
                fill_file_tree(); // TODO: async?
            }
        } ImGui::EndChild();

        auto begin_directory_child = [&]() {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0);
            defer { ImGui::PopStyleVar(2); };

            ImGui::BeginChild("child3", ImVec2(0,0), true);
        };

        bool menu_handled = false;

        begin_directory_child(); {
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
                    menu_handled = true;

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
                        delete_ft_node(it);
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
                        auto rel_path = ft_node_to_path(it);
                        glfwSetClipboardString(world.window, rel_path);
                    }

                    if (ImGui::Selectable("Copy absolute path")) {
                        SCOPED_FRAME();
                        auto rel_path = ft_node_to_path(it);
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

                if (ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered(0)) {
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

            if (!menu_handled && ImGui::OurBeginPopupContextWindow("file_explorer_context_menu")) {
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, old_item_spacing);
                    defer { ImGui::PopStyleVar(); };

                    if (ImGui::Selectable("Add new file...")) {
                        open_add_file_or_folder(false);
                    }

                    if (ImGui::Selectable("Add new folder...")) {
                        open_add_file_or_folder(true);
                    }
                }
                ImGui::EndPopup();
            }

        } ImGui::EndChild();

        if (wnd.focused) {
            auto mods = imgui_get_keymods();
            switch (mods) {
            case KEYMOD_NONE:
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
            case KEYMOD_PRIMARY:
                if (imgui_special_key_pressed(ImGuiKey_Delete) || imgui_special_key_pressed(ImGuiKey_Backspace)) {
                    auto curr = wnd.selection;
                    if (curr != NULL) delete_ft_node(curr);
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

        begin_window("Project Settings", &wnd, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking);

        if (imgui_get_keymods() == KEYMOD_NONE)
            if (imgui_special_key_pressed(ImGuiKey_Escape))
                wnd.show = false;

        auto &ps = wnd.tmp;

        if (ImGui::BeginTabBar("MyTabBar", 0)) {
            auto get_focus_flags = [&](bool *pfocus, int flags = 0) -> int {
                if (*pfocus) {
                    flags |= ImGuiTabItemFlags_SetSelected;
                    *pfocus = false;
                }
                return flags;
            };

#if 0
            if (ImGui::BeginTabItem("General Settings", NULL, get_focus_flags(&wnd.focus_general_settings))) {
                auto begin_container_child = [&]() {
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
                    defer { ImGui::PopStyleVar(); };

                    ImGui::BeginChild("container", ImVec2(600, 300), false, ImGuiWindowFlags_AlwaysUseWindowPadding);
                };

                begin_container_child(); {
                    imgui_with_disabled(true, [&]() {
                        ImGui::Checkbox("Search vendor before GOMODCACHE", &ps->search_vendor_before_modcache);
                    });
                } ImGui::EndChild();

                ImGui::EndTabItem();
            }
#endif

            auto begin_left_pane_child = [&]() {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0);
                defer { ImGui::PopStyleVar(2); };

                ImGui::BeginChild("left_pane_child", ImVec2(200, 300), true, ImGuiWindowFlags_AlwaysUseWindowPadding);
            };

            auto begin_right_pane_child = [&]() {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
                defer { ImGui::PopStyleVar(); };

                ImGui::BeginChild("right pane", ImVec2(400, 300), false, ImGuiWindowFlags_AlwaysUseWindowPadding);
            };

            auto profiles_buttons_padding = ImVec2(6, 6);

            auto get_profiles_left_pane_buttons_height = [&]() -> float {
                auto &style = ImGui::GetStyle();

                auto text_height = ImGui::CalcTextSize(ICON_MD_NOTE_ADD, NULL, true).y;
                return text_height + (icon_button_padding.y *style.FramePadding.y * 2.0f) + (style.WindowPadding.y * 2.0f);
            };

            auto begin_profiles_buttons_child = [&]() {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, profiles_buttons_padding);
                defer { ImGui::PopStyleVar(); };

                ImGui::BeginChild("child2", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysUseWindowPadding);
            };

            auto begin_profiles_child = [&]() {
                float height = 0;
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, profiles_buttons_padding);
                    defer { ImGui::PopStyleVar(); };

                    auto &style = ImGui::GetStyle();
                    height = get_profiles_left_pane_buttons_height() + style.ItemSpacing.y;
                }

                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0);
                defer { ImGui::PopStyleVar(2); };

                auto h = ImGui::GetContentRegionAvail().y - height;

                ImGui::BeginChild("child3", ImVec2(0, h), true, ImGuiWindowFlags_AlwaysUseWindowPadding);
            };

            if (ImGui::BeginTabItem("Debug Profiles", NULL, get_focus_flags(&wnd.focus_debug_profiles))) {

                begin_left_pane_child(); {
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
                    defer { ImGui::PopStyleVar(); };

                    begin_profiles_child(); {
                        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
                        defer { ImGui::PopStyleVar(); };

                        for (int i = 0; i < ps->debug_profiles->len; i++) {
                            auto &it = ps->debug_profiles->at(i);
                            auto label = our_sprintf("%s##debug_profile_%d", it.label, i);
                            if (ImGui::Selectable(label, wnd.current_debug_profile == i))
                                wnd.current_debug_profile = i;
                        }
                    } ImGui::EndChild();

                    begin_profiles_buttons_child(); {
                        if (imgui_icon_button(ICON_MD_ADD)) {
                            Debug_Profile prof; ptr0(&prof);
                            prof.type = DEBUG_TEST_PACKAGE;
                            prof.is_builtin = false;
                            strcpy_safe_fixed(prof.label, "New Profile");
                            prof.test_package.package_path[0] = '\0';
                            prof.test_package.use_current_package = true;

                            {
                                SCOPED_MEM(&wnd.pool);
                                ps->debug_profiles->append(&prof);
                            }
                            wnd.current_debug_profile = ps->debug_profiles->len - 1;
                        }

                        ImGui::SameLine(0.0, 4.0f);

                        auto can_remove = [&]() {
                            if (wnd.current_debug_profile < 0) return false;
                            if (wnd.current_debug_profile >= ps->debug_profiles->len) return false;
                            if (ps->debug_profiles->at(wnd.current_debug_profile).is_builtin) return false;

                            return true;
                        };

                        bool remove_clicked = false;
                        imgui_with_disabled(!can_remove(), [&]() {
                            remove_clicked = imgui_icon_button(ICON_MD_REMOVE);
                        });

                        if (remove_clicked) {
                            do {
                                if (wnd.current_debug_profile >= ps->debug_profiles->len) break;

                                if (ps->debug_profiles->at(wnd.current_debug_profile).is_builtin) {
                                    tell_user("Sorry, you can't remove a builtin profile.", "Can't remove profile");
                                    break;
                                }

                                if (ps->active_debug_profile > wnd.current_debug_profile)
                                    ps->active_debug_profile--;

                                ps->debug_profiles->remove(wnd.current_debug_profile);

                                if (wnd.current_debug_profile >= ps->debug_profiles->len)
                                    wnd.current_debug_profile = ps->debug_profiles->len - 1;

                                if (ps->active_debug_profile >= ps->debug_profiles->len)
                                    ps->active_debug_profile = ps->debug_profiles->len - 1;
                            } while (0);
                        }
                    } ImGui::EndChild();
                } ImGui::EndChild();

                ImGui::SameLine();

                begin_right_pane_child(); {
                    auto index = wnd.current_debug_profile;
                    if (0 <= index && index < ps->debug_profiles->len) {
                        auto &dp = ps->debug_profiles->at(wnd.current_debug_profile);

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
                                imgui_push_mono_font();
                                imgui_input_text_full_fixbuf("Package path", dp.test_package.package_path);
                                imgui_pop_font();
                            });

                            imgui_small_newline();
                            break;

                        case DEBUG_TEST_CURRENT_FUNCTION:
                            break;

                        case DEBUG_RUN_PACKAGE:
                            ImGui::Checkbox("Use package of current file", &dp.run_package.use_current_package);
                            imgui_small_newline();

                            imgui_with_disabled(dp.run_package.use_current_package, [&]() {
                                imgui_push_mono_font();
                                imgui_input_text_full_fixbuf("Package path", dp.run_package.package_path);
                                imgui_pop_font();
                            });

                            imgui_small_newline();
                            break;

                        case DEBUG_RUN_BINARY:
                            imgui_push_mono_font();
                            imgui_input_text_full_fixbuf("Binary path", dp.run_binary.binary_path);
                            imgui_pop_font();
                            imgui_small_newline();
                            break;
                        }

                        imgui_push_mono_font();
                        imgui_input_text_full_fixbuf("Additional arguments", dp.args);
                        imgui_pop_font();
                    } else {
                        if (ps->debug_profiles->len == 0) {
                            ImGui::Text("Create a profile on the left and it'll show up here.");
                        } else {
                            ImGui::Text("Select a profile on the left and it'll show up here.");
                        }
                    }
                } ImGui::EndChild();

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Build Profiles", NULL, get_focus_flags(&wnd.focus_build_profiles))) {
                begin_left_pane_child(); {
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
                    defer { ImGui::PopStyleVar(); };

                    begin_profiles_child(); {
                        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
                        defer { ImGui::PopStyleVar(); };

                        for (int i = 0; i < ps->build_profiles->len; i++) {
                            auto &it = ps->build_profiles->at(i);
                            auto label = our_sprintf("%s##build_profile_%d", it.label, i);
                            if (ImGui::Selectable(label, wnd.current_build_profile == i))
                                wnd.current_build_profile = i;
                        }
                    } ImGui::EndChild();

                    begin_profiles_buttons_child(); {
                        if (imgui_icon_button(ICON_MD_ADD)) {
                            Build_Profile prof; ptr0(&prof);
                            strcpy_safe_fixed(prof.label, "New Profile");
                            strcpy_safe_fixed(prof.cmd, "go build");

                            {
                                SCOPED_MEM(&wnd.pool);
                                ps->build_profiles->append(&prof);
                            }
                            wnd.current_build_profile = ps->build_profiles->len - 1;
                        }

                        ImGui::SameLine(0.0, 4.0f);

                        auto can_remove = [&]() {
                            if (wnd.current_build_profile < 0) return false;
                            if (wnd.current_build_profile >= ps->build_profiles->len) return false;

                            return true;
                        };

                        bool remove_clicked = false;
                        imgui_with_disabled(!can_remove(), [&]() {
                            remove_clicked = imgui_icon_button(ICON_MD_REMOVE);
                        });

                        if (remove_clicked) {
                            do {
                                if (wnd.current_build_profile >= ps->build_profiles->len) break;

                                if (ps->active_build_profile > wnd.current_build_profile)
                                    ps->active_build_profile--;

                                ps->build_profiles->remove(wnd.current_build_profile);

                                if (wnd.current_build_profile >= ps->build_profiles->len)
                                    wnd.current_build_profile = ps->build_profiles->len - 1;

                                if (ps->active_build_profile >= ps->build_profiles->len)
                                    ps->active_build_profile = ps->build_profiles->len - 1;
                            } while (0);
                        }
                    } ImGui::EndChild();
                } ImGui::EndChild();

                ImGui::SameLine();

                begin_right_pane_child(); {
                    auto index = wnd.current_build_profile;
                    if (0 <= index && index < ps->build_profiles->len) {
                        auto &bp = ps->build_profiles->at(index);

                        imgui_input_text_full_fixbuf("Name", bp.label);
                        imgui_small_newline();

                        imgui_push_mono_font();
                        imgui_input_text_full_fixbuf("Build command", bp.cmd);
                        imgui_pop_font();
                    } else {
                        if (ps->build_profiles->len == 0) {
                            ImGui::Text("Create a profile on the left and it'll show up here.");
                        } else {
                            ImGui::Text("Select a profile on the left and it'll show up here.");
                        }
                    }
                } ImGui::EndChild();

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
                if (wnd.tmp->active_build_profile >= wnd.tmp->build_profiles->len)
                    wnd.tmp->active_build_profile = wnd.tmp->build_profiles->len - 1;

                if (wnd.tmp->active_debug_profile >= wnd.tmp->debug_profiles->len)
                    wnd.tmp->active_debug_profile = wnd.tmp->debug_profiles->len - 1;

                {
                    world.project_settings_mem.reset();
                    SCOPED_MEM(&world.project_settings_mem);
                    memcpy(&project_settings, wnd.tmp->copy(), sizeof(Project_Settings));
                }
                write_project_settings();
                wnd.show = false;
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel")) {
                wnd.show = false;
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

        begin_centered_window("Go To File", &wnd, 0, 500);

        // close the window when we unfocus
        if (!wnd.focused) {
            wnd.show = false;
            ImGui::SetWindowFocus(NULL);
        }

        focus_keyboard(&wnd);

        if (imgui_input_text_full("Search for file:", wnd.query, _countof(wnd.query), ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (wnd.filtered_results->len > 0) {
                auto relpath = wnd.filepaths->at(wnd.filtered_results->at(wnd.selection));
                auto filepath = path_join(world.current_path, relpath);
                focus_editor(filepath);
            }
            wnd.show = false;
            ImGui::SetWindowFocus(NULL);
        }

        if (wnd.focused) {
            auto mods = imgui_get_keymods();
            switch (mods) {
            case KEYMOD_NONE:
                if (imgui_special_key_pressed(ImGuiKey_DownArrow)) go_down();
                if (imgui_special_key_pressed(ImGuiKey_UpArrow)) go_up();
                if (imgui_special_key_pressed(ImGuiKey_Escape)) wnd.show = false;
                break;
            }
        }

        if (ImGui::IsItemEdited()) {
            if (strlen(wnd.query) >= 2)
                filter_files();
            else
                wnd.filtered_results->len = 0; // maybe use logic from filter_files
            wnd.selection = 0;
        }

        if (wnd.filtered_results->len > 0) {
            imgui_small_newline();

            imgui_push_mono_font();
            defer { imgui_pop_font(); };

            auto pm = pretty_menu_start();

            for (u32 i = 0; i < wnd.filtered_results->len && i < settings.goto_file_max_results; i++) {
                pretty_menu_item(pm, i == wnd.selection);
                pretty_menu_text(pm, wnd.filepaths->at(wnd.filtered_results->at(i)));
            }
        }

        ImGui::End();
    }

    if (world.wnd_command.show) {
        auto& wnd = world.wnd_command;

        auto begin_window = [&]() {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
            defer { ImGui::PopStyleVar(); };

            begin_centered_window("Run Command", &wnd, 0, 400);
        };

        begin_window();

        focus_keyboard(&wnd);

        // We need to close this window when defocused, because otherwise the
        // user might move the cursor and fuck this up
        if (!wnd.focused) {
            wnd.show = false;
            ImGui::SetWindowFocus(NULL);
        }

        auto go_up = [&]() {
            if (wnd.filtered_results->len == 0) return;
            if (wnd.selection == 0)
                wnd.selection = min(wnd.filtered_results->len, settings.run_command_max_results) - 1;
            else
                wnd.selection--;
        };

        auto go_down = [&]() {
            if (wnd.filtered_results->len == 0) return;
            wnd.selection++;
            wnd.selection %= min(wnd.filtered_results->len, settings.run_command_max_results);
        };

        auto mods = imgui_get_keymods();
        switch (mods) {
        case KEYMOD_NONE:
            if (imgui_special_key_pressed(ImGuiKey_DownArrow)) go_down();
            if (imgui_special_key_pressed(ImGuiKey_UpArrow)) go_up();
            if (imgui_special_key_pressed(ImGuiKey_Escape)) wnd.show = false;
            break;
        }

        ImGui::PushItemWidth(-1);
        {
            if (ImGui::InputText("###run_command", wnd.query, _countof(wnd.query), ImGuiInputTextFlags_EnterReturnsTrue)) {
                wnd.show = false;
                ImGui::SetWindowFocus(NULL);

                if (wnd.filtered_results->len > 0)
                    handle_command((Command)wnd.filtered_results->at(wnd.selection), false);
            }
        }
        ImGui::PopItemWidth();

        if (ImGui::IsItemEdited()) {
            wnd.filtered_results->len = 0;
            wnd.selection = 0;

            if (strlen(wnd.query) > 0) {
                int i = 0;
                For (*wnd.actions) {
                    if (fzy_has_match(wnd.query, get_command_name(it)))
                        wnd.filtered_results->append(it);
                    if (i++ > 1000)
                        break;
                }

                fuzzy_sort_filtered_results(
                    wnd.query,
                    wnd.filtered_results,
                    _CMD_COUNT_,
                    [&](int i) { return get_command_name((Command)i); }
                );
            }
        }

        if (wnd.filtered_results->len > 0) {
            imgui_small_newline();

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            defer { ImGui::PopStyleVar(); };

            auto pm = pretty_menu_start();

            for (u32 i = 0; i < wnd.filtered_results->len && i < settings.run_command_max_results; i++) {
                pretty_menu_item(pm, i == wnd.selection);

                auto it = (Command)wnd.filtered_results->at(i);

                auto name = get_command_name(it);
                pretty_menu_text(pm, get_command_name(it));

                auto keystr = get_menu_command_key(it);
                if (keystr != NULL) {
                    // imgui_push_mono_font();
                    // defer { imgui_pop_font(); };

                    pm->pos.x = pm->text_br.x - ImGui::CalcTextSize(keystr).x;

                    // TODO: refactor
                    auto color = i == wnd.selection
                        ? IM_COL32(150, 150, 150, 255)
                        : IM_COL32(110, 110, 110, 255);

                    pretty_menu_text(pm, keystr, color);
                }
            }
        }

        ImGui::End();
    }

    {
        auto &wnd = world.wnd_goto_symbol;

        if (wnd.show && wnd.fill_running && current_time_milli() - wnd.fill_time_started_ms > 100) {
            begin_centered_window("Go To Symbol...###goto_symbol_filling", &wnd, 0, 600);
            ImGui::Text("Loading symbols...");
            ImGui::End();
        }

        if (wnd.show && !wnd.fill_running) {
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
                wnd.selection %= min(wnd.filtered_results->len, settings.goto_symbol_max_results);
            };

            begin_centered_window("Go To Symbol###goto_symbol_ready", &wnd, 0, 600);

            bool refilter = false;

            ImGui::Checkbox("Include symbols in current file only", &wnd.current_file_only);
            if (ImGui::IsItemEdited())
                refilter = true;

            imgui_small_newline();

            auto mods = imgui_get_keymods();
            switch (mods) {
            case KEYMOD_NONE:
                if (imgui_special_key_pressed(ImGuiKey_DownArrow)) go_down();
                if (imgui_special_key_pressed(ImGuiKey_UpArrow)) go_up();
                if (imgui_special_key_pressed(ImGuiKey_Escape)) {
                    wnd.show = false;
                    wnd.filtered_results->len = 0;
                    wnd.selection = 0;
                }
                break;
            }

            focus_keyboard(&wnd);

            if (!wnd.focused) {
                wnd.show = false;
                ImGui::SetWindowFocus(NULL);
            }

            if (imgui_input_text_full("Search for symbol:", wnd.query, _countof(wnd.query), ImGuiInputTextFlags_EnterReturnsTrue)) {
                wnd.show = false;
                ImGui::SetWindowFocus(NULL);

                do {
                    if (wnd.filtered_results->len == 0) break;

                    if (!world.indexer.try_acquire_lock(IND_READING)) break;
                    defer { world.indexer.release_lock(IND_READING); };

                    auto it = wnd.symbols->at(wnd.filtered_results->at(wnd.selection));

                    Jump_To_Definition_Result res;
                    res.file = world.indexer.ctx_to_filepath(it.decl->ctx);
                    res.pos = it.decl->decl->name_start;
                    res.decl = it.decl;
                    goto_jump_to_definition_result(&res);

                    wnd.filtered_results->len = 0;
                    wnd.selection = 0;
                } while (0);
            }

            if (ImGui::IsItemEdited())
                refilter = true;

            do {
                if (!refilter) continue;

                wnd.selection = 0;
                wnd.filtered_results->len = 0;

                if (strlen(wnd.query) < 2) break;

                Editor *editor = NULL;
                if (wnd.current_file_only) {
                    editor = get_current_editor();
                    if (editor == NULL)
                        break;
                }

                Timer t;
                t.init("filter_symbols");

                for (u32 i = 0; i < wnd.symbols->len && i < 1000; i++) {
                    auto &it = wnd.symbols->at(i);

                    if (wnd.current_file_only)
                        if (!are_filepaths_equal(it.filepath, editor->filepath))
                            continue;

                    if (!fzy_has_match(wnd.query, it.full_name()))
                        continue;

                    wnd.filtered_results->append(i);
                }

                t.log("matching");

                fuzzy_sort_filtered_results(
                    wnd.query,
                    wnd.filtered_results,
                    wnd.symbols->len,
                    [&](auto i) { return wnd.symbols->at(i).full_name(); }
                );

                t.log("sort");
            } while (0);

            if (wnd.filtered_results->len > 0) {
                imgui_small_newline();

                imgui_push_mono_font();
                defer { imgui_pop_font(); };

                auto pm = pretty_menu_start();

                for (u32 i = 0; i < wnd.filtered_results->len && i < settings.goto_file_max_results; i++) {
                    pretty_menu_item(pm, i == wnd.selection);

                    auto it = wnd.symbols->at(wnd.filtered_results->at(i));

                    auto get_decl_type = [&]() {
                        auto decl_type = it.decl->decl->type;
                        switch (decl_type) {
                        case GODECL_IMPORT:
                            return "import";
                        case GODECL_VAR:
                        case GODECL_SHORTVAR:
                            return "var";
                        case GODECL_CONST:
                            return "const";
                        case GODECL_TYPE:
                            return "type";
                        case GODECL_FUNC:
                            return "func";
                        }
                        return "unknown";
                    };

                    pretty_menu_text(pm, our_sprintf("(%s) ", get_decl_type()), IM_COL32(80, 80, 80, 255));

                    pretty_menu_text(pm, it.full_name());
                    pm->pos.x += 8;

                    auto import_path = it.decl->ctx->import_path;
                    if (path_has_descendant(wnd.current_import_path, import_path))
                        import_path = get_path_relative_to(import_path, wnd.current_import_path);
                    if (streq(import_path, ""))
                        import_path = "(root)";

                    int rem_chars = (pm->text_br.x - pm->pos.x) / font->width;

                    auto s = import_path;
                    if (strlen(s) > rem_chars)
                        s = our_sprintf("%.*s...", rem_chars - 3, s);

                    auto color = i == wnd.selection
                        ? IM_COL32(150, 150, 150, 255)
                        : IM_COL32(110, 110, 110, 255);

                    pretty_menu_text(pm, s, color);
                }
            }

            ImGui::End();
        }
    }

    // Don't show the debugger UI if we're still starting up, because we're
    // still building, and the build could fail.  Might regret this, can always
    // change later.
    if (world.dbg.state_flag != DLV_STATE_INACTIVE && world.dbg.state_flag != DLV_STATE_STARTING) {
        draw_debugger();
    }

    if (world.wnd_search_and_replace.show) {
        auto& wnd = world.wnd_search_and_replace;

        ImGui::SetNextWindowDockID(dock_sidebar_id, ImGuiCond_Once);

        auto title = our_sprintf("%s###search_and_replace", wnd.replace ? "Search and Replace" : "Search");
        begin_window(title, &wnd, ImGuiWindowFlags_AlwaysAutoResize);

        bool entered = false;

        imgui_push_mono_font();
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
        imgui_pop_font();

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
                world.searcher.cleanup();
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
                    imgui_push_mono_font();

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
                            auto pos = it.match_start;
                            if (is_mark_valid(it.mark_start))
                                pos = it.mark_start->pos();

                            auto s = our_sprintf("%d:%d ", pos.y+1, pos.x+1);
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
                                if (is_mark_valid(it.mark_start))
                                    pos = it.mark_start->pos();

                                goto_file_and_pos(filepath, pos);
                            } else {
                                wnd.selection = index;
                            }
                        }
                    }

                    imgui_pop_font();
                    ImGui::Unindent();
                }

                if (wnd.focused && !world.ui.keyboard_captured_by_imgui) {
                    auto mods = imgui_get_keymods();
                    switch (mods) {
                    case KEYMOD_NONE:
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

    if (world.wnd_style_editor.show) {
        begin_window("Style Editor", &world.wnd_style_editor);

        if (ImGui::BeginTabBar("style_editor_tabbar", 0)) {
            if (ImGui::BeginTabItem("Margins & Padding", NULL, 0)) {
                ImGui::SliderInt("status_padding_x", &settings.status_padding_x, 0.0, 20.0f, "%d");
                ImGui::SliderInt("status_padding_y", &settings.status_padding_y, 0.0, 20.0f, "%d");
                ImGui::SliderInt("line_number_margin_left", &settings.line_number_margin_left, 0.0, 20.0f, "%d");
                ImGui::SliderInt("line_number_margin_right", &settings.line_number_margin_right, 0.0, 20.0f, "%d");
                ImGui::SliderInt("autocomplete_menu_padding", &settings.autocomplete_menu_padding, 0.0, 20.0f, "%d");
                ImGui::SliderInt("autocomplete_item_padding_x", &settings.autocomplete_item_padding_x, 0.0, 20.0f, "%d");
                ImGui::SliderInt("autocomplete_item_padding_y", &settings.autocomplete_item_padding_y, 0.0, 20.0f, "%d");
                ImGui::SliderInt("tabs_offset", &settings.tabs_offset, 0.0, 20.0f, "%d");
                ImGui::SliderInt("parameter_hint_margin_y", &settings.parameter_hint_margin_y, 0, 0, "%d");
                ImGui::SliderInt("parameter_hint_padding_x", &settings.parameter_hint_padding_x, 0, 0, "%d");
                ImGui::SliderInt("parameter_hint_padding_y", &settings.parameter_hint_padding_y, 0, 0, "%d");
                ImGui::SliderInt("editor_margin_x", &settings.editor_margin_x, 0, 0, "%d");
                ImGui::SliderInt("editor_margin_y", &settings.editor_margin_y, 0, 0, "%d");
                ImGui::SliderFloat("line_height", &settings.line_height, 0, 0, "%f");
                ImGui::SliderInt("goto_file_max_results", &settings.goto_file_max_results, 0, 0, "%d");
                ImGui::SliderInt("goto_symbol_max_results", &settings.goto_symbol_max_results, 0, 0, "%d");
                ImGui::SliderInt("generate_implementation_max_results", &settings.generate_implementation_max_results, 0, 0, "%d");
                ImGui::SliderInt("run_command_max_results", &settings.run_command_max_results, 0, 0, "%d");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Colors", NULL, 0)) {
                ImGui::ColorEdit3("autocomplete_background", (float*)&global_colors.autocomplete_background);
                ImGui::ColorEdit3("autocomplete_border", (float*)&global_colors.autocomplete_border);
                ImGui::ColorEdit3("autocomplete_selection", (float*)&global_colors.autocomplete_selection);
                ImGui::ColorEdit3("background", (float*)&global_colors.background);
                ImGui::ColorEdit3("breakpoint_active", (float*)&global_colors.breakpoint_active);
                ImGui::ColorEdit3("breakpoint_current", (float*)&global_colors.breakpoint_current);
                ImGui::ColorEdit3("breakpoint_inactive", (float*)&global_colors.breakpoint_inactive);
                ImGui::ColorEdit3("breakpoint_other", (float*)&global_colors.breakpoint_other);
                ImGui::ColorEdit3("builtin", (float*)&global_colors.builtin);
                ImGui::ColorEdit3("comment", (float*)&global_colors.comment);
                ImGui::ColorEdit3("cursor", (float*)&global_colors.cursor);
                ImGui::ColorEdit3("cursor_foreground", (float*)&global_colors.cursor_foreground);
                ImGui::ColorEdit3("foreground", (float*)&global_colors.foreground);
                ImGui::ColorEdit3("green", (float*)&global_colors.green);
                ImGui::ColorEdit3("keyword", (float*)&global_colors.keyword);
                ImGui::ColorEdit3("muted", (float*)&global_colors.muted);
                ImGui::ColorEdit3("number_literal", (float*)&global_colors.number_literal);
                ImGui::ColorEdit3("pane_active", (float*)&global_colors.pane_active);
                ImGui::ColorEdit3("pane_inactive", (float*)&global_colors.pane_inactive);
                ImGui::ColorEdit3("pane_resizer", (float*)&global_colors.pane_resizer);
                ImGui::ColorEdit3("pane_resizer_hover", (float*)&global_colors.pane_resizer_hover);
                ImGui::ColorEdit3("punctuation", (float*)&global_colors.punctuation);
                ImGui::ColorEdit3("search_background", (float*)&global_colors.search_background);
                ImGui::ColorEdit3("search_foreground", (float*)&global_colors.search_foreground);
                ImGui::ColorEdit3("string_literal", (float*)&global_colors.string_literal);
                // ImGui::ColorEdit3("tab", (float*)&global_colors.tab);
                // ImGui::ColorEdit3("tab_hovered", (float*)&global_colors.tab_hovered);
                ImGui::ColorEdit3("tab_selected", (float*)&global_colors.tab_selected);
                ImGui::ColorEdit3("type", (float*)&global_colors.type);
                ImGui::ColorEdit3("visual_background", (float*)&global_colors.visual_background);
                ImGui::ColorEdit3("visual_foreground", (float*)&global_colors.visual_foreground);
                ImGui::ColorEdit3("visual_highlight", (float*)&global_colors.visual_highlight);
                ImGui::ColorEdit3("white", (float*)&global_colors.white);
                ImGui::ColorEdit3("white_muted", (float*)&global_colors.white_muted);

                ImGui::ColorEdit3("status_area_background", (float*)&global_colors.status_area_background);
                ImGui::ColorEdit3("command_background", (float*)&global_colors.command_background);
                ImGui::ColorEdit3("command_foreground", (float*)&global_colors.command_foreground);
                ImGui::ColorEdit3("status_mode_background", (float*)&global_colors.status_mode_background);
                ImGui::ColorEdit3("status_mode_foreground", (float*)&global_colors.status_mode_foreground);
                ImGui::ColorEdit3("status_debugger_paused_background", (float*)&global_colors.status_debugger_paused_background);
                ImGui::ColorEdit3("status_debugger_starting_background", (float*)&global_colors.status_debugger_starting_background);
                ImGui::ColorEdit3("status_debugger_running_background", (float*)&global_colors.status_debugger_running_background);
                ImGui::ColorEdit3("status_index_ready_background", (float*)&global_colors.status_index_ready_background);
                ImGui::ColorEdit3("status_index_ready_foreground", (float*)&global_colors.status_index_ready_foreground);
                ImGui::ColorEdit3("status_index_indexing_background", (float*)&global_colors.status_index_indexing_background);
                ImGui::ColorEdit3("status_index_indexing_foreground", (float*)&global_colors.status_index_indexing_foreground);

                if (ImGui::Button("Save to disk")) {
                    File f;
                    f.init_write("/Users/brandon/.cpcolors");
                    f.write((char*)&global_colors, sizeof(global_colors));
                    f.cleanup();
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();
    }

#ifdef DEBUG_MODE
    if (world.wnd_history.show) {
        ImGui::SetNextWindowDockID(dock_sidebar_id, ImGuiCond_Once);

        begin_window("History", &world.wnd_history, ImGuiWindowFlags_AlwaysAutoResize);

        bool handled = false;
        do {
            auto editor = get_current_editor();
            if (editor == NULL) break;
            if (!editor->buf->use_history) break;

            handled = true;

            auto buf = editor->buf;
            for (auto i = buf->hist_start; i != buf->hist_top; i = buf->hist_inc(i)) {
                auto change = buf->history[i];
                ImGui::Text("### %d%s", i, i == buf->hist_curr ? " (*)" : "");

                for (auto it = change; it != NULL; it = it->next) {
                    ImGui::BulletText(
                        "start = %s, oldend = %s, newend = %s, oldlen = %d, newlen = %d",
                        format_cur(it->start),
                        format_cur(it->old_end),
                        format_cur(it->new_end),
                        it->old_text.len,
                        it->new_text.len
                    );
                }
            }
        } while (0);

        if (!handled)
            ImGui::Text("no history to show");

        ImGui::End();
    }
#endif

    do {
        auto editor = get_current_editor();
        if (editor == NULL) break;

        auto tree = editor->buf->tree;
        if (tree == NULL) break;

        if (world.wnd_editor_tree.show) {
            auto &wnd = world.wnd_editor_tree;

            begin_window("AST", &wnd);

            ImGui::Checkbox("show anon?", &wnd.show_anon_nodes);
            ImGui::SameLine();
            ImGui::Checkbox("show comments?", &wnd.show_comments);
            ImGui::SameLine();

            cur2 open_cur = new_cur2(-1, -1);
            if (ImGui::Button("go to cursor"))
                open_cur = editor->cur;

            ts_tree_cursor_reset(&editor->buf->cursor, ts_tree_root_node(tree));
            render_ts_cursor(&editor->buf->cursor, open_cur);

            ImGui::End();
        }

        if (world.wnd_editor_toplevels.show) {
            begin_window("Toplevels", &world.wnd_editor_toplevels);

            List<Godecl> decls;
            decls.init();

            Parser_It it; ptr0(&it);
            it.init(editor->buf);

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

    if (world.flag_defocus_imgui) {
        ImGui::SetWindowFocus(NULL);
        world.flag_defocus_imgui = false;
    }

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

    int editor_index = 0;

    For (actual_cursor_positions) {
        it.x = -1;
        it.y = -1;
    }
    actual_parameter_hint_start.x = -1;
    actual_parameter_hint_start.y = -1;

    if (world.wnd_mouse_pos.show) {
        // always show this
        begin_window("Mouse Pos", &world.wnd_mouse_pos);
        ImGui::End();
    }

    // Draw panes.
    draw_rect(panes_area, rgba(global_colors.background));
    for (u32 current_pane = 0; current_pane < world.panes.len; current_pane++) {
        auto &pane = world.panes[current_pane];
        auto is_pane_selected = (current_pane == world.current_pane);

        pane_area.w = pane.width;
        pane_area.h = panes_area.h;

        boxf tabs_area, editor_area;
        get_tabs_and_editor_area(&pane_area, &tabs_area, &editor_area, pane.editors.len > 0);

        if (pane.editors.len > 0) {
            draw_rect(tabs_area, rgba(is_pane_selected ? global_colors.pane_active : global_colors.pane_inactive));
            auto editor = pane.get_current_editor();

            cur2 saved_pos = new_cur2(-1, -1);

            auto actually_calculate_pos_from_mouse = [&]() -> cur2 {
                auto buf = editor->buf;
                auto &view = editor->view;

                auto area = editor_area;
                area.x += settings.editor_margin_x;
                area.y += settings.editor_margin_y;
                area.w -= settings.editor_margin_x * 2;
                area.h -= settings.editor_margin_y * 2;

                area.x += settings.line_number_margin_left;
                area.x += settings.line_number_margin_right;
                area.x += world.font.width * get_line_number_width(editor);

                auto im_pos = ImGui::GetIO().MousePos;
                if (im_pos.x < 0 || im_pos.y < 0)
                    return new_cur2(-1, -1);

                auto pos = new_vec2f(im_pos.x, im_pos.y);
                pos.x -= area.x;
                pos.y -= area.y;

                auto y = view.y + pos.y / (world.font.height * settings.line_height);
                if (y >= buf->lines.len) {
                    y = buf->lines.len-1;
                    return new_cur2((i32)buf->lines[y].len, (i32)y);
                }

                auto vx = (int)(pos.x / world.font.width);
                auto x = buf->idx_vcp_to_cp(y, vx);

                return new_cur2((i32)x, (i32)y);
            };

            auto calculate_pos_from_mouse = [&]() -> cur2 {
                if (saved_pos.x != -1) return saved_pos;

                auto pos = actually_calculate_pos_from_mouse();
                saved_pos = pos;
                return pos;
            };

            boxf editor_area_considering_pane_resizers = editor_area;
            editor_area_considering_pane_resizers.x += PANE_RESIZER_WIDTH / 2;
            editor_area_considering_pane_resizers.w -= PANE_RESIZER_WIDTH;

            auto is_hovered = test_hover(editor_area_considering_pane_resizers, HOVERID_EDITORS + current_pane, ImGuiMouseCursor_TextInput);
            if (is_hovered) {
                if (world.ui.mouse_just_pressed[0]) {
                    focus_editor_by_id(editor->id, new_cur2(-1, -1));

                    auto pos = calculate_pos_from_mouse();
                    if (pos.x > 0 && pos.y > 0) {
                        auto &io = ImGui::GetIO();
                        if (OS_MAC ? io.KeySuper : io.KeyCtrl) {
                            handle_goto_definition(pos);
                        } else {
                            editor->select_start = pos;
                            editor->selecting = true;
                            editor->mouse_select.on = true;
                            editor->mouse_select.editor_id = editor->id;

                            auto opts = default_move_cursor_opts();
                            opts->is_user_movement = true;
                            editor->move_cursor(pos, opts);
                        }
                    }
                } else if (world.ui.mouse_down[0]) {
                    if (editor->mouse_select.on)
                        if (editor->mouse_select.editor_id == editor->id)
                            if (!editor->double_clicked_selection)
                                editor->move_cursor(calculate_pos_from_mouse());
                } else if (editor->mouse_select.on) {
                    editor->mouse_select.on = false;
                }

                if (world.ui.mouse_just_released[0]) {
                    if (editor->selecting)
                        if (editor->select_start == editor->cur)
                            editor->selecting = false;
                    editor->double_clicked_selection = false;
                    editor->mouse_select.on = false;
                }

                auto flags = get_mouse_flags(editor_area);
                if (flags & MOUSE_DBLCLICKED) {
                    auto pos = calculate_pos_from_mouse();

                    auto classify_char = [&](uchar ch) {
                        if (isspace(ch)) return 0;
                        if (isident(ch)) return 1;
                        return 2;
                    };

                    cur2 start, end;
                    auto type = classify_char(editor->iter(pos).peek());

                    // figure out start
                    {
                        auto it = editor->iter(pos);
                        while (true) {
                            it.prev();
                            if (classify_char(it.peek()) != type || it.y != pos.y) {
                                it.next();
                                break;
                            }
                            if (it.bof())
                                break;
                        }
                        start = it.pos;
                    }

                    // figure out end
                    {
                        auto it = editor->iter(pos);
                        while (true) {
                            it.next();
                            if (classify_char(it.peek()) != type || it.y != pos.y) {
                                if (it.y != pos.y)
                                    it.prev();
                                break;
                            }
                            if (it.eof())
                                break;
                        }
                        end = it.pos;
                    }

                    if (start < end) {
                        editor->selecting = true;
                        editor->select_start = start;
                        editor->double_clicked_selection = true;
                        editor->move_cursor(end);
                    }
                }

                // handle scrolling
                auto dy = ImGui::GetIO().MouseWheel;
                if (dy != 0) {
                    bool flip = true;
                    if (dy < 0) {
                        flip = false;
                        dy = -dy;
                    }

                    dy *= 6;
                    dy += editor->scroll_leftover;

                    editor->scroll_leftover = fmod(dy, font->height);
                    auto lines = (int)(dy / font->height);

                    for (int i = 0; i < lines; i++) {
                        if (flip) {
                            if (editor->view.y > 0) {
                                editor->view.y--;
                                editor->ensure_cursor_on_screen();
                            }
                        } else {
                            if (editor->view.y + 1 < editor->buf->lines.len) {
                                editor->view.y++;
                                editor->ensure_cursor_on_screen();
                            }
                        }
                    }

                    if (lines > 0) editor->scroll_leftover = 0;
                }
            }
        }

        draw_rect(editor_area, rgba(global_colors.background));

        vec2 tab_padding = { 8, 3 };

        boxf tab;
        tab.pos = tabs_area.pos + new_vec2(2, tabs_area.h - tab_padding.y * 2 - font->height);
        tab.x -= pane.tabs_offset;

        i32 tab_to_remove = -1;
        boxf current_tab;

        start_clip(tabs_area); // will become problem once we have popups on tabs

        if (current_pane == 0) {
            if (world.wnd_mouse_pos.show) {
                begin_window("Mouse Pos", &world.wnd_mouse_pos);
                ImGui::Text("mouse_pos = (%.4f, %.4f)", world.ui.mouse_pos.x, world.ui.mouse_pos.y);
                ImGui::Text(
                    "tabs_area: pos = (%.4f, %.4f), size = (%.4f, %.4f)",
                    tabs_area.x,
                    tabs_area.y,
                    tabs_area.w,
                    tabs_area.h
                );
                ImGui::End();
            }
        }

        // draw tabs
        u32 tab_id = 0;
        for (auto&& editor : pane.editors) {
            defer { editor_index++; };

            SCOPED_FRAME();

            bool is_selected = (tab_id == pane.current_editor);

            ccstr label = "<untitled>";
            if (!editor.is_untitled) {
                auto &ind = world.indexer;
                bool external = false;

                if (ind.goroot != NULL && path_has_descendant(ind.goroot, editor.filepath)) {
                    label = get_path_relative_to(editor.filepath, ind.goroot);
                    external = true;
                    // label = our_sprintf("$GOROOT/%s", label);
                } else if (ind.gomodcache != NULL && path_has_descendant(ind.gomodcache, editor.filepath)) {
                    label = get_path_relative_to(editor.filepath, ind.gomodcache);
                    external = true;
                    // label = our_sprintf("$GOMODCACHE/%s", label);
                } else {
                    label = get_path_relative_to(editor.filepath, world.current_path);
                }

                if (external) {
                    label = our_sprintf("[ext] %s", label);
                }
            }

            if (editor.is_unsaved())
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
            if (world.wnd_mouse_pos.show) {
                begin_window("Mouse Pos", &world.wnd_mouse_pos);

                ImGui::Separator();
                ImGui::Text("Tab %d: pos = (%.4f,%.4f), size = (%.4f,%.4f)",
                            editor_index,
                            tab.x, tab.y,
                            tab.w, tab.h);

                if (is_hovered)
                    ImGui::Text("hovered: pos = (%.4f, %.4f), size = (%.4f, %.4f)", tab.x, tab.y, tab.w, tab.h);
                else
                    ImGui::Text("not hovered");

                /*
                if (get_mouse_flags(tab) & MOUSE_HOVER)
                    ImGui::Text("hover flag set");
                else
                    ImGui::Text("hover flag not set");
                */

                ImGui::End();
            }

            vec4f tab_color;
            if (is_selected)
                tab_color = rgba(global_colors.tab_selected);
            else if (is_hovered)
                tab_color = rgba(global_colors.tab_selected, 0.75);
            else
                tab_color = rgba(global_colors.tab_selected, 0.5);

            /*
            if (!is_selected) {
                print("is_hovered: %d", is_hovered);
            }
            */

            draw_rounded_rect(tab, tab_color, 4, ROUND_TL | ROUND_TR);
            draw_string(tab.pos + tab_padding, label, rgba(is_selected ? global_colors.white : global_colors.white_muted));

            auto mouse_flags = get_mouse_flags(tab);
            if (mouse_flags & MOUSE_CLICKED) {
                activate_pane(&pane);
                pane.focus_editor_by_index(tab_id);
            }
            if (mouse_flags & MOUSE_MCLICKED)
                tab_to_remove = tab_id;

            tab.pos.x += tab.w + 5;
            tab_id++;
        }

        end_clip(); // tabs_area

        // if tabs are off screen, see if we can move them back
        if (pane.tabs_offset > 0) {
            auto margin = (pane.current_editor == pane.editors.len - 1) ? 5 : settings.tabs_offset;
            auto space_avail = fmin(
                relu_subf(tabs_area.x + tabs_area.w, (current_tab.x + current_tab.w + margin)),
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
            auto &editor = pane.editors[tab_to_remove];
            if (editor.ask_user_about_unsaved_changes()) {
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
        }

        // draw editor
        if (pane.editors.len > 0) {
            vec2f cur_pos = editor_area.pos + new_vec2f(settings.editor_margin_x, settings.editor_margin_y);
            cur_pos.y += font->offset_y;

            auto editor = pane.get_current_editor();

            struct Highlight {
                cur2 start;
                cur2 end;
                vec4f color;
            };

            List<Highlight> highlights;
            highlights.init();

            // generate editor highlights
            if (editor->buf->tree != NULL) {
                ts_tree_cursor_reset(&editor->buf->cursor, ts_tree_root_node(editor->buf->tree));

                auto start = new_cur2(0, editor->view.y);
                auto end = new_cur2(0, editor->view.y + editor->view.h);

                walk_ts_cursor(&editor->buf->cursor, false, [&](Ast_Node *node, Ts_Field_Type, int depth) -> Walk_Action {
                    auto node_start = node->start();
                    auto node_end = node->end();

                    if (node_end < start) return WALK_SKIP_CHILDREN;
                    if (node_start > end) return WALK_ABORT;
                    // if (node->child_count() != 0) return WALK_CONTINUE;

                    vec4f color; ptr0(&color);
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
                    auto muted = (current_pane != world.current_pane);

                    actual_cursor_positions[current_pane] = cur_pos;    // save position where cursor is drawn for later use
                    bool is_insert_cursor = !world.use_nvim; // (world.nvim.mode == VI_INSERT && is_pane_selected /* && !world.nvim.exiting_insert_mode */);

                    auto pos = cur_pos;
                    pos.y -= font->offset_y;

                    boxf b;
                    b.pos = pos;
                    b.h = (float)font->height;
                    b.w = is_insert_cursor ? 2 : ((float)font->width * chars);

                    auto py = font->height * (settings.line_height - 1.0) / 2;
                    b.y -= py;
                    b.h += py * 2;

                    b.y++;
                    b.h -= 2;

                    draw_rect(b, rgba(global_colors.cursor, muted ? 0.5 : 1.0));
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

                if (buf->lines.len == 0) draw_cursor(1);

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

                cur2 select_start, select_end;
                if (editor->selecting) {
                    auto a = editor->select_start;
                    auto b = editor->cur;
                    if (a > b) {
                        auto tmp = a;
                        a = b;
                        b = tmp;
                    }

                    select_start = a;
                    select_end = b;
                }

                auto draw_highlight = [&](vec4f color, int width, bool fullsize = false) {
                    boxf b;
                    b.pos = cur_pos;
                    b.y -= font->offset_y;
                    b.w = font->width * width;
                    b.h = font->height;

                    auto py = font->height * (settings.line_height - 1.0) / 2;
                    b.y -= py;
                    b.h += py * 2;

                    if (!fullsize) {
                        b.y++;
                        b.h -= 2;
                    }

                    draw_rect(b, color);
                };

                auto relative_y = 0;
                for (u32 y = view.y; y < view.y + view.h; y++, relative_y++) {
                    if (y >= buf->lines.len) break;

                    auto line = &buf->lines[y];

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
                    line_box.y++;
                    line_box.h -= 2;

                    auto bptype = find_breakpoint_stopped_at_this_line();
                    if (bptype == BREAKPOINT_CURRENT_GOROUTINE)
                        draw_rect(line_box, rgba(global_colors.breakpoint_current, 0.25));
                    else if (bptype == BREAKPOINT_OTHER_GOROUTINE)
                        draw_rect(line_box, rgba(global_colors.breakpoint_current, 0.1));
                    else if (bptype == BREAKPOINT_ACTIVE)
                        draw_rect(line_box, rgba(global_colors.breakpoint_active, 0.25));
                    else if (bptype == BREAKPOINT_INACTIVE)
                        draw_rect(line_box, rgba(global_colors.breakpoint_active, 0.15));

                    auto line_number_width = get_line_number_width(editor);

                    {
                        cur_pos.x += settings.line_number_margin_left;
                        ccstr line_number_str = NULL;
                        if (world.replace_line_numbers_with_bytecounts)
                            line_number_str = our_sprintf("%*d", line_number_width, buf->bytecounts[y]);
                        else
                            line_number_str = our_sprintf("%*d", line_number_width, y + 1);
                        auto len = strlen(line_number_str);
                        for (u32 i = 0; i < len; i++)
                            draw_char(&cur_pos, line_number_str[i], rgba(global_colors.white, 0.3));
                        cur_pos.x += settings.line_number_margin_right;
                    }

                    Grapheme_Clusterer gc;
                    gc.init();

                    int cp_idx = 0;
                    gc.feed(line->at(cp_idx)); // feed first character for GB1

                    // jump {view.x} clusters
                    int vx_start = 0;
                    {
                        int vx = 0;
                        while (vx < view.x && cp_idx < line->len) {
                            if (line->at(cp_idx) == '\t') {
                                vx += options.tabsize - (vx % options.tabsize);
                                cp_idx++;
                            } else {
                                auto width = our_wcwidth(line->at(cp_idx));
                                if (width == -1) width = 1;
                                vx += width;

                                cp_idx++;
                                while (cp_idx < line->len && !gc.feed(line->at(cp_idx)))
                                    cp_idx++;
                            }
                        }
                        vx_start = vx;
                    }

                    if (vx_start > view.x)
                        cur_pos.x += (vx_start - view.x) * font->width;

                    u32 x = buf->idx_cp_to_byte(y, cp_idx);
                    u32 vx = vx_start;
                    u32 newx = 0;

                    for (; vx < view.x + view.w; x = newx) {
                        newx = x;

                        if (cp_idx >= line->len) break;

                        auto curr_cp_idx = cp_idx;
                        int curr_cp = line->at(cp_idx);
                        int grapheme_cpsize = 0;

                        {
                            auto uch = curr_cp;
                            while (true) {
                                cp_idx++;
                                newx += uchar_size(uch);
                                grapheme_cpsize++;

                                if (cp_idx >= line->len) break;
                                if (gc.feed(uch = line->at(cp_idx))) break;
                            }
                        }

                        int glyph_width = 0;
                        if (grapheme_cpsize == 1 && curr_cp == '\t')
                            glyph_width = options.tabsize - (vx % options.tabsize);
                        else
                            glyph_width = our_wcwidth(curr_cp);

                        if (glyph_width == -1) glyph_width = 1;

                        vec4f text_color = rgba(global_colors.foreground);

                        if (next_hl != -1) {
                            auto curr = new_cur2((u32)curr_cp_idx, (u32)y);

                            while (next_hl != -1 && curr >= highlights[next_hl].end)
                                if (++next_hl >= highlights.len)
                                    next_hl = -1;

                            if (next_hl != -1) {
                                auto& hl = highlights[next_hl];
                                if (hl.start <= curr && curr < hl.end)
                                    text_color = hl.color;
                            }
                        }

                        if (editor->cur == new_cur2((u32)curr_cp_idx, (u32)y)) {
                            draw_cursor(glyph_width);
                            if (current_pane == world.current_pane && world.use_nvim)
                                text_color = rgba(global_colors.cursor_foreground);
                        } else if (world.use_nvim && world.nvim.mode != VI_INSERT) {
                            auto topline = editor->nvim_data.grid_topline;
                            if (topline <= y && y < topline + NVIM_DEFAULT_HEIGHT) {
                                int i = 0;
                                while (i < glyph_width && vx+i < _countof(editor->highlights[y - topline]))
                                    i++;

                                auto hl = editor->highlights[y - topline][vx];
                                switch (hl) {
                                case HL_INCSEARCH:
                                    draw_highlight(rgba(global_colors.foreground), i);
                                    text_color = rgba(global_colors.background);
                                    break;
                                case HL_SEARCH:
                                    draw_highlight(rgba(global_colors.search_background), i);
                                    text_color = rgba(global_colors.search_foreground);
                                    break;
                                case HL_VISUAL:
                                    draw_highlight(rgba(global_colors.visual_background), i);
                                    text_color = rgba(global_colors.visual_foreground);
                                    break;
                                }
                            }
                        }

                        if (!world.use_nvim && editor->selecting) {
                            auto pos = new_cur2((u32)curr_cp_idx, (u32)y);
                            if (select_start <= pos && pos < select_end) {
                                draw_highlight(rgba(global_colors.visual_background), glyph_width, true);
                                text_color = rgba(global_colors.visual_foreground);
                            }
                        }

                        if (hint.gotype != NULL)
                            if (new_cur2(x, y) == hint.start)
                                actual_parameter_hint_start = cur_pos;

                        uchar uch = curr_cp;
                        if (uch == '\t') {
                            cur_pos.x += font->width * glyph_width;
                        } else if (grapheme_cpsize > 1 || uch > 0x7f) {
                            auto pos = cur_pos;
                            pos.x += (font->width * glyph_width) / 2 - (font->width / 2);
                            draw_char(&pos, 0xfffd, text_color);

                            cur_pos.x += font->width * glyph_width;
                        } else {
                            draw_char(&cur_pos, uch, text_color);
                        }

                        vx += glyph_width;
                    }

                    if (line->len == 0) {
                        if (world.use_nvim) {
                            auto topline = editor->nvim_data.grid_topline;
                            if (topline <= y && y < topline + NVIM_DEFAULT_HEIGHT) {
                                switch (editor->highlights[y - topline][0]) {
                                case HL_INCSEARCH:
                                    draw_highlight(rgba(global_colors.foreground), 1);
                                    break;
                                case HL_SEARCH:
                                    draw_highlight(rgba(global_colors.search_background), 1);
                                    break;
                                case HL_VISUAL:
                                    draw_highlight(rgba(global_colors.visual_background), 1);
                                    break;
                                }
                            }
                        } else {
                            auto pos = new_cur2((u32)0, (u32)y);
                            if (select_start <= pos && pos < select_end)
                                draw_highlight(rgba(global_colors.visual_background), 1, true);
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

            boxf hitbox;
            hitbox.w = PANE_RESIZER_WIDTH;
            hitbox.h = panes_area.h;
            hitbox.y = panes_area.y;
            hitbox.x = panes_area.x + offset - PANE_RESIZER_WIDTH / 2;

            if (test_hover(hitbox, HOVERID_PANE_RESIZERS + i, ImGuiMouseCursor_ResizeEW)) {
                draw_rect(b, rgba(global_colors.pane_resizer_hover));
                if (world.ui.mouse_down[GLFW_MOUSE_BUTTON_LEFT])
                    if (world.resizing_pane == -1)
                        world.resizing_pane = i;
            } else if (world.resizing_pane == i) {
                draw_rect(b, rgba(global_colors.pane_resizer_hover));
            } else {
                draw_rect(b, rgba(global_colors.pane_resizer));
            }
        }

        if (!world.ui.mouse_down[GLFW_MOUSE_BUTTON_LEFT])
            world.resizing_pane = -1;
    }

    {
        draw_rect(status_area, rgba(global_colors.status_area_background));

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
                if (!world.use_nvim) return false;

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
                draw_status_piece(LEFT, command, rgba(global_colors.command_background), rgba(global_colors.command_foreground));
            } else {
                if (world.use_nvim) {
                    auto editor = get_current_editor();
                    if (editor != NULL) {
                        if (editor->is_modifiable()) {
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
                            draw_status_piece(LEFT, mode_str, rgba(global_colors.status_mode_background), rgba(global_colors.status_mode_foreground));
                        } else {
                            draw_status_piece(LEFT, "READONLY", rgba(global_colors.status_mode_background), rgba(global_colors.status_mode_foreground));
                        }
                    }
                }
            }
        }

        switch (world.dbg.state_flag) {
        case DLV_STATE_PAUSED:
            draw_status_piece(LEFT, "PAUSED", rgba(global_colors.status_debugger_paused_background), rgba(global_colors.white));
            break;
        case DLV_STATE_STARTING:
            draw_status_piece(LEFT, "STARTING", rgba(global_colors.status_debugger_starting_background), rgba(global_colors.white));
            break;
        case DLV_STATE_RUNNING:
            draw_status_piece(LEFT, "RUNNING", rgba(global_colors.status_debugger_running_background), rgba(global_colors.white));
            break;
        }

        int index_mouse_flags = 0;
        switch (world.indexer.status) {
        case IND_READY:
            {
                auto mouse_flags = get_mouse_flags(get_status_piece_rect(RIGHT, "INDEX READY"));
                auto opacity = mouse_flags & MOUSE_HOVER ? 1.0 : 0.8;
                index_mouse_flags = draw_status_piece(RIGHT, "INDEX READY", rgba(global_colors.status_index_ready_background, opacity), rgba(global_colors.status_index_ready_foreground, opacity));
            }
            break;
        case IND_WRITING:
            {
                auto mouse_flags = get_mouse_flags(get_status_piece_rect(RIGHT, "INDEXING..."));
                auto opacity = mouse_flags & MOUSE_HOVER ? 1.0 : 0.8;
                index_mouse_flags = draw_status_piece(RIGHT, "INDEXING...", rgba(global_colors.status_index_indexing_background, opacity), rgba(global_colors.status_index_indexing_foreground, opacity));
            }
            break;
        case IND_READING:
            {
                auto mouse_flags = get_mouse_flags(get_status_piece_rect(RIGHT, "RUNNING..."));
                auto opacity = mouse_flags & MOUSE_HOVER ? 1.0 : 0.8;
                index_mouse_flags = draw_status_piece(RIGHT, "RUNNING...", rgba(global_colors.status_index_indexing_background, opacity), rgba(global_colors.status_index_indexing_foreground, opacity));
            }
            break;
        }

        if (index_mouse_flags & MOUSE_CLICKED) {
            world.wnd_index_log.show ^= 1;
        }

        auto curr_editor = get_current_editor();
        if (curr_editor != NULL) {
            auto cur = curr_editor->cur;

            auto s = our_sprintf("%d,%d", cur.y+1, cur.x+1);

            if (world.use_nvim) {
                auto view = curr_editor->view;

                auto curr = view.y;
                auto total = relu_sub(curr_editor->buf->lines.len, view.h);

                auto blah = [&]() {
                    if (total == 0) return curr > 0 ? "Bot" : "All";
                    if (curr == 0) return "Top";
                    if (curr >= total) return "Bot";
                    return our_sprintf("%d%%", (int)((float)curr/(float)total * 100));
                };

                s = our_sprintf("%s  %s", s, blah());
            }

            draw_status_piece(RIGHT, s, rgba(global_colors.white, 0.0), rgba("#aaaaaa"));
        }
    }

    if (world.wnd_hover_info.show) {
        auto &wnd = world.wnd_hover_info;

        begin_window("Hover Info", &wnd);

        ImGui::Text("id: %d", hover.id);
        ImGui::Text("id last frame: %d", hover.id_last_frame);
        ImGui::Text("cursor: %d", hover.id);
        ImGui::Text("start_time: %llums ago", (current_time_nano() - hover.start_time) / 1000000);
        ImGui::Text("ready: %d", hover.ready);

        ImGui::End();
    }


    if (world.cmd_unfocus_all_windows) {
        world.cmd_unfocus_all_windows = false;
        ImGui::SetWindowFocus(NULL);
    }
}

ImVec2 icon_button_padding = ImVec2(4, 2);

bool UI::imgui_icon_button(ccstr icon) {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, icon_button_padding);
    auto ret = ImGui::Button(icon);
    ImGui::PopStyleVar();
    return ret;
}

void UI::end_frame() {
    flush_verts();

    ImGui::Render();

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
                if (len > AUTOCOMPLETE_TRUNCATE_LENGTH)
                    len = AUTOCOMPLETE_TRUNCATE_LENGTH + 3;

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

                // menu.x = fmin(actual_cursor_position.x - strlen(ac.ac.prefix) * font->width, world.window_size.x - menu.w);
                // menu.y = fmin(actual_cursor_position.y - font->offset_y + font->height, world.window_size.y - menu.h);

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

                draw_bordered_rect_outer(menu, rgba(global_colors.autocomplete_background), rgba(global_colors.autocomplete_border), 1, 4);

                boxf items_area = menu;
                items_area.w = menu.w;

                float menu_padding = settings.autocomplete_menu_padding;
                auto items_pos = items_area.pos + new_vec2f(menu_padding, menu_padding);

                for (int i = ac.view; i < ac.view + num_items; i++) {
                    auto idx = ac.filtered_results->at(i);

                    vec3f color = global_colors.white;

                    if (i == ac.selection) {
                        boxf b;
                        b.pos = items_pos;
                        b.h = font->height + (settings.autocomplete_item_padding_y * 2);
                        b.w = items_area.w - (settings.autocomplete_menu_padding * 2);
                        draw_rounded_rect(b, rgba(global_colors.autocomplete_selection), 4, ROUND_ALL);
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
                        else if (result.type == ACR_IMPORT && result.import_is_existing)
                            actual_color = new_vec3f(1.0, 0.8, 1.0);

                        // add icon based on type
                        // show a bit more helpful info (like inline signature for funcs)
                        // show extended info on a panel to the right

                        auto get_decl_type = [&]() {
                            switch (result.type) {
                            case ACR_DECLARATION:
                                switch (result.declaration_godecl->type) {
                                case GODECL_IMPORT:
                                    return "import";
                                case GODECL_CONST:
                                    return "const";
                                case GODECL_TYPE:
                                    return "type";
                                case GODECL_FUNC:
                                    return "func";
                                case GODECL_VAR:
                                case GODECL_SHORTVAR:
                                case GODECL_TYPECASE:
                                    return "var";
                                case GODECL_FIELD:
                                    return "field";
                                case GODECL_PARAM:
                                    return "param";
                                }
                                return "unknown";
                            case ACR_KEYWORD:
                                return "keyword";
                            case ACR_POSTFIX:
                                return "postfix";
                            case ACR_IMPORT:
                                return "import";
                            }
                            return "unknown";
                        };

                        auto pos = items_pos + new_vec2f(settings.autocomplete_item_padding_x, settings.autocomplete_item_padding_y);

                        auto type_str = our_sprintf("(%s) ", get_decl_type());
                        draw_string(pos, type_str, rgba(color, 0.5));
                        pos.x += font->width * strlen(type_str);

                        auto str = (cstr)our_strcpy(result.name);
                        if (strlen(str) > AUTOCOMPLETE_TRUNCATE_LENGTH)
                            str = (cstr)our_sprintf("%.*s...", AUTOCOMPLETE_TRUNCATE_LENGTH, str);

                        auto avail_width = items_area.w - settings.autocomplete_item_padding_x * 2;
                        if (strlen(str) * font->width > avail_width)
                            str[(int)(avail_width / font->width)] = '\0';

                        draw_string(pos, str, rgba(actual_color));

                        text_end = pos.x + strlen(str) * font->width;
                    }

                    auto render_description = [&]() -> ccstr {
                        switch (result.type) {
                        case ACR_DECLARATION: {
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
                            case GODECL_PARAM: {
                                auto gotype = result.declaration_evaluated_gotype;
                                if (gotype == NULL) return "";

                                Type_Renderer rend;
                                rend.init();
                                rend.write_type(gotype, false);
                                return rend.finish();
                            }
                            }
                            break;
                        }
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
                            break;
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

                    {
                        SCOPED_FRAME();

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

                    auto color = rgba(global_colors.white);

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
            bg.x = fmin(actual_parameter_hint_start.x, world.window_size.x - bg.w);
            bg.y = fmin(actual_parameter_hint_start.y - font->offset_y - bg.h - settings.parameter_hint_margin_y, world.window_size.y - bg.h);

            draw_bordered_rect_outer(bg, rgba(color_darken(global_colors.background, 0.1), 1.0), rgba(global_colors.white, 0.8), 1, 4);

            auto text_pos = bg.pos;
            text_pos.x += settings.parameter_hint_padding_x;
            text_pos.y += settings.parameter_hint_padding_y;

            text_pos.y += font->offset_y;

            {
                u32 len = strlen(help_text);
                vec4f color = rgba(global_colors.foreground, 0.75);
                float opacity = 1.0;

                int j = 0;

                for (u32 i = 0; i < len; i++) {
                    while (j < token_changes.len && i == token_changes[j].index) {
                        switch (token_changes[j].token) {
                        case HINT_CURRENT_PARAM: opacity = 1.0; break;
                        case HINT_NOT_CURRENT_PARAM: opacity = 0.5; break;
                        case HINT_NAME: color = rgba(global_colors.foreground); break;
                        case HINT_TYPE: color = rgba(global_colors.type); break;
                        case HINT_NORMAL: color = rgba(global_colors.foreground, 0.75); break;
                        }

                        j++;
                    }
                    draw_char(&text_pos, help_text[i], rgba(color.rgb, color.a * opacity));
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
        tabs_area.h = 24; // ???
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

            // Previously we called editor.ensure_cursor_on_screen(), but that
            // moves the *cursor* onto the screen. But often when the user
            // builds, their current cursor position is significant and we
            // don't want to move it. Instead we should move the *view* so the
            // cursor is on the screen.
            editor.ensure_cursor_on_screen_by_moving_view();

            if (world.use_nvim) {
                auto& nv = world.nvim;
                nv.start_request_message("nvim_win_set_option", 3);
                nv.writer.write_int(editor.nvim_data.win_id);
                nv.writer.write_string("scroll");
                nv.writer.write_int(editor.view.h / 2);
                nv.end_message();
            }
        }
    }
}

const u64 WINDOWS_RESIZE_FROM_EDGES_FEEDBACK_TIMER = 40000000;

bool UI::test_hover(boxf area, int id, ImGuiMouseCursor cursor) {
    if (!(get_mouse_flags(area) & MOUSE_HOVER))
        return false;

    hover.id = id;
    hover.cursor = cursor;

    auto now = current_time_nano();

    if (id != hover.id_last_frame) {
        if (world.wnd_hover_info.show) {
            ImGui::Begin("Hover Info");
            ImGui::Text("id = %d, last = %d", id, hover.id_last_frame);
            ImGui::End();
        }

        hover.start_time = now;
        hover.ready = false;
    }

    if (now - hover.start_time > WINDOWS_RESIZE_FROM_EDGES_FEEDBACK_TIMER)
        hover.ready = true;

    return hover.ready;
}
