#define _USE_MATH_DEFINES
#include <math.h>
#include <inttypes.h>

#include <fontconfig/fontconfig.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "ui.hpp"
#include "common.hpp"
#include "fonts.hpp"
#include "world.hpp"
#include "go.hpp"
#include "unicode.hpp"
#include "settings.hpp"
#include "icons.h"
#include "fzy_match.h"
#include "defer.hpp"
#include "enums.hpp"
#include "tree_sitter_crap.hpp"
#include "binaries.h"

namespace im = ImGui;

void open_ft_node(FT_Node *it);

UI ui;
Global_Colors global_colors;

const vec4f RGBA_TRANSPARENT = rgba("#000000", 0.0);

ccstr get_menu_command_key(Command cmd);
bool menu_command(Command cmd, bool selected = false);

const int ZOOM_LEVELS[] = {50, 67, 75, 80, 90, 100,110, 125, 133, 140, 150, 175, 200};
const int ZOOM_LEVELS_COUNT = _countof(ZOOM_LEVELS);

void init_global_colors() {
#if 0 // def DEBUG_BUILD
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

    if (mods & CP_MOD_CMD)   parts.append(icon ? ICON_MD_KEYBOARD_COMMAND_KEY : "Cmd");
    if (mods & CP_MOD_SHIFT) parts.append(icon ? ICON_MD_ARROW_UPWARD : "Shift");
    if (mods & CP_MOD_ALT)   parts.append(icon ? ICON_MD_KEYBOARD_OPTION_KEY : "Option");
    if (mods & CP_MOD_CTRL)  parts.append(icon ? ICON_MD_KEYBOARD_CONTROL_KEY : "Ctrl");

    Text_Renderer rend; rend.init();
    For (&parts) {
        if (icon)
            rend.write("%s", it);
        else
            rend.write("%s + ", it);
    }
    rend.write("%s", key);
    return rend.finish();
}

void keep_item_inside_scroll() {
    auto offset = im::GetWindowPos().y + im::GetScrollY();
    auto top = im::GetWindowContentRegionMin().y + offset;
    auto bot = im::GetWindowContentRegionMax().y + offset;
    auto pos = im::GetCursorScreenPos().y;

    auto hi = top + (bot - top) * 0.9;
    if (pos > hi)
        im::SetScrollY(im::GetScrollY() + (pos - hi));

    auto lo = top + (bot - top) * 0.1;
    if (pos < lo)
        im::SetScrollY(im::GetScrollY() - (lo - pos));
}

// These next two functions are copy pasted from imgui with one small change:
// passing ImGuiWindowFlags_NoMove to BeginPopupEx. This is because imgui
// refuses to just enable this obvious behavior by default, and also does not
// let us pass flags to BeginPopupEx, resulting in the need for this stupidity.
// Third-party libraries are the devil.
//
// Update: we now have an extra check at the end to handle the escape key.

bool UI::im_begin_popup_context_item(const char* str_id, ImGuiPopupFlags popup_flags) {
    ImGuiWindow* window = GImGui->CurrentWindow;
    if (window->SkipItems)
        return false;
    ImGuiID id = str_id ? window->GetID(str_id) : GImGui->LastItemData.ID; // If user hasn't passed an ID, we can use the LastItemID. Using LastItemID as a Popup ID won't conflict!
    IM_ASSERT(id != 0);                                                  // You cannot pass a NULL str_id if the last item has no identifier (e.g. a Text() item)
    int mouse_button = (popup_flags & ImGuiPopupFlags_MouseButtonMask_);
    if (im::IsMouseReleased(mouse_button) && im::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup))
        im::OpenPopupEx(id, popup_flags);
    auto ret = im::BeginPopupEx(id, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings);
    if (ret)
        if (im_key_pressed(CP_KEY_ESCAPE))
            im::CloseCurrentPopup();
    return ret;
}

bool UI::im_begin_popup_context_window(const char* str_id, ImGuiPopupFlags popup_flags) {
    ImGuiWindow* window = GImGui->CurrentWindow;
    if (!str_id)
        str_id = "window_context";
    ImGuiID id = window->GetID(str_id);
    int mouse_button = (popup_flags & ImGuiPopupFlags_MouseButtonMask_);
    if (im::IsMouseReleased(mouse_button) && im::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup))
        if (!(popup_flags & ImGuiPopupFlags_NoOpenOverItems) || !im::IsAnyItemHovered())
            im::OpenPopupEx(id, popup_flags);
    auto ret = im::BeginPopupEx(id, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings);
    if (ret)
        if (im_key_pressed(CP_KEY_ESCAPE))
            im::CloseCurrentPopup();
    return ret;
}

ccstr get_key_name(int key) {
    switch (key) {
    case CP_KEY_F1: return "F1";
    case CP_KEY_F2: return "F2";
    case CP_KEY_F3: return "F3";
    case CP_KEY_F4: return "F4";
    case CP_KEY_F5: return "F5";
    case CP_KEY_F6: return "F6";
    case CP_KEY_F7: return "F7";
    case CP_KEY_F8: return "F8";
    case CP_KEY_F9: return "F9";
    case CP_KEY_F10: return "F10";
    case CP_KEY_F11: return "F11";
    case CP_KEY_F12: return "F12";
    case CP_KEY_TAB: return "Tab";
    case CP_KEY_ENTER: return "Enter";
    case CP_KEY_A: return "A";
    case CP_KEY_B: return "B";
    case CP_KEY_C: return "C";
    case CP_KEY_D: return "D";
    case CP_KEY_E: return "E";
    case CP_KEY_F: return "F";
    case CP_KEY_G: return "G";
    case CP_KEY_H: return "H";
    case CP_KEY_I: return "I";
    case CP_KEY_J: return "J";
    case CP_KEY_K: return "K";
    case CP_KEY_L: return "L";
    case CP_KEY_M: return "M";
    case CP_KEY_N: return "N";
    case CP_KEY_O: return "O";
    case CP_KEY_P: return "P";
    case CP_KEY_Q: return "Q";
    case CP_KEY_R: return "R";
    case CP_KEY_S: return "S";
    case CP_KEY_T: return "T";
    case CP_KEY_U: return "U";
    case CP_KEY_V: return "V";
    case CP_KEY_W: return "W";
    case CP_KEY_X: return "X";
    case CP_KEY_Y: return "Y";
    case CP_KEY_Z: return "Z";
    case CP_KEY_LEFT_BRACKET: return "[";
    case CP_KEY_RIGHT_BRACKET: return "]";
    case CP_KEY_COMMA: return ",";
    case CP_KEY_MINUS: return "-";
    case CP_KEY_EQUAL: return "=";
    case CP_KEY_SLASH: return "/";
    }
    return NULL;
}

ImGuiKey cp_key_to_imgui_key(Key key) {
    switch (key) {
    case CP_KEY_TAB: return ImGuiKey_Tab;
    case CP_KEY_LEFT: return ImGuiKey_LeftArrow;
    case CP_KEY_RIGHT: return ImGuiKey_RightArrow;
    case CP_KEY_UP: return ImGuiKey_UpArrow;
    case CP_KEY_DOWN: return ImGuiKey_DownArrow;
    case CP_KEY_PAGE_UP: return ImGuiKey_PageUp;
    case CP_KEY_PAGE_DOWN: return ImGuiKey_PageDown;
    case CP_KEY_HOME: return ImGuiKey_Home;
    case CP_KEY_END: return ImGuiKey_End;
    case CP_KEY_INSERT: return ImGuiKey_Insert;
    case CP_KEY_DELETE: return ImGuiKey_Delete;
    case CP_KEY_BACKSPACE: return ImGuiKey_Backspace;
    case CP_KEY_SPACE: return ImGuiKey_Space;
    case CP_KEY_ENTER: return ImGuiKey_Enter;
    case CP_KEY_ESCAPE: return ImGuiKey_Escape;
    case CP_KEY_APOSTROPHE: return ImGuiKey_Apostrophe;
    case CP_KEY_COMMA: return ImGuiKey_Comma;
    case CP_KEY_MINUS: return ImGuiKey_Minus;
    case CP_KEY_PERIOD: return ImGuiKey_Period;
    case CP_KEY_SLASH: return ImGuiKey_Slash;
    case CP_KEY_SEMICOLON: return ImGuiKey_Semicolon;
    case CP_KEY_EQUAL: return ImGuiKey_Equal;
    case CP_KEY_LEFT_BRACKET: return ImGuiKey_LeftBracket;
    case CP_KEY_BACKSLASH: return ImGuiKey_Backslash;
    case CP_KEY_RIGHT_BRACKET: return ImGuiKey_RightBracket;
    case CP_KEY_GRAVE_ACCENT: return ImGuiKey_GraveAccent;
    case CP_KEY_CAPS_LOCK: return ImGuiKey_CapsLock;
    case CP_KEY_SCROLL_LOCK: return ImGuiKey_ScrollLock;
    case CP_KEY_NUM_LOCK: return ImGuiKey_NumLock;
    case CP_KEY_PRINT_SCREEN: return ImGuiKey_PrintScreen;
    case CP_KEY_PAUSE: return ImGuiKey_Pause;
    case CP_KEY_KP_0: return ImGuiKey_Keypad0;
    case CP_KEY_KP_1: return ImGuiKey_Keypad1;
    case CP_KEY_KP_2: return ImGuiKey_Keypad2;
    case CP_KEY_KP_3: return ImGuiKey_Keypad3;
    case CP_KEY_KP_4: return ImGuiKey_Keypad4;
    case CP_KEY_KP_5: return ImGuiKey_Keypad5;
    case CP_KEY_KP_6: return ImGuiKey_Keypad6;
    case CP_KEY_KP_7: return ImGuiKey_Keypad7;
    case CP_KEY_KP_8: return ImGuiKey_Keypad8;
    case CP_KEY_KP_9: return ImGuiKey_Keypad9;
    case CP_KEY_KP_DECIMAL: return ImGuiKey_KeypadDecimal;
    case CP_KEY_KP_DIVIDE: return ImGuiKey_KeypadDivide;
    case CP_KEY_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
    case CP_KEY_KP_SUBTRACT: return ImGuiKey_KeypadSubtract;
    case CP_KEY_KP_ADD: return ImGuiKey_KeypadAdd;
    case CP_KEY_KP_ENTER: return ImGuiKey_KeypadEnter;
    case CP_KEY_KP_EQUAL: return ImGuiKey_KeypadEqual;
    case CP_KEY_LEFT_SHIFT: return ImGuiKey_LeftShift;
    case CP_KEY_LEFT_CONTROL: return ImGuiKey_LeftCtrl;
    case CP_KEY_LEFT_ALT: return ImGuiKey_LeftAlt;
    case CP_KEY_LEFT_SUPER: return ImGuiKey_LeftSuper;
    case CP_KEY_RIGHT_SHIFT: return ImGuiKey_RightShift;
    case CP_KEY_RIGHT_CONTROL: return ImGuiKey_RightCtrl;
    case CP_KEY_RIGHT_ALT: return ImGuiKey_RightAlt;
    case CP_KEY_RIGHT_SUPER: return ImGuiKey_RightSuper;
    case CP_KEY_MENU: return ImGuiKey_Menu;
    case CP_KEY_0: return ImGuiKey_0;
    case CP_KEY_1: return ImGuiKey_1;
    case CP_KEY_2: return ImGuiKey_2;
    case CP_KEY_3: return ImGuiKey_3;
    case CP_KEY_4: return ImGuiKey_4;
    case CP_KEY_5: return ImGuiKey_5;
    case CP_KEY_6: return ImGuiKey_6;
    case CP_KEY_7: return ImGuiKey_7;
    case CP_KEY_8: return ImGuiKey_8;
    case CP_KEY_9: return ImGuiKey_9;
    case CP_KEY_A: return ImGuiKey_A;
    case CP_KEY_B: return ImGuiKey_B;
    case CP_KEY_C: return ImGuiKey_C;
    case CP_KEY_D: return ImGuiKey_D;
    case CP_KEY_E: return ImGuiKey_E;
    case CP_KEY_F: return ImGuiKey_F;
    case CP_KEY_G: return ImGuiKey_G;
    case CP_KEY_H: return ImGuiKey_H;
    case CP_KEY_I: return ImGuiKey_I;
    case CP_KEY_J: return ImGuiKey_J;
    case CP_KEY_K: return ImGuiKey_K;
    case CP_KEY_L: return ImGuiKey_L;
    case CP_KEY_M: return ImGuiKey_M;
    case CP_KEY_N: return ImGuiKey_N;
    case CP_KEY_O: return ImGuiKey_O;
    case CP_KEY_P: return ImGuiKey_P;
    case CP_KEY_Q: return ImGuiKey_Q;
    case CP_KEY_R: return ImGuiKey_R;
    case CP_KEY_S: return ImGuiKey_S;
    case CP_KEY_T: return ImGuiKey_T;
    case CP_KEY_U: return ImGuiKey_U;
    case CP_KEY_V: return ImGuiKey_V;
    case CP_KEY_W: return ImGuiKey_W;
    case CP_KEY_X: return ImGuiKey_X;
    case CP_KEY_Y: return ImGuiKey_Y;
    case CP_KEY_Z: return ImGuiKey_Z;
    case CP_KEY_F1: return ImGuiKey_F1;
    case CP_KEY_F2: return ImGuiKey_F2;
    case CP_KEY_F3: return ImGuiKey_F3;
    case CP_KEY_F4: return ImGuiKey_F4;
    case CP_KEY_F5: return ImGuiKey_F5;
    case CP_KEY_F6: return ImGuiKey_F6;
    case CP_KEY_F7: return ImGuiKey_F7;
    case CP_KEY_F8: return ImGuiKey_F8;
    case CP_KEY_F9: return ImGuiKey_F9;
    case CP_KEY_F10: return ImGuiKey_F10;
    case CP_KEY_F11: return ImGuiKey_F11;
    case CP_KEY_F12: return ImGuiKey_F12;
    default: return ImGuiKey_None;
    }
}

ccstr get_menu_command_key(Command cmd) {
    auto info = command_info_table[cmd];

    auto sc = info.shortcuts;
    if (!sc) return NULL;

    auto keyname = get_key_name(sc->key);
    if (!keyname) return NULL;

    auto s = new_list(char);
    for (int i = 0, len = strlen(keyname); i < len; i++) {
        auto it = keyname[i];
        if (!i) it = toupper(it);
        s->append(it);
    }
    s->append('\0');
    return format_key(sc->mods, s->items, true);
}

bool menu_command(Command cmd, bool selected) {
    bool clicked = im::MenuItem(
        get_command_name(cmd),
        get_menu_command_key(cmd),
        selected,
        is_command_enabled(cmd)
    );

    if (clicked) handle_command(cmd, true);
    return clicked;
}

int get_line_number_width(Editor *editor) {
    return max(4, (int)log10(editor->buf->lines.len) + 1);
}

vec3f merge_colors(vec3f a, vec3f b, float perc) {
    vec3f ret; ptr0(&ret);

    ret.r = a.r + (b.r - a.r) * perc;
    ret.g = a.g + (b.g - a.g) * perc;
    ret.b = a.b + (b.b - a.b) * perc;
    return ret;
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
    switch (editor->lang) {
    case LANG_GO:
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
        case TS_TYPE_IDENTIFIER: {
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

            auto start = node->start();
            auto end = node->start();

            start = new_cur2(editor->buf->idx_byte_to_cp(start.y, start.x), start.y);
            end = new_cur2(editor->buf->idx_byte_to_cp(end.y, end.x), end.y);

            char token[16] = {0};
            auto it = editor->iter(start);
            for (u32 i = 0; i < _countof(token) && it.pos != end; i++)
                token[i] = it.next();

            For (&keywords) {
                if (streq(it, token)) {
                    *out = rgba(global_colors.keyword);
                    return true;
                }
            }

            For (&builtin_types) {
                if (streq(it, token)) {
                    *out = rgba(global_colors.type);
                    return true;
                }
            }

            For (&builtin_others) {
                if (streq(it, token)) {
                    *out = rgba(global_colors.builtin);
                    return true;
                }
            }
            break;
        }
        }
    case LANG_GOMOD:
        switch (node->type()) {
        case TSGM_LPAREN:
        case TSGM_RPAREN:
            *out = rgba(global_colors.punctuation, 0.75);
            return true;
        case TSGM_MODULE_PATH:
        case TSGM_STRING_LITERAL:
        case TSGM_RAW_STRING_LITERAL:
        case TSGM_INTERPRETED_STRING_LITERAL:
            *out = rgba(global_colors.string_literal, 0.75);
            return true;
        case TSGM_GO_VERSION:
        case TSGM_VERSION:
            *out = rgba(global_colors.number_literal, 0.75);
            return true;
        case TSGM_REQUIRE:
        case TSGM_EXCLUDE:
        case TSGM_REPLACE:
        case TSGM_MODULE:
        case TSGM_GO:
            *out = rgba(global_colors.keyword);
            return true;
        }
        break;
    case LANG_GOWORK:
        switch (node->type()) {
        case TSGW_LPAREN:
        case TSGW_RPAREN:
            *out = rgba(global_colors.punctuation, 0.75);
            return true;
        case TSGW_MODULE_PATH:
        case TSGW_STRING_LITERAL:
        case TSGW_RAW_STRING_LITERAL:
        case TSGW_INTERPRETED_STRING_LITERAL:
            *out = rgba(global_colors.string_literal, 0.75);
            return true;
        case TSGW_GO_VERSION:
        case TSGW_VERSION:
            *out = rgba(global_colors.number_literal, 0.75);
            return true;
        case TSGW_REPLACE:
        case TSGW_USE:
        case TSGW_GO:
            *out = rgba(global_colors.keyword);
            return true;
        }
        break;
    }

    return false;
}

Pretty_Menu *UI::pretty_menu_start(ImVec2 padding) {
    auto ret = new_object(Pretty_Menu);
    ret->drawlist = im::GetWindowDrawList();
    ret->padding = padding;
    return ret;
}

void UI::pretty_menu_text(Pretty_Menu *pm, ccstr text, ImU32 color) {
    if (color == PM_DEFAULT_COLOR)
        color = pm->text_color;

    pm->drawlist->AddText(pm->pos, color, text);
    pm->pos.x += im::CalcTextSize(text).x;
}

void UI::pretty_menu_item(Pretty_Menu *pm, bool selected) {
    auto h = im::CalcTextSize("Some Text").y;
    auto w = im::GetContentRegionAvail().x;

    auto pad = pm->padding;
    auto tl = im::GetCursorScreenPos();
    auto br = tl + ImVec2(w, h);
    br.y += (pad.y * 2);

    pm->tl = tl;
    pm->br = br;
    pm->text_tl = tl + pad;
    pm->text_br = br - pad;

    im::Dummy(ImVec2(0.0, pm->br.y - pm->tl.y));
    if (selected) {
        pm->text_color = IM_COL32(0, 0, 0, 255);
        pm->drawlist->AddRectFilled(pm->tl, pm->br, IM_COL32(255, 255, 255, 255), 4);
    } else {
        pm->text_color = im::GetColorU32(ImGuiCol_Text);
    }

    pm->pos = pm->text_tl;
}

void UI::begin_window(ccstr title, Wnd *wnd, int flags, bool noclose, bool noescape) {
    // Removing ImGuiWindowFlags_NoNavInputs for now. We originally added it
    // because ImGui was adding builtin key handling for tree nodes for us,
    // fucking with our arrow key handling for search results. Not sure if it
    // was the recent upgrade, but that no longer happens, so we're turning
    // this back on because it also disabled tabbing between inputs.
    im::Begin(title, noclose ? NULL : &wnd->show, flags); //| ImGuiWindowFlags_NoNavInputs);

    init_window(wnd);

    if (wnd->focused && !noescape)
        if (im_get_keymods() == CP_MOD_NONE)
            if (im_key_pressed(CP_KEY_ESCAPE))
                wnd->show = false;
}

void UI::init_window(Wnd *wnd) {
    // https://github.com/ocornut/imgui/issues/4293#issuecomment-914322632
    bool might_be_focusing = (!wnd->focused_prev && wnd->focused);
    wnd->focused_prev = wnd->focused;
    wnd->focused = im_is_window_focused();
    wnd->focusing = might_be_focusing && wnd->focused;

    wnd->appearing = im::IsWindowAppearing();

    if (check_cmd_flag(&wnd->cmd_focus)) {
        im::SetWindowFocus();
    }

    if (check_cmd_flag(&wnd->cmd_make_visible_but_dont_focus)) {
        im::SetWindowFocus();
        im::SetWindowFocus(NULL);
    }
}

void UI::begin_centered_window(ccstr title, Wnd *wnd, int flags, int width, bool noclose, bool noescape) {
    if (width != -1) {
        im::SetNextWindowSize(ImVec2(width, -1));
    } else {
        flags |= ImGuiWindowFlags_AlwaysAutoResize;
    }
    flags |= ImGuiWindowFlags_NoDocking;

    im::SetNextWindowPos(ImVec2(world.display_size.x/2, 150), ImGuiCond_Always, ImVec2(0.5f, 0));
    begin_window(title, wnd, flags, noclose, noescape);
}

void UI::help_marker(fn<void()> cb) {
    im::TextDisabled(ICON_MD_HELP_OUTLINE);
    if (im::IsItemHovered()) {
        im::BeginTooltip();
        im::PushTextWrapPos(im::GetFontSize() * 20.0f);
        cb();
        im::PopTextWrapPos();
        im::EndTooltip();
    }
}

void UI::help_marker(ccstr text) {
    help_marker([&]() {
        im::TextWrapped("%s", text);
    });
}

void UI::render_godecl(Godecl *decl) {
    auto flags = ImGuiTreeNodeFlags_SpanAvailWidth;
    bool open = im::TreeNodeEx(decl, flags, "%s", godecl_type_str(decl->type));

    /*
    if (im::IsItemClicked())
        goto_file_and_pos(current_render_godecl_filepath, decl->name_start, true);
    */

    if (open) {
        im::Text("decl_start: %s", decl->decl_start.str());
        im::Text("spec_start: %s", decl->spec_start.str());
        im::Text("name_start: %s", decl->name_start.str());
        im::Text("name: %s", decl->name);

        switch (decl->type) {
        case GODECL_IMPORT:
            im::Text("import_path: %s", decl->import_path);
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
        im::TreePop();
    }
}

void UI::render_gotype(Gotype *gotype, ccstr field) {
    if (!gotype) {
        im::Text("%s: null", field);
        return;
    }

    auto flags = ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_SpanAvailWidth;
    bool is_open = false;

    if (!field)
        is_open = im::TreeNodeEx(gotype, flags, "%s", gotype_type_str(gotype->type));
    else
        is_open = im::TreeNodeEx(gotype, flags, "%s: %s", field, gotype_type_str(gotype->type));

    if (is_open) {
        switch (gotype->type) {
        case GOTYPE_ID:
            im::Text("name: %s", gotype->id_name);
            im::Text("pos: %s", gotype->id_pos.str());
            break;
        case GOTYPE_SEL:
            im::Text("package: %s", gotype->sel_name);
            im::Text("sel: %s", gotype->sel_sel);
            break;
        case GOTYPE_MAP:
            render_gotype(gotype->map_key, "key");
            render_gotype(gotype->map_value, "value");
            break;
        case GOTYPE_STRUCT:
        case GOTYPE_INTERFACE: {
            auto render_shit = [&](void* ptr, Godecl *field, ccstr tag, int i) {
                if (im::TreeNodeEx(ptr, flags, "spec %d", i)) {
                    if (tag)
                        im::Text("tag: %s", tag);
                    render_godecl(field);
                    im::TreePop();
                }
            };

            if (gotype->type == GOTYPE_STRUCT) {
                int i = 0;
                For (gotype->struct_specs)
                    render_shit(&it, it.field, it.tag, i++);
            } else {
                int i = 0;
                For (gotype->interface_specs)
                    render_shit(&it, it.field, NULL, i++);
            }
            break;
        }

        case GOTYPE_POINTER:
        case GOTYPE_SLICE:
        case GOTYPE_ARRAY:
        case GOTYPE_LAZY_INDEX:
        case GOTYPE_LAZY_CALL:
        case GOTYPE_LAZY_DEREFERENCE:
        case GOTYPE_LAZY_REFERENCE:
        case GOTYPE_LAZY_ARROW:
        case GOTYPE_ASSERTION:
            render_gotype(gotype->base, "base");
            break;

        case GOTYPE_CHAN:
            render_gotype(gotype->chan_base, "base");
            im::Text("direction: %d", gotype->chan_direction);
            break;

        case GOTYPE_FUNC:
            render_gotype(gotype->func_recv, "recv");

            if (!gotype->func_sig.params) {
                im::Text("params: NULL");
            } else if (im::TreeNodeEx(&gotype->func_sig.params, flags, "params:")) {
                For (gotype->func_sig.params)
                    render_godecl(&it);
                im::TreePop();
            }

            if (!gotype->func_sig.result) {
                im::Text("result: NULL");
            } else if (im::TreeNodeEx(&gotype->func_sig.result, flags, "result:")) {
                For (gotype->func_sig.result)
                    render_godecl(&it);
                im::TreePop();
            }

            render_gotype(gotype->func_recv);
            break;

        case GOTYPE_MULTI:
            For (gotype->multi_types) render_gotype(it);
            break;

        case GOTYPE_RANGE:
            render_gotype(gotype->range_base, "base");
            im::Text("type: %d", gotype->range_type);
            break;

        case GOTYPE_LAZY_ID:
            im::Text("name: %s", gotype->lazy_id_name);
            im::Text("pos: %s", gotype->lazy_id_pos.str());
            break;

        case GOTYPE_LAZY_SEL:
            render_gotype(gotype->lazy_sel_base, "base");
            im::Text("sel: %s", gotype->lazy_sel_sel);
            break;

        case GOTYPE_LAZY_ONE_OF_MULTI:
            render_gotype(gotype->lazy_one_of_multi_base, "base");
            im::Text("index: %d", gotype->lazy_one_of_multi_index);
            break;
        }
        im::TreePop();
    }
}

void UI::render_ts_cursor(TSTreeCursor *curr, Parse_Lang lang, cur2 open_cur) {
    int last_depth = 0;
    bool last_open = false;

    auto pop = [&](int new_depth) {
        if (new_depth > last_depth) return;

        if (last_open)
            im::TreePop();
        for (i32 i = 0; i < last_depth - new_depth; i++)
            im::TreePop();
    };

    walk_ts_cursor(curr, false, [&](Ast_Node *node, Ts_Field_Type field_type, int depth) -> Walk_Action {
        if (node->anon() && !world.wnd_tree_viewer.show_anon_nodes)
            return WALK_SKIP_CHILDREN;

        if (node->type() == TS_COMMENT && !world.wnd_tree_viewer.show_comments)
            return WALK_SKIP_CHILDREN;

        // auto changed = ts_node_has_changes(node->node);

        pop(depth);
        last_depth = depth;

        auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

        if (!node->child_count())
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;

        ccstr type_str = "(unknown)";
        switch (lang) {
        case LANG_GO:     type_str = ts_ast_type_str((Ts_Ast_Type)node->type());   break;
        case LANG_GOMOD:  type_str = tsgm_ast_type_str((Tsgm_Ast_Type)node->type()); break;
        case LANG_GOWORK: type_str = tsgw_ast_type_str((Tsgw_Ast_Type)node->type()); break;
        }

        if (node->anon())
            im::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(128, 128, 128));
        if (node->type() == TS_COMMENT)
            im::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(100, 130, 100));

        if (open_cur.x != -1) {
            bool open = node->start() <= open_cur && open_cur < node->end();
            im::SetNextItemOpen(open, ImGuiCond_Always);
        }

        ccstr field_type_str = NULL;
        if (lang == LANG_GO)
            field_type_str = ts_field_type_str(field_type);

        if (!field_type_str)
            last_open = im::TreeNodeEx(
                node->id(),
                flags,
                "%s, start = %s, end = %s",
                type_str,
                node->start().str(),
                node->end().str()
            );
        else
            last_open = im::TreeNodeEx(
                node->id(),
                flags,
                "(%s) %s, start = %s, end = %s",
                field_type_str,
                type_str,
                node->start().str(),
                node->end().str()
            );

        if (node->anon())
            im::PopStyleColor();
        if (node->type() == TS_COMMENT)
            im::PopStyleColor();

        if (im::IsMouseDoubleClicked(0) && im::IsItemHovered(0)) {
            auto editor = get_current_editor();
            if (editor) {
                auto pos = node->start();
                pos.x = editor->buf->idx_byte_to_cp(pos.y, pos.x);
                editor->move_cursor(pos);
            }
        }

        return last_open ? WALK_CONTINUE : WALK_SKIP_CHILDREN;
    });

    pop(0);
}


Font *init_builtin_font(u8 *data, u32 len, int size, ccstr name, bool notify_error) {
    auto fd = new_object(Font_Data);
    fd->type = FONT_DATA_FIXED;
    fd->data = data;
    fd->len = len;

    auto ret = new_object(Font);
    return ret->init(name, size, fd, notify_error) ? ret : NULL;
}

Font *init_builtin_base_font() {
    return init_builtin_font(vera_mono_ttf, vera_mono_ttf_len, CODE_FONT_SIZE, "Bitstream Vera Sans Mono", true);
}

Font *init_builtin_base_ui_font() {
    return init_builtin_font(open_sans_ttf, open_sans_ttf_len, UI_FONT_SIZE, "Open Sans", true);
}

Font* UI::acquire_system_ui_font() {
    SCOPED_MEM(&world.ui_mem);
    Frame frame;

    auto data = load_system_ui_font();
    if (!data) return NULL;

    auto font = new_object(Font);
    if (!font->init("<system ui font>", UI_FONT_SIZE, data)) {
        frame.restore();
        // error("unable to acquire system font");
        return NULL;
    }

    return font;
}

bool UI::init() {
    ptr0(this);

    // init fonts
    {
        font_cache.init();
        glyph_cache.init();

        {
            ccstr candidates[] = { "SF Mono", "Menlo", "Monaco", "Consolas", "Liberation Mono" };
            For (&candidates) {
                base_font = acquire_font(it, false);
                if (base_font) break;
            }
        }

        if (!base_font) base_font = init_builtin_base_font();
        if (!base_font) {
            tell_user_error("Sorry, we were unable to initialize the base font.");
            cp_panic("unable to initialize base font");
        }

        {
            // base_ui_font = acquire_system_ui_font();
            ccstr candidates[] = { "Lucida Grande", "Segoe UI", "Helvetica Neue" };
            For (&candidates) {
                base_ui_font = acquire_font(it, false);
                if (base_ui_font) break;
            }
        }

        if (!base_ui_font) base_ui_font = init_builtin_base_ui_font();
        if (!base_ui_font) {
            tell_user_error("Sorry, we were unable to initialize the base UI font.");
            cp_panic("unable to initialize base ui font");
        }

        // list available font names
        all_font_names = new_list(ccstr);
        if (!list_all_fonts(all_font_names)) {
            tell_user_error("Sorry, we were unable to list available fonts.");
            cp_panic("unable to list available fonts");
        }
    }

    editor_sizes.init(LIST_FIXED, _countof(_editor_sizes), _editor_sizes);

    // make sure panes_area is nonzero, so that panes can be initialized
    panes_area.w = 1;
    panes_area.h = 1;
    return true;
}

void UI::flush_verts() {
    if (!verts.len) return;

    glBufferData(GL_ARRAY_BUFFER, sizeof(Vert) * verts.len, verts.items, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, verts.len);
    verts.len = 0;
}

void UI::start_clip(boxf b) {
    flush_verts();
    glEnable(GL_SCISSOR_TEST);

    auto scale = world.get_display_scale();

    boxf bs;
    memcpy(&bs, &b, sizeof(boxf));
    bs.x *= scale.x;
    bs.y *= scale.y;
    bs.w *= scale.x;
    bs.h *= scale.y;
    glScissor(bs.x, world.frame_size.y - (bs.y + bs.h), bs.w, bs.h);

    clipping = true;
    current_clip = b;
}

void UI::end_clip() {
    flush_verts();
    glDisable(GL_SCISSOR_TEST);
    clipping = false;
}

void UI::draw_quad(boxf box, boxf uv, vec4f color, Draw_Mode mode, Texture_Id texture, float round_r, int round_flags) {
    if (verts.len + 6 >= verts.cap)
        flush_verts();

    auto draw_triangle = [&](vec2f a, vec2f b, vec2f c, vec2f uva, vec2f uvb, vec2f uvc) {
        auto scale = world.get_display_scale();
        a.x *= scale.x;
        a.y *= scale.y;
        b.x *= scale.x;
        b.y *= scale.y;
        c.x *= scale.x;
        c.y *= scale.y;

        if (round_flags) {
            verts.append({ a.x, a.y, uva.x, uva.y, color, mode, texture, box.w, box.h, round_r, round_flags });
            verts.append({ b.x, b.y, uvb.x, uvb.y, color, mode, texture, box.w, box.h, round_r, round_flags });
            verts.append({ c.x, c.y, uvc.x, uvc.y, color, mode, texture, box.w, box.h, round_r, round_flags });
        } else {
            verts.append({ a.x, a.y, uva.x, uva.y, color, mode, texture });
            verts.append({ b.x, b.y, uvb.x, uvb.y, color, mode, texture });
            verts.append({ c.x, c.y, uvc.x, uvc.y, color, mode, texture });
        }
    };

    draw_triangle(
        {box.x, box.y + box.h},
        {box.x, box.y},
        {box.x + box.w, box.y},
        {uv.x, uv.y + uv.h},
        {uv.x, uv.y},
        {uv.x + uv.w, uv.y}
    );

    draw_triangle(
        {box.x, box.y + box.h},
        {box.x + box.w, box.y},
        {box.x + box.w, box.y + box.h},
        {uv.x, uv.y + uv.h},
        {uv.x + uv.w, uv.y},
        {uv.x + uv.w, uv.y + uv.h}
    );

    auto &wnd = world.wnd_poor_mans_gpu_debugger;
    do {
        if (!wnd.tracking) break;

        if (!box.contains(world.ui.mouse_pos)) break;

        auto existing = wnd.logs->find([&](auto it) -> bool {
            if (it->b != box) return false;
            if (it->color != color) return false;
            if (it->mode != mode) return false;
            if (it->texture != texture) return false;

            return true;
        });

        // if it already exists, get out
        if (existing) break;

        auto output = generate_stack_trace();
        if (!output) break;

        Drawn_Quad item; ptr0(&item);
        item.b = box;
        item.color = color;
        item.mode = mode;
        item.texture = texture;
        {
            SCOPED_MEM(&wnd.mem);
            item.backtrace = cp_strdup(output);
        }
        wnd.logs->append(&item);
    } while (0);
}

void UI::draw_rect(boxf b, vec4f color) {
    draw_quad(b, { 0, 0, 1, 1 }, color, DRAW_SOLID);
}

void UI::draw_rounded_rect(boxf b, vec4f color, float radius, int round_flags) {
    draw_quad(b, { 0, 0, 1, 1 }, color, DRAW_SOLID_ROUNDED, TEXTURE_FONT, radius, round_flags);
}

void UI::draw_bordered_rect_outer(boxf b, vec4f color, vec4f border_color, int border_width, float radius) {
    auto b2 = b;
    b2.x -= border_width;
    b2.y -= border_width;
    b2.h += border_width * 2;
    b2.w += border_width * 2;

    if (!radius) {
        draw_rect(b2, border_color);
        draw_rect(b, color);
    } else {
        draw_rounded_rect(b2, border_color, radius, ROUND_ALL);
        draw_rounded_rect(b, color, radius, ROUND_ALL);
    }
}

Glyph *UI::lookup_glyph_for_grapheme(Grapheme gr) {
    auto utf8chars = ustr_to_cstr(gr);
    auto utf8str = utf8chars->items;

    auto glyph = glyph_cache.get(utf8str);
    if (glyph) return glyph;

    SCOPED_MEM(&world.ui_mem);

    auto font = find_font_for_grapheme(gr);
    if (!font) return NULL; // TODO: handle error

    auto buf = hb_buffer_create();
    if (!buf) return NULL;
    defer { hb_buffer_destroy(buf); };

    hb_buffer_add_utf8(buf, utf8chars->items, utf8chars->len-1, 0, utf8chars->len-1);
    hb_buffer_guess_segment_properties(buf);
    hb_shape(font->hbfont, buf, NULL, 0);

    unsigned int glyph_count;
    auto hb_glyph_info = hb_buffer_get_glyph_infos(buf, &glyph_count);
    auto hb_glyph_pos = hb_buffer_get_glyph_positions(buf, &glyph_count);

    int cur_x = 0;
    int cur_y = 0;
    int bx0 = -1, by0 = -1, bx1 = -1, by1 = -1;

    auto glyph_bitmaps = new_list(u8*);
    auto glyph_positions = new_list(box);

    if (!glyph_count) return NULL; // but then won't it keep calling this every single frame?

    float oversample_x = 3.0f;
    float oversample_y = 2.0f;

    for (u32 i = 0; i < glyph_count; i++) {
        auto glyph_index = hb_glyph_info[i].codepoint;

        auto scale_x = stbtt_ScaleForPixelHeight(&font->stbfont, (float)font->height) * oversample_x;
        auto scale_y = stbtt_ScaleForPixelHeight(&font->stbfont, (float)font->height) * oversample_y;

        int x0 = 0, y0 = 0, x1 = 0 , y1 = 0;
        stbtt_GetGlyphBitmapBoxSubpixel(&font->stbfont, glyph_index, scale_x, scale_y, 0.0f, 0.0f, &x0, &y0, &x1, &y1);

        auto &pos = hb_glyph_pos[i];
        x0 += (cur_x + pos.x_offset) * oversample_x;
        x1 += (cur_x + pos.x_offset) * oversample_x;
        y0 += (cur_x + pos.y_offset) * oversample_y;
        y1 += (cur_x + pos.y_offset) * oversample_y;

        if (i == 0 || x0 < bx0) bx0 = x0;
        if (i == 0 || y0 < by0) by0 = y0;
        if (i == 0 || x1 < bx1) bx1 = x1;
        if (i == 0 || y1 < by1) by1 = y1;

        int w = 0, h = 0;
        auto data = stbtt_GetGlyphBitmapSubpixel(&font->stbfont, scale_x, scale_y, 0.0f, 0.0f, hb_glyph_info[i].codepoint, &w, &h, NULL, NULL);
        if (!data) continue;
        if (!w || !h) continue;

        box b; ptr0(&b);
        b.x = x0;
        b.y = y0;
        b.w = x1-x0;
        b.h = y1-y0;

        glyph_bitmaps->append(data);
        glyph_positions->append(&b);

        cur_x += pos.x_advance;
        cur_y += pos.y_advance;
    }

    int leftmost = 0;
    int topmost = 0;

    if (glyph_positions->len) {
        leftmost = glyph_positions->at(0).x;
        topmost = glyph_positions->at(0).y;
        For (glyph_positions) {
            if (it.y < topmost) topmost = it.y;
            if (it.x < leftmost) leftmost = it.x;
        }
        For (glyph_positions) it.y -= topmost;
        For (glyph_positions) it.x -= leftmost;
    }

    boxf bbox; ptr0(&bbox);
    bbox.x = (float)bx0;
    bbox.y = (float)by0;
    bbox.w = (float)(bx1-bx0);
    bbox.h = (float)(by1-by0);

    // idk when these would ever happen, but check anyway
    if (bbox.w > ATLAS_SIZE || bbox.h > ATLAS_SIZE) return NULL;

    auto atlas = atlases_head;
    bool xover, yover;

    if (atlas) {
        // reached the end of the row? move back to start
        if (atlas->pos.x + bbox.w + 1 > ATLAS_SIZE) {
            atlas->pos.x = 0;
            atlas->pos.y += atlas->tallest + 1;
            atlas->tallest = 0;
        }

        xover = atlas->pos.x + bbox.w > ATLAS_SIZE;
        yover = atlas->pos.y + bbox.h > ATLAS_SIZE;
    }

    if (!atlas || xover || yover) {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        GLuint texture_id;
        glGenTextures(1, &texture_id);
        glBindTexture(GL_TEXTURE_2D, texture_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, ATLAS_SIZE, ATLAS_SIZE, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        auto a = new_object(Atlas);
        a->pos = new_cur2(0, 0);
        a->tallest = 0;
        a->gl_texture_id = texture_id;
        a->next = atlases_head;
        atlas = atlases_head = a;
    }

    // because of check above, glyph will always fit in the new atlas here

    glBindTexture(GL_TEXTURE_2D, atlas->gl_texture_id);

    Fori (glyph_positions) {
        auto ax = atlas->pos.x;
        auto ay = atlas->pos.y;
        auto bitmap_data = glyph_bitmaps->at(i);

        glTexSubImage2D(GL_TEXTURE_2D, 0, it.x+ax, it.y+ay, it.w, it.h, GL_RED, GL_UNSIGNED_BYTE, bitmap_data);
        stbtt_FreeBitmap(bitmap_data, NULL);
    }

    boxf uv;
    uv.x = (float)atlas->pos.x / (float)ATLAS_SIZE;
    uv.y = (float)atlas->pos.y / (float)ATLAS_SIZE;
    uv.w = (float)bbox.w / (float)ATLAS_SIZE;
    uv.h = (float)bbox.h / (float)ATLAS_SIZE;

    if (bbox.h > atlas->tallest) atlas->tallest = bbox.h;
    atlas->pos.x += bbox.w + 1;

    glyph = new_object(Glyph);
    glyph->single = gr->len == 1;
    if (glyph->single) {
        glyph->codepoint = gr->at(0);
    } else {
        auto copy = new_list(uchar);
        copy->concat(gr);
        glyph->grapheme = copy;
    }

    {
        boxf b;
        b.x = bbox.x / oversample_x;
        b.w = bbox.w / oversample_x;
        b.y = bbox.y / oversample_y;
        b.h = bbox.h / oversample_y;
        glyph->box = b;
    }
    glyph->atlas = atlas;
    glyph->uv = uv;

    {
        SCOPED_MEM(&world.ui_mem);
        glyph_cache.set(cp_strdup(utf8str), glyph);
    }
    return glyph;
}

// advances pos forward
int UI::draw_char(vec2f* pos, Grapheme grapheme, vec4f color) {
    if (!grapheme->len) return -1;

    glActiveTexture(GL_TEXTURE0 + TEXTURE_FONT);

    auto glyph = lookup_glyph_for_grapheme(grapheme);
    if (!glyph) return -1;

    if (current_texture_id != glyph->atlas->gl_texture_id) {
        flush_verts();
        glBindTexture(GL_TEXTURE_2D, glyph->atlas->gl_texture_id);
        current_texture_id = glyph->atlas->gl_texture_id;
    }

    boxf b = glyph->box;
    b.x += pos->x;
    b.y += pos->y;

    draw_quad(b, glyph->uv, color, DRAW_FONT_MASK);
    auto gw = cp_wcswidth(grapheme->items, grapheme->len);
    if (gw == -1) gw = 2;
    pos->x += base_font->width * gw;
    return gw;
}

int UI::draw_char(vec2f* pos, uchar codepoint, vec4f color) {
    auto grapheme = new_list(uchar);
    grapheme->append(codepoint);
    return draw_char(pos, grapheme, color);
}

/*
 * This is currently used in:
 *
 *  Tabs (i.e. filenames).
 *  Status bar pieces. This is all ascii, mostly predefined.
 *  Autocomplete.
 *
 * There's also parameter hints, which calls draw_char itself, but doesn't currently handle unicode (graphemes, etc.).
 * We'll want this function to take a unicode string.
 */
vec2f UI::draw_string(vec2f pos, ccstr s, vec4f color) {
    // TODO: handle graphemes
    pos.y += base_font->offset_y;

    auto codepoints = cstr_to_ustr(s);
    if (!codepoints->len) return pos;

    Grapheme_Clusterer gc; gc.init();
    int i = 0;
    auto grapheme = new_list(uchar);

    gc.feed(codepoints->at(0));
    while (i < codepoints->len) {
        grapheme->len = 0;
        do {
            grapheme->append(codepoints->at(i));
            i++;
        } while (i < codepoints->len && !gc.feed(codepoints->at(i)));
        draw_char(&pos, grapheme, color);
    }

    pos.y -= base_font->offset_y;
    return pos;
}

// currently only called to render tabs
float UI::get_text_width(ccstr s) {
    SCOPED_FRAME();

    auto codepoints = cstr_to_ustr(s);
    if (!codepoints->len) return 0;

    return cp_wcswidth(codepoints->items, codepoints->len);
}

boxf UI::get_status_area() {
    boxf b;
    b.w = world.display_size.x;
    b.h = base_font->height + settings.status_padding_y * 2;
    b.x = 0;
    b.y = world.display_size.y - b.h;
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
        if (im::IsMouseClicked(0)) ret |= MOUSE_CLICKED;
        if (im::IsMouseClicked(1)) ret |= MOUSE_RCLICKED;
        if (im::IsMouseClicked(2)) ret |= MOUSE_MCLICKED;

        if (im::IsMouseDoubleClicked(0)) ret |= MOUSE_DBLCLICKED;
        if (im::IsMouseDoubleClicked(1)) ret |= MOUSE_RDBLCLICKED;
        if (im::IsMouseDoubleClicked(2)) ret |= MOUSE_MDBLCLICKED;
    }
    return ret;
}

void UI::draw_debugger_var(Draw_Debugger_Var_Args *args) {
    SCOPED_FRAME();

    im::TableNextRow();
    im::TableNextColumn();

    bool open = false;
    auto pvar = args->var;
    auto var = *args->var;
    auto watch = args->watch;

    Dlv_Var** selection = NULL;
    if (watch)
        selection = &world.wnd_debugger.watch_selection;
    else
        selection = &world.wnd_debugger.locals_selection;

    {
        SCOPED_FRAME();

        int tree_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        bool leaf = true;

        if (var) {
            if (*selection == var) tree_flags |= ImGuiTreeNodeFlags_Selected;

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

        if (watch && !args->is_child) {
            if (watch->editing) {
                im::PushStyleColor(ImGuiCol_FrameBg, 0);
                im::SetNextItemWidth(-FLT_MIN);
                if (watch->edit_first_frame) {
                    watch->edit_first_frame = false;
                    im::SetKeyboardFocusHere();
                }
                bool changed = im::InputText(
                    cp_sprintf("##newwatch%x", (iptr)(void*)watch),
                    watch->expr_tmp,
                    _countof(watch->expr_tmp),
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll
                );
                im::PopStyleColor();

                if (changed || im::IsItemDeactivated()) {
                    if (watch->expr_tmp[0] != '\0') {
                        world.dbg.push_call(DLVC_EDIT_WATCH, [&](auto it) {
                            it->edit_watch.expression = cp_strdup(watch->expr_tmp);
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
                    im::Indent();

                // if (leaf) im::Unindent(im::GetTreeNodeToLabelSpacing());

                final_var_name = watch->expr;
                open = im::TreeNodeEx(watch, tree_flags, "%s", watch->expr) && !leaf;

                // if (leaf) im::Indent(im::GetTreeNodeToLabelSpacing());

                for (int i = 0; i < args->indent; i++)
                    im::Unindent();
            }
        } else {
            ccstr var_name = NULL;
            switch (args->index_type) {
            case INDEX_NONE:
                var_name = var->name;
                if (var->is_shadowed)
                    var_name = cp_sprintf("(%s)", var_name);
                break;
            case INDEX_ARRAY:
                var_name = cp_sprintf("[%d]", args->index);
                break;
            case INDEX_MAP:
                var_name = cp_sprintf("[%s]", var_value_as_string(args->key));
                break;
            }
            final_var_name = var_name;

            for (int i = 0; i < args->indent; i++)
                im::Indent();

            // if (leaf) im::Unindent(im::GetTreeNodeToLabelSpacing());

            open = im::TreeNodeEx(var, tree_flags, "%s", var_name) && !leaf;

            // if (leaf) im::Indent(im::GetTreeNodeToLabelSpacing());

            for (int i = 0; i < args->indent; i++)
                im::Unindent();
        }

        if (final_var_name) {
            if (im_begin_popup_context_item(cp_sprintf("dbg_copyvalue_%lld", (uptr)var))) {
                if (im::Selectable("Copy Name")) {
                    world.window->set_clipboard_string(final_var_name);
                }
                im::EndPopup();
            }
        }

        // Assumes the last thing drawn was the TreeNode.

        if (*selection == var) {
            if (watch) {
                auto delete_that_fucker = [&]() -> bool {
                    if (args->some_watch_being_edited) return false;
                    if (dbg_editing_new_watch) return false;

                    if (im_get_keymods() == CP_MOD_NONE) {
                        if (im_key_pressed(CP_KEY_BACKSPACE))
                            return true;
                        if (im_key_pressed(CP_KEY_DELETE))
                            return true;
                    }

                    return false;
                };

                if (delete_that_fucker()) {
                    auto old_len = world.dbg.watches.len;

                    world.dbg.push_call(DLVC_DELETE_WATCH, [&](auto it) {
                        it->delete_watch.watch_idx = args->watch_index;
                    });

                    if (watch) {
                        auto next = args->watch_index;
                        if (next >= old_len-1)
                            next--;
                        if (next < old_len-1)
                            *selection = world.dbg.watches[next].value;
                    } else {
                        do {
                            auto &state = world.dbg.mt_state;
                            auto goroutine = state.goroutines->find([&](auto it) { return it->id == state.current_goroutine_id; });
                            if (!goroutine)
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
                                *selection = frame->locals->at(next);
                            } else {
                                next -= frame->locals->len;
                                *selection = frame->args->at(next);
                            }
                        } while (0);
                    }
                }
            }

            if (im_get_keymods() == CP_MOD_PRIMARY && im_key_pressed(CP_KEY_C)) {
                // ???
            }
        }

        if (watch) {
            if (!args->is_child) {
                if (im::IsMouseDoubleClicked(0) && im::IsItemHovered(0)) {
                    watch->editing = true;
                    watch->open_before_editing = open;
                    watch->edit_first_frame = true;
                }
            }
        }

        if (!watch || !watch->editing) {
            if (im::IsItemClicked()) {
                *selection = var;
            }
        }
    }

    if (var && var->incomplete()) {
        for (int i = 0; i < args->indent; i++)
            im::Indent();
        im::Indent(im::GetTreeNodeToLabelSpacing());

        im_push_ui_font();
        bool clicked = im::SmallButton("Load more...");
        im_pop_font();

        if (clicked) {
            world.dbg.push_call(DLVC_VAR_LOAD_MORE, [&](Dlv_Call *it) {
                it->var_load_more.state_id = world.dbg.state_id;
                it->var_load_more.var = pvar;
                it->var_load_more.is_watch = (bool)watch;
            });
        }

        for (int i = 0; i < args->indent; i++)
            im::Unindent();
        im::Unindent(im::GetTreeNodeToLabelSpacing());
    }

    if (!watch || watch->fresh) {
        im::TableNextColumn();

        ccstr value_label = NULL;
        ccstr underlying_value = NULL;

        auto muted = (watch && watch->state == DBGWATCH_ERROR);
        if (muted) {
            value_label ="<unable to read>";
            underlying_value = value_label;
        } else {
            value_label = var_value_as_string(var);
            if (var->kind == GO_KIND_STRING) {
                auto len = strlen(value_label) - 2;
                auto buf = new_array(char, len+1);
                strncpy(buf, value_label+1, len);
                buf[len] = '\0';
                underlying_value = buf;
            } else {
                underlying_value = value_label;
            }
        }

        ImGuiStyle &style = im::GetStyle();

        if (muted) im::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
        im::TextWrapped("%s", value_label);
        if (muted) im::PopStyleColor();

        bool copy = false;

        if (*selection == var)
            if (im_get_keymods() == CP_MOD_PRIMARY)
                if (im_key_pressed(CP_KEY_C))
                    copy = true;

        if (im_begin_popup_context_item(cp_sprintf("dbg_copyvalue_%lld", (uptr)var))) {
            if (im::Selectable("Copy Value"))
                copy = true;
            im::EndPopup();
        }

        if (copy) world.window->set_clipboard_string(underlying_value);

        im::TableNextColumn();
        if (!watch || watch->state != DBGWATCH_ERROR) {
            ccstr type_name = NULL;
            if (!var->type || var->type[0] == '\0') {
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

            if (type_name) {
                im::TextWrapped("%s", type_name);
                if (im_begin_popup_context_item(cp_sprintf("dbg_copyvalue_%lld", (uptr)var))) {
                    if (im::Selectable("Copy Type")) {
                        world.window->set_clipboard_string(type_name);
                    }
                    im::EndPopup();
                }
            }
        }
    } else {
        im::TableNextColumn();
        if (watch && !watch->fresh) {
            // TODO: grey out
            // im::TextWrapped("Reading...");
        }
        im::TableNextColumn();
    }

    if (open && (!watch || (watch->fresh && watch->state != DBGWATCH_ERROR))) {
        if (var->children) {
            if (var->kind == GO_KIND_MAP) {
                for (int k = 0; k < var->children->len; k += 2) {
                    Draw_Debugger_Var_Args a;
                    a.watch = watch;
                    a.some_watch_being_edited = args->some_watch_being_edited;
                    a.watch_index = args->watch_index;
                    a.is_child = true;
                    a.var = &var->children->items[k+1];
                    a.index_type = INDEX_MAP;
                    a.key = var->children->at(k);
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
                    a.var = &var->children->items[k];
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
    if (var->unreadable_description)
        return cp_sprintf("<unreadable: %s>", var->unreadable_description);

    switch (var->kind) {
    case GO_KIND_INVALID: // i don't think this should even happen
        return "<invalid>";

    case GO_KIND_ARRAY:
    case GO_KIND_SLICE:
        return cp_sprintf("0x%" PRIx64 " (Len = %d, Cap = %d)", var->address, var->len, var->cap);

    case GO_KIND_STRUCT:
    case GO_KIND_INTERFACE:
        return cp_sprintf("0x%" PRIx64, var->address);

    case GO_KIND_MAP:
        return cp_sprintf("0x%" PRIx64 " (Len = %d)", var->address, var->len);

    case GO_KIND_STRING:
        return cp_sprintf("\"%s%s\"", var->value, var->incomplete() ? "..." : "");

    case GO_KIND_UNSAFEPOINTER:
    case GO_KIND_CHAN:
    case GO_KIND_FUNC:
    case GO_KIND_PTR:
        return cp_sprintf("0x%" PRIx64, var->address);

    default:
        return var->value;
    }
}

bool UI::im_is_window_focused() {
    return im::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
}

void UI::draw_debugger() {
    world.wnd_debugger.focused = im_is_window_focused();

    auto &dbg = world.dbg;
    auto &state = dbg.mt_state;
    auto &wnd = world.wnd_debugger;

    auto can_show_stuff = (state.state_flag != DLV_STATE_INACTIVE && !dbg.exiting);

    do {
        im::SetNextWindowDockID(dock_bottom_id, ImGuiCond_Once);
        im::Begin("Call Stack", NULL, ImGuiWindowFlags_NoFocusOnAppearing);
        defer { im::End(); };

        if (check_cmd_flag(&world.wnd_call_stack.cmd_focus))
            im::SetWindowFocus();

        if (!can_show_stuff) break;

        im_push_mono_font();
        defer { im_pop_font(); };

        for (int i = 0; i < state.goroutines->len; i++) {
            auto &goroutine = state.goroutines->at(i);

            int tree_flags = ImGuiTreeNodeFlags_OpenOnArrow;

            bool is_current = (state.current_goroutine_id == goroutine.id);
            if (is_current) {
                tree_flags |= ImGuiTreeNodeFlags_Bullet;
                im::SetNextItemOpen(true);
                im::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(255, 100, 100));
            }

            auto open = im::TreeNodeEx(
                (void*)(uptr)goroutine.id, tree_flags,
                "%s (%s)", goroutine.curr_func_name, goroutine.breakpoint_hit ? "BREAKPOINT HIT" : "PAUSED"
            );

            if (is_current) im::PopStyleColor();

            if (im::IsItemClicked() && state.state_flag == DLV_STATE_PAUSED) {
                dbg.push_call(DLVC_SET_CURRENT_FRAME, [&](auto call) {
                    call->set_current_frame.goroutine_id = goroutine.id;
                    call->set_current_frame.frame = 0;
                });
            }

            if (!open) continue;

            if (goroutine.fresh) {
                for (int j = 0; j < goroutine.frames->len; j++) {
                    auto &frame = goroutine.frames->items[j];

                    int tree_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    if (state.current_goroutine_id == goroutine.id && state.current_frame == j)
                        tree_flags |= ImGuiTreeNodeFlags_Selected;

                    im::TreeNodeEx(&frame, tree_flags, "%s (%s:%d)", frame.func_name, cp_basename(frame.filepath), frame.lineno);
                    if (im::IsItemClicked()) {
                        dbg.push_call(DLVC_SET_CURRENT_FRAME, [&](auto call) {
                            call->set_current_frame.goroutine_id = goroutine.id;
                            call->set_current_frame.frame = j;
                        });
                    }
                }
            } else {
                im::Text("Loading...");
            }

            im::TreePop();
        }
    } while (0);

    do {
        im::SetNextWindowDockID(dock_bottom_right_id, ImGuiCond_Once);

        {
            im::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            im::Begin("Local Variables", NULL, ImGuiWindowFlags_NoFocusOnAppearing);
            im::PopStyleVar();
        }
        defer { im::End(); };

        if (check_cmd_flag(&world.wnd_local_variables.cmd_focus))
            im::SetWindowFocus();

        if (!can_show_stuff) break;

        im_push_mono_font();
        defer { im_pop_font(); };

        auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable;
        if (im::BeginTable("vars", 3, flags)) {
            im::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
            im::TableSetupColumn("Value", ImGuiTableColumnFlags_NoHide);
            im::TableSetupColumn("Type", ImGuiTableColumnFlags_NoHide);
            im::TableHeadersRow();

            bool loading = false;
            bool done = false;
            Dlv_Frame *frame = NULL;

            do {
                if (state.current_goroutine_id == -1) break;
                if (state.current_frame == -1) break;

                auto goroutine = state.goroutines->find([&](auto it) { return it->id == state.current_goroutine_id; });
                if (!goroutine) break;

                loading = true;

                if (!goroutine->fresh) break;
                if (state.current_frame >= goroutine->frames->len) break;

                frame = &goroutine->frames->items[state.current_frame];
                if (!frame->fresh) {
                    frame = NULL;
                    break;
                }

                int index = 0;

                if (frame->locals) {
                    Fori (frame->locals) {
                        Draw_Debugger_Var_Args a; ptr0(&a);
                        a.var = &frame->locals->items[i];
                        a.is_child = false;
                        a.watch = NULL;
                        a.index_type = INDEX_NONE;
                        a.locals_index = index++;
                        draw_debugger_var(&a);
                    }
                }

                if (frame->args) {
                    Fori (frame->args) {
                        Draw_Debugger_Var_Args a; ptr0(&a);
                        a.var = &frame->args->items[i];
                        a.is_child = false;
                        a.watch = NULL;
                        a.index_type = INDEX_NONE;
                        a.locals_index = index++;
                        draw_debugger_var(&a);
                    }
                }
            } while (0);

            im::EndTable();

            if (frame) {
                if (isempty(frame->locals) && isempty(frame->args))
                    im::Text("No variables to show.");
            } else if (loading) {
                im::Text("Loading...");
            }
        }
    } while (0);

    {
        im::SetNextWindowDockID(dock_bottom_right_id, ImGuiCond_Once);

        {
            im::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            im::Begin("Watches", NULL, ImGuiWindowFlags_NoFocusOnAppearing);
            im::PopStyleVar();
        }

        if (check_cmd_flag(&world.wnd_watches.cmd_focus))
            im::SetWindowFocus();

        if (can_show_stuff) {
            im_push_mono_font();
            defer { im_pop_font(); };

            auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable;
            if (im::BeginTable("vars", 3, flags)) {
                im::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
                im::TableSetupColumn("Value", ImGuiTableColumnFlags_NoHide);
                im::TableSetupColumn("Type", ImGuiTableColumnFlags_NoHide);
                im::TableHeadersRow();

                bool some_watch_being_edited = false;
                For (&dbg.watches) {
                    if (it.editing) {
                        some_watch_being_edited = true;
                        break;
                    }
                }

                for (int k = 0; k < dbg.watches.len; k++) {
                    auto &it = dbg.watches[k];
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

                    im::TableNextRow();
                    im::TableNextColumn(); // name

                    im::PushStyleColor(ImGuiCol_FrameBg, 0);
                    im::SetNextItemWidth(-FLT_MIN);
                    bool changed = im::InputText(
                        "##newwatch",
                        world.wnd_debugger.new_watch_buf,
                        _countof(world.wnd_debugger.new_watch_buf),
                        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll
                    );
                    im::PopStyleColor();

                    if (changed || im::IsItemDeactivated())
                        if (world.wnd_debugger.new_watch_buf[0] != '\0') {
                            dbg.push_call(DLVC_CREATE_WATCH, [&](auto it) {
                                it->create_watch.expression = cp_strdup(world.wnd_debugger.new_watch_buf);
                            });
                            world.wnd_debugger.new_watch_buf[0] = '\0';
                        }

                    dbg_editing_new_watch = im::IsItemFocused();

                    im::TableNextColumn(); // value
                    im::TableNextColumn(); // type
                }

                im::EndTable();
            }
        }

        im::End();
    }

    /*
    {
        im::SetNextWindowDockID(dock_bottom_right_id, ImGuiCond_Once);
        im::Begin("Global Variables", NULL, ImGuiWindowFlags_NoFocusOnAppearing);
        im_push_mono_font();
        im::Text("@Incomplete: global vars go here");
        im_pop_font();
        im::End();
    }
    */
}

void open_rename(FT_Node *target) {
    auto &wnd = world.wnd_rename_file_or_folder;

    wnd.show = true;
    wnd.target = target;

    cp_strcpy_fixed(wnd.location, ft_node_to_path(target));
    cp_strcpy_fixed(wnd.name, target->name);
}

void UI::im_small_newline() {
    im::Dummy(ImVec2(0.0f, im::GetFrameHeightWithSpacing() * 1/4));
}

bool UI::im_input_text_ex(ccstr label, ccstr inputid, char *buf, int count, int flags) {
    im::PushItemWidth(-1);
    defer { im::PopItemWidth(); };

    if (label) {
        im_push_ui_font();
        im::Text("%s", label);
        im_pop_font();
    }

    return im::InputText(cp_sprintf("###%s", inputid), buf, count, flags);
}

bool UI::im_input_text(ccstr label, char *buf, int count, int flags) {
    return im_input_text_ex(label, label, buf, count, flags);
}

void UI::open_project_settings() {
    auto &wnd = world.wnd_project_settings;
    if (wnd.show) return;

    wnd.focus_general_settings = true;

    {
        wnd.pool.cleanup();
        wnd.pool.init("project_settings");
        SCOPED_MEM(&wnd.pool);
        wnd.tmp = project_settings.copy();
    }

    wnd.show = true;
}

void UI::im_with_disabled(bool disable, fn<void()> f) {
    if (disable) {
        im::PushItemFlag(ImGuiItemFlags_Disabled, true);
        im::PushStyleVar(ImGuiStyleVar_Alpha, im::GetStyle().Alpha * 0.5f);
    }

    f();

    if (disable) {
        im::PopItemFlag();
        im::PopStyleVar();
    }
}

bool UI::im_key_pressed(Key key) {
    if (im_is_window_focused())
        if (im::IsKeyPressed(cp_key_to_imgui_key(key)))
            return true;
    return false;
}

u32 UI::im_get_keymods() {
    auto &io = im::GetIO();

    u32 ret = 0;
    if (io.KeySuper) ret |= CP_MOD_CMD;
    if (io.KeyCtrl) ret |= CP_MOD_CTRL;
    if (io.KeyShift) ret |= CP_MOD_SHIFT;
    if (io.KeyAlt) ret |= CP_MOD_ALT;
    return ret;
}

void UI::im_push_mono_font() {
    im::PushFont(world.ui.im_font_mono);
}

void UI::im_push_ui_font() {
    im::PushFont(world.ui.im_font_ui);
}

void UI::im_pop_font() {
    im::PopFont();
}

void open_ft_node(FT_Node *it) {
    SCOPED_FRAME();
    auto rel_path = ft_node_to_path(it);
    auto full_path = path_join(world.current_path, rel_path);
    if (focus_editor(full_path))
        im::SetWindowFocus(NULL);
}

struct Coarse_Clipper {
    ImVec2 pos;
    ImRect unclipped_rect;
    float last_line_height;
    bool before;

    void init() {
        before = true;
        unclipped_rect = get_window_clip_area();
        pos = im::GetCursorScreenPos();
    }

    ImRect get_window_clip_area() {
        auto g = GImGui;
        auto window = g->CurrentWindow;
        auto ret = window->ClipRect;

        // idk/idc what this does, it's ripped from imgui.cpp and just works
        if (g->NavMoveSubmitted)
            ret.Add(g->NavScoringRect);
        if (g->NavJustMovedToId && window->NavLastIds[0] == g->NavJustMovedToId)
            ret.Add(ImRect(window->Pos + window->NavRectRel[0].Min, window->Pos + window->NavRectRel[0].Max));

        return ret;
    }

    bool add(float h) {
        if (before) {
            if (pos.y + h >= unclipped_rect.Min.y) {
                im::SetCursorScreenPos(pos);
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
        auto g = im::GetCurrentContext();
        ImGuiWindow* window = g->CurrentWindow;
        window->DC.CursorPos.y = pos.y;

        window->DC.CursorPos.y = pos.y;
        window->DC.CursorMaxPos.y = ImMax(window->DC.CursorMaxPos.y, pos.y);
        window->DC.CursorPosPrevLine.y = window->DC.CursorPos.y - last_line_height;
        window->DC.PrevLineSize.y = last_line_height;
    }
};

void UI::focus_keyboard_here(Wnd *wnd, int cond) {
    if (wnd->appearing) {
        if (cond & FKC_APPEARING)
            im::SetKeyboardFocusHere();
    } else if (!wnd->first_open_focus_twice_done) {
        wnd->first_open_focus_twice_done = true;
        if (cond & FKC_APPEARING) {
            im::SetKeyboardFocusHere();
        }
    } else if (wnd->focusing) {
        if (cond & FKC_FOCUSING)
            im::SetKeyboardFocusHere();
    }
}

void UI::handle_popup_window_logic(Wnd *wnd, Wnd *parent) {
    // close the window when we unfocus
    if (!wnd->focused) wnd->show = false;

    // if we just closed the window, refocus the parent
    if (!wnd->show) {
        parent->cmd_focus = true;

        // need to set this to false - it gets set when we press escape
        world.cmd_unfocus_all_windows = false;
    }
}

void UI::draw_everything() {
    verts.len = 0;

    hover.id_last_frame = hover.id;
    hover.id = 0;
    hover.cursor = ImGuiMouseCursor_Arrow;

    // start rendering imgui
    im::NewFrame();

    bool old_mouse_captured_by_imgui = world.ui.mouse_captured_by_imgui;
    bool old_keyboard_captured_by_imgui = world.ui.keyboard_captured_by_imgui;
    bool old_input_captured_by_imgui = world.ui.input_captured_by_imgui;

    ImGuiIO& io = im::GetIO();
    world.ui.mouse_captured_by_imgui = io.WantCaptureMouse;
    world.ui.keyboard_captured_by_imgui = io.WantCaptureKeyboard;
    world.ui.input_captured_by_imgui = io.WantTextInput;

    // prevent ctrl+tab from doing shit
    im::GetCurrentContext()->NavWindowingTarget = NULL;

    // draw the main dockspace
    {
        const ImGuiViewport* viewport = im::GetMainViewport();

        auto dock_size = viewport->WorkSize;
        dock_size.y -= get_status_area().h;

        im::SetNextWindowPos(viewport->WorkPos);
        im::SetNextWindowSize(dock_size);
        im::SetNextWindowViewport(viewport->ID);
        im::SetNextWindowBgAlpha(0.0f);

        fstlog("draw dockspace - set shit");

        auto window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar
            | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        {
            im::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2, 2));
            im::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            im::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            /**/
            im::Begin("main_dockspace", NULL, window_flags);
            /**/
            im::PopStyleVar(3);
        }

        fstlog("draw dockspace - begin window");

        /*
        // if the dockspace is focused, means we just closed last docked window
        // but keyboard capture still going to imgui, so we need to SetWindowFocus(NULL)
        if (im::IsWindowFocused(ImGuiFocusedFlags_RootWindow))
            im::SetWindowFocus(NULL);
        */

        ImGuiID dockspace_id = im::GetID("main_dockspace");

        // set up dock layout
        if (!dock_initialized) {
            im::DockBuilderRemoveNode(dockspace_id);
            im::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            im::DockBuilderSetNodeSize(dockspace_id, dock_size);

            dock_main_id = dockspace_id;
            dock_sidebar_id = im::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.33f, NULL, &dock_main_id);
            dock_bottom_id = im::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.20f, NULL, &dock_main_id);
            dock_bottom_right_id = im::DockBuilderSplitNode(dock_bottom_id, ImGuiDir_Right, 0.66f, NULL, &dock_bottom_id);

            /*
            im::DockBuilderDockWindow("Call Stack", dock_bottom_id);
            im::DockBuilderDockWindow("Build Results", dock_bottom_id);

            im::DockBuilderDockWindow("Watches", dock_bottom_right_id);
            im::DockBuilderDockWindow("Local Variables", dock_bottom_right_id);
            im::DockBuilderDockWindow("Global Variables", dock_bottom_right_id);

            im::DockBuilderDockWindow("File Explorer", dock_sidebar_id);
            im::DockBuilderDockWindow("Search Results", dock_sidebar_id);
            */

            im::DockBuilderFinish(dockspace_id);
            dock_initialized = true;
        }

        fstlog("draw dockspace - setup layout");

        auto dock_flags = ImGuiDockNodeFlags_NoDockingInCentralNode | ImGuiDockNodeFlags_PassthruCentralNode;
        im::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dock_flags);

        fstlog("draw dockspace - im::DockSpace()");

        {
            // get panes_area
            auto node = im::DockBuilderGetCentralNode(dockspace_id);
            if (node) {
                panes_area.x = node->Pos.x;
                panes_area.y = node->Pos.y;
                panes_area.w = node->Size.x;
                panes_area.h = node->Size.y;
            } else {
                panes_area.w = 1;
                panes_area.h = 1;
            }
        }

        fstlog("draw dockspace - get panes_area");

        im::End();
        fstlog("draw dockspace");
    }

    // now that we have the panes area, recalculate view sizes
    recalculate_view_sizes();

    bool is_running = world.dbg.mt_state.state_flag != DLV_STATE_INACTIVE;

    if (is_running) {
        im::PushStyleColor(ImGuiCol_MenuBarBg, to_imcolor(rgba("#30571C")));
    }

    if (im::BeginMainMenuBar()) {
        im::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
        im::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(7, 5));

        if (im_begin_menu("File")) {
            menu_command(CMD_NEW_FILE);
            menu_command(CMD_OPEN_FILE_MANUALLY);
            menu_command(CMD_OPEN_FOLDER);
            menu_command(CMD_SAVE_FILE);
            menu_command(CMD_SAVE_ALL);
            im::Separator();
            menu_command(CMD_CLOSE_EDITOR);
            menu_command(CMD_CLOSE_ALL_EDITORS);
            menu_command(CMD_OPEN_LAST_CLOSED_EDITOR);
            im::Separator();
            menu_command(CMD_EXIT);
            im::EndMenu();
        }

        if (im_begin_menu("Edit")) {
            menu_command(CMD_UNDO);
            menu_command(CMD_REDO);
            im::Separator();
            menu_command(CMD_CUT);
            menu_command(CMD_COPY);
            menu_command(CMD_PASTE);
            menu_command(CMD_SELECT_ALL);
            im::Separator();
            menu_command(CMD_SEARCH);
            menu_command(CMD_SEARCH_AND_REPLACE);
            im::EndMenu();
        }

        if (im_begin_menu("View")) {
            menu_command(CMD_FILE_EXPLORER, world.file_explorer.show);
            menu_command(CMD_ERROR_LIST, world.error_list.show);
            menu_command(CMD_COMMAND_PALETTE);
            im::Separator();
            menu_command(CMD_ZOOM_ORIGINAL);
            menu_command(CMD_ZOOM_IN);
            menu_command(CMD_ZOOM_OUT);
            if (im_begin_menu("Zoom")) {
                For (&ZOOM_LEVELS)
                    if (im::MenuItem(cp_sprintf("%d%%", it), NULL, it == options.zoom_level))
                        set_zoom_level(it);
                im::EndMenu();
            }
            im::EndMenu();
        }

        if (im_begin_menu("Navigate")) {
            menu_command(CMD_GO_BACK);
            menu_command(CMD_GO_FORWARD);
            im::Separator();
            menu_command(CMD_GO_TO_FILE);
            menu_command(CMD_GO_TO_SYMBOL);
            menu_command(CMD_GO_TO_NEXT_ERROR);
            menu_command(CMD_GO_TO_PREVIOUS_ERROR);
            menu_command(CMD_GO_TO_DEFINITION);
            im::Separator();
            menu_command(CMD_FIND_REFERENCES);
            menu_command(CMD_FIND_IMPLEMENTATIONS);
            menu_command(CMD_FIND_INTERFACES);
            im::Separator();
            menu_command(CMD_FIND_NEXT);
            menu_command(CMD_FIND_PREVIOUS);
            menu_command(CMD_FIND_CLEAR);
            im::Separator();
            menu_command(CMD_GO_TO_NEXT_SEARCH_RESULT);
            menu_command(CMD_GO_TO_PREVIOUS_SEARCH_RESULT);
            im::EndMenu();
        }

        if (im_begin_menu("Format")) {
            menu_command(CMD_FORMAT_FILE);
            menu_command(CMD_ORGANIZE_IMPORTS);
            // menu_command(CMD_FORMAT_SELECTION);
            im::EndMenu();
        }

        if (im_begin_menu("Refactor")) {
            menu_command(CMD_RENAME);
            menu_command(CMD_GENERATE_IMPLEMENTATION);
            im::EndMenu();
        }

        if (im_begin_menu("Project")) {
            menu_command(CMD_ADD_NEW_FILE);
            menu_command(CMD_ADD_NEW_FOLDER);
            im::Separator();
            menu_command(CMD_PROJECT_SETTINGS);
            im::EndMenu();
        }

        if (im_begin_menu("Build")) {
            menu_command(CMD_BUILD);

            im::Separator();

            if (im_begin_menu("Windows...")) {
                menu_command(CMD_BUILD_RESULTS, world.error_list.show);
                im::EndMenu();
            }

            im::Separator();

            // TODO: add these as commands
            if (im_begin_menu("Select Active Build Profile..."))  {
                if (project_settings.build_profiles->len > 0) {
                    Fori (project_settings.build_profiles) {
                        if (im::MenuItem(it.label, NULL, project_settings.active_build_profile == i, true)) {
                            project_settings.active_build_profile = i;
                            write_project_settings();
                        }
                    }
                } else {
                    im::MenuItem("No profiles to select.", NULL, false, false);
                }
                im::EndMenu();
            }

            menu_command(CMD_BUILD_PROFILES);

            im::EndMenu();
        }

        if (im_begin_menu("Debug")) {
            if (world.dbg.mt_state.state_flag == DLV_STATE_PAUSED)
                menu_command(CMD_CONTINUE);
            else
                menu_command(CMD_START_DEBUGGING);

            menu_command(CMD_DEBUG_TEST_UNDER_CURSOR);
            menu_command(CMD_BREAK_ALL);
            menu_command(CMD_STOP_DEBUGGING);
            menu_command(CMD_STEP_OVER);
            menu_command(CMD_STEP_INTO);
            menu_command(CMD_STEP_OUT);
            // menu_command(CMD_RUN_TO_CURSOR); world.dbg.mt_state.state_flag == DLV_STATE_PAUSED

            im::Separator();

            menu_command(CMD_TOGGLE_BREAKPOINT);
            menu_command(CMD_DELETE_ALL_BREAKPOINTS);

            im::Separator();

            if (im_begin_menu("Windows..."))  {
                menu_command(CMD_DEBUG_OUTPUT, world.wnd_debug_output.show);
                im::EndMenu();
            }

            im::Separator();

            if (im_begin_menu("Select Active Debug Profile..."))  {
                // TODO(robust): we should handle the builtins more explicitly, instead of using hardcoded value of 1
                if (project_settings.debug_profiles->len > 1) {
                    for (int i = 1; i < project_settings.debug_profiles->len; i++) {
                        auto &it = project_settings.debug_profiles->at(i);
                        if (im::MenuItem(it.label, NULL, project_settings.active_debug_profile == i, true)) {
                            project_settings.active_debug_profile = i;
                            write_project_settings();
                        }
                    }
                } else {
                    im::MenuItem("No profiles to select.", NULL, false, false);
                }
                im::EndMenu();
            }

            menu_command(CMD_DEBUG_PROFILES);

            im::EndMenu();
        }

        if (im_begin_menu("Tools")) {
            // should we allow this even when not ready, so it can be used as an escape hatch if the indexer gets stuck?

            menu_command(CMD_RESCAN_INDEX);
            menu_command(CMD_OBLITERATE_AND_RECREATE_INDEX);
            im::Separator();
            menu_command(CMD_OPTIONS);

#ifdef DEBUG_BUILD
// #if 1
            im::Separator();

            im::MenuItem("ImGui demo", NULL, &world.windows_open.im_demo);
            im::MenuItem("ImGui metrics", NULL, &world.windows_open.im_metrics);

            im::Separator();

            im::MenuItem("Tree viewer", NULL, &world.wnd_tree_viewer.show);
            im::MenuItem("Memory viewer", NULL, &world.wnd_mem_viewer.show);
            im::MenuItem("History viewer", NULL, &world.wnd_history.show);
            im::MenuItem("Style editor", NULL, &world.wnd_style_editor.show);
            im::MenuItem("Poor man's GPU debugger", NULL, &world.wnd_poor_mans_gpu_debugger.show);
            im::MenuItem("Mouse Info", NULL, &world.wnd_mouse_pos.show);
            im::MenuItem("Hover Info", NULL, &world.wnd_hover_info.show);

            im::Separator();

            im::MenuItem("Disable framerate cap", NULL, &world.turn_off_framerate_cap);
            im::MenuItem("Show frame index", NULL, &world.show_frame_index);
            im::MenuItem("Show frameskips", NULL, &world.show_frameskips);
            im::MenuItem("Time type_char()", NULL, &world.time_type_char);

            im::Separator();

            if (im::MenuItem("Flash cursor red")) {
                auto editor = get_current_editor();
                if (editor) editor->flash_cursor_error();
            }

            im::Separator();

            if (im::MenuItem("Crash")) {
                cp_panic("This is an intentionally caused crash");
            }

            if (im::MenuItem("Crash by using abort()")) {
                abort();
            }

            if (im::MenuItem("Crash on separate thread")) {
                auto thread = [](auto) {
                    Pool pool;
                    pool.init("crash_separate_thread");
                    SCOPED_MEM(&pool);
                    cp_panic("panicking from another thread");
                };

                auto t = create_thread(thread);
                close_thread_handle(t);
            }

            if (im::MenuItem("Crash on another thread w/ corrupted mem")) {
                auto thread = [](auto) {
                    Pool pool;
                    pool.init("crash_separate_thread_corrupted_mem");
                    SCOPED_MEM(&pool);
                    pool.curr = NULL; // corrupt the pool

                    cp_panic("panicking from another thread w/ corrupted mem");
                };

                auto t = create_thread(thread);
                close_thread_handle(t);
            }

            im::Separator();

            if (im::MenuItem("Restart CodePerfect")) {
                fork_self();
            }

            if (im::MenuItem("Delete .last_folder")) {
                delete_file(path_join(world.configdir, ".last_folder"));
            }

            im::Separator();

            if (im::MenuItem("Message box - Ok")) {
                tell_user("This is a message box.", "Message");
            }

            if (im::MenuItem("Message box - Yes/No")) {
                ask_user_yes_no("Would you like to suck a dick?", "Dick", "Suck dick", "Don't suck dick");
            }

            if (im::MenuItem("Message box - Yes/No/Cancel")) {
                ask_user_yes_no_cancel(
                    "Before you suck a dick, you need to do this other thing. Do you want to do that?",
                    "Dick",
                    "Do other thing",
                    "Don't do other thing"
                );
            }

            im::Separator();

            if (im::MenuItem("Process file")) {
                do {
                    auto editor = get_current_editor();
                    if (!editor) break;

                    Go_File file; ptr0(&file);

                    file.use_pool = true;
                    file.pool = new_object(Pool);
                    file.pool->init("file pool");
                    defer { file.pool->cleanup(); };

                    {
                        SCOPED_MEM(file.pool);
                        file.filename = cp_basename(editor->filepath);
                        file.scope_ops = new_list(Go_Scope_Op);
                        file.decls = new_list(Godecl);
                        file.imports = new_list(Go_Import);
                        file.references = new_list(Go_Reference);
                    }

                    if (!world.indexer.try_acquire_lock(IND_READING)) break;
                    defer { world.indexer.release_lock(IND_READING); };

                    Parser_It it; ptr0(&it);
                    it.init(editor->buf);

                    auto tree = editor->buf->tree;
                    if (!tree) break;

                    Ast_Node root; ptr0(&root);
                    root.init(ts_tree_root_node(tree), &it);

                    world.indexer.process_tree_into_gofile(&file, &root, file.filename, NULL, file.pool);
                } while (0);
            }

            if (im::MenuItem("Cleanup unused memory")) {
                world.indexer.message_queue.add([&](auto msg) {
                    msg->type = GOMSG_CLEANUP_UNUSED_MEMORY;
                });
            }

#endif
            im::EndMenu();
        }

        if (im_begin_menu("Help")) {
            im::MenuItem(cp_sprintf("CodePerfect %s", world.gh_version), NULL, false, false);

            im::Separator();

            menu_command(CMD_DOCUMENTATION);

            im::EndMenu();
        }

        im::PopStyleVar(2);
        fstlog("menubar");

        // draw debugger
        {
            ccstr dbgstate = NULL;
            switch (world.dbg.mt_state.state_flag) {
            case DLV_STATE_PAUSED: dbgstate = "PAUSED"; break;
            case DLV_STATE_STARTING: dbgstate = "STARTING"; break;
            case DLV_STATE_RUNNING: dbgstate = "RUNNING"; break;
            }

            if (dbgstate) {
                im_push_mono_font();
                im::SetCursorPosX(im::GetCursorPosX() + im::GetContentRegionAvail().x - im::CalcTextSize(dbgstate).x);
                im::Text("%s", dbgstate);
                im_pop_font();
            }
        }

        im::EndMainMenuBar();
    }

    if (is_running) {
        im::PopStyleColor();
    }

    if (world.wnd_mem_viewer.show) {
        auto &wnd = world.wnd_mem_viewer;

        begin_window("Memory Viewer", &world.wnd_mem_viewer, ImGuiWindowFlags_AlwaysAutoResize, false, true);

        if (im::BeginTabBar("wnd_mem_viewer", 0)) {
            defer { im::EndTabBar(); };

            if (im::BeginTabItem("Fridges", NULL)) {
                defer { im::EndTabItem(); };

                int flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_BordersH | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterV | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInner;

                auto render_fridge = [&](ccstr name, auto fridge) {
                    im::TableNextRow();

                    im::TableSetColumnIndex(0);
                    im::Text("%s", name);
                    im::TableSetColumnIndex(1);
                    im::Text("%d", fridge->blocks_given_out);
                };

                if (im::BeginTable("fridges", 2, flags)) {
                    im::TableSetupColumn("fridge");
                    im::TableSetupColumn("# blocks given out");
                    im::TableHeadersRow();

                    render_fridge("mark", &world.mark_fridge);
                    render_fridge("avl_node", &world.avl_node_fridge);
                    render_fridge("treap", &world.treap_fridge);
                    render_fridge("change", &world.change_fridge);
                    render_fridge("chunk0", &world.chunk0_fridge);
                    render_fridge("chunk1", &world.chunk1_fridge);
                    render_fridge("chunk2", &world.chunk2_fridge);
                    render_fridge("chunk3", &world.chunk3_fridge);
                    render_fridge("chunk4", &world.chunk4_fridge);
                    render_fridge("chunk5", &world.chunk5_fridge);
                    render_fridge("chunk6", &world.chunk6_fridge);
                    render_fridge("chunk7", &world.chunk7_fridge);

                    im::EndTable();
                }
            }
        }

        im::End();
    }

    if (world.wnd_options.show) {
        auto &wnd = world.wnd_options;
        auto &tmp = wnd.tmp;

        begin_window("Options", &wnd, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking, false, false);

        auto &outer_style = im::GetStyle();
        int outer_window_padding = outer_style.WindowPadding.y;

        if (im::BeginTabBar("wnd_options_tab_bar", 0)) {
            auto begin_tab = [&](ccstr name) -> bool {
                if (!im::BeginTabItem(name, NULL)) return false;

                im::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
                im::BeginChild("container", ImVec2(400, 250), false, ImGuiWindowFlags_AlwaysUseWindowPadding);
                im::PopStyleVar();
                return true;
            };

            auto end_tab = [&]() {
                im::EndChild();
                im::EndTabItem();
            };


            if (begin_tab("General")) {
                auto fps_limit_str = [&](Fps_Limit it) {
                    switch (it) {
                    case FPS_30: return "30";
                    case FPS_60: return "60";
                    case FPS_120: return "120";
                    }
                    return "60";
                };

                im::PushItemWidth(-1);
                im_push_ui_font();
                {
                    im::Text("FPS limit");
                    if (im::BeginCombo("###fps_limit", fps_limit_str((Fps_Limit)tmp.fps_limit_enum), 0)) {
                        Fps_Limit opts[] = { FPS_30, FPS_60, FPS_120 };
                        For (&opts) {
                            const bool selected = (tmp.fps_limit_enum == it);
                            if (im::Selectable(fps_limit_str(it), selected))
                                tmp.fps_limit_enum = it;
                            if (selected)
                                im::SetItemDefaultFocus();
                        }
                        im::EndCombo();
                    }

                    im_small_newline();
                    im::Checkbox("Open last folder on startup", &tmp.open_last_folder);
                }
                im_pop_font();
                im::PopItemWidth();

                end_tab();
            }

            if (begin_tab("Editor Settings")) {
                im::PushItemWidth(-1);
                im_push_ui_font();
                {
                    im::Checkbox("Enable vim keybindings", &tmp.enable_vim_mode);
                    if (im::IsItemEdited())
                        wnd.something_that_needs_restart_was_changed = true;

                    im_small_newline();

                    im::Indent();
                    {
                        im_with_disabled(!tmp.enable_vim_mode, [&]() {
                            im::Checkbox("Use clipboard for Vim", &tmp.vim_use_clipboard);
                        });
                    }
                    im::Unindent();

                    im_small_newline();

                    im::Text("Formatting");
                    im_small_newline();

                    im::Indent();
                    {
                        im::Checkbox("Format on save", &tmp.format_on_save);
                        im_small_newline();

                        im::Checkbox("Organize imports on save", &tmp.organize_imports_on_save);
                        im::SameLine();
                        help_marker("This adds missing imports and removes unused ones.");
                        im_small_newline();

                        im::Checkbox("Use gofumpt", &tmp.format_with_gofumpt);
                    }
                    im::Unindent();

                    /*
                    im_small_newline();
                    im::Text("Scroll offset");
                    im::SameLine();
                    help_marker("The number of lines the editor will keep between your cursor and the top/bottom of the screen.");
                    im::InputInt("###scroll_offset", &tmp.scrolloff);
                    */

                    im_small_newline();
                    im::Text("Tab size");
                    im::InputInt("###tab_size", &tmp.tabsize);
                }
                im_pop_font();
                im::PopItemWidth();

                end_tab();
            }

            if (begin_tab("Code Intelligence")) {
                im::PushItemWidth(-1);
                im_push_ui_font();
                {
                    im::Text("Struct tag casing");
                    if (im::BeginCombo("###struct_tag_casing", case_style_pretty_str(tmp.struct_tag_case_style))) {
                        Case_Style styles[] = {CASE_SNAKE, CASE_PASCAL, CASE_CAMEL};
                        For (&styles) {
                            im::PushID((void*)it);
                            if (im::Selectable(case_style_pretty_str(it), it == tmp.struct_tag_case_style))
                                tmp.struct_tag_case_style = it;
                            im::PopID();
                        }
                        im::EndCombo();
                    }

                    im_small_newline();
                    im::Checkbox("Add a `(` after autocompleting a func type", &tmp.autocomplete_func_add_paren);

                    // im_small_newline();
                }
                im_pop_font();
                im::PopItemWidth();

                end_tab();
            }

            if (begin_tab("Debugger")) {
                im::Checkbox("Hide system goroutines", &tmp.dbg_hide_system_goroutines);

                end_tab();
            }

            if (begin_tab("Privacy")) {
                im::Checkbox("Send crash reports", &tmp.send_crash_reports);
                end_tab();
            }

            im::EndTabBar();
        }

        im::Separator();

        {
            ImGuiStyle &style = im::GetStyle();

            float button1_w = im::CalcTextSize("Save").x + style.FramePadding.x * 2.f;
            float button2_w = im::CalcTextSize("Cancel").x + style.FramePadding.x * 2.f;
            float width_needed = button1_w + style.ItemSpacing.x + button2_w;
            im::SetCursorPosX(im::GetCursorPosX() + im::GetContentRegionAvail().x - width_needed);

            if (im::Button("Save")) {
                memcpy(&options, &tmp, sizeof(options));

                // write out options
                do {
                    auto filepath = path_join(world.configdir, ".options");

                    File f;
                    if (f.init_write(filepath) != FILE_RESULT_OK)
                        break;
                    defer { f.cleanup(); };

                    Serde serde;
                    serde.init(&f);
                    serde.write_type(&options, SERDE_OPTIONS);
                } while (0);

                if (wnd.something_that_needs_restart_was_changed) {
                    auto res = ask_user_yes_no("One of the settings changed requires you to restart CodePerfect. Restart now?", "Restart needed", "Restart", "Don't restart");
                    if (res == ASKUSER_YES)
                        fork_self();
                }

                wnd.show = false;
            }

            im::SameLine();

            if (im::Button("Cancel")) {
                wnd.show = false;
            }
        }

        im::End();

        fstlog("wnd_options");
    }

    fn<void(Call_Hier_Node*, Go_Workspace*, bool)> render_call_hier;
    render_call_hier = [&](auto it, auto workspace, auto show_tests_benches) {
        auto should_hide = [&](Call_Hier_Node *it) {
            auto decl = it->decl->decl->decl;
            if (!world.indexer.get_godecl_recvname(decl))
                if (!show_tests_benches)
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
        if (recvname)
            name = cp_sprintf("%s.%s", recvname, name);

        auto has_children = [&]() {
            For (it->children)
                if (!should_hide(&it))
                    return true;
            return false;
        };

        auto flags = 0;
        if (has_children())
            flags = ImGuiTreeNodeFlags_OpenOnArrow;
        else
            flags = ImGuiTreeNodeFlags_Bullet;

        ccstr label = get_import_path_label(ctx->import_path, workspace);
        bool open = im::TreeNodeEx((void*)it, flags, "%s.%s (%s)", fd->package_name, name, label);

        if (im::IsItemClicked()) {
            auto ref = it->ref;
            auto start = ref->is_sel ? ref->x_start : ref->start;
            goto_file_and_pos(fd->filepath, start, true);
        }

        if (open) {
            For (it->children)
                render_call_hier(&it, workspace, show_tests_benches);
            im::TreePop();
        }
    };

    if (world.wnd_callee_hierarchy.show) {
        auto &wnd = world.wnd_callee_hierarchy;

        im::SetNextWindowDockID(dock_sidebar_id, ImGuiCond_Once);
        begin_window(
            cp_sprintf("Callee Hierarchy for %s###callee_hierarchy", wnd.declres->decl->name),
            &wnd,
            0,
            !wnd.done,
            true
        );

        if (wnd.done) {
            im::Text("Done!");
            For (wnd.results) render_call_hier(&it, wnd.workspace, true);
        } else {
            im::Text("Generating callee hierarchy...");
            im::SameLine();
            if (im::Button("Cancel")) {
                cancel_callee_hierarchy();
                wnd.show = false;
            }
        }

        im::End();
        fstlog("wnd_callee_hierarchy");
    }

    if (world.wnd_caller_hierarchy.show) {
        auto &wnd = world.wnd_caller_hierarchy;

        im::SetNextWindowDockID(dock_sidebar_id, ImGuiCond_Once);
        begin_window(
            cp_sprintf("Caller Hierarchy for %s###caller_hierarchy", wnd.declres->decl->name),
            &wnd,
            0,
            !wnd.done,
            true
        );

        if (wnd.done) {
            im::Checkbox("Show tests, examples, and benchmarks", &wnd.show_tests_benches);
            For (wnd.results) render_call_hier(&it, wnd.workspace, wnd.show_tests_benches);
        } else {
            im::Text("Generating caller hierarchy...");
            im::SameLine();
            if (im::Button("Cancel")) {
                cancel_caller_hierarchy();
                wnd.show = false;
            }
        }

        im::End();
        fstlog("wnd_caller_hierarchy");
    }

    if (world.wnd_find_interfaces.show) {
        auto &wnd = world.wnd_find_interfaces;

        im::SetNextWindowDockID(dock_sidebar_id, ImGuiCond_Once);

        begin_window("Find Interfaces", &wnd, ImGuiWindowFlags_AlwaysAutoResize, !wnd.done, true);

        if (wnd.done) {
            im::Checkbox("Show empty interfaces", &wnd.include_empty);
            im::SameLine();
            help_marker("An empty interface{} is always implemented by every type. This checkbox lets you hide these from the results.");

            im::Checkbox("Search everywhere", &wnd.search_everywhere);
            bool search_everywhere_changed = im::IsItemEdited();
            im::SameLine();
            help_marker("By default, Find Interfaces only looks at interfaces inside your workspace. This setting will search everywhere in your dependency tree.");

            defer {
                if (search_everywhere_changed) {
                    if (wnd.search_everywhere)
                        wnd.include_empty = false;
                    do_find_interfaces();
                }
            };

            im_small_newline();

            if (!isempty(wnd.results)) {
                im_push_mono_font();

                auto results = new_list(Find_Decl*);
                For (wnd.results) {
                    if (!wnd.include_empty) {
                        auto gotype = it->decl->decl->gotype;
                        if (gotype)
                            if (gotype->type == GOTYPE_INTERFACE)
                                if (isempty(gotype->interface_specs))
                                    continue;
                    }
                    results->append(it);
                }

                Fori (results) {
                    auto index = i;

                    // TODO: refactor out custom draw
                    auto availwidth = im::GetContentRegionAvail().x;
                    auto text_size = ImVec2(availwidth, im::CalcTextSize("blah").y);
                    auto drawpos = im::GetCursorScreenPos();
                    auto drawlist = im::GetWindowDrawList();

                    auto draw_selectable = [&]() {
                        auto label = cp_sprintf("##find_implementations_result_%d", index);
                        return im::Selectable(label, wnd.selection == index, 0, text_size);
                    };

                    auto clicked = draw_selectable();

                    if (wnd.scroll_to == index) {
                        keep_item_inside_scroll();
                        wnd.scroll_to = -1;
                    }

                    auto draw_text = [&](ccstr text) {
                        drawlist->AddText(drawpos, im::GetColorU32(ImGuiCol_Text), text);
                        drawpos.x += im::CalcTextSize(text).x;
                    };

                    auto import_path = it->decl->ctx->import_path;

                    if (!wnd.workspace->find_module_containing(import_path)) {
                        im::PushStyleColor(ImGuiCol_Text, to_imcolor(global_colors.muted));
                        defer { im::PopStyleColor(); };

                        draw_text("(ext) ");
                    }

                    draw_text(cp_sprintf("%s.%s", it->package_name, it->decl->decl->name));

                    im::PushStyleColor(ImGuiCol_Text, to_imcolor(global_colors.muted));
                    {
                        auto label = get_import_path_label(import_path, wnd.workspace);
                        draw_text(cp_sprintf(" (%s)", label));
                    }
                    im::PopStyleColor();

                    // TODO: previews?
                    if (clicked) {
                        wnd.scroll_to = index;
                        wnd.selection = index;
                        goto_file_and_pos(it->filepath, it->decl->decl->name_start, true);
                    }
                }
                im_pop_font();

                auto oob = !(0 <= wnd.selection && wnd.selection < wnd.results->len);

                switch (get_keyboard_nav(&wnd, KNF_ALLOW_HJKL)) {
                case KN_ENTER: {
                    if (oob) break;
                    auto it = results->at(wnd.selection);
                    goto_file_and_pos(it->filepath, it->decl->decl->name_start, true);
                    break;
                }
                case KN_UP:
                    if (oob) {
                        wnd.selection = results->len-1;
                        wnd.scroll_to = wnd.selection;
                        break;
                    }
                    if (wnd.selection > 0) {
                        wnd.selection--;
                        wnd.scroll_to = wnd.selection;
                    }
                    break;
                case KN_DOWN:
                    if (oob) {
                        wnd.selection = results->len-1;
                        wnd.scroll_to = wnd.selection;
                        break;
                    }

                    if (wnd.selection + 1 < results->len) {
                        wnd.selection++;
                        wnd.scroll_to = wnd.selection;
                    }
                    break;
                }

            } else {
                im::Text("No interfaces found.");
            }
        } else {
            im::Text("Searching...");
            im::SameLine();
            if (im::Button("Cancel")) {
                cancel_find_interfaces();
                wnd.show = false;
            }
        }

        im::End();
        fstlog("wnd_find_interfaces");
    }

    if (world.wnd_find_implementations.show) {
        auto &wnd = world.wnd_find_implementations;

        im::SetNextWindowDockID(dock_sidebar_id, ImGuiCond_Once);

        begin_window("Find Implementations", &wnd, ImGuiWindowFlags_AlwaysAutoResize, !wnd.done, true);

        if (wnd.done) {
            im::Checkbox("Search everywhere", &wnd.search_everywhere);
            bool search_everywhere_changed = im::IsItemEdited();
            im::SameLine();
            help_marker("By default, Find Implementations only looks at types inside your workspace. This setting will search everywhere in your dependency tree.");

            defer {
                if (search_everywhere_changed) {
                    do_find_implementations();
                }
            };

            im_small_newline();

            if (!isempty(wnd.results)) {
                im_push_mono_font();

                Fori (wnd.results) {
                    int index = i;

                    // TODO: refactor out custom draw
                    auto availwidth = im::GetContentRegionAvail().x;
                    auto text_size = ImVec2(availwidth, im::CalcTextSize("blah").y);
                    auto drawpos = im::GetCursorScreenPos();
                    auto drawlist = im::GetWindowDrawList();

                    auto draw_selectable = [&]() {
                        auto label = cp_sprintf("##find_implementations_result_%d", index);
                        return im::Selectable(label, wnd.selection == index, 0, text_size);
                    };

                    auto clicked = draw_selectable();

                    if (wnd.scroll_to == index) {
                        keep_item_inside_scroll();
                        wnd.scroll_to = -1;
                    }

                    auto draw_text = [&](ccstr text) {
                        drawlist->AddText(drawpos, im::GetColorU32(ImGuiCol_Text), text);
                        drawpos.x += im::CalcTextSize(text).x;
                    };

                    draw_text(cp_sprintf("%s.%s", it->package_name, it->decl->decl->name));

                    im::PushStyleColor(ImGuiCol_Text, to_imcolor(global_colors.muted));
                    {
                        auto label = get_import_path_label(it->decl->ctx->import_path, wnd.workspace);
                        draw_text(cp_sprintf(" (%s)", label));
                    }
                    im::PopStyleColor();

                    // TODO: previews?
                    if (clicked) {
                        wnd.selection = index;
                        wnd.scroll_to = index;
                        goto_file_and_pos(it->filepath, it->decl->decl->name_start, true);
                    }
                }

                im_pop_font();

                auto oob = !(0 <= wnd.selection && wnd.selection < wnd.results->len);

                switch (get_keyboard_nav(&wnd, KNF_ALLOW_HJKL)) {
                case KN_ENTER: {
                    if (oob) break;
                    auto it = wnd.results->at(wnd.selection);
                    goto_file_and_pos(it->filepath, it->decl->decl->name_start, true);
                    break;
                }
                case KN_UP:
                    if (oob) {
                        wnd.selection = wnd.results->len-1;
                        wnd.scroll_to = wnd.selection;
                        break;
                    }
                    if (wnd.selection > 0) {
                        wnd.selection--;
                        wnd.scroll_to = wnd.selection;
                    }
                    break;
                case KN_DOWN:
                    if (oob) {
                        wnd.selection = wnd.results->len-1;
                        wnd.scroll_to = wnd.selection;
                        break;
                    }

                    if (wnd.selection + 1 < wnd.results->len) {
                        wnd.selection++;
                        wnd.scroll_to = wnd.selection;
                    }
                    break;
                }
            }
        } else {
            im::Text("Searching...");
            im::SameLine();
            if (im::Button("Cancel")) {
                cancel_find_implementations();
                wnd.show = false;
            }
        }

        im::End();
        fstlog("wnd_find_implementations");
    }

    if (world.wnd_find_references.show) {
        auto &wnd = world.wnd_find_references;

        im::SetNextWindowDockID(dock_sidebar_id, ImGuiCond_Once);

        begin_window("Find References", &wnd, ImGuiWindowFlags_AlwaysAutoResize, !wnd.done, true);

        if (wnd.done) {
            if (!isempty(wnd.results)) {
                bool go_prev = false;
                bool go_next = false;

                switch (get_keyboard_nav(&wnd, KNF_ALLOW_HJKL)) {
                case KN_ENTER: {
                    auto results = wnd.results;
                    int fidx = wnd.current_file;
                    int ridx = wnd.current_result;

                    if (fidx == -1) break;
                    if (!(0 <= fidx && fidx < results->len)) break;

                    auto file = results->at(fidx);
                    if (!(0 <= ridx && ridx < file.results->len)) break;

                    auto filepath = get_path_relative_to(file.filepath, world.current_path);

                    auto result = file.results->at(ridx);
                    auto ref = result.reference;
                    auto pos = ref->is_sel ? ref->x_start : ref->start;

                    goto_file_and_pos(filepath, pos, true);
                    break;
                }
                case KN_DOWN:
                    go_next = true;
                    break;
                case KN_UP:
                    go_prev = true;
                    break;
                }

                int last_file = -1;
                int last_result = -1;

                auto goto_result = [&](int file, int result) {
                    wnd.current_file = file;
                    wnd.current_result = result;
                    wnd.scroll_to_file = file;
                    wnd.scroll_to_result = result;
                };

                im_push_mono_font();

                Fori (wnd.results) {
                    int file_index = i;

                    auto filepath = get_path_relative_to(it.filepath, world.current_path);

                    auto render_header = [&]() {
                        im::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(ImColor(60, 60, 60)));
                        defer { im::PopStyleColor(); };

                        auto flags = ImGuiTreeNodeFlags_DefaultOpen
                            | ImGuiTreeNodeFlags_SpanAvailWidth
                            | ImGuiTreeNodeFlags_NoTreePushOnOpen;

                        return im::TreeNodeEx(filepath, flags);
                    };

                    if (!render_header()) {
                        if (wnd.current_file == file_index) {
                            if (go_prev) {
                                if (wnd.current_file) {
                                    int idx = wnd.current_file - 1;
                                    goto_result(idx, wnd.results->at(idx).results->len - 1);
                                }
                            } else if (go_next) {
                                if (wnd.current_file + 1 < it.results->len)
                                    goto_result(wnd.current_file + 1, 0);
                                go_next = false;
                            }
                        }
                        continue;
                    }

                    im::Indent();
                    im_push_mono_font();

                    Fori (it.results) {
                        int result_index = i;

                        if (go_prev)
                            if (file_index == wnd.current_file && result_index == wnd.current_result)
                                if (last_file != -1 && last_result != -1)
                                    goto_result(last_file, last_result);

                        if (go_next) {
                            if (last_file == wnd.current_file && last_result == wnd.current_result) {
                                goto_result(file_index, result_index);
                                go_next = false;
                            }
                        }

                        auto ref = it.reference;
                        auto pos = ref->is_sel ? ref->x_start : ref->start;

                        auto availwidth = im::GetContentRegionAvail().x;
                        auto text_size = ImVec2(availwidth, im::CalcTextSize("blah").y);
                        auto drawpos = im::GetCursorScreenPos();

                        bool selected = wnd.current_file == file_index && wnd.current_result == result_index;

                        if (selected) im::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(ImColor(60, 60, 60)));

                        if (im::Selectable(cp_sprintf("##find_references_result_%d_%d", file_index, result_index), selected, 0, text_size)) {
                            wnd.current_file = file_index;
                            wnd.current_result = result_index;
                            wnd.scroll_to_file = file_index;
                            wnd.scroll_to_result = result_index;
                            goto_file_and_pos(filepath, pos, true);
                        }

                        if (wnd.scroll_to_file == file_index && wnd.scroll_to_result == result_index) {
                            keep_item_inside_scroll();
                            wnd.scroll_to_file = -1;
                            wnd.scroll_to_result = -1;
                        }

                        if (selected) im::PopStyleColor();

                        // copied from search results, do we need to refactor?
                        auto draw_text = [&](ccstr text, ImColor color) {
                            im::PushStyleColor(ImGuiCol_Text, ImVec4(color));
                            defer { im::PopStyleColor(); };

                            im::GetWindowDrawList()->AddText(drawpos, im::GetColorU32(ImGuiCol_Text), text);
                            drawpos.x += im::CalcTextSize(text).x;
                        };

                        draw_text(new_cur2(pos.x+1, pos.y+1).str(), ImColor(200, 200, 200));

                        if (it.toplevel_name) {
                            draw_text(" (in ", ImColor(120, 120, 120));
                            draw_text(it.toplevel_name, ImColor(200, 200, 200));
                            draw_text(")", ImColor(120, 120, 120));
                        }

                        last_file = file_index;
                        last_result = result_index;
                    }

                    im_pop_font();
                    im::Unindent();
                }

                im_pop_font();
            } else {
                im::Text("No results found.");
            }

            // ...
        } else {
            im::Text("Searching...");
            im::SameLine();
            if (im::Button("Cancel")) {
                cancel_find_references();
                wnd.show = false;
            }
        }

        im::End();
        fstlog("wnd_find_references");
    }

    {
        auto &wnd = world.wnd_generate_implementation;

        if (wnd.show && wnd.fill_running && current_time_milli() - wnd.fill_time_started_ms > 100) {
            begin_centered_window("Generate Implementation...###generate_implementation_filling", &wnd, 0, 650);
            im::Text("Loading...");
            im::End();
        }

        if (wnd.show && !wnd.fill_running) {
            begin_centered_window("Generate Implementation###generate_impelmentation_ready", &wnd, 0, 650);

            if (wnd.selected_interface)
                im::TextWrapped("You've selected an interface. Please select a type and we'll add this interface's methods to that type.");
            else
                im::TextWrapped("You've selected a type. Please select an interface and we'll add that interface's methods to this type.");

            // closing
            if (!wnd.show) {
                wnd.filtered_results->len = 0;
                wnd.selection = 0;
            }

            switch (get_keyboard_nav(&wnd, KNF_ALLOW_IMGUI_FOCUSED)) {
            case KN_DOWN:
                if (!wnd.filtered_results->len) break;
                wnd.selection++;
                wnd.selection %= min(wnd.filtered_results->len, settings.generate_implementation_max_results);
                break;
            case KN_UP:
                if (!wnd.filtered_results->len) break;
                if (!wnd.selection)
                    wnd.selection = min(wnd.filtered_results->len, settings.generate_implementation_max_results) - 1;
                else
                    wnd.selection--;
                break;
            }

            focus_keyboard_here(&wnd);

            if (im_input_text_fixbuf("", wnd.query, ImGuiInputTextFlags_EnterReturnsTrue)) {
                do_generate_implementation();
                wnd.show = false;
                im::SetWindowFocus(NULL);
            }

            auto symbol_to_name = [&](auto &it) {
                return cp_sprintf("%s.%s", it.pkgname, it.name);
            };

            if (im::IsItemEdited()) {
                wnd.filtered_results->len = 0;
                wnd.selection = 0;

                if (strlen(wnd.query) >= 2) {
                    Fori (wnd.symbols) {
                        if (fzy_has_match(wnd.query, symbol_to_name(it))) {
                            wnd.filtered_results->append(i);
                            if (wnd.filtered_results->len > 10000) break;
                        }
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
                im_push_mono_font();
                defer { im_pop_font(); };

                for (u32 i = 0; i < wnd.filtered_results->len && i < settings.goto_file_max_results; i++) {
                    auto it = wnd.symbols->at(wnd.filtered_results->at(i));
                    auto text = symbol_to_name(it);

                    if (i == wnd.selection)
                        im::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "%s", text);
                    else
                        im::Text("%s", text);

                    im::SameLine();
                    im::TextColored(ImVec4(1.0f, 1.0, 1.0f, 0.4f), "\"%s\"", it.decl->ctx->import_path);
                }
            }

            im::End();
            fstlog("wnd_generate_implementation");
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

        begin_centered_window(cp_sprintf("Rename %s###rename_identifier", get_type_str()), &wnd, 0, 400, wnd.running);

        // if it's running, make sure the window stays focused
        if (wnd.running)
            im::SetWindowFocus();

        /*
        im::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(140, 194, 248)));
        im_push_mono_font();
        im::Text("%s", wnd.decl->name);
        im::PopStyleColor();
        im_pop_font();

        im_small_newline();
        */

        bool submitted = false;

        im_push_mono_font();

        focus_keyboard_here(&wnd);
        if (im_input_text_fixbuf(cp_sprintf("Rename %s to", wnd.declres->decl->name), wnd.rename_to, ImGuiInputTextFlags_EnterReturnsTrue)) {
            submitted = true;
        }

        im_pop_font();

        im_small_newline();

        /*
        im::RadioButton("Discard unsaved changes", &wnd.how_to_handle_unsaved_files, DISCARD_UNSAVED);
        im::SameLine();
        im::RadioButton("Save unsaved changes", &wnd.how_to_handle_unsaved_files, SAVE_UNSAVED);

        im_small_newline();
        */

        // im::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(230, 180, 180)));
        im::TextWrapped("Please note: we don't currently support undo. (You can always just rename it back.)");
        // im::PopStyleColor();

        im_small_newline();

        if (wnd.running) {
            if (wnd.too_late_to_cancel) {
                im::Text("Applying changes...");
            } else {
                im::Text("Renaming...");
                im::SameLine();
                if (im::Button("Cancel")) {
                    cancel_rename_identifier();
                }
            }
        } else {
            if (im::Button(cp_sprintf("Rename", wnd.declres->decl->name)))
                submitted = true;
        }

        // We need to close this window when defocused, because otherwise the
        // user might move the cursor and fuck this up
        if (!wnd.focused) {
            wnd.show = false;
            im::SetWindowFocus(NULL);
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

        im::End();
        fstlog("wnd_rename_identifier");
    }

    if (world.wnd_index_log.show) {
        auto &wnd = world.wnd_index_log;

        im::SetNextWindowDockID(dock_bottom_id, ImGuiCond_Once);
        begin_window("Index Log", &wnd, 0, false, true);

        im_push_mono_font();

        ImGuiListClipper clipper;
        clipper.Begin(wnd.len);
        while (clipper.Step())
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                im::Text("%s", wnd.buf[(wnd.start + i) % INDEX_LOG_CAP]);

        if (wnd.cmd_scroll_to_end) {
            wnd.cmd_scroll_to_end = false;
            if (im::GetScrollY() >= im::GetScrollMaxY())
                im::SetScrollHereY(1.0f);
        }

        im_pop_font();

        im::End();
        fstlog("wnd_index_log");
    }

    if (world.wnd_debug_output.show) {
        auto &wnd = world.wnd_debug_output;

        im::SetNextWindowDockID(dock_bottom_id, ImGuiCond_Once);
        begin_window("Debug Output", &wnd);

        im_push_mono_font();

        auto &lines = world.dbg.stdout_lines;

        ImGuiListClipper clipper;
        clipper.Begin(lines.len);
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                auto &it = lines[i];

                im::Text("%s", it);
                if (im_begin_popup_context_item(cp_sprintf("##debug_output_hidden_%d", i))) {
                    defer { im::EndPopup(); };

                    if (im::Selectable("Copy")) {
                        world.window->set_clipboard_string(it);
                    }

                    if (im::Selectable("Copy All")) {
                        auto output = new_list(char);
                        For (&lines) {
                            for (auto p = it; *p != '\0'; p++) {
                                output->append(*p);
                            }
                            output->append('\n');
                        }
                        output->len--; // remove last '\n'
                        output->append('\0');
                        world.window->set_clipboard_string(output->items);
                    }

                    if (im::Selectable("Clear")) {
                        // TODO
                        break;
                    }
                }
            }
        }

        if (wnd.cmd_scroll_to_end) {
            wnd.cmd_scroll_to_end = false;
            if (im::GetScrollY() >= im::GetScrollMaxY())
                im::SetScrollHereY(1.0f);
        }

        im_pop_font();

        im::End();
        fstlog("wnd_debug_output");
    }

    if (world.error_list.show) {
        im::SetNextWindowDockID(dock_bottom_id, ImGuiCond_Once);
        begin_window("Build Results", &world.error_list);

        static Build_Error *menu_current_error = NULL;

        if (im::IsWindowAppearing()) {
            im::SetWindowFocus(NULL);
        }

        auto &b = world.build;

        if (b.ready()) {
            if (!b.errors.len) {
                im::TextColored(to_imcolor(global_colors.green), "Build \"%s\" was successful!", b.build_profile_name);
            } else {
                im::Text("Building \"%s\"...", b.build_profile_name);

                im_push_mono_font();

                for (int i = 0; i < b.errors.len; i++) {
                    auto &it = b.errors[i];

                    if (!it.valid) {
                        im::TextColored(to_imcolor(global_colors.muted), "%s", it.message);
                        continue;
                    }

                    /*
                    auto flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth;
                    if (i == b.current_error)
                        flags |= ImGuiTreeNodeFlags_Selected;

                    im::Unindent(im::GetTreeNodeToLabelSpacing());
                    im::TreeNodeEx(&it, flags, "%s:%d:%d: %s", it.file, it.row, it.col, it.message);
                    im::Indent(im::GetTreeNodeToLabelSpacing());
                    */

                    if (i == b.scroll_to) {
                        keep_item_inside_scroll();
                        b.scroll_to = -1;
                    }

                    auto label = cp_sprintf("%s:%d:%d: %s", it.file, it.row, it.col, it.message);
                    auto wrap_width = im::GetContentRegionAvail().x;
                    auto text_size = ImVec2(wrap_width, im::CalcTextSize(label, NULL, false, wrap_width).y);
                    auto pos = im::GetCursorScreenPos();

                    bool clicked = im::Selectable(cp_sprintf("##hidden_%d", i), i == b.current_error, 0, text_size);
                    im::GetWindowDrawList()->AddText(NULL, 0.0f, pos, im::GetColorU32(ImGuiCol_Text), label, NULL, wrap_width);

                    if (im_begin_popup_context_item()) {
                        if (im::Selectable("Copy")) {
                            world.window->set_clipboard_string(label);
                        }
                        im::EndPopup();
                    }

                    /*
                    if (im::IsMouseReleased(ImGuiMouseButton_Right) && im::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)) {
                        menu_current_error = &it;
                        im::OpenPopup("error list menu");
                    }
                    */

                    if (clicked) {
                        b.current_error = i;
                        goto_error(i);
                    }
                }

                im_pop_font();
            }
        } else if (b.started) {
            im::Text("Building \"%s\"...", b.build_profile_name);
        } else {
            im::Text("No build in progress.");
        }

        im::End();
        fstlog("wnd_error_list");
    }

    if (world.wnd_rename_file_or_folder.show) {
        auto &wnd = world.wnd_rename_file_or_folder;

        auto label = cp_sprintf("Rename %s", wnd.target->is_directory ? "folder" : "file");
        begin_centered_window(cp_sprintf("%s###add_file_or_folder", label), &wnd, 0, 300);

        handle_popup_window_logic(&wnd, &world.file_explorer);

        im_with_disabled(true, [&]() {
            im_push_mono_font();
            im_input_text_fixbuf("Old name", wnd.location, ImGuiInputTextFlags_ReadOnly);
            im_pop_font();
        });

        im_small_newline();

        focus_keyboard_here(&wnd);

        // close the window when we unfocus
        if (!wnd.focused) wnd.show = false;

        // if we just closed the window, refocus the file explorer
        if (!wnd.show) {
            world.file_explorer.cmd_focus = true;
            world.cmd_unfocus_all_windows = false;
        }

        im_push_mono_font();
        bool entered = im_input_text_fixbuf("New name", wnd.name, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        im_pop_font();

        if (check_cmd_flag(&wnd.cmd_focus_textbox))
            im_select_all_last();

        do {
            if (!entered) break;

            auto error_out = [&](ccstr msg) {
                tell_user(msg, "Error");
                wnd.cmd_focus_textbox = true;
                // wnd.name[0] = '\0';
            };

            if (!strlen(wnd.name)) {
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
            auto newpath = path_join(cp_dirname(oldpath), wnd.name);

            GHRenameFileOrDirectory((char*)oldpath, (char*)newpath);
            reload_file_subtree(cp_dirname(wnd.location));

            wnd.show = false;
        } while (0);

        im::End();
        fstlog("wnd_rename_file_or_folder");
    }

    if (world.wnd_add_file_or_folder.show) {
        auto &wnd = world.wnd_add_file_or_folder;

        auto label = cp_sprintf("Add %s", wnd.folder ? "Folder" : "File");
        begin_centered_window(cp_sprintf("%s###add_file_or_folder", label), &wnd, 0, 300);

        handle_popup_window_logic(&wnd, &world.file_explorer);

        im::Text("Destination");

        im::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(140, 194, 248)));
        im_push_mono_font();

        if (wnd.location_is_root) {
            if (world.file_tree)
                im::Text("%s (root)", cp_basename(world.current_path));
            else
                im::Text("(root)");
        } else {
            im::Text("%s", wnd.location);
        }

        im::PopStyleColor();
        im_pop_font();

        im_small_newline();

        focus_keyboard_here(&wnd);

        im_push_mono_font();
        bool entered = im_input_text_fixbuf("Name", wnd.name, ImGuiInputTextFlags_EnterReturnsTrue);
        im_pop_font();

        if (entered) {
            wnd.show = false;

            do {
                if (world.file_tree_busy) {
                    tell_user_error("Sorry, the file tree is currently being generated.");
                    break;
                }

                if (!strlen(wnd.name)) break;

                auto dest = wnd.location_is_root ? world.current_path : path_join(world.current_path, wnd.location);
                auto path = path_join(dest, wnd.name);

                auto ok = wnd.folder ? create_directory(path) : touch_file(path);
                if (!ok) break;

                auto destnode = wnd.location_is_root ? world.file_tree : find_ft_node(wnd.location);
                add_ft_node(destnode, [&](auto child) {
                    child->is_directory = wnd.folder;
                    child->name = cp_strdup(wnd.name);
                });

                if (!wnd.folder) {
                    focus_editor(path);
                    im::SetWindowFocus(NULL);
                }
            } while (0);
        }

        im::End();
        fstlog("wnd_add_file_or_folder");
    }

    if (world.file_explorer.show) {
        auto &wnd = world.file_explorer;

        auto old_item_spacing = im::GetStyle().ItemSpacing;

        im::SetNextWindowDockID(dock_sidebar_id, ImGuiCond_Once);
        im::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        im::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        begin_window("File Explorer", &wnd, 0, false, true);
        im::PopStyleVar();

        fstlog("wnd_file_explorer - start window");

        auto begin_buttons_child = [&]() {
            im::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
            defer { im::PopStyleVar(); };

            auto &style = im::GetStyle();

            auto text_height = im::CalcTextSize(ICON_MD_NOTE_ADD, NULL, true).y;
            float child_height = text_height + (icon_button_padding.y * 2.0f) + (style.WindowPadding.y * 2.0f);
            im::BeginChild("child2", ImVec2(0, child_height), false, ImGuiWindowFlags_AlwaysUseWindowPadding);
        };

        begin_buttons_child(); {
            if (im_icon_button(ICON_MD_NOTE_ADD)) {
                open_add_file_or_folder(false);
            }

            im::SameLine(0.0, 6.0f);

            if (im_icon_button(ICON_MD_CREATE_NEW_FOLDER)) {
                open_add_file_or_folder(true);
            }

            im::SameLine(0.0, 6.0f);

            if (im_icon_button(ICON_MD_REFRESH)) {
                fill_file_tree(); // TODO: async?
            }
        } im::EndChild();

        fstlog("wnd_file_explorer - draw buttons");

        auto begin_directory_child = [&]() {
            im::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
            im::PushStyleVar(ImGuiStyleVar_ChildRounding, 0);
            defer { im::PopStyleVar(2); };

            im::BeginChild("child3", ImVec2(0,0), true);
        };

        bool menu_handled = false;

        auto handle_drag_drop = [&](FT_Node *source, FT_Node *dest) {
            auto source_path = path_join(world.current_path, ft_node_to_path(source));
            auto dest_path = path_join(world.current_path, ft_node_to_path(dest), source->name);

            if (are_filepaths_equal(source_path, dest_path))
                return;

            auto result = check_path(dest_path);
            if (result != CPR_NONEXISTENT) {
                ccstr existing_type = (result == CPR_FILE ? "file" : "directory");
                auto msg = cp_sprintf("A %s already exists there with the same name. Do you want to replace it?", existing_type);

                auto msgbox_result = ask_user_yes_no_cancel(msg, "Replace", "Rename", "Cancel");
                if (msgbox_result == ASKUSER_CANCEL)
                    return;

                if (msgbox_result == ASKUSER_YES) {
                    delete_rm_rf(dest_path);
                } else {
                    bool found = false;
                    for (int i = 1; i < 1000; i++) {
                        dest_path = path_join(world.current_path, ft_node_to_path(dest), cp_sprintf("%s (%d)", source->name, i));
                        if (check_path(dest_path) == CPR_NONEXISTENT) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        tell_user_error("Unable to find a suitable name to rename file to.");
                        return;
                    }
                }
            }

            if (!move_file_or_directory(source_path, dest_path))
                tell_user_error("Unable to move file or directory.");

            reload_file_subtree(get_path_relative_to(cp_dirname(source_path), world.current_path));
            reload_file_subtree(get_path_relative_to(cp_dirname(dest_path), world.current_path));

            auto node = find_ft_node(get_path_relative_to(cp_dirname(dest_path), world.current_path));
            if (node) node->open = true;

            node = find_ft_node(get_path_relative_to(dest_path, world.current_path));
            if (node) wnd.selection = node;
        };

        begin_directory_child(); {
            SCOPED_FRAME();

            FT_Node *new_dragging_source = NULL;
            FT_Node *new_dragging_dest = NULL;
            FT_Node *new_hovered_item = NULL;

            fn<void(FT_Node*, bool)> draw = [&](auto it, bool highlight_dragging_into) {
                for (u32 j = 0; j < it->depth; j++) im::Indent();

                if (it == wnd.dragging_dest) highlight_dragging_into = true;

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
                    ImGuiStyle &style = im::GetStyle();

                    int pushed_colors = 0;

                    auto push_color = [&](auto x, auto y) {
                        im::PushStyleColor(x, y);
                        pushed_colors++;
                    };

                    if (wnd.selection != it) {
                        if (mute)
                            push_color(ImGuiCol_Text, ImVec4(0.5, 0.5, 0.5, 1.0));
                        push_color(ImGuiCol_HeaderHovered, ImVec4(0.2, 0.2, 0.2, 1.0));
                        push_color(ImGuiCol_HeaderActive, ImVec4(0.2, 0.2, 0.2, 1.0));
                    }

                    im::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));

                    ccstr short_label = cp_sprintf("%s %s", icon, it->parent ? it->name : cp_basename(world.current_path));

                    ccstr label = short_label;
                    if (it->is_directory)
                        label = cp_sprintf("%s %s", label, it->open ? ICON_MD_EXPAND_MORE : ICON_MD_CHEVRON_RIGHT);

                    if (it == wnd.scroll_to) {
                        keep_item_inside_scroll();
                        wnd.scroll_to = NULL;
                    }

                    im::PushID(it);
                    im::Selectable(label, wnd.selection == it, ImGuiSelectableFlags_AllowDoubleClick);
                    im::PopID();

                    if (highlight_dragging_into) {
                        auto rmin = im::GetItemRectMin();
                        auto rmax = im::GetItemRectMax();
                        rmin.x = 0;
                        im::GetWindowDrawList()->AddRectFilled(rmin, rmax, IM_COL32(255, 255, 255, 50));
                    }

                    im::PopStyleVar();
                    im::PopStyleColor(pushed_colors);

                    // handle drag and drop

                    if (wnd.dragging_source)
                        if (im::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
                            new_hovered_item = it;

                    if (im::BeginDragDropSource()) {
                        new_dragging_source = it;
                        im::SetDragDropPayload("file_explorer_dnd", &it, sizeof(it));

                        im::Text("%s", short_label);
                        im::EndDragDropSource();
                    }

                    if (im::BeginDragDropTarget()) {
                        new_dragging_dest = it;
                        if (!it->is_directory)
                            new_dragging_dest = new_dragging_dest->parent;

                        auto payload = ImGui::AcceptDragDropPayload("file_explorer_dnd");
                        if (payload)
                            handle_drag_drop(*(FT_Node**)payload->Data, new_dragging_dest);

                        im::EndDragDropTarget();
                    }
                }

                if (im_begin_popup_context_item(NULL)) {
                    menu_handled = true;

                    im::PushStyleVar(ImGuiStyleVar_ItemSpacing, old_item_spacing);

                    // wnd.selection = it;

                    if (!it->is_directory) {
                        if (im::Selectable("Open")) {
                            open_ft_node(it);
                        }
                    }

                    if (im::Selectable("Rename")) {
                        open_rename(it);
                    }

                    if (im::Selectable("Delete")) {
                        delete_ft_node(it);
                    }

                    im::Separator();

                    if (im::Selectable("Add new file...")) {
                        open_add_file_or_folder(false, it);
                    }

                    if (im::Selectable("Add new folder...")) {
                        open_add_file_or_folder(true, it);
                    }

                    /*
                    if (im::Selectable("Cut")) {
                        wnd.last_file_cut = it;
                        wnd.last_file_copied = NULL;
                    }

                    if (im::Selectable("Copy")) {
                        wnd.last_file_copied = it;
                        wnd.last_file_cut = NULL;
                    }

                    if (im::Selectable("Paste")) {
                        FT_Node *src = wnd.last_file_copied;
                        bool cut = false;
                        if (!src) {
                            src = wnd.last_file_cut;
                            cut = true;
                        }

                        if (src) {
                            auto dest = it;
                            if (!dest->is_directory) dest = dest->parent;

                            ccstr srcpath = ft_node_to_path(src);
                            ccstr destpath = NULL;

                            // if we're copying to the same place
                            if (src->parent == dest)
                                destpath = path_join(ft_node_to_path(dest), cp_sprintf("copy of %s", src->name));
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

                    im::Separator();

                    if (im::Selectable("Copy relative path")) {
                        SCOPED_FRAME();
                        auto rel_path = ft_node_to_path(it);
                        world.window->set_clipboard_string(rel_path);
                    }

                    if (im::Selectable("Copy absolute path")) {
                        SCOPED_FRAME();
                        auto rel_path = ft_node_to_path(it);
                        auto full_path = path_join(world.current_path, rel_path);
                        world.window->set_clipboard_string(full_path);
                    }

                    im::PopStyleVar();
                    im::EndPopup();
                }

                for (u32 j = 0; j < it->depth; j++) im::Unindent();

                if (im::IsItemClicked(0) || im::IsItemClicked(1)) {
                    wnd.selection = it;
                }

                if (im::IsMouseDoubleClicked(0) && im::IsItemHovered(0)) {
                    if (it->is_directory)
                        it->open ^= 1;
                    else
                        open_ft_node(it);
                }

                if (it->is_directory && it->open)
                    for (auto child = it->children; child; child = child->next)
                        draw(child, highlight_dragging_into);
            };

            if (world.file_tree_busy) {
                im::Text("Generating file tree...");
            } else {
                draw(world.file_tree, false);

                fstlog("wnd_file_explorer - draw files");

                if (!menu_handled && im_begin_popup_context_window("file_explorer_context_menu")) {
                    {
                        im::PushStyleVar(ImGuiStyleVar_ItemSpacing, old_item_spacing);
                        defer { im::PopStyleVar(); };

                        if (im::Selectable("Add new file...")) {
                            open_add_file_or_folder(false);
                        }

                        if (im::Selectable("Add new folder...")) {
                            open_add_file_or_folder(true);
                        }
                    }
                    im::EndPopup();
                }

                fstlog("wnd_file_explorer - right click");
            }

            wnd.dragging_source = new_dragging_source;
            wnd.dragging_dest = new_dragging_dest;

            if (new_hovered_item != wnd.hovered) {
                wnd.hovered = new_hovered_item;
                if (wnd.hovered) {
                    wnd.hovered_start_milli = current_time_milli();
                    print("setting new time: %llu", wnd.hovered_start_milli);
                }
            }

            if (wnd.hovered && current_time_milli() - wnd.hovered_start_milli > 700)
                wnd.hovered->open = true;
        } im::EndChild();

        // if users clicks on empty space in child, unset selection
        if (im::IsItemClicked()) {
            wnd.selection = NULL;
        }

        if (!world.file_tree_busy) {
            switch (get_keyboard_nav(&wnd, KNF_ALLOW_HJKL)) {
            case KN_DOWN: {
                auto getnext = [&]() -> FT_Node * {
                    auto curr = wnd.selection;
                    if (!curr) return world.file_tree->children;

                    if (curr->children && curr->open)
                        return curr->children;
                    if (curr->next)
                        return curr->next;

                    while (curr->parent) {
                        curr = curr->parent;
                        if (curr->next)
                            return curr->next;
                    }

                    return NULL;
                };

                auto next = getnext();
                if (next) {
                    wnd.selection = next;
                    wnd.scroll_to = next;
                }
                break;
            }
            case KN_LEFT: {
                auto curr = wnd.selection;
                if (curr)
                    if (curr->is_directory)
                        curr->open = false;
                break;
            }
            case KN_RIGHT: {
                auto curr = wnd.selection;
                if (curr)
                    if (curr->is_directory)
                        curr->open = true;
                break;
            }
            case KN_UP: {
                auto curr = wnd.selection;
                if (curr) {
                    if (curr->prev) {
                        curr = curr->prev;
                        // as long as curr has children, keep grabbing the last child
                        while (curr->is_directory && curr->open && curr->children) {
                            curr = curr->children;
                            while (curr->next)
                                curr = curr->next;
                        }
                    } else {
                        curr = curr->parent;
                        if (!curr->parent) // if we're at the root
                            curr = NULL; // don't set selection to root
                    }
                }

                if (curr) {
                    wnd.selection = curr;
                    wnd.scroll_to = curr;
                }
                break;
            }

            case KN_DELETE: {
                auto curr = wnd.selection;
                if (curr) delete_ft_node(curr);
                break;
            }

            case KN_ENTER: {
                auto curr = wnd.selection;
                if (!curr) break;
                if (curr->is_directory)
                    curr->open ^= 1;
                else
                    open_ft_node(curr);
                break;
            }
            }

            if (wnd.selection)
                if (im_wnd_key_pressed(&wnd, CP_KEY_R, CP_MOD_NONE))
                    open_rename(wnd.selection);

            fstlog("wnd_file_explorer - handle keys");
        }

        im::End();
        im::PopStyleVar();
        fstlog("wnd_file_explorer");
    }

    if (world.wnd_project_settings.show) {
        auto &wnd = world.wnd_project_settings;

        begin_window("Project Settings", &wnd, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking);

        auto &ps = wnd.tmp;

        if (im::BeginTabBar("MyTabBar", 0)) {
            auto get_focus_flags = [&](bool *pfocus, int flags = 0) -> int {
                if (*pfocus) {
                    flags |= ImGuiTabItemFlags_SetSelected;
                    *pfocus = false;
                }
                return flags;
            };

#if 0
            if (im::BeginTabItem("General Settings", NULL, get_focus_flags(&wnd.focus_general_settings))) {
                auto begin_container_child = [&]() {
                    im::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
                    defer { im::PopStyleVar(); };

                    im::BeginChild("container", ImVec2(600, 300), false, ImGuiWindowFlags_AlwaysUseWindowPadding);
                };

                begin_container_child(); {
                    im_with_disabled(true, [&]() {
                        im::Checkbox("Search vendor before GOMODCACHE", &ps->search_vendor_before_modcache);
                    });
                } im::EndChild();

                im::EndTabItem();
            }
#endif

            auto begin_left_pane_child = [&]() {
                im::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                im::PushStyleVar(ImGuiStyleVar_ChildRounding, 0);
                defer { im::PopStyleVar(2); };

                im::BeginChild("left_pane_child", ImVec2(200, 300), true, ImGuiWindowFlags_AlwaysUseWindowPadding);
            };

            auto begin_right_pane_child = [&]() {
                im::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
                defer { im::PopStyleVar(); };

                im::BeginChild("right pane", ImVec2(400, 300), false, ImGuiWindowFlags_AlwaysUseWindowPadding);
            };

            auto profiles_buttons_padding = ImVec2(6, 6);

            auto get_profiles_left_pane_buttons_height = [&]() -> float {
                auto &style = im::GetStyle();

                auto text_height = im::CalcTextSize(ICON_MD_NOTE_ADD, NULL, true).y;
                return text_height + (icon_button_padding.y *style.FramePadding.y * 2.0f) + (style.WindowPadding.y * 2.0f);
            };

            auto begin_profiles_buttons_child = [&]() {
                im::PushStyleVar(ImGuiStyleVar_WindowPadding, profiles_buttons_padding);
                defer { im::PopStyleVar(); };

                im::BeginChild("child2", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysUseWindowPadding);
            };

            auto begin_profiles_child = [&]() {
                float height = 0;
                {
                    im::PushStyleVar(ImGuiStyleVar_WindowPadding, profiles_buttons_padding);
                    defer { im::PopStyleVar(); };

                    auto &style = im::GetStyle();
                    height = get_profiles_left_pane_buttons_height() + style.ItemSpacing.y;
                }

                im::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
                im::PushStyleVar(ImGuiStyleVar_ChildRounding, 0);
                defer { im::PopStyleVar(2); };

                auto h = im::GetContentRegionAvail().y - height;

                im::BeginChild("child3", ImVec2(0, h), true, ImGuiWindowFlags_AlwaysUseWindowPadding);
            };

            if (im::BeginTabItem("Debug Profiles", NULL, get_focus_flags(&wnd.focus_debug_profiles))) {

                begin_left_pane_child(); {
                    im::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
                    defer { im::PopStyleVar(); };

                    begin_profiles_child(); {
                        im::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
                        defer { im::PopStyleVar(); };

                        Fori (ps->debug_profiles) {
                            auto label = cp_sprintf("%s##debug_profile_%d", it.label, i);
                            if (im::Selectable(label, wnd.current_debug_profile == i))
                                wnd.current_debug_profile = i;
                        }
                    } im::EndChild();

                    begin_profiles_buttons_child(); {
                        if (im_icon_button(ICON_MD_ADD)) {
                            Debug_Profile prof; ptr0(&prof);
                            prof.type = DEBUG_TEST_PACKAGE;
                            prof.is_builtin = false;
                            cp_strcpy_fixed(prof.label, "New Profile");
                            prof.test_package.package_path[0] = '\0';
                            prof.test_package.use_current_package = true;

                            {
                                SCOPED_MEM(&wnd.pool);
                                ps->debug_profiles->append(&prof);
                            }
                            wnd.current_debug_profile = ps->debug_profiles->len - 1;
                        }

                        im::SameLine(0.0, 4.0f);

                        auto can_remove = [&]() {
                            if (wnd.current_debug_profile < 0) return false;
                            if (wnd.current_debug_profile >= ps->debug_profiles->len) return false;
                            if (ps->debug_profiles->at(wnd.current_debug_profile).is_builtin) return false;

                            return true;
                        };

                        bool remove_clicked = false;
                        im_with_disabled(!can_remove(), [&]() {
                            remove_clicked = im_icon_button(ICON_MD_REMOVE);
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
                    } im::EndChild();
                } im::EndChild();

                im::SameLine();

                begin_right_pane_child(); {
                    auto index = wnd.current_debug_profile;
                    if (0 <= index && index < ps->debug_profiles->len) {
                        auto &dp = ps->debug_profiles->at(wnd.current_debug_profile);

                        if (dp.is_builtin) {
                            if (dp.type == DEBUG_TEST_CURRENT_FUNCTION) {
                                ImGuiStyle &style = im::GetStyle();
                                im::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
                                im::TextWrapped("This is a built-in debug profile, used for the Debug Test Under Cursor command. It can't be changed, except to add command-line arguments.");
                                im::PopStyleColor();
                                im_small_newline();
                            }
                        }

                        im_with_disabled(dp.is_builtin, [&]() {
                            im_input_text_fixbuf("Name", dp.label);
                        });

                        im_small_newline();

                        const char* labels[] = {
                            "Test Package",
                            "Test Function Under Cursor",
                            "Run Package",
                            "Run Binary",
                        };

                        im_with_disabled(dp.is_builtin, [&]() {
                            im::Text("Type");
                            im::PushItemWidth(-1);
                            im::Combo("##dp_type", (int*)&dp.type, labels, _countof(labels));
                            im::PopItemWidth();
                        });

                        im_small_newline();

                        switch (dp.type) {
                        case DEBUG_TEST_PACKAGE:
                            im::Checkbox("Use package of current file", &dp.test_package.use_current_package);

                            im_small_newline();

                            im_with_disabled(dp.test_package.use_current_package, [&]() {
                                im_push_mono_font();
                                im_input_text_fixbuf("Package path", dp.test_package.package_path);
                                im_pop_font();
                            });

                            im_small_newline();
                            break;

                        case DEBUG_TEST_CURRENT_FUNCTION:
                            break;

                        case DEBUG_RUN_PACKAGE:
                            im::Checkbox("Use package of current file", &dp.run_package.use_current_package);
                            im_small_newline();

                            im_with_disabled(dp.run_package.use_current_package, [&]() {
                                im_push_mono_font();
                                im_input_text_fixbuf("Package path", dp.run_package.package_path);
                                im_pop_font();
                            });

                            im_small_newline();
                            break;

                        case DEBUG_RUN_BINARY:
                            im_push_mono_font();
                            im_input_text_fixbuf("Binary path", dp.run_binary.binary_path);
                            im_pop_font();
                            im_small_newline();
                            break;
                        }

                        im_push_mono_font();
                        im_input_text_fixbuf("Additional arguments", dp.args);
                        im_pop_font();
                    } else {
                        if (!ps->debug_profiles->len) {
                            im::Text("Create a profile on the left and it'll show up here.");
                        } else {
                            im::Text("Select a profile on the left and it'll show up here.");
                        }
                    }
                } im::EndChild();

                im::EndTabItem();
            }

            if (im::BeginTabItem("Build Profiles", NULL, get_focus_flags(&wnd.focus_build_profiles))) {
                begin_left_pane_child(); {
                    im::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
                    defer { im::PopStyleVar(); };

                    begin_profiles_child(); {
                        im::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
                        defer { im::PopStyleVar(); };

                        Fori (ps->build_profiles) {
                            auto label = cp_sprintf("%s##build_profile_%d", it.label, i);
                            if (im::Selectable(label, wnd.current_build_profile == i))
                                wnd.current_build_profile = i;
                        }
                    } im::EndChild();

                    begin_profiles_buttons_child(); {
                        if (im_icon_button(ICON_MD_ADD)) {
                            Build_Profile prof; ptr0(&prof);
                            cp_strcpy_fixed(prof.label, "New Profile");
                            cp_strcpy_fixed(prof.cmd, "go build");

                            {
                                SCOPED_MEM(&wnd.pool);
                                ps->build_profiles->append(&prof);
                            }
                            wnd.current_build_profile = ps->build_profiles->len - 1;
                        }

                        im::SameLine(0.0, 4.0f);

                        auto can_remove = [&]() {
                            if (wnd.current_build_profile < 0) return false;
                            if (wnd.current_build_profile >= ps->build_profiles->len) return false;

                            return true;
                        };

                        bool remove_clicked = false;
                        im_with_disabled(!can_remove(), [&]() {
                            remove_clicked = im_icon_button(ICON_MD_REMOVE);
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
                    } im::EndChild();
                } im::EndChild();

                im::SameLine();

                begin_right_pane_child(); {
                    auto index = wnd.current_build_profile;
                    if (0 <= index && index < ps->build_profiles->len) {
                        auto &bp = ps->build_profiles->at(index);

                        im_input_text_fixbuf("Name", bp.label);
                        im_small_newline();

                        im_push_mono_font();
                        im_input_text_fixbuf("Build command", bp.cmd);
                        im_pop_font();
                    } else {
                        if (!ps->build_profiles->len) {
                            im::Text("Create a profile on the left and it'll show up here.");
                        } else {
                            im::Text("Select a profile on the left and it'll show up here.");
                        }
                    }
                } im::EndChild();

                im::EndTabItem();
            }

            im::EndTabBar();
        }

        im::Separator();

        {
            ImGuiStyle &style = im::GetStyle();

            float button1_w = im::CalcTextSize("Save").x + style.FramePadding.x * 2.f;
            float button2_w = im::CalcTextSize("Cancel").x + style.FramePadding.x * 2.f;
            float width_needed = button1_w + style.ItemSpacing.x + button2_w;
            im::SetCursorPosX(im::GetCursorPosX() + im::GetContentRegionAvail().x - width_needed);

            if (im::Button("Save")) {
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

            im::SameLine();

            if (im::Button("Cancel")) {
                wnd.show = false;
            }
        }

        im::End();
        fstlog("wnd_project_settings");
    }

#ifdef DEBUG_BUILD
    if (world.windows_open.im_demo) {
        im::ShowDemoWindow(&world.windows_open.im_demo);
        fstlog("im_demo");
    }

    if (world.windows_open.im_metrics) {
        im::ShowMetricsWindow(&world.windows_open.im_metrics);
        fstlog("im_metrics");
    }
#endif

    if (world.wnd_goto_file.show) {
        auto& wnd = world.wnd_goto_file;

        begin_centered_window("Go To File", &wnd, 0, 650);

        /*
        // close the window when we unfocus
        if (!wnd.focused) {
            wnd.show = false;
            im::SetWindowFocus(NULL);
        }
        */

        focus_keyboard_here(&wnd);

        if (im_input_text_fixbuf("Search for file:", wnd.query, ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (wnd.filtered_results->len > 0) {
                auto relpath = wnd.filepaths->at(wnd.filtered_results->at(wnd.selection));
                auto filepath = path_join(world.current_path, relpath);
                focus_editor(filepath);
            }
            wnd.show = false;
            im::SetWindowFocus(NULL);
        }

        switch (get_keyboard_nav(&wnd, KNF_ALLOW_IMGUI_FOCUSED)) {
        case KN_UP:
            if (!wnd.filtered_results->len) break;
            if (!wnd.selection)
                wnd.selection = min(wnd.filtered_results->len, settings.goto_file_max_results) - 1;
            else
                wnd.selection--;
            break;
        case KN_DOWN:
            if (!wnd.filtered_results->len) break;
            wnd.selection++;
            wnd.selection %= min(wnd.filtered_results->len, settings.goto_file_max_results);
            break;
        }

        if (im::IsItemEdited()) {
            if (strlen(wnd.query) >= 2)
                filter_files();
            else
                wnd.filtered_results->len = 0; // maybe use logic from filter_files
            wnd.selection = 0;
        }

        if (wnd.filtered_results->len > 0) {
            im_small_newline();

            im_push_mono_font();
            defer { im_pop_font(); };

            auto pm = pretty_menu_start();

            for (u32 i = 0; i < wnd.filtered_results->len && i < settings.goto_file_max_results; i++) {
                pretty_menu_item(pm, i == wnd.selection);
                pretty_menu_text(pm, wnd.filepaths->at(wnd.filtered_results->at(i)));
            }
        }

        im::End();
        fstlog("wnd_goto_file");
    }

    do {
        if (!world.wnd_local_search.show) break;

        auto editor = get_current_editor();
        if (!editor) break;

        auto &wnd = world.wnd_local_search;

        im::SetNextWindowSize(ImVec2(300, -1));

        im::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        defer { im::PopStyleVar(); };

        if (!editor->ui_rect_set) break;

        {
            auto &r = editor->ui_rect;
            im::SetNextWindowPos(ImVec2(r.x + r.w - 10, r.y - 1), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        }

        int flags = ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoScrollWithMouse
            | ImGuiWindowFlags_NoDecoration;
        begin_window("Search", &wnd, flags, true, false);

        focus_keyboard_here(&wnd);

        bool search_again = false;

        auto &fs = editor->file_search;
        auto buf = editor->buf;
        auto tree = buf->search_tree;

        int num_results = tree->get_size();

        ccstr label = NULL;
        if (!num_results)
            label = "No results";
        else if (fs.current_idx == -1)
            label = cp_sprintf("%d result%s", num_results, num_results == 1 ? "" : "s");
        else
            label = cp_sprintf("%d of %d", fs.current_idx + 1, num_results);

        auto text_input_id = "current_file_search_search";

        if (im_input_text_ex_fixbuf(cp_sprintf("Search (%s)", label), text_input_id, wnd.query, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            if (wnd.opened_from_vim) {
                wnd.show = false;
                im::SetWindowFocus(NULL);
            } else {
                im::SetKeyboardFocusHere(-1);
                auto idx = editor->move_file_search_result(!(im_get_keymods() & CP_MOD_SHIFT), 1);
                if (idx != -1) {
                    fs.current_idx = idx;
                    editor->move_cursor(tree->get_node(idx)->pos);
                }
            }
        }

        if (check_cmd_flag(&wnd.cmd_focus_textbox))
            im_select_all_last();

        if (im::IsItemEdited()) {
            search_again = true;
            if (wnd.show)
                cp_strcpy_fixed(wnd.permanent_query, wnd.query);
        }

        if (im::Checkbox("Case-sensitive", &wnd.case_sensitive))
            search_again = true;

        im::SameLine();
        if (im::Checkbox("Regex", &wnd.use_regex))
            search_again = true;

        if (wnd.replace) {
            im_small_newline();

            if (im_input_text_fixbuf("Replace:", wnd.replace_str, ImGuiInputTextFlags_EnterReturnsTrue)) {
                SCOPED_BATCH_CHANGE(editor->buf);

                auto repl_parts = parse_search_replacement(wnd.replace_str);

                for (auto it = tree->get_min(); it; it = tree->successor(it)) {
                    auto chars = new_list(char);
                    auto m = it;

                    if (m->search_result.group_starts || m->search_result.group_ends) {
                        cp_assert(m->search_result.group_starts);
                        cp_assert(m->search_result.group_ends);
                        cp_assert(m->search_result.group_starts->len == m->search_result.group_ends->len);
                    }

                    For (repl_parts) {
                        ccstr newstr = NULL;
                        if (it.dollar) {
                            if (!it.group)
                                newstr = editor->buf->get_text(m->pos, m->search_result.end);
                            else if (it.group-1 < m->search_result.group_starts->len)
                                newstr = editor->buf->get_text(m->search_result.group_starts->at(it.group-1), m->search_result.group_ends->at(it.group-1));
                            else
                                newstr = it.string;
                        } else {
                            newstr = it.string;
                        }

                        for (auto p = newstr; *p; p++) chars->append(*p);
                    }

                    chars->append('\0');
                    auto uchars = cstr_to_ustr(chars->items);
                    editor->buf->apply_edit(m->pos, m->search_result.end, uchars->items, uchars->len);
                }

                wnd.show = false;
            }
            // if (im::IsItemEdited()) { }
        }

        do {
            if (!search_again) break;
            if (!wnd.show) break; // if we closed

            editor->trigger_file_search();

            if (wnd.replace) break; // don't jump while typing if we're replacing

            num_results = buf->search_tree->get_size();
            if (!num_results) break;

            bool in_match = false;
            auto idx = editor->find_current_or_next_match(editor->cur, &in_match);

            auto match = tree->get_node(idx);

            // if we're in match and it's first char, just stay there
            // otherwise, go to next one
            if (in_match) {
                if (match->pos != editor->cur) {
                    idx = (idx+1) % num_results;
                    editor->move_cursor(match->pos);
                } else {
                    // do nothing
                }
            } else {
                // idx already points to the next result
                editor->move_cursor(match->pos);
            }
        } while (0);

        im::End();
        fstlog("wnd_local_search");
    } while (0);

    if (world.wnd_command.show) {
        auto &wnd = world.wnd_command;

        auto begin_window = [&]() {
            im::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
            begin_centered_window("Run Command", &wnd, 0, 400);
            im::PopStyleVar();
        };

        begin_window();

        focus_keyboard_here(&wnd);

        // We need to close this window when defocused, because otherwise the
        // user might move the cursor and fuck this up
        if (!wnd.focused) {
            wnd.show = false;
            im::SetWindowFocus(NULL);
        }

        switch (get_keyboard_nav(&wnd, KNF_ALLOW_IMGUI_FOCUSED)) {
        case KN_DOWN:
            if (!wnd.filtered_results->len) break;
            wnd.selection++;
            wnd.selection %= min(wnd.filtered_results->len, settings.run_command_max_results);
            break;
        case KN_UP:
            if (!wnd.filtered_results->len) break;
            if (!wnd.selection)
                wnd.selection = min(wnd.filtered_results->len, settings.run_command_max_results) - 1;
            else
                wnd.selection--;
            break;
        }

        im::PushItemWidth(-1);
        {
            if (im_input_text_ex_fixbuf(NULL, "run_command", wnd.query, ImGuiInputTextFlags_EnterReturnsTrue)) {
                wnd.show = false;
                im::SetWindowFocus(NULL);

                if (!wnd.query[0]) {
                    if (world.last_manually_run_command != CMD_INVALID)
                        handle_command(world.last_manually_run_command, false);
                } else if (wnd.filtered_results->len > 0) {
                    auto cmd = (Command)wnd.filtered_results->at(wnd.selection);
                    handle_command(cmd, false);
                    world.last_manually_run_command = cmd;
                }
            }
        }
        im::PopItemWidth();

        if (im::IsItemEdited()) {
            wnd.filtered_results->len = 0;
            wnd.selection = 0;

            if (strlen(wnd.query) > 0) {
                For (wnd.actions) {
                    if (fzy_has_match(wnd.query, get_command_name(it))) {
                        wnd.filtered_results->append(it);
                        if (wnd.filtered_results->len > 10000) break;
                    }
                }

                fuzzy_sort_filtered_results(
                    wnd.query,
                    wnd.filtered_results,
                    _CMD_COUNT_,
                    [&](int i) { return get_command_name((Command)i); }
                );
            }
        }

        auto render_command_in_menu = [&](auto pm, Command it, bool selected) {
            pretty_menu_item(pm, selected);

            pretty_menu_text(pm, get_command_name(it));

            auto keystr = get_menu_command_key(it);
            if (!keystr) return;

            // im_push_mono_font();
            // defer { im_pop_font(); };
            pm->pos.x = pm->text_br.x - im::CalcTextSize(keystr).x;
            pretty_menu_text(
                pm,
                keystr,
                selected ? IM_COL32(150, 150, 150, 255) : IM_COL32(110, 110, 110, 255)
            );
        };

        // if empty query, show last command
        if (!wnd.query[0]) {
            if (world.last_manually_run_command != CMD_INVALID) {
                im::Text("Last command");

                auto pm = pretty_menu_start();
                render_command_in_menu(pm, world.last_manually_run_command, true);
            }
        }

        if (wnd.filtered_results->len > 0) {
            im_small_newline();

            im::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            defer { im::PopStyleVar(); };

            auto pm = pretty_menu_start();
            for (u32 i = 0; i < wnd.filtered_results->len && i < settings.run_command_max_results; i++) {
                auto it = (Command)wnd.filtered_results->at(i);
                render_command_in_menu(pm, it, i == wnd.selection);
            }
        }

        im::End();
        fstlog("wnd_command");
    }

    if (world.wnd_goto_symbol.show) {
        auto &wnd = world.wnd_goto_symbol;

        switch (wnd.state) {
        case GOTO_SYMBOL_RUNNING:
            if (current_time_milli() - wnd.fill_time_started_ms > 100) {
                begin_centered_window("Go To Symbol###goto_symbol_filling", &wnd, 0, 650);
                im::Text("Loading symbols...");
                im::End();
            }
            break;

        case GOTO_SYMBOL_ERROR:
            begin_centered_window("Go To Symbol###goto_symbol_filling", &wnd, 0, 650);
            im::Text("Unable to load list of symbols.");
            im::End();
            break;

        case GOTO_SYMBOL_WAITING:
            begin_centered_window("Go To Symbol###goto_symbol_filling", &wnd, 0, 650);
            im::Text("Waiting for indexer to become ready.");
            im::End();
            if (world.indexer.status == IND_READY)
                init_goto_symbol();
            break;

        case GOTO_SYMBOL_READY: {
            begin_centered_window("Go To Symbol###goto_symbol_ready", &wnd, 0, 650);

            bool refilter = false;

            im::Checkbox("Include symbols in current file only", &wnd.current_file_only);
            if (im::IsItemEdited())
                refilter = true;

            im_small_newline();

            if (!wnd.show) {
                wnd.filtered_results->len = 0;
                wnd.selection = 0;
            }

            switch (get_keyboard_nav(&wnd, KNF_ALLOW_IMGUI_FOCUSED)) {
            case KN_UP:
                if (!wnd.filtered_results->len) break;
                if (!wnd.selection)
                    wnd.selection = min(wnd.filtered_results->len, settings.goto_symbol_max_results) - 1;
                else
                    wnd.selection--;
                break;
            case KN_DOWN:
                if (!wnd.filtered_results->len) break;
                wnd.selection++;
                wnd.selection %= min(wnd.filtered_results->len, settings.goto_symbol_max_results);
                break;
            }

            focus_keyboard_here(&wnd);

            if (!wnd.focused) {
                wnd.show = false;
                im::SetWindowFocus(NULL);
            }

            if (im_input_text_fixbuf("Search for symbol:", wnd.query, ImGuiInputTextFlags_EnterReturnsTrue)) {
                wnd.show = false;
                im::SetWindowFocus(NULL);

                do {
                    if (!wnd.filtered_results->len) break;

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

            if (im::IsItemEdited())
                refilter = true;

            do {
                if (!refilter) continue;

                wnd.selection = 0;
                wnd.filtered_results->len = 0;

                if (strlen(wnd.query) < 2) break;

                Editor *editor = NULL;
                if (wnd.current_file_only) {
                    editor = get_current_editor();
                    if (!editor)
                        break;
                }

                Timer t;
                t.init("filter_symbols");

                for (u32 i = 0, k = 0; i < wnd.symbols->len && k < 10000; i++) {
                    auto &it = wnd.symbols->at(i);

                    if (wnd.current_file_only)
                        if (!are_filepaths_equal(it.filepath, editor->filepath))
                            continue;

                    if (!fzy_has_match(wnd.query, it.full_name()))
                        continue;

                    wnd.filtered_results->append(i);
                    k++;
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
                im_small_newline();

                im_push_mono_font();
                defer { im_pop_font(); };

                auto pm = pretty_menu_start();

                for (u32 i = 0; i < wnd.filtered_results->len && i < settings.goto_file_max_results; i++) {
                    pretty_menu_item(pm, i == wnd.selection);

                    auto it = wnd.symbols->at(wnd.filtered_results->at(i));

                    auto get_decl_type = [&]() {
                        auto decl_type = it.decl->decl->type;
                        switch (decl_type) {
                        case GODECL_IMPORT: return "import";
                        case GODECL_VAR: return "var";
                        case GODECL_SHORTVAR: return "var";
                        case GODECL_CONST: return "const";
                        case GODECL_TYPE: return "type";
                        case GODECL_FUNC: return "func";
                        }
                        return "unknown";
                    };

                    pretty_menu_text(pm, cp_sprintf("(%s) ", get_decl_type()), IM_COL32(80, 80, 80, 255));

                    pretty_menu_text(pm, it.full_name());
                    pm->pos.x += 8;

                    auto label = get_import_path_label(it.decl->ctx->import_path, wnd.workspace);

                    int rem_chars = (pm->text_br.x - pm->pos.x) / base_font->width;

                    auto s = label;
                    if (strlen(s) > rem_chars)
                        s = cp_sprintf("%.*s...", rem_chars - 3, s);

                    auto color = i == wnd.selection
                        ? IM_COL32(150, 150, 150, 255)
                        : IM_COL32(110, 110, 110, 255);

                    pretty_menu_text(pm, s, color);
                }
            }

            im::End();
            fstlog("wnd_goto_symbol");
        }
        }
    }

    // Don't show the debugger UI if we're still starting up, because we're
    // still building, and the build could fail.  Might regret this, can always
    // change later.
    if (world.dbg.mt_state.state_flag != DLV_STATE_INACTIVE && world.dbg.mt_state.state_flag != DLV_STATE_STARTING) {
        draw_debugger();
        fstlog("debugger");
    }

    if (world.wnd_search_and_replace.show) {
        auto& wnd = world.wnd_search_and_replace;

        im::SetNextWindowDockID(dock_sidebar_id, ImGuiCond_Once);

        auto title = cp_sprintf("%s###search_and_replace", wnd.replace ? "Search and Replace" : "Search");
        begin_window(title, &wnd, ImGuiWindowFlags_AlwaysAutoResize, false, true);

        bool search_again = false;
        bool search_box_focused = false;
        bool replace_box_focused = false;

        im_push_mono_font();
        {
            bool focus_textbox = im::IsWindowAppearing() || check_cmd_flag(&wnd.cmd_focus_textbox);

            if (focus_textbox) im::SetScrollHereY();

            if (im_input_text_fixbuf("Search for", wnd.find_str, ImGuiInputTextFlags_EnterReturnsTrue)) {
                search_again = true;

                // don't defocus or select text when pressing enter
                im::SetKeyboardFocusHere(-1);
                im_select_clear_last();
            }

            search_box_focused = im::IsItemFocused();

            if (focus_textbox)
                im_select_all_last();

            if (im::IsItemEdited())
                if (strlen(wnd.find_str) >= 3)
                    search_again = true;

            if (wnd.replace) {
                im_input_text_fixbuf("Replace with", wnd.replace_str, ImGuiInputTextFlags_EnterReturnsTrue);
                replace_box_focused = im::IsItemFocused();

                if (check_cmd_flag(&wnd.cmd_focus_replace_textbox))
                    im_select_all_last();
            }
        }
        im_pop_font();

        im_small_newline();

        if (im::Checkbox("Case sensitive", &wnd.case_sensitive))
            search_again = true;

        im::SameLine();
        if (im::Checkbox("Regex", &wnd.use_regex))
            search_again = true;

        im::SameLine();
        if (im::Checkbox("Go files only", &wnd.search_go_files_only))
            search_again = true;

        im_small_newline();

        if (search_again) {
            wnd.files_open = NULL;
            wnd.sel_file = -1;
            wnd.sel_result = -1;
            wnd.scroll_file = -1;
            wnd.scroll_result = -1;

            Searcher_Opts opts; ptr0(&opts);
            opts.case_sensitive = wnd.case_sensitive;
            opts.literal = !wnd.use_regex;
            opts.query = wnd.find_str;
            opts.search_go_files_only = wnd.search_go_files_only;
            world.searcher.start_search(&opts);
        }

        switch (world.searcher.mt_state.type) {
        case SEARCH_SEARCH_IN_PROGRESS:
            // wait 100ms before showing "searching..." to avoid flicker
            if (current_time_milli() - world.searcher.mt_state.start_time_milli > 100) {
                im::Text("Searching...");
                im::SameLine();
                if (im::Button("Cancel")) {
                    world.searcher.cancel();
                }
            }
            break;
        case SEARCH_SEARCH_DONE: {
            if (!wnd.files_open) {
                wnd.mem.cleanup();
                wnd.mem.init("search_wnd");

                auto len = world.searcher.mt_state.results->len;
                SCOPED_MEM(&wnd.mem);
                wnd.files_open = new_array(bool, len);
                wnd.set_file_open = new_array(bool, len);
                wnd.set_file_close = new_array(bool, len);
            }

            if (wnd.replace)
                if (im::Button("Perform Replacement"))
                    // TODO: if we have more results than we're showing, warn user about that
                    world.searcher.start_replace(wnd.replace_str);

            int index = 0;
            auto search_results = world.searcher.mt_state.results;
            int num_files = search_results->len;

            Fori (search_results) {
                auto file_idx = i;

                if (index >= 1000) {
                    num_files = file_idx;
                    break;
                }

                bool open = false;

                {
                    im::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(ImColor(60, 60, 60)));
                    defer { im::PopStyleColor(); };

                    auto flags = ImGuiTreeNodeFlags_DefaultOpen
                        | ImGuiTreeNodeFlags_SpanAvailWidth
                        | ImGuiTreeNodeFlags_NoTreePushOnOpen;

                    if (wnd.set_file_open[file_idx]) {
                        im::SetNextItemOpen(true);
                        wnd.set_file_open[file_idx] = false;
                    } else if (wnd.set_file_close[file_idx]) {
                        im::SetNextItemOpen(false);
                        wnd.set_file_close[file_idx] = false;
                    }

                    if (wnd.sel_file == file_idx && wnd.sel_result == -1)
                        flags |= ImGuiTreeNodeFlags_Selected;

                    if (wnd.scroll_file == file_idx && wnd.scroll_result == -1) {
                        keep_item_inside_scroll();
                        wnd.scroll_file = -1;
                        wnd.scroll_result = -1;
                    }

                    open = im::TreeNodeEx(cp_sprintf("%s", get_path_relative_to(it.filepath, world.current_path)), flags);
                }

                wnd.files_open[file_idx] = open;

                if (open) {
                    im::Indent();
                    im_push_mono_font();

                    auto filepath = it.filepath;

                    Fori (it.results) {
                        auto result_idx = i;

                        if (index >= 1000) break;
                        defer { index++; };

                        auto availwidth = im::GetContentRegionAvail().x;
                        auto text_size = ImVec2(availwidth, im::CalcTextSize("blah").y);
                        auto drawpos = im::GetCursorScreenPos();

                        im::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(ImColor(60, 60, 60)));

                        bool clicked = im::Selectable(
                            cp_sprintf("##search_result_%d", index),
                            file_idx == wnd.sel_file && result_idx == wnd.sel_result,
                            ImGuiSelectableFlags_AllowDoubleClick,
                            text_size
                        );

                        im::PopStyleColor();

                        if (wnd.scroll_file == file_idx && wnd.scroll_result == result_idx) {
                            keep_item_inside_scroll();
                            wnd.scroll_file = -1;
                            wnd.scroll_result = -1;
                        }

                        // may need to compress...
                        struct Draw_Opts {
                            vec4f color;
                            bool bg;
                            vec4f bgcolor;
                            bool strike;
                        };

                        auto draw_text = [&](ccstr s, int len, Draw_Opts *opts) {
                            auto text = cp_sprintf("%.*s", len, s);
                            auto size = im::CalcTextSize(text);

                            auto drawlist = im::GetWindowDrawList();
                            if (opts->bg) {
                                auto bgcolor = im::GetColorU32(to_imcolor(opts->bgcolor));
                                drawlist->AddRectFilled(drawpos, drawpos+size, bgcolor);
                            }

                            auto imcolor = im::GetColorU32(to_imcolor(opts->color));
                            drawlist->AddText(drawpos, imcolor, text);

                            if (opts->strike) {
                                ImVec2 a = drawpos, b = drawpos;
                                b.y += size.y/2;
                                a.y += size.y/2;
                                b.x += size.x;
                                // im::GetFontSize() / 2
                                drawlist->AddLine(a, b, imcolor, 1.0f);
                            }

                            drawpos.x += size.x;
                        };

                        // draw match position
                        {
                            auto pos = get_search_mark_pos(filepath, result_idx, true);
                            if (pos == NULL_CUR)
                                pos = it.match_start;

                            auto s = cp_sprintf("%d:%d ", pos.y+1, pos.x+1);

                            Draw_Opts opts; ptr0(&opts);
                            opts.color = rgba("#c8b2b2");
                            draw_text(s, strlen(s), &opts);
                        }

                        auto default_text_color = rgba("#b2b2b2");

                        Draw_Opts opts; ptr0(&opts);
                        opts.color = default_text_color;
                        draw_text(it.preview, it.match_offset_in_preview, &opts);

                        if (wnd.replace) {
                            // draw old
                            Draw_Opts opts; ptr0(&opts);
                            opts.color = rgba("#ffb4b4");
                            opts.strike = true;
                            draw_text(it.match, it.match_len, &opts);

                            // draw new
                            auto newtext = world.searcher.get_replacement_text(&it, wnd.replace_str);
                            ptr0(&opts);
                            opts.color = rgba("b4ffb4");
                            draw_text(newtext, strlen(newtext), &opts);
                        } else {
                            Draw_Opts opts; ptr0(&opts);
                            opts.color = rgba("#ffffff");
                            opts.bg = true;
                            opts.bgcolor = rgba("#ffffff", 0.2);
                            draw_text(it.match, it.match_len, &opts);
                        }

                        ptr0(&opts);
                        opts.color = default_text_color;
                        draw_text(
                            &it.preview[it.match_offset_in_preview + it.match_len],
                            it.preview_len - it.match_offset_in_preview - it.match_len,
                            &opts
                        );

                        if (clicked) {
                            if (im::IsMouseDoubleClicked(0)) {
                                auto pos = get_search_mark_pos(filepath, result_idx, true);
                                if (pos == NULL_CUR)
                                    pos = it.match_start;
                                goto_file_and_pos(filepath, pos, true);
                            } else {
                                wnd.sel_file = file_idx;
                                wnd.sel_result = result_idx;
                            }
                        }
                    }

                    im_pop_font();
                    im::Unindent();

                    // im::TreePop();
                }
            }

            auto goto_result = [&](int file, int result) {
                wnd.sel_file = file;
                wnd.sel_result = result;
                wnd.scroll_file = file;
                wnd.scroll_result = result;
            };

            auto handle_up = [&]() {
                if (wnd.sel_file == -1) {
                    if (!world.ui.input_captured_by_imgui)
                        goto_result(0, -1);
                    return;
                }

                if (wnd.sel_result >= 0 && wnd.files_open[wnd.sel_file]) {
                    goto_result(wnd.sel_file, wnd.sel_result-1);
                    return;
                }

                if (!wnd.sel_file) {
                    if (wnd.replace)
                        wnd.cmd_focus_replace_textbox = true;
                    else
                        wnd.cmd_focus_textbox = true;
                    return;
                }

                int file_idx = wnd.sel_file-1;
                if (wnd.files_open[file_idx])
                    goto_result(file_idx, search_results->at(file_idx).results->len - 1);
                else
                    goto_result(file_idx, -1);
            };

            auto handle_down = [&]() {
                if (wnd.sel_file == -1) {
                    goto_result(0, -1);
                    return;
                }

                if (wnd.files_open[wnd.sel_file]) {
                    auto &file = search_results->at(wnd.sel_file);
                    if ((int)wnd.sel_result < (int)file.results->len-1) {
                        goto_result(wnd.sel_file, wnd.sel_result+1);
                        return;
                    }
                }

                if (wnd.sel_file + 1 >= search_results->len) return;

                goto_result(wnd.sel_file+1, -1);
            };

            if (world.ui.input_captured_by_imgui) {
                if (!old_input_captured_by_imgui) {
                    wnd.sel_file = -1;
                    wnd.sel_result = -1;
                }

                switch (get_keyboard_nav(&wnd, KNF_ALLOW_IMGUI_FOCUSED)) {
                case KN_UP:
                    if (search_box_focused)
                        break;
                    wnd.cmd_focus_textbox = true;
                    // handle_up();
                    break;
                case KN_DOWN:
                    if (wnd.replace && search_box_focused) {
                        wnd.cmd_focus_replace_textbox = true;
                    } else {
                        // defocus everything
                        im::ClearActiveID();
                        handle_down();
                    }
                    break;
                }
            } else {
                switch (get_keyboard_nav(&wnd, 0)) {
                case KN_LEFT:
                    if (wnd.sel_file == -1) break;
                    wnd.set_file_close[wnd.sel_file] = true;
                    goto_result(wnd.sel_file, -1);
                    break;
                case KN_RIGHT:
                    if (wnd.sel_file == -1) break;
                    wnd.set_file_open[wnd.sel_file] = true;
                    goto_result(wnd.sel_file, -1);
                    break;
                case KN_DOWN:
                    handle_down();
                    break;
                case KN_UP:
                    handle_up();
                    break;
                case KN_ENTER: {
                    if (!(0 <= wnd.sel_file && wnd.sel_file < search_results->len)) break;

                    if (wnd.sel_result == -1) {
                        if (wnd.files_open[wnd.sel_file])
                            wnd.set_file_close[wnd.sel_file] = true;
                        else
                            wnd.set_file_open[wnd.sel_file] = true;
                        goto_result(wnd.sel_file, -1);
                    } else {
                        auto &file = search_results->at(wnd.sel_file);
                        if (!(0 <= wnd.sel_result && wnd.sel_result < file.results->len)) break;

                        auto &result = file.results->at(wnd.sel_result);
                        goto_file_and_pos(file.filepath, result.match_start, true);
                    }
                    break;
                }
                }
            }

            if (num_files < search_results->len) im::Text("There were too many results; some are omitted.");
            break;
        }
        case SEARCH_REPLACE_IN_PROGRESS:
            im::Text("Replacing...");
            im::SameLine();
            if (im::Button("Cancel")) {
                // TODO
            }
            break;
        case SEARCH_REPLACE_DONE:
            im::Text("Done!"); // TODO: show # files replaced, i guess, and undo button
            break;
        case SEARCH_NOTHING_HAPPENING:
            break;
        }

        im::End();
        fstlog("wnd_search_and_replace");
    }

    if (world.wnd_style_editor.show) {
        begin_window("Style Editor", &world.wnd_style_editor);

        if (im::BeginTabBar("style_editor_tabbar", 0)) {
            if (im::BeginTabItem("Margins & Padding", NULL, 0)) {
                im::InputInt("status_padding_x", &settings.status_padding_x);
                im::InputInt("status_padding_y", &settings.status_padding_y);
                im::InputInt("line_number_margin_left", &settings.line_number_margin_left);
                im::InputInt("line_number_margin_right", &settings.line_number_margin_right);
                im::InputInt("autocomplete_menu_padding", &settings.autocomplete_menu_padding);
                im::InputInt("autocomplete_item_padding_x", &settings.autocomplete_item_padding_x);
                im::InputInt("autocomplete_item_padding_y", &settings.autocomplete_item_padding_y);
                im::InputInt("tabs_offset", &settings.tabs_offset);
                im::InputInt("parameter_hint_margin_y", &settings.parameter_hint_margin_y);
                im::InputInt("parameter_hint_padding_x", &settings.parameter_hint_padding_x);
                im::InputInt("parameter_hint_padding_y", &settings.parameter_hint_padding_y);
                im::InputInt("editor_margin_x", &settings.editor_margin_x);
                im::InputInt("editor_margin_y", &settings.editor_margin_y);
                im::InputFloat("line_height", &settings.line_height, 0.01, 0.1, "%.3f");
                im::InputInt("goto_file_max_results", &settings.goto_file_max_results);
                im::InputInt("goto_symbol_max_results", &settings.goto_symbol_max_results);
                im::InputInt("generate_implementation_max_results", &settings.generate_implementation_max_results);
                im::InputInt("run_command_max_results", &settings.run_command_max_results);
                im::EndTabItem();
            }

            if (im::BeginTabItem("Colors", NULL, 0)) {
                im::ColorEdit3("autocomplete_background", (float*)&global_colors.autocomplete_background);
                im::ColorEdit3("autocomplete_border", (float*)&global_colors.autocomplete_border);
                im::ColorEdit3("autocomplete_selection", (float*)&global_colors.autocomplete_selection);
                im::ColorEdit3("background", (float*)&global_colors.background);
                im::ColorEdit3("breakpoint_active", (float*)&global_colors.breakpoint_active);
                im::ColorEdit3("breakpoint_current", (float*)&global_colors.breakpoint_current);
                im::ColorEdit3("breakpoint_inactive", (float*)&global_colors.breakpoint_inactive);
                im::ColorEdit3("breakpoint_other", (float*)&global_colors.breakpoint_other);
                im::ColorEdit3("builtin", (float*)&global_colors.builtin);
                im::ColorEdit3("comment", (float*)&global_colors.comment);
                im::ColorEdit3("cursor", (float*)&global_colors.cursor);
                im::ColorEdit3("cursor_foreground", (float*)&global_colors.cursor_foreground);
                im::ColorEdit3("foreground", (float*)&global_colors.foreground);
                im::ColorEdit3("green", (float*)&global_colors.green);
                im::ColorEdit3("keyword", (float*)&global_colors.keyword);
                im::ColorEdit3("muted", (float*)&global_colors.muted);
                im::ColorEdit3("number_literal", (float*)&global_colors.number_literal);
                im::ColorEdit3("pane_active", (float*)&global_colors.pane_active);
                im::ColorEdit3("pane_inactive", (float*)&global_colors.pane_inactive);
                im::ColorEdit3("pane_resizer", (float*)&global_colors.pane_resizer);
                im::ColorEdit3("pane_resizer_hover", (float*)&global_colors.pane_resizer_hover);
                im::ColorEdit3("punctuation", (float*)&global_colors.punctuation);
                im::ColorEdit3("search_background", (float*)&global_colors.search_background);
                im::ColorEdit3("search_foreground", (float*)&global_colors.search_foreground);
                im::ColorEdit3("string_literal", (float*)&global_colors.string_literal);
                // im::ColorEdit3("tab", (float*)&global_colors.tab);
                // im::ColorEdit3("tab_hovered", (float*)&global_colors.tab_hovered);
                im::ColorEdit3("tab_selected", (float*)&global_colors.tab_selected);
                im::ColorEdit3("type", (float*)&global_colors.type);
                im::ColorEdit3("visual_background", (float*)&global_colors.visual_background);
                im::ColorEdit3("visual_foreground", (float*)&global_colors.visual_foreground);
                im::ColorEdit3("visual_highlight", (float*)&global_colors.visual_highlight);
                im::ColorEdit3("white", (float*)&global_colors.white);
                im::ColorEdit3("white_muted", (float*)&global_colors.white_muted);

                im::ColorEdit3("status_area_background", (float*)&global_colors.status_area_background);
                im::ColorEdit3("command_background", (float*)&global_colors.command_background);
                im::ColorEdit3("command_foreground", (float*)&global_colors.command_foreground);
                im::ColorEdit3("status_mode_background", (float*)&global_colors.status_mode_background);
                im::ColorEdit3("status_mode_foreground", (float*)&global_colors.status_mode_foreground);
                im::ColorEdit3("status_debugger_paused_background", (float*)&global_colors.status_debugger_paused_background);
                im::ColorEdit3("status_debugger_starting_background", (float*)&global_colors.status_debugger_starting_background);
                im::ColorEdit3("status_debugger_running_background", (float*)&global_colors.status_debugger_running_background);
                im::ColorEdit3("status_index_ready_background", (float*)&global_colors.status_index_ready_background);
                im::ColorEdit3("status_index_ready_foreground", (float*)&global_colors.status_index_ready_foreground);
                im::ColorEdit3("status_index_indexing_background", (float*)&global_colors.status_index_indexing_background);
                im::ColorEdit3("status_index_indexing_foreground", (float*)&global_colors.status_index_indexing_foreground);

                im::ColorEdit4("preview_background", (float*)&global_colors.preview_background);
                im::ColorEdit4("preview_border", (float*)&global_colors.preview_border);
                im::ColorEdit4("preview_foreground", (float*)&global_colors.preview_foreground);

                if (im::Button("Save to disk")) {
                    File f;
                    auto filepath = path_join(cp_dirname(cp_dirname(cp_dirname(get_executable_path()))), ".cpcolors");
                    print("%s", filepath);
                    f.init_write(filepath); // "/Users/bh/brandon/.cpcolors"
                    f.write((char*)&global_colors, sizeof(global_colors));
                    f.cleanup();
                }

                im::EndTabItem();
            }

            im::EndTabBar();
        }

        im::End();
        fstlog("wnd_style_editor");
    }

#ifdef DEBUG_BUILD
    if (world.wnd_history.show) {
        im::SetNextWindowDockID(dock_sidebar_id, ImGuiCond_Once);

        begin_window("History", &world.wnd_history, ImGuiWindowFlags_AlwaysAutoResize, false, true);

        bool handled = false;
        do {
            auto editor = get_current_editor();
            if (!editor) break;
            if (!editor->buf->use_history) break;

            handled = true;

            auto buf = editor->buf;
            for (auto i = buf->hist_start; i != buf->hist_top; i = buf->hist_inc(i)) {
                auto change = buf->history[i];
                im::Text("### %d%s", i, i == buf->hist_curr ? " (*)" : "");

                for (auto it = change; it; it = it->next) {
                    im::BulletText(
                        "start = %s, oldend = %s, newend = %s, oldlen = %d, newlen = %d",
                        it->start.str(),
                        it->old_end.str(),
                        it->new_end.str(),
                        (u32)it->old_text.len,
                        (u32)it->new_text.len
                    );
                }
            }
        } while (0);

        if (!handled)
            im::Text("no history to show");

        im::End();
        fstlog("wnd_history");
    }
#endif

    do {
        auto &wnd = world.wnd_tree_viewer;
        if (!wnd.show) break;

        begin_window("Tree Viewer", &wnd);
        defer { im::End(); };

        auto editor = get_current_editor();
        if (!editor) {
            im::Text("No editor open.");
            break;
        }

        auto tree = editor->buf->tree;
        if (!tree) {
            im::Text("Editor has no tree.");
            break;
        }

        if (im::BeginTabBar("wnd_tree_viewer", 0)) {
            defer { im::EndTabBar(); };

            if (im::BeginTabItem("AST", NULL)) {
                defer {im::EndTabItem(); };

                im::Checkbox("show anon?", &wnd.show_anon_nodes);
                im::SameLine();
                im::Checkbox("show comments?", &wnd.show_comments);
                im::SameLine();

                cur2 open_cur = NULL_CUR;
                if (im::Button("go to cursor"))
                    open_cur = editor->cur;

                ts_tree_cursor_reset(&editor->buf->cursor, ts_tree_root_node(tree));
                render_ts_cursor(&editor->buf->cursor, editor->lang, open_cur);
            }

            if (im::BeginTabItem("Gofile", NULL)) {
                defer {im::EndTabItem(); };

                if (editor->buf->lang == LANG_GO) {
                    if (im::Button("Get current file")) {
                        do {
                            auto editor = get_current_editor();
                            if (!editor) break;

                            auto &ind = world.indexer;
                            if (!ind.try_acquire_lock(IND_READING)) break;
                            defer { ind.release_lock(IND_READING); };

                            if (wnd.gofile) {
                                wnd.gofile->cleanup();
                                wnd.gofile = NULL;
                            }

                            ind.reload_all_editors(true);

                            auto ctx = ind.filepath_to_ctx(editor->filepath);
                            auto gofile = ind.find_gofile_from_ctx(ctx);
                            if (!gofile) break;

                            wnd.pool.cleanup();
                            wnd.pool.init("tree_viewer");

                            {
                                SCOPED_MEM(&wnd.pool);
                                wnd.gofile = gofile->copy();
                                wnd.filepath = cp_strdup(editor->filepath);
                            }
                        } while (0);
                    }

                    auto gofile = wnd.gofile;
                    if (gofile) {
                        if (im::BeginTabBar("wnd_gofile_viewer_tab_bar", 0)) {
                            defer { im::EndTabBar(); };

                            if (im::BeginTabItem("Scope Ops", NULL)) {
                                defer { im::EndTabItem(); };

                                int open_scopes = 0;

                                int i = 0;
                                while (i < gofile->scope_ops->len) {
                                    auto &it = gofile->scope_ops->at(i++);
                                    auto pos = NULL_CUR;

                                    switch (it.type) {
                                    case GSOP_OPEN_SCOPE: {
                                        auto flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
                                        bool open = im::TreeNodeEx(&it, flags, "scope open at %s", it.pos.str());

                                        if (im::IsItemClicked())
                                            pos = it.pos;

                                        if (open) {
                                            open_scopes++;
                                            break;
                                        }

                                        int depth = 1;
                                        while (i < gofile->scope_ops->len) {
                                            auto &it2 = gofile->scope_ops->at(i++);
                                            if (it2.type == GSOP_OPEN_SCOPE) {
                                                depth++;
                                            } else if (it2.type == GSOP_CLOSE_SCOPE) {
                                                depth--;
                                                if (!depth)
                                                    break;
                                            }
                                        }

                                        open_scopes += depth;
                                        break;
                                    }
                                    case GSOP_DECL: {
                                        render_godecl(it.decl);
                                        break;
                                    }
                                    case GSOP_CLOSE_SCOPE: {
                                        auto flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                                        im::TreeNodeEx(&it, flags, "scope close at %s", it.pos.str());

                                        if (im::IsItemClicked())
                                            pos = it.pos;

                                        open_scopes--;
                                        im::TreePop();
                                        break;
                                    }
                                    }

                                    if (pos.x != -1 && pos.y != -1)
                                        goto_file_and_pos(wnd.filepath, pos, true);
                                }

                                if (open_scopes) {
                                    for (int i = 0; i < open_scopes; i++)
                                        im::TreePop();
                                    im::Text("%d unclosed scpoes", open_scopes);
                                }
                            }

                            if (im::BeginTabItem("Decls", NULL)) {
                                defer { im::EndTabItem(); };

                                For (gofile->decls) render_godecl(&it);
                            }

                            if (im::BeginTabItem("Imports", NULL)) {
                                defer { im::EndTabItem(); };

                                For (gofile->imports) {
                                    im::Text(
                                        "%s %s %s %s",
                                        it.package_name,
                                        go_package_name_type_str(it.package_name_type),
                                        it.import_path,
                                        it.decl->decl_start.str()
                                    );
                                }
                            }

                            if (im::BeginTabItem("References", NULL)) {
                                defer { im::EndTabItem(); };

                                For (gofile->references) {
                                    if (it.is_sel) {
                                        auto flags = ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
                                        bool is_open = im::TreeNodeEx(&it, flags, "<selector> at %s", it.x_start.str());

                                        if (im::IsItemClicked())
                                            goto_file_and_pos(wnd.filepath, it.x_start, true);

                                        if (is_open) {
                                            render_gotype(it.x);

                                            auto flags = ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                                            im::TreeNodeEx(it.sel, flags);
                                            if (im::IsItemClicked())
                                                goto_file_and_pos(wnd.filepath, it.sel_start, true);

                                            im::TreePop();
                                        }
                                    } else {
                                        auto flags = ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                                        im::TreeNodeEx(it.name, flags, "%s at %s", it.name, it.start.str());
                                        if (im::IsItemClicked())
                                            goto_file_and_pos(wnd.filepath, it.start, true);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    } while (0);

    if (world.flag_defocus_imgui) {
        im::SetWindowFocus(NULL);
        world.flag_defocus_imgui = false;
    }

    {
        // prepare opengl for drawing shit
        glViewport(0, 0, world.frame_size.x, world.frame_size.y);
        glUseProgram(world.ui.program);
        glBindVertexArray(world.ui.vao); // bind my vertex array & buffers
        glBindBuffer(GL_ARRAY_BUFFER, world.ui.vbo);
        verts.init(LIST_FIXED, 6 * 128, new_array(Vert, 6 * 128));
    }

    boxf status_area = get_status_area();

    boxf pane_area;
    pane_area.pos = panes_area.pos;

    int editor_index = 0;

    For (&actual_cursor_positions) {
        it.x = -1;
        it.y = -1;
    }
    actual_parameter_hint_start.x = -1;
    actual_parameter_hint_start.y = -1;

    if (world.wnd_mouse_pos.show) {
        // always show this
        begin_window("Mouse Pos", &world.wnd_mouse_pos);
        im::End();
    }

    fstlog("blah blah");

    enum Close_Action {
        CLOSE_TAB,
        CLOSE_OTHERS_TO_LEFT,
        CLOSE_OTHERS_TO_RIGHT,
        CLOSE_OTHERS_IN_PANE,
        CLOSE_OTHERS_IN_WINDOW,
        CLOSE_ALL,
    };

    int tab_to_close = -1;
    Close_Action tab_close_action;

    // Draw panes.
    draw_rect(panes_area, rgba(global_colors.background));
    for (u32 current_pane = 0; current_pane < world.panes.len; current_pane++) {
        auto &pane = world.panes[current_pane];
        auto is_pane_selected = (current_pane == world.current_pane);

        pane_area.w = pane.width;
        pane_area.h = panes_area.h;

        auto areas = get_pane_areas(&pane_area, pane.editors.len > 0);
        boxf tabs_area = areas->tabs_area;
        boxf editor_area = areas->editor_area;
        boxf scrollbar_area = areas->scrollbar_area;
        boxf preview_area = areas->preview_area;

        For (&pane.editors) {
            it.ui_rect = editor_area;
            it.ui_rect_set = true;
        }

        if (pane.editors.len) {
            draw_rect(tabs_area, rgba(is_pane_selected ? global_colors.pane_active : global_colors.pane_inactive));
            auto editor = pane.get_current_editor();

            cur2 saved_pos = NULL_CUR;

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
                area.x += ui.base_font->width * get_line_number_width(editor);

                auto im_pos = im::GetIO().MousePos;
                if (im_pos.x < 0 || im_pos.y < 0)
                    return NULL_CUR;

                auto pos = new_vec2f(im_pos.x, im_pos.y);
                if (pos.y < area.y || pos.y >= area.y + area.h) return NULL_CUR;

                pos.y -= area.y;
                auto y = view.y + pos.y / (ui.base_font->height * settings.line_height);
                if (y > view.y + view.h) return NULL_CUR;

                if (y >= buf->lines.len)
                    return buf->end_pos();

                if (pos.x < area.x)           return new_cur2(0, y);
                if (pos.x >= area.x + area.w) return new_cur2(buf->lines[y].len, y);

                auto vx = (int)((pos.x - area.x) / ui.base_font->width);

                auto ret = new_cur2(buf->idx_vcp_to_cp(y, vx), y);

                // in normal mode, don't allow to go past end of line
                if (world.vim.on && world.vim_mode() == VI_NORMAL)
                    if (ret.x == buf->lines[ret.y].len && ret.x > 0)
                        ret = buf->dec_gr(ret);

                return ret;
            };

            auto calculate_pos_from_mouse = [&]() -> cur2 {
                if (saved_pos.x != -1) return saved_pos;
                saved_pos = actually_calculate_pos_from_mouse();
                return saved_pos;
            };

            boxf editor_area_considering_pane_resizers = editor_area;
            editor_area_considering_pane_resizers.x += PANE_RESIZER_WIDTH / 2;
            editor_area_considering_pane_resizers.w -= PANE_RESIZER_WIDTH;

            bool is_dragging = false;

            // handle dragging text above/below the area
            do {
                if (!editor->mouse_selecting) break;

                auto im_pos = im::GetIO().MousePos;
                auto pos = new_vec2f(im_pos.x, im_pos.y);
                auto area = editor_area;

                if (pos.y >= area.y && pos.y <= area.y + area.h) break;

                is_dragging = true;

                auto now = current_time_milli();
                if (editor->mouse_drag_last_time_ms == -1)
                    editor->mouse_drag_last_time_ms = now;
                i64 delta = now - editor->mouse_drag_last_time_ms;
                if (pos.y < area.y) delta = -delta;

                editor->mouse_drag_accum += delta;
                editor->mouse_drag_last_time_ms = now;

                cur2 cur = editor->cur;
                cur2 old_cur = cur;

                auto period = 30; // how often to move a line, in milliseconds

                // for every 30 pixels, reduce 1 millisecond
                if (pos.y < area.y)          period -= min(28, (area.y - pos.y) / 10);
                if (pos.y > area.y + area.h) period -= min(28, (pos.y - (area.y + area.h)) / 10);

                while (editor->mouse_drag_accum < -period) {
                    if (cur.y)
                        cur.y--;
                    else
                        cur.x = 0;
                    editor->mouse_drag_accum += period;
                }

                while (editor->mouse_drag_accum > period) {
                    int last = editor->buf->lines.len-1;
                    if (cur.y < last)
                        cur.y++;
                    else
                        cur.x = editor->buf->lines[last].len;
                    editor->mouse_drag_accum -= period;
                }

                if (cur != old_cur) editor->move_cursor(cur);
            } while (0);

            if (!is_dragging) {
                editor->mouse_drag_last_time_ms = -1;
                editor->mouse_drag_accum = 0;
            }

            auto is_hovered = test_hover(editor_area_considering_pane_resizers, HOVERID_EDITORS + current_pane, ImGuiMouseCursor_TextInput);
            auto &pressed = world.ui.mouse_just_pressed;

            if (pressed[0] || pressed[1] || pressed[2]) {
                do {
                    if (!is_hovered) break;

                    focus_editor_by_id(editor->id, NULL_CUR);

                    auto pos = calculate_pos_from_mouse();
                    if (pos == NULL_CUR) break;

                    if (pressed[0]) {
                        auto &io = im::GetIO();
                        if (io.KeySuper) {
                            handle_goto_definition(pos);
                        } else {
                            if (!world.vim.on) {
                                editor->select_start = pos;
                                editor->selecting = true;
                            }
                            editor->mouse_selecting = true;

                            auto opts = default_move_cursor_opts();
                            opts->is_user_movement = true;
                            editor->move_cursor(pos, opts);

                            if (world.vim.on) {
                                // we have to do it this way so it registers in dotcomplete etc
                                if (world.vim_mode() != VI_NORMAL)
                                    editor->vim_handle_key(CP_KEY_ESCAPE, 0);
                                editor->vim_handle_char('v');
                            }
                        }
                        break;
                    }

                    if (pressed[1]) {
                        auto it = editor->iter(pos);
                        if (gr_isident(it.gr_peek())) {
                            auto &menu = world.editor_context_menu;
                            menu.pos = pos;

                            while (!it.bol()) {
                                auto old = it.pos;
                                if (!gr_isident(it.gr_prev())) {
                                    it.pos = old;
                                    break;
                                }
                            }

                            menu.token_start = it.pos;

                            it.pos = pos;
                            while (!it.eol()) {
                                it.gr_next();
                                if (!gr_isident(it.gr_peek()))
                                    break;
                            }

                            menu.token_end = it.pos;

                            cp_assert(menu.token_start.y == menu.token_end.y);
                            cp_assert(menu.token_start < menu.token_end);

                            menu.token = editor->buf->get_text(menu.token_start, menu.token_end);
                            im::OpenPopup("editor_context_menu");
                        }
                        break;
                    }

                    if (pressed[2]) {
                        handle_goto_definition(pos);
                        break;
                    }
                } while (0);
            } else if (editor->mouse_selecting) {
                if (world.ui.mouse_down[0]) {
                    if (!editor->double_clicked_selection) {
                        auto pos = calculate_pos_from_mouse();
                        if (pos.x != -1 && pos.y != -1)
                            editor->move_cursor(pos);
                    }
                } else {
                    editor->mouse_selecting = false;
                }
            }

            if (im_begin_popup("editor_context_menu")) {
                auto &menu = world.editor_context_menu;

                if (im::Selectable("Go To Definition")) {
                    handle_goto_definition(menu.pos);
                }

                if (im::Selectable("Find References")) {
                    initiate_find_references(menu.pos);
                }

                if (im::Selectable("Rename")) {
                    initiate_rename_identifier(menu.pos);
                }

                /*
                im::Separator();

                if (im::Selectable("Cut")) {
                }

                if (im::Selectable("Copy")) {
                }

                if (im::Selectable("Paste")) {

                }
                */

                im::EndPopup();
            }

            // handle double click
            do {
                auto flags = get_mouse_flags(editor_area);
                if (!(flags & MOUSE_DBLCLICKED)) break;

                auto pos = calculate_pos_from_mouse();
                if (pos == NULL_CUR) break;

                auto classify_char = [&](uchar ch) {
                    if (isspace(ch)) return 0;
                    if (isident(ch)) return 1;
                    return 2;
                };

                auto type = classify_char(editor->iter(pos).peek());

                // figure out start
                auto its = editor->iter(pos);
                auto ite = editor->iter(pos);

                while (!its.bof()) {
                    its.prev();
                    if (classify_char(its.peek()) != type || its.y != pos.y) {
                        its.next();
                        break;
                    }
                }

                // figure out end
                while (!ite.eof()) {
                    ite.next();
                    if (classify_char(ite.peek()) != type || ite.y != pos.y) {
                        if (ite.y != pos.y)
                            ite.prev();
                        break;
                    }
                }

                if (its.pos == ite.pos) break;

                if (world.vim.on) {
                    if (world.vim_mode() != VI_NORMAL)
                        editor->vim_handle_key(CP_KEY_ESCAPE, 0);
                    editor->move_cursor(its.pos);
                    editor->vim_handle_char('v');
                    ite.prev();
                } else {
                    editor->selecting = true;
                    editor->select_start = its.pos;
                }
                editor->double_clicked_selection = true;
                editor->move_cursor(ite.pos);
            } while (0);

            // handle scrolling
            auto dy = im::GetIO().MouseWheel;
            if (is_hovered && dy) {
                bool flip = true;
                if (dy < 0) {
                    flip = false;
                    dy = -dy;
                }

                dy *= 30;
                dy += editor->scroll_leftover;

                editor->scroll_leftover = fmod(dy, base_font->height);
                auto lines = (int)(dy / base_font->height);

                auto old = editor->view.y;

                for (int i = 0; i < lines; i++) {
                    if (flip) {
                        if (editor->view.y > 0)
                            editor->view.y--;
                    } else {
                        if (editor->view.y + 1 < editor->buf->lines.len)
                            editor->view.y++;
                    }
                }

                if (old != editor->view.y) editor->ensure_cursor_on_screen();

                if (lines) editor->scroll_leftover = 0;
            }

            if (world.ui.mouse_just_released[0]) {
                if (world.vim.on) {
                    if (world.vim_mode() == VI_VISUAL)
                        if (editor->cur == editor->vim.visual_start)
                            editor->vim_handle_key(CP_KEY_ESCAPE, 0);
                } else {
                    if (editor->selecting)
                        if (editor->select_start == editor->cur)
                            editor->selecting = false;
                }

                editor->double_clicked_selection = false;
                editor->mouse_selecting = false;
            }
        }

        draw_rect(editor_area, rgba(global_colors.background));

        if (!pane.editors.len && world.panes.len == 1)
            draw_tutorial(editor_area);

        vec2 tab_padding = { 8, 3 };

        boxf tab;
        tab.pos = tabs_area.pos + new_vec2(2, tabs_area.h - tab_padding.y * 2 - base_font->height);
        tab.x -= pane.tabs_offset;

        boxf current_tab;

        start_clip(tabs_area); // will become problem once we have popups on tabs

        if (!current_pane) {
            if (world.wnd_mouse_pos.show) {
                begin_window("Mouse Pos", &world.wnd_mouse_pos);
                im::Text("mouse_pos = (%.4f, %.4f)", world.ui.mouse_pos.x, world.ui.mouse_pos.y);
                im::Text(
                    "tabs_area: pos = (%.4f, %.4f), size = (%.4f, %.4f)",
                    tabs_area.x,
                    tabs_area.y,
                    tabs_area.w,
                    tabs_area.h
                );
                im::End();
            }
        }

        // draw tabs
        u32 tab_id = 0;
        for (auto&& editor : pane.editors) {
            defer { editor_index++; };

            SCOPED_FRAME();

            bool is_selected = (tab_id == pane.current_editor);
            bool is_external = false;
            ccstr prefix = NULL;
            ccstr label = "<untitled>";

            if (!editor.is_untitled) {
                auto &ind = world.indexer;

                if (ind.goroot && path_has_descendant(ind.goroot, editor.filepath)) {
                    label = get_path_relative_to(editor.filepath, ind.goroot);
                    prefix = "GOROOT/";
                    is_external = true;
                } else if (ind.gomodcache && path_has_descendant(ind.gomodcache, editor.filepath)) {
                    auto path = get_path_relative_to(editor.filepath, ind.gomodcache);

                    auto p = make_path(path);
                    int base_index = -1;
                    auto prefix_parts = new_list(ccstr);
                    auto label_parts  = new_list(ccstr);

                    Fori (p->parts) {
                        auto part = it;
                        bool found = false;
                        if (strchr(part, '@')) {
                            auto arr = split_string(part, '@');
                            arr->len--; // remove last part
                            part = join_array(arr, '@');
                            found = true;
                        }
                        if (base_index == -1)
                            prefix_parts->append(part);
                        else
                            label_parts->append(part);
                        if (found) base_index = i;
                    }

                    prefix = join_array(prefix_parts, PATH_SEP);
                    label = join_array(label_parts, PATH_SEP);
                    prefix = cp_sprintf("%s/", prefix);
                    is_external = true;
                } else {
                    Go_Work_Module *mod = NULL;
                    if (world.workspace)
                        mod = world.workspace->find_module_containing_resolved(editor.filepath);

                    if (mod) {
                        label = get_path_relative_to(editor.filepath, mod->resolved_path);
                        if (!are_filepaths_equal(mod->resolved_path, world.current_path)) {
                            prefix = cp_basename(mod->resolved_path);
                            prefix = cp_sprintf("%s/", prefix);
                        }
                    } else {
                        label = get_path_relative_to(editor.filepath, world.current_path);
                    }
                }
            }

            if (editor.is_unsaved())
                label = cp_sprintf("%s*", label);

            // color change?
            if (editor.file_was_deleted)
                label = cp_sprintf("%s [deleted]", label);

            auto full_text = prefix ? cp_sprintf("%s%s", prefix, label) : label;
            auto text_width = get_text_width(full_text) * base_font->width;

            tab.w = text_width + tab_padding.x * 2;
            tab.h = base_font->height + tab_padding.y * 2;

            // Now `tab` is filled out, and I can do my logic to make sure it's visible on screen
            if (tab_id == pane.current_editor) {
                current_tab = tab;

                auto margin = !tab_id ? 5 : settings.tabs_offset;
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

                im::Separator();
                im::Text("Tab %d: pos = (%.4f,%.4f), size = (%.4f,%.4f)",
                            editor_index,
                            tab.x, tab.y,
                            tab.w, tab.h);

                if (is_hovered)
                    im::Text("hovered: pos = (%.4f, %.4f), size = (%.4f, %.4f)", tab.x, tab.y, tab.w, tab.h);
                else
                    im::Text("not hovered");

                /*
                if (get_mouse_flags(tab) & MOUSE_HOVER)
                    im::Text("hover flag set");
                else
                    im::Text("hover flag not set");
                */

                im::End();
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

            auto pos = tab.pos + tab_padding;
            if (prefix)
                pos = draw_string(pos, prefix, rgba(is_selected ? global_colors.white : global_colors.white_muted, 0.4));
            pos = draw_string(pos, label, rgba(is_selected ? global_colors.white : global_colors.white_muted));

            auto mouse_flags = get_mouse_flags(tab);
            if (mouse_flags & MOUSE_CLICKED) {
                activate_pane(&pane);
                pane.focus_editor_by_index(tab_id);
            }
            if (mouse_flags & MOUSE_MCLICKED) {
                tab_to_close = editor.id;
                tab_close_action = CLOSE_TAB;
            }

            if (im_begin_popup_rect(cp_sprintf("tab_editor_%d", editor_index), tab)) {
                if (im::Selectable("Close")) {
                    tab_to_close = editor.id;
                    tab_close_action = CLOSE_TAB;
                }

                im_with_disabled(tab_id == 0, [&]() {
                    if (im::Selectable("Close Others to Left")) {
                        tab_to_close = editor.id;
                        tab_close_action = CLOSE_OTHERS_TO_LEFT;
                    }
                });

                im_with_disabled(tab_id == pane.editors.len-1, [&]() {
                    if (im::Selectable("Close Others to Right")) {
                        tab_to_close = editor.id;
                        tab_close_action = CLOSE_OTHERS_TO_RIGHT;
                    }
                });

                im_with_disabled(pane.editors.len == 1, [&]() {
                    if (im::Selectable("Close Others in Pane")) {
                        tab_to_close = editor.id;
                        tab_close_action = CLOSE_OTHERS_IN_PANE;
                    }
                });

                im_with_disabled(world.panes.len == 1 && pane.editors.len == 1, [&]() {
                    if (im::Selectable("Close Others in Window")) {
                        tab_to_close = editor.id;
                        tab_close_action = CLOSE_OTHERS_IN_WINDOW;
                    }
                });

                if (im::Selectable("Close All")) {
                    tab_to_close = editor.id;
                    tab_close_action = CLOSE_ALL;
                }

                im::Separator();

                if (im::Selectable("Copy Path")) {
                    world.window->set_clipboard_string(editor.filepath);
                }

                if (im::Selectable("Copy Relative Path")) {
                    auto relative_path = get_path_relative_to(editor.filepath, world.current_path);
                    world.window->set_clipboard_string(relative_path);
                }

                im::EndPopup();
            }

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

        // draw scrollbar
        do {
            if (!pane.editors.len) break;
            auto editor = pane.get_current_editor();
            auto view = editor->view;

            {
                auto b = scrollbar_area;
                b.x--;
                b.w++;
                draw_rect(b, rgba(merge_colors(global_colors.background, rgb_hex("#ffffff"), 0.1)));
                b.x++;
                b.w--;
                draw_rect(b, rgba(merge_colors(global_colors.background, rgb_hex("#ffffff"), 0.03)));
            }

            auto buf = editor->buf;

            int ymax = buf->lines.len-1;
            if (!ymax) break;

            float M = 2; // margin
            boxf handle; ptr0(&handle);

            auto real_area = scrollbar_area;
            real_area.x += M;
            real_area.w -= M*2;
            real_area.y += M;
            real_area.h -= M*2;

            if (buf->lines.len < view.h) {
                handle = real_area;
            } else {
                float ystart = 0;
                float yend = buf->lines.len - view.h;

                handle.x = real_area.x;
                handle.w = real_area.w;
                handle.h = real_area.h * view.h / buf->lines.len;
                handle.y = real_area.y + ((real_area.h - handle.h) * fmin(1.0, (view.y - ystart) / (yend - ystart)));
            }

            bool is_hovered = test_hover(handle, HOVERID_PANE_RESIZERS + current_pane, ImGuiMouseCursor_Arrow);

            if (is_hovered)
                draw_rounded_rect(handle, rgba(global_colors.white, 0.3), 2, ROUND_ALL);
            else
                draw_rounded_rect(handle, rgba(global_colors.white, 0.2), 2, ROUND_ALL);

            if (world.ui.mouse_just_pressed[0]) {
                if (is_hovered) {
                    auto im_pos = im::GetIO().MousePos;
                    auto pos = new_vec2f(im_pos.x, im_pos.y);

                    pane.scrollbar_drag_offset = pos.y - handle.y;
                    pane.scrollbar_drag_start = pos.y;
                    pane.scrollbar_dragging = true;
                }
            } else if (pane.scrollbar_dragging) {
                if (world.ui.mouse_down[CP_MOUSE_LEFT]) {
                    auto im_pos = im::GetIO().MousePos;
                    auto pos = new_vec2f(im_pos.x, im_pos.y);

                    auto new_handle_y = pos.y - pane.scrollbar_drag_offset;
                    auto y = (int)((buf->lines.len - view.h) * (new_handle_y - real_area.y) / (real_area.h - handle.h));

                    if (buf->lines.len < view.h) {
                        y = 0;
                    } else {
                        if (y < 0) y = 0;
                        if (y > buf->lines.len - view.h) y = buf->lines.len - view.h;
                    }

                    editor->view.y = y;
                    editor->ensure_cursor_on_screen();
                } else {
                    pane.scrollbar_dragging = false;
                }
            }
        } while (0);

        // draw editor
        do {
            if (!pane.editors.len) break;

            auto editor = pane.get_current_editor();
            // if (!editor->is_nvim_ready()) break;

            bool flash_cursor = false;
            float flash_cursor_perc;

            if (editor->flash_cursor_error_start_time) {
                flash_cursor = true;

                auto t = current_time_milli() - editor->flash_cursor_error_start_time;
                if (t < 100)
                    flash_cursor_perc = t / 100.0f;
                else if (t < 350)
                    flash_cursor_perc = 1.0f;
                else if (t < 450)
                    flash_cursor_perc = (450 - t) / 100.0f;
                else
                    flash_cursor = false;
            }

            struct Highlight {
                cur2 start;
                cur2 end;
                vec4f color;
            };

            List<Highlight> highlights;
            highlights.init();

            // generate editor highlights
            if (editor->buf->tree) {
                ts_tree_cursor_reset(&editor->buf->cursor, ts_tree_root_node(editor->buf->tree));

                auto start = new_cur2(0, editor->view.y);
                auto end = new_cur2(0, editor->view.y + editor->view.h);

                walk_ts_cursor(&editor->buf->cursor, false, [&](Ast_Node *node, Ts_Field_Type, int depth) -> Walk_Action {
                    auto node_start = node->start();
                    auto node_end = node->end();

                    if (node_end < start) return WALK_SKIP_CHILDREN;
                    if (node_start > end) return WALK_ABORT;
                    // if (node->child_count()) return WALK_CONTINUE;

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

            auto &buf = editor->buf;
            auto &view = editor->view;

            vec2f cur_pos = editor_area.pos + new_vec2f(settings.editor_margin_x, settings.editor_margin_y);
            cur_pos.y += base_font->offset_y;

            auto draw_cursor = [&](int chars) {
                auto muted = (current_pane != world.current_pane);

                actual_cursor_positions[current_pane] = cur_pos; // save position where cursor is drawn for later use

                auto pos = cur_pos;
                pos.y -= base_font->offset_y;

                boxf b;
                b.pos = pos;

                if (world.vim.on && (world.vim_mode() != VI_INSERT || !is_pane_selected)) {
                    b.w = ((float)base_font->width * chars);
                    if (world.vim_mode() == VI_REPLACE) {
                        b.y += base_font->height / 2;
                        b.h = (float)base_font->height / 2;
                    } else {
                        b.h = (float)base_font->height;
                    }
                } else {
                    b.h = (float)base_font->height;
                    b.w = 2;
                }

                auto py = base_font->height * (settings.line_height - 1.0) / 2;
                b.y -= py;
                b.h += py * 2;

                b.y++;
                b.h -= 2;

                draw_rect(b, rgba(global_colors.cursor, muted ? 0.5 : 1.0));
                if (flash_cursor)
                    draw_rect(b, rgba("#ff0000", flash_cursor_perc));
            };

            List<Client_Breakpoint> breakpoints_for_this_editor;

            {
                u32 len = 0;
                For (&world.dbg.breakpoints)
                    if (streq(it.file, editor->filepath))
                        len++;

                alloc_list(&breakpoints_for_this_editor, len);
                For (&world.dbg.breakpoints) {
                    if (are_filepaths_equal(it.file, editor->filepath)) {
                        auto p = breakpoints_for_this_editor.append();
                        memcpy(p, &it, sizeof(it));
                    }
                }
            }

            if (!buf->lines.len) draw_cursor(1);

            auto &hint = editor->parameter_hint;

            int next_hl = (highlights.len ? 0 : -1);

            int next_search_match = -1;

            // if (world.wnd_local_search.show) {
            if (editor->buf->search_tree) {
                auto tree = editor->buf->search_tree;
                if (tree->get_size(tree->root))
                    next_search_match = 0;
            }

            auto goroutines_hit = new_list(Dlv_Goroutine*);
            u32 current_goroutine_id = 0;
            Dlv_Goroutine *current_goroutine = NULL;
            Dlv_Frame *current_frame = NULL;
            bool is_current_goroutine_on_current_file = false;

            if (world.dbg.mt_state.state_flag == DLV_STATE_PAUSED) {
                current_goroutine_id = world.dbg.mt_state.current_goroutine_id;
                For (world.dbg.mt_state.goroutines) {
                    if (it.id == current_goroutine_id) {
                        current_goroutine = &it;
                        if (current_goroutine->fresh) {
                            current_frame = &current_goroutine->frames->at(world.dbg.mt_state.current_frame);
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

            auto editor_selection = editor->get_selection();
            int editor_curr_range = 0;
            /*
            cur2 select_start, select_end;
            if (editor->selecting) {
                auto a = editor->select_start;
                auto b = editor->cur;
                ORDER(a, b);

                select_start = a;
                select_end = b;
            }
            */

            struct {
                bool on;
                vec4f color;
                cur2 start;
                cur2 end;
            } highlight_snippet;
            ptr0(&highlight_snippet);

            if (editor->highlight_snippet_state.on) {
                double t = (current_time_milli() - editor->highlight_snippet_state.time_start_milli);
                double alpha = 0;

                if (0 < t && t < 200) {
                    alpha = t/200;
                } else if (t < 600) {
                    alpha = 1 - ((t - 200) / 400);
                }

                if (alpha) {
                    highlight_snippet.on = true;
                    highlight_snippet.color = rgba("#eeeebb", alpha * 0.15);
                    highlight_snippet.start = editor->highlight_snippet_state.start;
                    highlight_snippet.end = editor->highlight_snippet_state.end;
                }
            }

            struct {
                bool on;
                cur2 start;
                cur2 end;
                int curr_sib;
                List<Ast_Node*> *siblings;
            } ast_navigation;

            ptr0(&ast_navigation);
            ast_navigation.on = false;
            ast_navigation.start = new_cur2(0, 0);
            ast_navigation.end = new_cur2(0, 0);
            ast_navigation.curr_sib = 0;
            ast_navigation.siblings = new_list(Ast_Node*);

            do {
                auto &nav = editor->ast_navigation;
                if (!nav.on) break;

                if (nav.tree_version != editor->buf->tree_version) {
                    nav.on = false;
                    break;
                }

                auto node = nav.node;
                if (!node) break;
                if (!node) break;

                auto &out = ast_navigation;
                out.start = node->start();
                out.end = node->end();
                out.on = true;
                out.siblings = editor->ast_navigation.siblings;
            } while (0);

            auto draw_highlight = [&](vec4f color, int width, bool fullsize = false) {
                boxf b;
                b.pos = cur_pos;
                b.y -= base_font->offset_y;
                b.w = base_font->width * width;
                b.h = base_font->height;

                auto py = base_font->height * (settings.line_height - 1.0) / 2;
                b.y -= py;
                b.h += py * 2;

                if (!fullsize) {
                    b.y++;
                    b.h -= 2;
                }

                draw_rect(b, color);
            };

            u32 byte_offset = buf->bctree.sum(view.y);

            auto grapheme_codepoints = new_list(uchar);

            auto relative_y = 0;
            for (u32 y = view.y; y < view.y + view.h && y < buf->lines.len; y++, relative_y++) {
                defer { byte_offset += buf->bctree.get(y); };

                auto line = &buf->lines[y];

                enum {
                    BREAKPOINT_NONE,
                    BREAKPOINT_CURRENT_GOROUTINE,
                    BREAKPOINT_OTHER_GOROUTINE,
                    BREAKPOINT_ACTIVE,
                    BREAKPOINT_INACTIVE,
                };

                auto find_breakpoint_stopped_at_this_line = [&]() -> int {
                    if (world.dbg.mt_state.state_flag == DLV_STATE_PAUSED) {
                        if (is_current_goroutine_on_current_file) {
                            if (current_frame) {
                                if (current_frame->lineno == y + 1)
                                    return BREAKPOINT_CURRENT_GOROUTINE;
                            } else if (current_goroutine->curr_line == y + 1)
                                return BREAKPOINT_CURRENT_GOROUTINE;
                        }

                        For (goroutines_hit)
                            if (it->curr_line == y + 1)
                                return BREAKPOINT_OTHER_GOROUTINE;
                    }

                    For (&breakpoints_for_this_editor) {
                        if (it.line == y + 1) {
                            bool inactive = (it.pending || world.dbg.mt_state.state_flag == DLV_STATE_INACTIVE);
                            return inactive ? BREAKPOINT_INACTIVE : BREAKPOINT_ACTIVE;
                        }
                    }

                    return BREAKPOINT_NONE;
                };

                boxf line_box = {
                    cur_pos.x,
                    cur_pos.y - base_font->offset_y,
                    (float)editor_area.w,
                    (float)base_font->height,
                };

                auto py = base_font->height * (settings.line_height - 1.0) / 2;
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
                    ccstr line_number_str = line_number_str = cp_sprintf("%*d", line_number_width, y + 1);
                    auto len = strlen(line_number_str);
                    for (u32 i = 0; i < len; i++)
                        draw_char(&cur_pos, line_number_str[i], rgba(global_colors.foreground, 0.3));
                    cur_pos.x += settings.line_number_margin_right;
                }

                if (line->len) {
                    Grapheme_Clusterer gc;
                    gc.init();

                    int cp_idx = 0;
                    int byte_idx = 0;

                    auto inc_cp_idx = [&]() {
                        auto uch = line->at(cp_idx);

                        cp_idx++;
                        byte_idx += uchar_size(uch);
                    };

                    gc.feed(line->at(cp_idx)); // feed first character for GB1

                    // jump {view.x} clusters
                    int vx_start = 0;
                    {
                        int vx = 0;
                        while (vx < view.x && cp_idx < line->len) {
                            if (line->at(cp_idx) == '\t') {
                                vx += options.tabsize - (vx % options.tabsize);
                                inc_cp_idx();
                            } else {
                                grapheme_codepoints->len = 0;
                                do {
                                    auto codepoint = line->at(cp_idx);
                                    grapheme_codepoints->append(codepoint);
                                    inc_cp_idx();
                                } while (cp_idx < line->len && !gc.feed(line->at(cp_idx)));

                                auto width = cp_wcswidth(grapheme_codepoints->items, grapheme_codepoints->len);
                                if (width == -1) width = 1;
                                vx += width;
                            }
                        }
                        vx_start = vx;
                    }

                    if (vx_start > view.x)
                        cur_pos.x += (vx_start - view.x) * base_font->width;

                    u32 x = buf->idx_cp_to_byte(y, cp_idx);
                    u32 vx = vx_start;
                    u32 newx = 0;

                    for (; vx < view.x + view.w; x = newx) {
                        newx = x;

                        if (cp_idx >= line->len) break;

                        auto curr_cp_idx = cp_idx;
                        auto curr_byte_idx = byte_idx;
                        int curr_cp = line->at(cp_idx);

                        grapheme_codepoints->len = 0;

                        do {
                            auto codepoint = line->at(cp_idx);
                            newx += uchar_size(codepoint);
                            grapheme_codepoints->append(codepoint);
                            inc_cp_idx();
                        } while (cp_idx < line->len && !gc.feed(line->at(cp_idx)));

                        int glyph_width = 0;
                        if (grapheme_codepoints->len == 1 && curr_cp == '\t')
                            glyph_width = options.tabsize - (vx % options.tabsize);
                        else
                            glyph_width = cp_wcswidth(grapheme_codepoints->items, grapheme_codepoints->len);

                        if (glyph_width == -1) glyph_width = 1;

                        vec4f text_color = rgba(global_colors.foreground);

                        if (next_hl != -1) {
                            auto curr = new_cur2(curr_byte_idx, y);

                            while (next_hl != -1 && curr >= highlights[next_hl].end)
                                if (++next_hl >= highlights.len)
                                    next_hl = -1;

                            if (next_hl != -1) {
                                auto& hl = highlights[next_hl];
                                if (hl.start <= curr && curr < hl.end)
                                    text_color = hl.color;
                            }
                        }

                        if (next_search_match != -1) {
                            auto curr = new_cur2(curr_byte_idx, y);
                            auto &wnd = world.wnd_local_search;

                            auto tree = editor->buf->search_tree;
                            auto total = tree->get_size();

                            while (next_search_match != -1 && curr >= tree->get_node(next_search_match)->search_result.end)
                                if (++next_search_match >= total)
                                    next_search_match = -1;

                            if (next_search_match != -1) {
                                auto match = tree->get_node(next_search_match);
                                if (match->pos <= curr && curr < match->search_result.end) {
                                    if (next_search_match == editor->file_search.current_idx) {
                                        draw_highlight(rgba("#ffffdd", 0.4), glyph_width);
                                        text_color = rgba("#ffffdd", 1.0);
                                    } else {
                                        draw_highlight(rgba("#ffffdd", 0.2), glyph_width);
                                        text_color = rgba("#ffffdd", 0.8);
                                    }
                                }
                            }
                        }

                        if (ast_navigation.on) {
                            auto &ref = ast_navigation;

                            auto pos = new_cur2(curr_byte_idx, y);
                            if (ref.start <= pos && pos < ref.end) {
                                draw_highlight(rgba(global_colors.cursor), glyph_width, true);
                                text_color = rgba(global_colors.cursor_foreground);
                            } else {
                                while (ref.curr_sib < ref.siblings->len && pos >= ref.siblings->at(ref.curr_sib)->end())
                                    ref.curr_sib++;

                                if (ref.curr_sib < ref.siblings->len) {
                                    auto node = ref.siblings->at(ref.curr_sib);
                                    if (node->start() <= pos && pos < node->end())
                                        draw_highlight(rgba("#ffffff", 0.1), glyph_width, true);
                                }
                            }
                        }

                        if (highlight_snippet.on) {
                            auto pos = new_cur2(curr_byte_idx, y);
                            if (highlight_snippet.start <= pos && pos < highlight_snippet.end)
                                draw_highlight(highlight_snippet.color, glyph_width);
                        }

                        if (editor_selection) {
                            auto pos = new_cur2(curr_cp_idx, y);

                            auto ranges = editor_selection->ranges;
                            if (editor_curr_range < ranges->len) {
                                auto range = &ranges->at(editor_curr_range);
                                while (pos > range->end) {
                                    editor_curr_range++;
                                    if (editor_curr_range >= ranges->len)
                                        break;
                                    range = &ranges->at(editor_curr_range);
                                }

                                if (range->start <= pos && pos < range->end) {
                                    draw_highlight(rgba(global_colors.visual_background), glyph_width, true);
                                    text_color = rgba(global_colors.visual_foreground);
                                }
                            }
                        }

                        if (editor->cur == new_cur2(curr_cp_idx, y))
                            draw_cursor(glyph_width);

                        if (hint.gotype)
                            if (new_cur2(x, y) == hint.start)
                                actual_parameter_hint_start = cur_pos;

                        uchar uch = curr_cp;
                        if (uch == '\t') {
                            cur_pos.x += base_font->width * glyph_width;
                        } else {
                            draw_char(&cur_pos, grapheme_codepoints, text_color);
                        }

                        vx += glyph_width;
                    }
                } else {
                    if (ast_navigation.on) {
                        auto &ref = ast_navigation;

                        auto pos = new_cur2(0, (int)y);
                        if (ref.start <= pos && pos < ref.end) {
                            draw_highlight(rgba(global_colors.cursor), 1, true);
                        } else if (ref.curr_sib < ref.siblings->len) {
                            auto node = ref.siblings->at(ref.curr_sib);
                            if (node->start() <= pos && pos < node->end())
                                draw_highlight(rgba("#ffffff", 0.1), 1, true);
                        }
                    }

                    if (editor_selection && editor_selection->type != SEL_BLOCK) {
                        auto range = editor_selection->ranges->at(0);

                        auto pos = new_cur2(0, y);
                        if (range.start <= pos && pos < range.end)
                            draw_highlight(rgba(global_colors.visual_background), 1, true);
                    }
                }

                if (editor->cur == new_cur2(line->len, y))
                    draw_cursor(1);

                cur_pos.x = editor_area.x + settings.editor_margin_x;
                cur_pos.y += base_font->height * settings.line_height;
            }

            auto is_hovered = test_hover(preview_area, HOVERID_EDITOR_PREVIEWS + editor_index);

            // draw box
            {
                auto b = preview_area;

                b.h++;
                draw_rect(b, global_colors.preview_border);
                b.h--;
                draw_rect(b, rgba(merge_colors(global_colors.background, global_colors.preview_background.rgb, global_colors.preview_background.a * (is_hovered ? 1.2 : 1))));
            }

            // render toplevel first line if it's offscreen
            do {
                Parser_It it; ptr0(&it);
                it.init(editor->buf);
                auto tree = editor->buf->tree;
                if (!tree) break;
                Ast_Node root; ptr0(&root);
                root.init(ts_tree_root_node(tree), &it);

                Ast_Node *toplevel = NULL;
                int first_line = -1;

                // hide if we're currently selecting with mouse, because it
                // looks weird if we're dragging and random toplevel firstline
                // previews keep showing up and disappearing
                // if (editor->mouse_selecting) break;

                find_nodes_containing_pos(&root, editor->cur, true, [&](auto it) -> Walk_Action {
                    switch (it->type()) {
                    case TS_VAR_DECLARATION:
                    case TS_CONST_DECLARATION:
                    case TS_FUNCTION_DECLARATION:
                    case TS_METHOD_DECLARATION:
                    case TS_TYPE_DECLARATION:
                    case TS_TYPE_ALIAS:
                    case TS_SHORT_VAR_DECLARATION:
                        toplevel = new_object(Ast_Node);
                        memcpy(toplevel, it, sizeof(Ast_Node));
                        return WALK_ABORT;
                    }
                    return WALK_CONTINUE;
                });

                if (!toplevel) break;

                int start = toplevel->start().y;

                // is it offscreen? if not, break
                if (start >= editor->view.y) break;

                int x = 0;
                auto &line = editor->buf->lines[start];

                for (; x < line.len; x++)
                    if (line[x] >= 128 || !isspace(line[x]))
                        break;

                if (x >= line.len) break;

                cur2 toplevel_firstline_pos = new_cur2(x, start);

                if (test_hover(preview_area, HOVERID_EDITOR_PREVIEWS + editor_index)) {
                    if (get_mouse_flags(preview_area) & MOUSE_CLICKED) {
                        editor->move_cursor(toplevel_firstline_pos);
                        editor->selecting = false;
                        editor->mouse_selecting = false;
                    }
                }

                u32 cp_idx = x;
                u32 vx = x;

                Grapheme_Clusterer gc;
                gc.init();
                gc.feed(line[cp_idx]); // feed first character for GB1

                auto cur_pos = preview_area.pos;
                cur_pos.x += settings.editor_margin_x;
                cur_pos.y += areas->preview_margin;
                cur_pos.y += base_font->offset_y;

                auto line_number_width = get_line_number_width(editor);

                {
                    cur_pos.x += settings.line_number_margin_left;
                    ccstr line_number_str = cp_sprintf("%*d", line_number_width, start + 1);
                    for (auto p = line_number_str; *p; p++)
                        draw_char(&cur_pos, *p, rgba(global_colors.preview_foreground.rgb, global_colors.preview_foreground.a * 0.7));
                    cur_pos.x += settings.line_number_margin_right;
                }

                start_clip(preview_area);

                while (cur_pos.x < preview_area.x + preview_area.w) {
                    if (cp_idx >= line.len) break;

                    // collect the entire grapheme cluster
                    auto grapheme = new_list(uchar);
                    do {
                        auto uch = line[cp_idx];
                        grapheme->append(uch);
                        cp_idx++;
                    } while (cp_idx < line.len && !gc.feed(line[cp_idx]));

                    // render it
                    int gw = 0;
                    if (grapheme->len == 1 && grapheme->at(0) == '\t') {
                        int gw = options.tabsize - (vx % options.tabsize);
                        cur_pos.x += base_font->width * gw;
                        vx += gw;
                    } else {
                        int gw = draw_char(&cur_pos, grapheme, global_colors.preview_foreground);
                        if (gw != -1) vx += gw;
                    }
                }

                end_clip();
            } while (0);
        } while (0);

        pane_area.x += pane_area.w;
    }

    if (tab_to_close != -1) {
        Pane *pane = NULL;
        int editor_index = -1;

        auto find_pane_and_editor = [&]() {
            For (&world.panes) {
                auto &p = it;
                Fori (&p.editors) {
                    if (it.id == tab_to_close) {
                        pane = &p;
                        editor_index = i;
                        return;
                    }
                }
            }
        };

        find_pane_and_editor();

        auto &panes = world.panes;

        auto do_shit = [&]() {
            switch (tab_close_action) {
            case CLOSE_TAB:
                if (!pane) return;
                close_editor(pane, editor_index);
                break;
            case CLOSE_OTHERS_TO_LEFT:
                if (!pane) return;
                for (int i = 0; i < editor_index; i++)
                    if (!close_editor(pane, 0))
                        return;
                break;
            case CLOSE_OTHERS_TO_RIGHT:
                for (int i = editor_index + 1; i < pane->editors.len;)
                    if (!close_editor(pane, i))
                        return;
                break;
            case CLOSE_OTHERS_IN_PANE: {
                for (int i = 0; i < pane->editors.len;) {
                    if (pane->editors[i].id == tab_to_close) {
                        i++;
                        continue;
                    }
                    if (!close_editor(pane, i)) return;
                }
                break;
            }
            case CLOSE_OTHERS_IN_WINDOW:
                for (int pane_idx = 0; pane_idx < panes.len;) {
                    auto &pane = panes[pane_idx];
                    for (int i = 0; i < pane.editors.len;) {
                        if (pane.editors[i].id == tab_to_close) {
                            i++;
                            continue;
                        }
                        if (!close_editor(&pane, i)) return;
                    }
                    if (!pane.editors.len) {
                        close_pane(pane_idx);
                        continue;
                    }
                    pane_idx++;
                }
                break;
            case CLOSE_ALL:
                while (true) {
                    auto pane = panes.last();
                    while (pane->editors.len)
                        if (!close_editor(pane, pane->editors.len-1))
                            return;
                    if (panes.len == 1) break;
                    close_pane(panes.len-1);
                }
                break;
            }
        };

        do_shit();
    }

    fstlog("draw panes");

    {
        // Draw pane resizers.

        float offset = 0;

        bool is_any_pane_scrolling = false;
        For (&world.panes) {
            if (it.scrollbar_dragging) {
                is_any_pane_scrolling = true;
                break;
            }
        }

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

            if (!is_any_pane_scrolling && test_hover(hitbox, HOVERID_PANE_RESIZERS + i, ImGuiMouseCursor_ResizeEW)) {
                draw_rect(b, rgba(global_colors.pane_resizer_hover));
                if (world.ui.mouse_down[CP_MOUSE_LEFT])
                    if (world.resizing_pane == -1)
                        world.resizing_pane = i;
            } else if (world.resizing_pane == i) {
                draw_rect(b, rgba(global_colors.pane_resizer_hover));
            } else {
                draw_rect(b, rgba(global_colors.pane_resizer));
            }
        }

        if (!world.ui.mouse_down[CP_MOUSE_LEFT])
            world.resizing_pane = -1;
    }

    fstlog("draw pane resizers");

    {
        draw_rect(status_area, rgba(global_colors.status_area_background));

        float status_area_left = status_area.x;
        float status_area_right = status_area.x + status_area.w;

        enum { LEFT = 0, RIGHT = 1 };

        auto get_status_piece_rect = [&](int dir, ccstr s) -> boxf {
            boxf ret; ptr0(&ret);
            ret.y = status_area.y;
            ret.h = status_area.h;
            ret.w = base_font->width * strlen(s) + (settings.status_padding_x * 2);

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

        auto curr_editor = get_current_editor();

        if (curr_editor) {
            if (!curr_editor->is_modifiable()) {
                draw_status_piece(LEFT, "READONLY", rgba(global_colors.status_mode_background), rgba(global_colors.status_mode_foreground));
            } else if (world.vim.on) {
                ccstr mode_str = NULL;
                switch (world.vim_mode()) {
                case VI_NORMAL: mode_str = "NORMAL"; break;
                case VI_INSERT: mode_str = "INSERT"; break;
                case VI_REPLACE: mode_str = "REPLACE"; break;
                // case VI_OPERATOR: mode_str = "OPERATOR"; break;
                // case VI_CMDLINE: mode_str = "CMDLINE"; break;
                case VI_VISUAL:
                    switch (curr_editor->vim.visual_type) {
                    case SEL_CHAR: mode_str = "VISUAL"; break;
                    case SEL_LINE: mode_str = "VISUAL LINE"; break;
                    case SEL_BLOCK: mode_str = "VISUAL BLOCK"; break;
                    }
                    break;
                default: mode_str = "UNKNOWN"; break;
                }
                draw_status_piece(LEFT, mode_str, rgba(global_colors.status_mode_background), rgba(global_colors.status_mode_foreground));

                if (world.vim.macro_state == MACRO_RECORDING)
                    draw_status_piece(LEFT, cp_sprintf("Recording @%c", world.vim.macro_record.macro), rgba("#000000", 0), rgba(global_colors.foreground, 0.5));
                else if (world.vim.macro_state == MACRO_RUNNING)
                    draw_status_piece(LEFT, cp_sprintf("Running @%c (Ctrl+C to stop)", world.vim.macro_run.macro), rgba("#000000", 0), rgba(global_colors.foreground, 0.5));
            }
        }

        if (world.show_frame_index) {
            auto s = cp_sprintf("%d", world.frame_index);
            auto bg = rgba("#000000");
            auto fg = rgba("#ffffff");

            draw_status_piece(RIGHT, s, bg, fg);
        }

        int index_mouse_flags = 0;
        switch (world.indexer.status) {
        case IND_READY:
        case IND_WRITING: {
            bool writing = false;
            if (world.indexer.status == IND_WRITING) {
                // if indexer runs for less than 500ms, just don't flash the indicator
                if (current_time_milli() - world.indexer.time_started_writing_milli > 500)
                    writing = true;
            }

            if (writing) {
                auto mouse_flags = get_mouse_flags(get_status_piece_rect(RIGHT, "INDEXING..."));
                auto opacity = mouse_flags & MOUSE_HOVER ? 1.0 : 0.8;
                index_mouse_flags = draw_status_piece(RIGHT, "INDEXING...", rgba(global_colors.status_index_indexing_background, opacity), rgba(global_colors.status_index_indexing_foreground, opacity));
            } else {
                auto mouse_flags = get_mouse_flags(get_status_piece_rect(RIGHT, "INDEX READY"));
                auto opacity = mouse_flags & MOUSE_HOVER ? 1.0 : 0.8;
                index_mouse_flags = draw_status_piece(RIGHT, "INDEX READY", rgba(global_colors.status_index_ready_background, opacity), rgba(global_colors.status_index_ready_foreground, opacity));
            }
            break;
        }
        case IND_READING: {
            auto mouse_flags = get_mouse_flags(get_status_piece_rect(RIGHT, "RUNNING..."));
            auto opacity = mouse_flags & MOUSE_HOVER ? 1.0 : 0.8;
            index_mouse_flags = draw_status_piece(RIGHT, "RUNNING...", rgba(global_colors.status_index_indexing_background, opacity), rgba(global_colors.status_index_indexing_foreground, opacity));
            break;
        }
        }

        if (index_mouse_flags & MOUSE_CLICKED) {
            world.wnd_index_log.show ^= 1;
        }

        if (curr_editor) {
            auto cur = curr_editor->cur;
            {
                auto s = cp_sprintf("%d,%d", cur.y+1, cur.x+1);
                s = cp_sprintf("%8s", s);
                draw_status_piece(RIGHT, s, RGBA_TRANSPARENT, rgba("#aaaaaa"));
            }

            do {
                if (!world.vim.on) break;

                auto buf = curr_editor->vim.command_buffer;
                if (!buf->len) break;

                Text_Renderer r; r.init();
                For (buf) {
                    if (it.is_key) {
                        r.write("<");

                        if (it.mods & CP_MOD_CTRL) r.write("C-");
                        if (it.mods & CP_MOD_ALT) r.write("M-");
                        if (it.mods & CP_MOD_SHIFT) r.write("S-");

                        auto key_name = get_key_name(it.key);
                        if (!key_name) key_name = "?";

                        r.write("%s>", key_name);
                    } else {
                        r.write("%c", it.ch);
                    }
                }

                status_area_right -= 5;
                draw_status_piece(RIGHT, r.finish(), RGBA_TRANSPARENT, rgba("#aaaaaa"));
            } while (0);

            auto search_tree = curr_editor->buf->search_tree;
            int search_result_count = search_tree->get_size();
            if (search_result_count) {
                auto s = cp_sprintf("Search: [%d/%d]", curr_editor->file_search.current_idx + 1, search_result_count);
                draw_status_piece(RIGHT, s, RGBA_TRANSPARENT, rgba("#a89f7e"));
            }
        }
    }

    fstlog("draw status area");

    if (world.wnd_hover_info.show) {
        auto &wnd = world.wnd_hover_info;

        begin_window("Hover Info", &wnd);

        im::Text("id: %d", hover.id);
        im::Text("id last frame: %d", hover.id_last_frame);
        im::Text("cursor: %d", hover.id);
        im::Text("start_time: %llums ago", (current_time_nano() - hover.start_time) / 1000000);
        im::Text("ready: %d", hover.ready);

        im::End();
        fstlog("wnd_hover_info");
    }

#ifdef DEBUG_BUILD
    if (world.wnd_poor_mans_gpu_debugger.show) {
        auto &wnd = world.wnd_poor_mans_gpu_debugger;

        begin_window("Poor man's GPU debugger", &wnd);

        if (wnd.tracking) {
            if (im::Button("stop tracking")) {
                wnd.tracking = false;
            }
        } else {
            if (im::Button("start tracking")) {
                wnd.mem.cleanup();
                wnd.mem.init("gpu_debugger");
                {
                    SCOPED_MEM(&wnd.mem);
                    wnd.logs = new_list(Drawn_Quad);
                }
                wnd.selected_quad = -1;
                wnd.tracking = true;
            }
        }

        if (wnd.logs) {
            im::SameLine();

            Drawn_Quad *sel = NULL;
            if (wnd.selected_quad != -1)
                sel = &wnd.logs->at(wnd.selected_quad);

            auto render_drawn_quad = [&](auto q) {
                ccstr texture_str = texture_id_str(q->texture);
                if (!texture_str)
                    texture_str = cp_sprintf("%d", q->texture);

                auto col = q->color;

                return cp_sprintf(
                     "%dx%d at (%d,%d), color = #%02x%02x%02x, mode = %s, texture = %s",
                    (int)q->b.w, (int)q->b.h,
                    (int)q->b.pos.x, (int)q->b.pos.y,
                    (int)(col.r*255), (int)(col.g*255), (int)(col.b*255),
                    draw_mode_str(q->mode),
                    texture_str
                );
            };

            if (im::BeginCombo("###quad_picker", sel ? render_drawn_quad(sel) : NULL, 0)) {
                Fori (wnd.logs) {
                    bool selected = (i == wnd.selected_quad);
                    if (im::Selectable(render_drawn_quad(&it), selected))
                        wnd.selected_quad = i;
                    if (selected)
                        im::SetItemDefaultFocus();
                }
                im::EndCombo();
            }

            if (sel) {
                im::SameLine();

                auto color = to_imcolor(sel->color);
                im::PushStyleColor(ImGuiCol_Button, color);
                im::PushStyleColor(ImGuiCol_ButtonHovered, color);
                im::PushStyleColor(ImGuiCol_ButtonActive, color);
                im::Button(" ");
                im::PopStyleColor(3);

                im_push_mono_font();
                im::InputTextMultiline(
                    "##backtrace", (char*)sel->backtrace, strlen(sel->backtrace),
                    ImVec2(-FLT_MIN, im::GetTextLineHeight() * 16),
                    ImGuiInputTextFlags_ReadOnly);
                im_pop_font();
            }
        }

        im::End();
    }
#endif

    if (world.show_frameskips) {
        auto now = current_time_milli();
        auto pos = new_cur2(
            (int)(world.display_size.x),
            (int)(get_status_area().y - base_font->height)
        );

        For (&world.frameskips) {
            auto s = cp_sprintf("failed frame deadline by %llums", it.ms_over);

            auto newpos = pos;
            newpos.x -= get_text_width(s) * base_font->width;

            auto opacity = 1.0 - (0.7 * (now - it.timestamp) / 2000.f);
            auto color = rgba("#ff0000", opacity);
            draw_string(newpos, s, color);

            pos.y -= base_font->height;
        }
    }

    if (world.cmd_unfocus_all_windows) {
        world.cmd_unfocus_all_windows = false;
        if (!im::IsPopupOpen((char*)NULL, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
            im::SetWindowFocus(NULL);
    }
}

ImVec2 icon_button_padding = ImVec2(4, 2);

ccstr get_import_path_label(ccstr import_path, Go_Workspace *workspace) {
    auto mod = workspace->find_module_containing(import_path);
    if (!mod) return import_path;

    // just get the last part
    int len = strlen(mod->import_path);
    int i = len-1;
    for (; i >= 0; i--)
        if (mod->import_path[i] == '/')
            break;
    auto mod_short = mod->import_path+i+1;

    import_path = get_path_relative_to(import_path, mod->import_path);
    if (streq(import_path, ""))
        return mod_short;
    return cp_sprintf("%s/%s", mod_short, import_path);
}

void UI::im_select_clear_last() {
    auto g = im::GetCurrentContext();
    if (g) {
        auto state = im::GetInputTextState(g->LastItemData.ID);
        if (state) state->ClearSelection();
    }
}

void UI::im_select_all_last() {
    im::SetKeyboardFocusHere(-1);

    auto g = im::GetCurrentContext();
    if (g) {
        auto state = im::GetInputTextState(g->LastItemData.ID);
        if (state) state->SelectAll();
    }
}

bool UI::im_begin_menu(ccstr str_id) {
    auto ret = im::BeginMenu(str_id);
    if (ret)
        if (im_key_pressed(CP_KEY_ESCAPE))
            im::CloseCurrentPopup();
    return ret;
}

bool UI::im_begin_popup(ccstr str_id) {
    auto ret = im::BeginPopup(str_id);
    if (ret)
        if (im_key_pressed(CP_KEY_ESCAPE))
            im::CloseCurrentPopup();
    return ret;
}

bool UI::im_begin_popup_rect(ccstr str_id, boxf rect) {
    if (get_mouse_flags(rect) & MOUSE_RCLICKED)
        im::OpenPopup(str_id);
    return im_begin_popup(str_id);
}

bool UI::im_can_window_receive_keys(Wnd *wnd, int flags) {
    if (wnd->focused)
        if (!world.ui.keyboard_captured_by_imgui || (flags & KNF_ALLOW_IMGUI_FOCUSED))
            return true;
    return false;
}

bool UI::im_wnd_key_pressed(Wnd *wnd, Key key, int mods, int flags) {
    if (im_can_window_receive_keys(wnd, flags))
        if (im_get_keymods() == mods)
            if (im_key_pressed(key))
                return true;
    return false;
}

Keyboard_Nav UI::get_keyboard_nav(Wnd *wnd, int flags) {
    if (!im_can_window_receive_keys(wnd, flags)) return KN_NONE;

    switch (im_get_keymods()) {
    case CP_MOD_NONE:
        if (im_key_pressed(CP_KEY_DOWN))
            return KN_DOWN;
        if (im_key_pressed(CP_KEY_LEFT))
            return KN_LEFT;
        if (im_key_pressed(CP_KEY_RIGHT))
            return KN_RIGHT;
        if (im_key_pressed(CP_KEY_UP))
            return KN_UP;
        if (im_key_pressed(CP_KEY_ENTER))
            return KN_ENTER;
        if (flags & KNF_ALLOW_HJKL) {
            if (im_key_pressed(CP_KEY_J)) return KN_DOWN;
            if (im_key_pressed(CP_KEY_H)) return KN_LEFT;
            if (im_key_pressed(CP_KEY_L)) return KN_RIGHT;
            if (im_key_pressed(CP_KEY_K)) return KN_UP;
            if (im_key_pressed(CP_KEY_R)) return KN_UP;
        }
        break;
    case CP_MOD_PRIMARY:
        if (im_key_pressed(CP_KEY_DELETE)) return KN_DELETE;
        if (im_key_pressed(CP_KEY_BACKSPACE)) return KN_DELETE;
        if (im_key_pressed(CP_KEY_ENTER)) return KN_SUPER_ENTER;
        break;
    }

    return KN_NONE;
}

void UI::draw_tutorial(boxf rect) {
    Command commands[] = {
        CMD_GO_TO_FILE,
        CMD_GO_TO_SYMBOL,
        CMD_SEARCH,
        CMD_COMMAND_PALETTE,
    };

    float spacing_x = 20;
    float spacing_y = 16;
    float hotkey_margin_x = 4;
    float hotkey_padding_x = 3;
    float hotkey_padding_y = 2;

    float max_name_width = get_text_width("Switch Pane");

    float max_hotkey_width = 0;
    max_hotkey_width += base_font->width * get_text_width(CP_MOD_PRIMARY == CP_MOD_CMD ? "Cmd" : "Ctrl");
    max_hotkey_width += base_font->width * get_text_width("1/2/3/4");
    max_hotkey_width += hotkey_padding_x * 4;
    max_hotkey_width += hotkey_margin_x;

    For (&commands) {
        auto name = get_command_name(it);
        cp_assert(name);

        auto name_width = get_text_width(name) * base_font->width;
        if (name_width > max_name_width) max_name_width = name_width;

        auto info = command_info_table[it];

        auto sc = info.shortcuts;
        cp_assert(sc);

        float hotkey_width = 0;

        auto add_hotkey_part = [&](ccstr s) {
            if (hotkey_width != 0)
                hotkey_width += hotkey_margin_x;
            hotkey_width += base_font->width * get_text_width(s);
            hotkey_width += hotkey_padding_x * 2;
        };

        auto mods = sc->mods;
        if (mods & CP_MOD_CMD)   add_hotkey_part("Cmd");
        if (mods & CP_MOD_SHIFT) add_hotkey_part("Shift");
        if (mods & CP_MOD_ALT)   add_hotkey_part("Alt");
        if (mods & CP_MOD_CTRL)  add_hotkey_part("Ctrl");

        auto keyname = get_key_name(sc->key);
        cp_assert(strlen(keyname) <= 3);
        add_hotkey_part(keyname);

        if (hotkey_width > max_hotkey_width) max_hotkey_width = hotkey_width;
    }

    float total_width = max_name_width + max_hotkey_width + spacing_x;
    int rows = _countof(commands)+1;
    float total_height = rows * base_font->height + (rows+1) * spacing_y;

    vec2f start;
    start.x = rect.x + rect.w/2 - total_width/2;
    start.y = rect.y + rect.h/2 - total_height/2;

    vec2f cur = start;

    auto draw_hotkey_part = [&](ccstr s) {
        boxf b;
        b.pos = cur;
        b.x -= hotkey_padding_x;
        b.y -= hotkey_padding_y;
        b.w = get_text_width(s) * base_font->width + hotkey_padding_x * 2;
        b.h = base_font->height + hotkey_padding_y * 2;
        draw_rounded_rect(b, rgba("#ffffff", 0.15), 3, ROUND_ALL);

        draw_string(cur, s, rgba("#ffffff", 0.6));

        cur.x += (b.w - hotkey_padding_x) + hotkey_margin_x * 2;
    };

    auto draw_command_name = [&](ccstr name) {
        cur.x = start.x + max_name_width - get_text_width(name) * base_font->width;
        draw_string(cur, name, rgba("#ffffff", 0.5));

        cur.x = start.x + max_name_width + spacing_x;
    };

    For (&commands) {
        auto name = get_command_name(it);
        cp_assert(name);
        draw_command_name(name);

        auto info = command_info_table[it];
        auto sc = info.shortcuts;
        cp_assert(sc);

        auto mods = sc->mods;
        if (mods & CP_MOD_CMD)   draw_hotkey_part("Cmd");
        if (mods & CP_MOD_SHIFT) draw_hotkey_part("Shift");
        if (mods & CP_MOD_ALT)   draw_hotkey_part("Alt");
        if (mods & CP_MOD_CTRL)  draw_hotkey_part("Ctrl");
        draw_hotkey_part(get_key_name(sc->key));

        cur.y += base_font->height + spacing_y;
    }

    draw_command_name("Switch Pane");
    draw_hotkey_part(CP_MOD_PRIMARY == CP_MOD_CMD ? "Cmd" : "Ctrl");
    draw_hotkey_part("1/2/3/4");
}

bool UI::im_icon_button(ccstr icon) {
    im::PushStyleVar(ImGuiStyleVar_FramePadding, icon_button_padding);
    auto ret = im::Button(icon);
    im::PopStyleVar();
    return ret;
}

void UI::end_frame() {
    flush_verts();

    im::Render();

    {
        // draw imgui buffers
        ImDrawData* draw_data = im::GetDrawData();
        auto scale = world.get_display_scale();
        draw_data->ScaleClipRects(ImVec2(scale.x, scale.y));

        glViewport(0, 0, world.frame_size.x, world.frame_size.y);
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
                glScissor(cmd->ClipRect.x, (world.frame_size.y - cmd->ClipRect.w), (cmd->ClipRect.z - cmd->ClipRect.x), (cmd->ClipRect.w - cmd->ClipRect.y));
                glDrawElements(GL_TRIANGLES, cmd->ElemCount, elem_size, offset);
                offset += cmd->ElemCount;
            }
        }
    }

    // now go back and draw things that go on top, like autocomplete and param hints
    glViewport(0, 0, world.frame_size.x, world.frame_size.y);
    glUseProgram(world.ui.program);
    glBindVertexArray(world.ui.vao); // bind my vertex array & buffers
    glBindBuffer(GL_ARRAY_BUFFER, world.ui.vbo);
    glDisable(GL_SCISSOR_TEST);

    for (u32 current_pane = 0; current_pane < world.panes.len; current_pane++) {
        auto &pane = world.panes[current_pane];
        if (!pane.editors.len) continue;

        auto editor = pane.get_current_editor();
        do {
            auto actual_cursor_position = actual_cursor_positions[current_pane];
            if (actual_cursor_position.x == -1) break;

            auto &ac = editor->autocomplete;

            if (!ac.ac.results) break;

            s32 max_len = 0;
            s32 num_items = min(ac.filtered_results->len, AUTOCOMPLETE_WINDOW_ITEMS);

            For (ac.filtered_results) {
                auto len = get_text_width(ac.ac.results->at(it).name);
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
                // float preview_width = settings.autocomplete_preview_width_in_chars * base_font->width;

                menu.w = (
                    // options
                    (base_font->width * max_len)
                    + (settings.autocomplete_item_padding_x * 2)
                    + (settings.autocomplete_menu_padding * 2)

                    // preview
                    // + preview_width
                    // + (settings.autocomplete_menu_padding * 2)
                );

                menu.h = (
                    (base_font->height * num_items)
                    + (settings.autocomplete_item_padding_y * 2 * num_items)
                    + (settings.autocomplete_menu_padding * 2)
                );

                // menu.x = fmin(actual_cursor_position.x - strlen(ac.ac.prefix) * base_font->width, world.display_size.x - menu.w);
                // menu.y = fmin(actual_cursor_position.y - base_font->offset_y + base_font->height, world.display_size.y - menu.h);

                {
                    auto y1 = actual_cursor_position.y - base_font->offset_y - settings.autocomplete_menu_margin_y;
                    auto y2 = actual_cursor_position.y - base_font->offset_y + (base_font->height * settings.line_height) + settings.autocomplete_menu_margin_y;

                    if (y2 + menu.h < world.display_size.y) {
                        menu.y = y2;
                    } else if (y1 >= menu.h) {
                        menu.y = y1 - menu.h;
                    } else {
                        auto space_under = world.display_size.y - y2;
                        auto space_above = y1;

                        if (space_under > space_above) {
                            menu.y = y2;
                            menu.h = space_under;
                        } else {
                            menu.y = 0;
                            menu.h = y1;
                        }
                    }

                    if (menu.w > world.display_size.x - 4) // small margin
                        menu.w = world.display_size.x - 4;

                    auto x1 = actual_cursor_position.x - strlen(ac.ac.prefix) * base_font->width;
                    if (x1 + menu.w + 4 > world.display_size.x) // margin of 4
                        x1 = world.display_size.x - menu.w - 4;
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
                        b.h = base_font->height + (settings.autocomplete_item_padding_y * 2);
                        b.w = items_area.w - (settings.autocomplete_menu_padding * 2);
                        draw_rounded_rect(b, rgba(global_colors.autocomplete_selection), 4, ROUND_ALL);
                    }

                    auto &result = ac.ac.results->at(idx);

                    float text_end = 0;

                    {
                        SCOPED_FRAME();

                        auto actual_color = color;
                        if (result.type == ACR_POSTFIX)
                            actual_color = new_vec3f(0.5, 0.5, 0.5);
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

                        auto type_str = cp_sprintf("(%s) ", get_decl_type());
                        draw_string(pos, type_str, rgba(color, 0.5));
                        pos.x += base_font->width * strlen(type_str);

                        auto str = (cstr)cp_strdup(result.name);
                        if (strlen(str) > AUTOCOMPLETE_TRUNCATE_LENGTH)
                            str = (cstr)cp_sprintf("%.*s...", AUTOCOMPLETE_TRUNCATE_LENGTH, str);

                        auto avail_width = items_area.w - settings.autocomplete_item_padding_x * 2;
                        if (strlen(str) * base_font->width > avail_width)
                            str[(int)(avail_width / base_font->width)] = '\0';

                        draw_string(pos, str, rgba(actual_color));

                        text_end = pos.x + strlen(str) * base_font->width;
                    }

                    auto render_description = [&]() -> ccstr {
                        switch (result.type) {
                        case ACR_DECLARATION: {
                            auto decl = result.declaration_godecl;
                            switch (decl->type) {
                            case GODECL_IMPORT:
                                // is this even possible here?
                                // handle either way
                                return cp_sprintf("\"%s\"", decl->import_path);
                            case GODECL_TYPE:
                            case GODECL_VAR:
                            case GODECL_CONST:
                            case GODECL_SHORTVAR:
                            case GODECL_FUNC:
                            case GODECL_FIELD:
                            case GODECL_PARAM: {
                                auto gotype = result.declaration_evaluated_gotype;
                                if (!gotype) return "";

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
                            return cp_sprintf("\"%s\"", result.import_path);
                        }
                        return "";
                    };

                    {
                        SCOPED_FRAME();

                        auto desc = render_description();
                        auto desclen = strlen(desc);

                        auto pos = items_pos + new_vec2f(settings.autocomplete_item_padding_x, settings.autocomplete_item_padding_y);
                        pos.x += (items_area.w - (settings.autocomplete_item_padding_x*2) - (settings.autocomplete_menu_padding*2));
                        pos.x -= base_font->width * desclen;

                        auto desclimit = desclen;
                        while (pos.x < text_end + 10) {
                            pos.x += base_font->width;
                            desclimit--;
                        }

                        if (desclen > desclimit)
                            desc = cp_sprintf("%.*s...", desclimit-3, desc);

                        draw_string(pos, desc, rgba(new_vec3f(0.5, 0.5, 0.5)));
                    }

                    items_pos.y += base_font->height + settings.autocomplete_item_padding_y * 2;
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
            if (!hint.gotype) break;

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
                    if (result && result->len > 0) {
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
            bg.w = base_font->width * get_text_width(help_text) + settings.parameter_hint_padding_x * 2;
            bg.h = base_font->height + settings.parameter_hint_padding_y * 2;
            bg.x = fmin(actual_parameter_hint_start.x, world.display_size.x - bg.w);
            bg.y = fmin(actual_parameter_hint_start.y - base_font->offset_y - bg.h - settings.parameter_hint_margin_y, world.display_size.y - bg.h);

            draw_bordered_rect_outer(bg, rgba(color_darken(global_colors.background, 0.1), 1.0), rgba(global_colors.white, 0.8), 1, 4);

            auto text_pos = bg.pos;
            text_pos.x += settings.parameter_hint_padding_x;
            text_pos.y += settings.parameter_hint_padding_y;

            text_pos.y += base_font->offset_y;

            {
                u32 len = strlen(help_text);
                vec4f color = rgba(global_colors.foreground, 0.75);
                float opacity = 1.0;
                int j = 0;
                Cstr_To_Ustr conv; conv.init();

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
                    if (conv.feed(help_text[i]))
                        draw_char(&text_pos, conv.uch, rgba(color.rgb, color.a * opacity));
                }
            }
        } while (0);
    }

    flush_verts();

    recalculate_view_sizes();
}

Pane_Areas* UI::get_pane_areas(boxf* pane_area, bool has_tabs) {
    boxf tabs_area, editor_area, scrollbar_area, preview_area;

    if (has_tabs) {
        tabs_area.pos = pane_area->pos;
        tabs_area.w = pane_area->w;
        tabs_area.h = 24; // TODO
    }

    editor_area.pos = pane_area->pos;
    if (has_tabs) editor_area.y += tabs_area.h;
    editor_area.w = pane_area->w;
    editor_area.h = pane_area->h;
    if (has_tabs) editor_area.h -= tabs_area.h;

    float preview_margin = 2;

    preview_area.pos = editor_area.pos;
    preview_area.w = editor_area.w;
    preview_area.h = base_font->height + preview_margin*2;
    editor_area.y += preview_area.h;
    editor_area.h -= preview_area.h;

    scrollbar_area.y = editor_area.y;
    scrollbar_area.h = editor_area.h;
    scrollbar_area.w = 12;
    scrollbar_area.x = editor_area.x + editor_area.w - scrollbar_area.w;
    editor_area.w -= scrollbar_area.w;

    auto ret = new_object(Pane_Areas);
    ret->editor_area = editor_area;
    ret->preview_area = preview_area;
    ret->preview_margin = preview_margin;
    ret->scrollbar_area = scrollbar_area;
    if (has_tabs) ret->tabs_area = tabs_area;
    return ret;
}

void UI::recalculate_view_sizes(bool force) {
    auto new_sizes = new_list(vec2f);

    float total = 0;
    For (&world.panes) total += it.width;

    boxf pane_area;
    pane_area.pos = {0, 0};
    pane_area.h = panes_area.h;

    For (&world.panes) {
        it.width = it.width / total * panes_area.w;
        pane_area.w = it.width;

        int line_number_width = 0;
        auto editor = it.get_current_editor();
        if (editor)
            line_number_width = get_line_number_width(editor);

        auto areas = get_pane_areas(&pane_area, it.editors.len > 0);

        auto editor_area = areas->editor_area;;
        editor_area.w -= ((line_number_width * base_font->width) + settings.line_number_margin_left + settings.line_number_margin_right);
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
    For (new_sizes) editor_sizes.append(&it);

    for (u32 i = 0; i < world.panes.len; i++) {
        for (auto&& editor : world.panes[i].editors) {
            editor.view.w = (i32)((editor_sizes[i].x - settings.editor_margin_x) / ui.base_font->width);
            editor.view.h = (i32)((editor_sizes[i].y - settings.editor_margin_y) / ui.base_font->height / settings.line_height);

            // Previously we called editor.ensure_cursor_on_screen(), but that
            // moves the *cursor* onto the screen. But often when the user
            // builds, their current cursor position is significant and we
            // don't want to move it. Instead we should move the *view* so the
            // cursor is on the screen.
            editor.ensure_cursor_on_screen_by_moving_view();
        }
    }
}

const u64 WINDOWS_RESIZE_FROM_EDGES_FEEDBACK_TIMER = 40000000;

bool UI::test_hover(boxf area, int id, ImGuiMouseCursor cursor) {
    if (!(get_mouse_flags(area) & MOUSE_HOVER))
        return false;

    auto now = current_time_nano();

    if (id < hover.id_last_frame)
        return false;

    hover.id = id;
    hover.cursor = cursor;

    if (id > hover.id_last_frame) {
        hover.start_time = now;
        hover.ready = false;
    }

    if (now - hover.start_time > WINDOWS_RESIZE_FROM_EDGES_FEEDBACK_TIMER)
        hover.ready = true;

    return hover.ready;
}

Font* UI::acquire_font(ccstr name, bool dont_check) {
    bool found = false;
    auto font = font_cache.get(name, &found);
    if (found) return font;

    SCOPED_MEM(&world.ui_mem);
    Frame frame;

    font = new_object(Font);
    if (!font->init(name, CODE_FONT_SIZE, dont_check)) {
        frame.restore();
        // error("unable to acquire font: %s", name);
        font = NULL;
    }

    font_cache.set(cp_strdup(name), font);
    return font;
}

Font* UI::find_font_for_grapheme(Grapheme grapheme) {
    if (base_font->can_render_grapheme(grapheme))
        return base_font;

    auto pat = FcPatternCreate();
    if (!pat) return error("FcPatternCreate failed"), (Font*)NULL;
    defer { FcPatternDestroy(pat); };

    auto charset = FcCharSetCreate();
    if (!charset) return error("FcCharSetCreate failed"), (Font*)NULL;
    defer { FcCharSetDestroy(charset); };

    For (grapheme)
        if (!FcCharSetAddChar(charset, it))
            return error("FcCharSetAddChar failed"), (Font*)NULL;

    if (!FcPatternAddCharSet(pat, FC_CHARSET, charset))
        return error("FcPatternAddCharSet failed"), (Font*)NULL;

    if (!FcPatternAddString(pat, FC_FONTFORMAT, (const FcChar8*)"TrueType"))
        return error("FcPatternAddString failed"), (Font*)NULL;

    FcConfigSubstitute(NULL, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);

    FcResult result;
    auto match = FcFontMatch(NULL, pat, &result);
    if (!match) return error("FcFontMatch failed"), (Font*)NULL;
    defer { FcPatternDestroy(match); };

    FcChar8 *uncasted_name = NULL;
    if (FcPatternGetString(match, FC_POSTSCRIPT_NAME, 0, &uncasted_name) != FcResultMatch) {
        error("unable to get postscript name for font");
        return NULL;
    }

    auto name = cp_strdup((ccstr)uncasted_name);
    if (streq(name, "LastResort")) {
        bool found = false;
        For (all_font_names) {
            auto font = acquire_font(it, false);
            if (font->can_render_grapheme(grapheme))
                return font;
        }
        return NULL;
    }

    auto font = acquire_font(name, true);
    if (!font) return NULL;
    if (!font->can_render_grapheme(grapheme)) return NULL;
    return font;
}

bool Font::init(ccstr font_name, u32 font_size, Font_Data *fontdata, bool notify_error) {
    ptr0(this);
    name = font_name;
    height = font_size;
    data = fontdata;

    bool ok = false;
    defer { if (!ok) cleanup(); };

    u8 *data_ptr = data->type == FONT_DATA_MMAP ? data->fm->data : data->data;
    u32 data_len = data->type == FONT_DATA_MMAP ? data->fm->len : data->len;

    // create harfbuzz crap
    hbblob = hb_blob_create((char*)data_ptr, data_len, HB_MEMORY_MODE_READONLY, NULL, NULL);
    if (!hbblob) {
        if (notify_error)
            tell_user_error(cp_sprintf("Unable to initialize font. (hb_blob_create, %s)", font_name));
        return false;
    }

    hbface = hb_face_create(hbblob, 0);
    if (!hbface) {
        if (notify_error)
            tell_user_error(cp_sprintf("Unable to initialize font. (hb_face_create, %s)", font_name));
        return false;
    }
    hbfont = hb_font_create(hbface);
    if (!hbfont) {
        if (notify_error)
            tell_user_error(cp_sprintf("Unable to initialize font. (hb_font_create, %s)", font_name));
        return false;
    }

    // create stbtt font
    if (!stbtt_InitFont(&stbfont, data_ptr, stbtt_GetFontOffsetForIndex(data_ptr, 0))) {
        if (notify_error)
            tell_user_error(cp_sprintf("Unable to initialize font. (stbtt_InitFont, %s)", font_name));
        return false;
    }

    int unscaled_width = 0, unscaled_offset_y = 0;
    stbtt_GetCodepointHMetrics(&stbfont, 'A', &unscaled_width, NULL);
    stbtt_GetFontVMetrics(&stbfont, &unscaled_offset_y, NULL, NULL);

    auto scale = stbtt_ScaleForPixelHeight(&stbfont, (float)height);
    width = scale * unscaled_width;
    offset_y = scale * unscaled_offset_y;

    ok = true;
    return true;
}

bool Font::init(ccstr font_name, u32 font_size, bool dont_check) {
    auto data = load_font_data_by_name(font_name, dont_check);
    if (!data) return false;

    return init(font_name, font_size, data);
}

void Font::cleanup() {
    if (hbfont) { hb_font_destroy(hbfont); hbfont = NULL; }
    if (hbface) { hb_face_destroy(hbface); hbface = NULL; }
    if (hbblob) { hb_blob_destroy(hbblob); hbblob = NULL; }
    if (data) { data->cleanup(); data = NULL; }
}

bool Font::can_render_grapheme(Grapheme gr) {
    For (gr)
        if (!stbtt_FindGlyphIndex(&stbfont, it))
            return false;
    return true;
}

bool check_cmd_flag(bool *b) {
    auto ret = *b;
    *b = false;
    return ret;
}
