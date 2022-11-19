#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <functional>
#include <inttypes.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdexcept>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utility>

#include "common.hpp"
#include "dbg.hpp"
#include "defer.hpp"
#include "editor.hpp"
#include "enums.hpp"
#include "fonts.hpp"
#include "go.hpp"
#include "list.hpp"
#include "nvim.hpp"
#include "os.hpp"
#include "settings.hpp"
#include "ui.hpp"
#include "unicode.hpp"
#include "utils.hpp"
#include "world.hpp"

#include "binaries.h"
#include "fzy_match.h"
#include "icons.h"
#include "imgui.h"

static const char WINDOW_TITLE[] = "CodePerfect 95";

GLint compile_program(cstr vert_code, u32 vert_len, cstr frag_code, u32 frag_len) {
    auto compile_shader = [](GLchar *code, GLint len, u32 type) -> GLuint {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &code, &len);
        glCompileShader(shader);

        i32 status;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if ((GLboolean)status) return shader;

        char log[512];
        glGetShaderInfoLog(shader, 512, NULL, log);
        cp_panic(cp_sprintf("failed to build shader, error: %s", log));
        return 0;
    };

    auto vert = compile_shader(vert_code, vert_len, GL_VERTEX_SHADER);
    auto frag = compile_shader(frag_code, frag_len, GL_FRAGMENT_SHADER);

    GLint id = glCreateProgram();
    glAttachShader(id, vert);
    glAttachShader(id, frag);
    glLinkProgram(id);
    glDeleteShader(vert);
    glDeleteShader(frag);

    i32 status = 0;
    glGetProgramiv(id, GL_LINK_STATUS, &status);

    if (!status) {
        char log[512];
        glGetProgramInfoLog(id, 512, NULL, log);
        fprintf(stderr, "error linking program: %s", log);
        return -1;
    }

    return id;
}

void new_ortho_matrix(float* mat, float l, float r, float b, float t) {
    mat[0] = 2 / (r - l);
    mat[1] = 0;
    mat[2] = 0;
    mat[3] = 0;

    mat[4] = 0;
    mat[5] = 2 / (t - b);
    mat[6] = 0;
    mat[7] = 0;

    mat[8] = 0;
    mat[9] = 0;
    mat[10] = -1;
    mat[11] = 0;

    mat[12] = -(r + l) / (r - l);
    mat[13] = -(t + b) / (t - b);
    mat[14] = 0;
    mat[15] = 1;
}

#define MAX_RETRIES 5

void goto_next_tab() {
    auto pane = get_current_pane();
    if (!pane->editors.len) return;

    auto idx = (pane->current_editor + 1) % pane->editors.len;
    pane->focus_editor_by_index(idx);
}

void goto_previous_tab() {
    auto pane = get_current_pane();
    if (!pane->editors.len) return;

    u32 idx;
    if (!pane->current_editor)
        idx = pane->editors.len - 1;
    else
        idx = pane->current_editor - 1;
    pane->focus_editor_by_index(idx);
}

bool is_git_folder(ccstr path) {
    SCOPED_FRAME();
    auto pathlist = make_path(path);
    return pathlist->parts->find([&](auto it) { return streqi(*it, ".git"); });
}

void recalc_display_size() {
    auto scale = world.get_display_scale();
    // calculate display_size
    world.display_size.x = (int)(world.frame_size.x / scale.x);
    world.display_size.y = (int)(world.frame_size.y / scale.y);

    // set projection based on new display size
    mat4f projection;
    new_ortho_matrix(projection, 0, world.display_size.x, world.display_size.y, 0);
    glUseProgram(world.ui.im_program);
    glUniformMatrix4fv(glGetUniformLocation(world.ui.im_program, "projection"), 1, GL_FALSE, (float*)projection);
}

void handle_window_event(Window_Event *it) {
    switch (it->type) {
    case WINEV_FOCUS:
        clear_key_states();
        break;

    case WINEV_BLUR:
        clear_key_states();
        break;

    case WINEV_WINDOW_SIZE: {
        auto w = it->window_size.w;
        auto h = it->window_size.h;

        world.window_size.x = w;
        world.window_size.y = h;
        break;
    }

    case WINEV_FRAME_SIZE: {
        auto w = it->frame_size.w;
        auto h = it->frame_size.h;

        Timer t; t.init("framebuffersize callback", &world.trace_next_frame); defer { t.log("done"); };

        world.frame_size.x = w;
        world.frame_size.y = h;
        recalc_display_size();

        mat4f projection;
        new_ortho_matrix(projection, 0, w, h, 0);
        glUseProgram(world.ui.program);
        glUniformMatrix4fv(glGetUniformLocation(world.ui.program, "projection"), 1, GL_FALSE, (float*)projection);
        break;
    }

    case WINEV_MOUSE_MOVE: {
        auto x = it->mouse_move.x;
        auto y = it->mouse_move.y;

        Timer t; t.init("cursorpos callback", &world.trace_next_frame); defer { t.log("done"); };

        world.ui.mouse_pos.x = (int)((float)x / world.window_size.x * world.display_size.x);
        world.ui.mouse_pos.y = (int)((float)y / world.window_size.y * world.display_size.y);

        if (world.resizing_pane != -1) {
            auto i = world.resizing_pane;

            float leftoff = ui.panes_area.x;
            for (int j = 0; j < i; j++)
                leftoff += world.panes[j].width;

            auto& pane1 = world.panes[i];
            auto& pane2 = world.panes[i+1];

            // leftoff += pane1.width;
            auto desired_width = relu_sub(x, leftoff);
            auto delta = desired_width - pane1.width;

            if (delta < (100 - pane1.width)) delta = 100 - pane1.width;
            if (delta > pane2.width - 100) delta = pane2.width - 100;

            pane1.width += delta;
            pane2.width -= delta;
        }
        break;
    }

    case WINEV_MOUSE: {
        auto button = it->mouse.button;
        auto press = it->mouse.press;
        auto mods = it->mouse.mods;

        Timer t; t.init("mousebutton callback", &world.trace_next_frame); defer { t.log("done"); };

        // Don't set world.ui.mouse_down here. We set it based on
        // world.ui.mouse_just_pressed and some additional logic below, while
        // we're setting io.MouseDown for ImGui.

        if (button < 0 || button >= IM_ARRAYSIZE(world.ui.mouse_just_pressed))
            return;

        if (press)
            world.ui.mouse_just_pressed[button] = true;
        else
            world.ui.mouse_just_released[button] = true;
        break;
    }

    case WINEV_SCROLL: {
        auto dx = it->scroll.dx;
        auto dy = it->scroll.dy;

        Timer t; t.init("scroll callback", &world.trace_next_frame); defer { t.log("done"); };

        auto &io = ImGui::GetIO();
        io.MouseWheelH += (float)dx;
        io.MouseWheel += (float)dy;
        break;
    }

    case WINEV_WINDOW_SCALE: {
        auto xscale = it->window_scale.xscale;
        auto yscale = it->window_scale.yscale;

        Timer t; t.init("windowcontentscale callback", &world.trace_next_frame); defer { t.log("done"); };

        world.display_scale = { xscale, yscale };
        recalc_display_size();
        break;
    }

    case WINEV_KEY: {
        auto key = it->key.key;
        auto press = it->key.press;

        Timer t; t.init("key callback", &world.trace_next_frame); defer { t.log("done"); };

        ImGuiIO& io = ImGui::GetIO();
        io.KeysDown[key] = press;

        io.KeyCtrl = io.KeysDown[CP_KEY_LEFT_CONTROL] || io.KeysDown[CP_KEY_RIGHT_CONTROL];
        io.KeyShift = io.KeysDown[CP_KEY_LEFT_SHIFT] || io.KeysDown[CP_KEY_RIGHT_SHIFT];
        io.KeyAlt = io.KeysDown[CP_KEY_LEFT_ALT] || io.KeysDown[CP_KEY_RIGHT_ALT];
        io.KeySuper = io.KeysDown[CP_KEY_LEFT_SUPER] || io.KeysDown[CP_KEY_RIGHT_SUPER];

        if (!press) return;

        // ==================
        // handle global keys
        // ==================

        auto keymods = it->key.mods;
        // print("key = %d, mods = %d", key, keymods);
        switch (keymods) {
        case CP_MOD_PRIMARY:
            switch (key) {
            case CP_KEY_1:
            case CP_KEY_2:
            case CP_KEY_3:
            case CP_KEY_4:
                activate_pane_by_index(key - CP_KEY_1);
                world.cmd_unfocus_all_windows = true;
                break;
            }
            break;

        case CP_MOD_NONE:
            switch (key) {
            case CP_KEY_ESCAPE:
                if (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow))
                    world.cmd_unfocus_all_windows = true; // see if this causes any sync problems
                break;

            }
            break;
        }

        // ===============
        // handle commands
        // ===============

        for (int i = 0; i < _CMD_COUNT_; i++) {
            auto cmd = (Command)i;

            auto info = command_info_table[cmd];
            if (info.mods == keymods && info.key == key) {
                if (is_command_enabled(cmd)) {
                    handle_command(cmd, false);
                    if (cmd == CMD_GO_TO_FILE)
                        world.trace_next_frame = true;

                    return;
                }
            }
        }

        if (world.ui.keyboard_captured_by_imgui) return;
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) return;

        // ======================
        // handle non-global keys
        // ======================

        auto editor = get_current_editor();

        auto make_nvim_string = [&](ccstr s) {
            List<ccstr> parts; parts.init();

            if (keymods & CP_MOD_CMD)   parts.append("D");
            if (keymods & CP_MOD_SHIFT) parts.append("S");
            if (keymods & CP_MOD_ALT)   parts.append("A");
            if (keymods & CP_MOD_CTRL)  parts.append("C");

            Text_Renderer rend; rend.init();
            For (parts) rend.write("%s-", it);
            rend.write("<%s>", s);
            return rend.finish();
        };

        auto handle_enter = [&]() {
            if (!editor) return;

            if (world.use_nvim) {
                if (world.nvim.mode != VI_INSERT) {
                    send_nvim_keys(make_nvim_string("Enter"));
                    return;
                }
            }

            if (!world.use_nvim) editor->delete_selection();

            editor->type_char_in_insert_mode('\n');

            auto indent_chars = editor->get_autoindent(editor->cur.y);
            editor->insert_text_in_insert_mode(indent_chars);
        };

        auto handle_tab = [&]() {
            if (!editor) return;

            if (keymods == CP_MOD_NONE) {
                auto& ac = editor->autocomplete;
                if (ac.ac.results && ac.filtered_results->len) {
                    auto idx = ac.filtered_results->at(ac.selection);
                    auto& result = ac.ac.results->at(idx);
                    editor->perform_autocomplete(&result);
                    return;
                }
            }

            if (world.use_nvim) {
                if (world.nvim.mode != VI_INSERT) {
                    send_nvim_keys(make_nvim_string("Tab"));
                    return;
                }
            }

            editor->type_char_in_insert_mode('\t');
        };

        auto alt_move = [&](bool back, bool backspace) -> cur2 {
            auto it = editor->iter();

            if (back) {
                if (it.bof()) return it.pos;
                it.prev();
            }

            auto done = [&]() { return back ? it.bof() : it.eof(); };
            auto advance = [&]() { back ? it.prev() : it.next(); };

            enum { TYPE_SPACE, TYPE_IDENT, TYPE_OTHER };

            auto get_char_type = [](char ch) -> int {
                if (isspace(ch)) return TYPE_SPACE;
                if (isident(ch)) return TYPE_IDENT;
                return TYPE_OTHER;
            };

            int start_type = get_char_type(it.peek());
            int chars_moved = 0;

            for (; !done(); advance(), chars_moved++) {
                if (get_char_type(it.peek()) != start_type) {
                    if (start_type == TYPE_SPACE && chars_moved == 1) {
                        // If we only found one space, start over with the next space type.
                        start_type = get_char_type(it.peek());
                        chars_moved = 0;
                        continue;
                    }

                    if (back) it.next();
                    break;
                }
            }

            return it.pos;
        };

        auto handle_backspace = [&]() {
            if (!editor) return;

            if (world.use_nvim) {
                if (world.nvim.mode != VI_INSERT) {
                    send_nvim_keys(make_nvim_string("Backspace"));
                    return;
                }

                if (world.nvim.exiting_insert_mode) {
                    world.nvim.chars_after_exiting_insert_mode.append('\b');
                    return;
                }
            }

            if (!world.use_nvim && editor->selecting) {
                editor->delete_selection();
            } else if (keymods & CP_MOD_TEXT) {
                auto new_cur = alt_move(true, true);
                while (editor->cur > new_cur)
                    editor->backspace_in_insert_mode(1, 0);
            } else {
                editor->backspace_in_insert_mode(1, 0); // erase one grapheme
            }

            editor->update_autocomplete(false);
            editor->update_parameter_hint();
        };

        if (editor && editor->ast_navigation.on) {
            switch (keymods) {
            case CP_MOD_SHIFT:
                switch (key) {
                case CP_KEY_DOWN:
                case CP_KEY_RIGHT:
                case CP_KEY_J:
                case CP_KEY_L:
                    editor->ast_navigate_in();
                    break;
                case CP_KEY_UP:
                case CP_KEY_LEFT:
                case CP_KEY_H:
                case CP_KEY_K:
                    editor->ast_navigate_out();
                    break;
                }
                break;
            case CP_MOD_NONE:
                switch (key) {
                case CP_KEY_DOWN:
                case CP_KEY_RIGHT:
                case CP_KEY_J:
                case CP_KEY_L:
                    editor->ast_navigate_next();
                    break;

                case CP_KEY_UP:
                case CP_KEY_LEFT:
                case CP_KEY_H:
                case CP_KEY_K:
                    editor->ast_navigate_prev();
                    break;

                case CP_KEY_ESCAPE:
                    editor->ast_navigation.on = false;
                    break;
                }
                break;
            }
            break;
        }

        // =======================
        // handle non-vim movement
        // =======================

        do {
            if (world.use_nvim) break;
            if (!editor) break;

            bool handled = false;

            auto buf = editor->buf;
            auto cur = editor->cur;

            switch (keymods) {
            case CP_MOD_SHIFT:
            case CP_MOD_NONE:
                switch (key) {
                case CP_KEY_LEFT:
                    if (!(keymods & CP_MOD_SHIFT) && editor->selecting) {
                        auto a = editor->select_start;
                        auto b = cur;
                        cur = a < b ? a : b;
                    } else if (cur.x) {
                        cur.x--;
                    } else if (cur.y) {
                        cur.y--;
                        cur.x = buf->lines[cur.y].len;
                    }
                    handled = true;
                    break;

                case CP_KEY_RIGHT:
                    if (!(keymods & CP_MOD_SHIFT) && editor->selecting) {
                        auto a = editor->select_start;
                        auto b = cur;
                        cur = a > b ? a : b;
                    } else if (cur.x < buf->lines[cur.y].len) {
                        cur.x++;
                    } else if (cur.y < buf->lines.len-1) {
                        cur.y++;
                        cur.x = 0;
                    }
                    handled = true;
                    break;

                case CP_KEY_PAGE_UP:
                    editor->view.y -= editor->view.h;
                    if (editor->view.y < 0) {
                        editor->view.y = 0;
                        cur = new_cur2(0, 0);
                    } else {
                        editor->ensure_cursor_on_screen();
                        cur = editor->cur;
                    }
                    handled = true;
                    break;

                case CP_KEY_PAGE_DOWN:
                    editor->view.y += editor->view.h;
                    if (editor->view.y > buf->lines.len-1) {
                        editor->view.y = buf->lines.len-1;
                        cur = new_cur2((int)buf->lines[buf->lines.len-1].len, (int)buf->lines.len-1);
                    } else {
                        editor->ensure_cursor_on_screen();
                        cur = editor->cur;
                    }
                    handled = true;
                    break;

                case CP_KEY_HOME:
                    cur.x = 0;
                    handled = true;
                    break;

                case CP_KEY_END:
                    cur.x = buf->lines[cur.y].len;
                    handled = true;
                    break;
                }
                break;

            case CP_MOD_TEXT | CP_MOD_SHIFT:
            case CP_MOD_TEXT:
                switch (key) {
                case CP_KEY_LEFT:
                case CP_KEY_RIGHT:
                    if (!(keymods & CP_MOD_SHIFT) && editor->selecting) {
                        auto a = editor->select_start;
                        auto b = cur;
                        if (key == CP_KEY_LEFT)
                            cur = a < b ? a : b;
                        else
                            cur = a > b ? a : b;
                    } else {
                        cur = alt_move(key == CP_KEY_LEFT, false);
                    }
                    handled = true;
                    break;
                }
                break;
            }

            switch (keymods) {
            case CP_MOD_NONE:
            case CP_MOD_SHIFT:
            case CP_MOD_TEXT | CP_MOD_SHIFT:
            case CP_MOD_TEXT:
                switch (key) {
                case CP_KEY_DOWN:
                case CP_KEY_UP: {
                    if (keymods == CP_MOD_NONE)
                        if (move_autocomplete_cursor(editor, key == CP_KEY_DOWN ? 1 : -1))
                            break;

                    auto old_savedvx = editor->savedvx;

                    auto calc_x = [&]() -> int {
                        return buf->idx_vcp_to_cp(cur.y, editor->savedvx);
                    };

                    if (key == CP_KEY_DOWN) {
                        if (cur.y < buf->lines.len-1) {
                            cur.y++;
                            cur.x = calc_x();
                        }
                    } else {
                        if (cur.y) {
                            cur.y--;
                            cur.x = calc_x();
                        }
                    }
                    handled = true;
                    break;
                }
                }
                break;
            }

            if (!handled) break;

            if (keymods & CP_MOD_SHIFT) {
                if (!editor->selecting) {
                    editor->select_start = editor->cur;
                    editor->selecting = true;
                }
            } else {
                editor->selecting = false;
            }

            auto old_savedvx = editor->savedvx;

            auto opts = default_move_cursor_opts();
            opts->is_user_movement = true;
            editor->move_cursor(cur, opts);

            if (key == CP_KEY_UP || key == CP_KEY_DOWN)
                editor->savedvx = old_savedvx;

            editor->update_autocomplete(false); // TODO: why call this here?
            editor->update_parameter_hint();
            return;
        } while (0);

        // ============================
        // handle enter, backspace, tab
        // ============================

        switch (keymods) {
        case CP_MOD_NONE:
        case CP_MOD_SHIFT:
        case CP_MOD_CTRL:
        case CP_MOD_ALT:
        case CP_MOD_CTRL | CP_MOD_ALT:
        case CP_MOD_CTRL | CP_MOD_SHIFT:
        case CP_MOD_ALT | CP_MOD_SHIFT:
        case CP_MOD_CTRL | CP_MOD_ALT | CP_MOD_SHIFT:
            switch (key) {
            case CP_KEY_ENTER:
                handle_enter();
                return;
            case CP_KEY_BACKSPACE:
                handle_backspace();
                return;
            case CP_KEY_TAB:
                if (keymods == CP_MOD_CTRL || keymods == (CP_MOD_CTRL | CP_MOD_SHIFT)) {
                    break;
                }
#if OS_WINBLOWS
                if (keymods & CP_MOD_ALT) break;
#endif
                handle_tab();
                return;
            }
        }

        // ====================
        // handle rest of cases
        // ====================

        switch (keymods) {
        case CP_MOD_SHIFT:
            switch (key) {
            case CP_KEY_ESCAPE:
                if (!editor) break;
                if (!editor->trigger_escape()) send_nvim_keys("<S-Esc>");
                break;
            }
            break;

        case CP_MOD_CTRL: {
            bool handled = false;

            switch (key) {
            case CP_KEY_ENTER:
                // was there a reason this didn't work?
                /*
                if (!editor) break;
                if (world.nvim.mode == VI_INSERT && editor->postfix_stack.len > 0) {
                    auto pf = editor->postfix_stack.last();
                    cp_assert(pf->current_insert_position < pf->insert_positions.len);

                    auto pos = pf->insert_positions[pf->current_insert_position++];
                    editor->trigger_escape(pos);

                    if (pf->current_insert_position == pf->insert_positions.len)
                        editor->postfix_stack.len--;
                } else {
                    handle_enter("<C-Enter>");
                }
                */
                break;

            case CP_KEY_ESCAPE:
                if (!editor) break;
                if (!editor->trigger_escape()) send_nvim_keys("<C-Esc>");
                break;

            case CP_KEY_TAB:
                goto_next_tab();
                break;

            case CP_KEY_R:
            case CP_KEY_O:
            case CP_KEY_I:
            case CP_KEY_D:
            case CP_KEY_U:
            case CP_KEY_B:
#if !OS_WIN
            case CP_KEY_F:
#endif
            case CP_KEY_E:
            case CP_KEY_Y:
            case CP_KEY_V:
                if (world.use_nvim)
                    if (world.nvim.mode != VI_INSERT)
                        send_nvim_keys(cp_sprintf("<C-%c>", tolower((char)key)));
                break;

            case CP_KEY_SLASH: {
                auto &nv = world.nvim;
                nv.start_request_message("nvim_exec", 2);
                nv.writer.write_string("nohlsearch");
                nv.writer.write_bool(false);
                nv.end_message();
                break;
            }
            case CP_KEY_SPACE: {
                auto ed = get_current_editor();
                if (!ed) break;
                ed->trigger_autocomplete(false, false);
                break;
            }
            }
            break;
        }

        case CP_MOD_CMD | CP_MOD_SHIFT:
            switch (key) {
#if OS_MAC
            case CP_KEY_LEFT_BRACKET:
                goto_previous_tab();
                break;
            case CP_KEY_RIGHT_BRACKET:
                goto_next_tab();
                break;
#endif
            }
            break;

        case CP_MOD_CTRL | CP_MOD_SHIFT:
            switch (key) {
            case CP_KEY_ESCAPE:
                if (!editor) break;
                if (!editor->trigger_escape())
                    send_nvim_keys("<C-S-Esc>");
                break;
            case CP_KEY_TAB:
                goto_previous_tab();
                break;
            case CP_KEY_SPACE: {
                if (!editor) break;
                editor->trigger_parameter_hint();
                break;
            }
            }
            break;

        case CP_MOD_NONE: {
            switch (key) {
            case CP_KEY_LEFT:
            case CP_KEY_RIGHT:
                if (!editor) break;
                if (editor->ast_navigation.on) {
                    if (key == CP_KEY_LEFT)
                        editor->ast_navigate_prev();
                    else
                        editor->ast_navigate_next();
                    break;
                }
                if (world.use_nvim && world.nvim.mode != VI_INSERT)
                    send_nvim_keys(key == CP_KEY_LEFT ? "<Left>" : "<Right>");
                break;

            case CP_KEY_DOWN:
            case CP_KEY_UP:
                if (!editor) break;
                if (editor->ast_navigation.on) {
                    if (key == CP_KEY_DOWN)
                        editor->ast_navigate_next();
                    else
                        editor->ast_navigate_prev();
                    break;
                }
                if (world.use_nvim) {
                    if (world.nvim.mode == VI_INSERT) {
                        if (!move_autocomplete_cursor(editor, key == CP_KEY_DOWN ? 1 : -1))  {
                            // ???
                        }
                    } else {
                        send_nvim_keys(key == CP_KEY_DOWN ? "<Down>" : "<Up>");
                    }
                }
                break;

            case CP_KEY_ESCAPE:
                if (!editor) break;
                if (editor->trigger_escape()) break;
                if (world.use_nvim) send_nvim_keys("<Esc>");
                if (world.escape_flashes_cursor_red) editor->flash_cursor_error();
                break;
            }
            break;
        }
        }

        // separate switch for CP_MOD_PRIMARY

        switch (keymods) {
        case CP_MOD_PRIMARY:
            switch (key) {
#ifndef RELEASE_MODE
            case CP_KEY_F12:
                world.windows_open.im_demo ^= 1;
                break;
#endif

            case CP_KEY_A:
                if (!world.use_nvim) {
                    auto editor = get_current_editor();
                    if (!editor) break;

                    editor->select_start = new_cur2(0, 0);

                    auto buf = editor->buf;
                    int y = buf->lines.len-1;
                    int x = buf->lines[y].len;
                    editor->raw_move_cursor(new_cur2(x, y));

                    if (editor->select_start != editor->cur)
                        editor->selecting = true;
                }
                break;

            case CP_KEY_C:
            case CP_KEY_X:
                if (world.use_nvim) {
                    auto& nv = world.nvim;
                    nv.start_request_message("nvim_call_function", 2);
                    nv.writer.write_string("CPGetVisual");
                    nv.writer.write_array(1);
                    nv.writer.write_string("copy_visual");
                    nv.end_message();
                } else {
                    auto editor = get_current_editor();
                    if (!editor) break;
                    if (!editor->selecting) break;

                    auto a = editor->select_start;
                    auto b = editor->cur;
                    if (a > b) {
                        auto tmp = a;
                        a = b;
                        b = tmp;
                    }

                    auto s = editor->buf->get_text(a, b);
                    world.window->set_clipboard_string(s);

                    if (key == CP_KEY_X) {
                        editor->buf->remove(a, b);
                        editor->selecting = false;
                        editor->move_cursor(a);
                    }
                }
                break;

            case CP_KEY_V:
                if (!world.use_nvim || world.nvim.mode == VI_INSERT) {
                    auto clipboard_contents = world.window->get_clipboard_string();
                    if (!clipboard_contents) break;

                    if (editor->selecting) {
                        auto a = editor->select_start;
                        auto b = editor->cur;
                        if (a > b) {
                            auto tmp = a;
                            a = b;
                            b = tmp;
                        }

                        editor->buf->remove(a, b);
                        editor->raw_move_cursor(a);
                        editor->selecting = false;
                    }

                    editor->insert_text_in_insert_mode(clipboard_contents);
                }
                break;

#if 0
            // We need to rethink this, because cmd+k is now used for the
            // command palette.

            case CP_KEY_J:
            case CP_KEY_K: {
                auto ed = get_current_editor();
                if (!ed) return;
                move_autocomplete_cursor(ed, key == CP_KEY_J ? 1 : -1);
                break;
            }
#endif

            case CP_KEY_W: {
                auto pane = get_current_pane();
                if (!pane) break;

                auto editor = pane->get_current_editor();
                if (!editor) {
                    // can't close the last pane
                    if (world.panes.len <= 1) break;

                    pane->cleanup();
                    world.panes.remove(world.current_pane);
                    if (world.current_pane >= world.panes.len)
                        activate_pane_by_index(world.panes.len - 1);
                } else {
                    if (!editor->ask_user_about_unsaved_changes()) break;

                    editor->cleanup();

                    pane->editors.remove(pane->current_editor);
                    if (!pane->editors.len)
                        pane->current_editor = -1;
                    else {
                        auto new_idx = pane->current_editor;
                        if (new_idx >= pane->editors.len)
                            new_idx = pane->editors.len - 1;
                        pane->focus_editor_by_index(new_idx);
                    }

                    if (world.use_nvim) send_nvim_keys("<Esc>");
                }
                break;
            }
            }
        }
        break;
    }

    case WINEV_CHAR: {
        auto ch = it->character.ch;

        // print("char = %x", ch);

        Timer t; t.init("char callback", &world.trace_next_frame); defer { t.log("done"); };

        u32 mods = 0; // normalized mod
        if (world.window->key_states[CP_KEY_LEFT_SUPER]) mods |= CP_MOD_CMD;
        if (world.window->key_states[CP_KEY_RIGHT_SUPER]) mods |= CP_MOD_CMD;
        if (world.window->key_states[CP_KEY_LEFT_CONTROL]) mods |= CP_MOD_CTRL;
        if (world.window->key_states[CP_KEY_RIGHT_CONTROL]) mods |= CP_MOD_CTRL;
        if (world.window->key_states[CP_KEY_LEFT_SHIFT]) mods |= CP_MOD_SHIFT;
        if (world.window->key_states[CP_KEY_RIGHT_SHIFT]) mods |= CP_MOD_SHIFT;
        if (world.window->key_states[CP_KEY_LEFT_ALT]) mods |= CP_MOD_ALT;
        if (world.window->key_states[CP_KEY_RIGHT_ALT]) mods |= CP_MOD_ALT;

        if (mods == CP_MOD_CTRL) return;

        ImGuiIO& io = ImGui::GetIO();
        if (ch > 0 && ch < 0x10000)
            io.AddInputCharacter((u16)ch);

        if (world.ui.keyboard_captured_by_imgui) return;
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) return;
        if (!uni_isprint(ch)) return;

        auto ed = get_current_editor();
        if (!ed) return;

        if (ed->ast_navigation.on) {
            do {
                if (mods != CP_MOD_NONE) break;
                if (!world.use_nvim) break;

                switch (tolower(ch)) {
                case 's':
                case 'c':
                case 'x': {
                    if (!world.use_nvim) break;

                    auto &nav = ed->ast_navigation;

                    if (nav.tree_version != ed->buf->tree_version) break;

                    auto start = nav.node->start();
                    auto end = nav.node->end();
                    ed->buf->remove(start, end);

                    ed->skip_next_nvim_update();

                    auto& nv = world.nvim;
                    nv.start_request_message("nvim_buf_set_lines", 5);
                    nv.writer.write_int(ed->nvim_data.buf_id);
                    nv.writer.write_int(start.y);
                    nv.writer.write_int(end.y+1);
                    nv.writer.write_bool(false);
                    nv.writer.write_array(1);
                    {
                        nv.write_line(&ed->buf->lines[start.y]);
                    }
                    nv.end_message();

                    nav.on = false;

                    ed->move_cursor(start);

                    if (tolower(ch) != 'x') send_nvim_keys("i");
                    break;
                }
                }
            } while (0);
            return;
        }

        if (world.use_nvim) {
            if (world.nvim.mode == VI_INSERT) {
                if (world.nvim.exiting_insert_mode) {
                    world.nvim.chars_after_exiting_insert_mode.append(ch);
                } else {
                    ed->type_char_in_insert_mode(ch);
                }
            } else {
                send_nvim_keys(ch == '<' ? "<LT>" : uchar_to_cstr(ch));
            }
        } else if (ed->is_modifiable()) {
            ed->delete_selection();
            ed->type_char_in_insert_mode(ch);
        }
        break;
    }
    }
}

int realmain(int argc, char **argv) {
    is_main_thread = true;

#ifdef DEBUG_BUILD
    {
        Pool mem; mem.init();
        mem.owns_address(0);
        mem.cleanup();
    }
#endif

    gargc = argc;
    gargv = argv;

    Pool tmpmem;
    tmpmem.init();
    SCOPED_MEM(&tmpmem);

    Timer t;
    t.init();

    init_platform_crap();

    t.log("init platform crap");
    if (!window_init_everything())
        return error("window init failed"), EXIT_FAILURE;
    t.log("init window everything");

#if 0
    {
        Pool pool;
        pool.init();
        SCOPED_MEM(&pool);
        random_macos_tests();
    }
#endif

    world.init();

    t.log("init world");
    SCOPED_MEM(&world.frame_mem);

    {
        SCOPED_MEM(&world.world_mem);
        world.window = alloc_object(Window);
    }

    auto init_glew = []() -> bool {
        glewExperimental = GL_TRUE;
        auto err = glewInit();
        if (err != GLEW_OK) {
            error("unable to init GLEW: %s", glewGetErrorString(err));
            return false;
        }
        return true;
    };

#if !WIN_GLFW
    {
        // init glew using a dummy context
        make_bootstrap_context();
        defer { destroy_bootstrap_context(); };
        if (!init_glew()) return EXIT_FAILURE;
    }
#endif

    if (!world.window->init(1280, 720, WINDOW_TITLE))
        return error("could not create window"), EXIT_FAILURE;

    world.window->make_context_current();
    t.log("init window");

#if WIN_GLFW
    if (!init_glew()) return EXIT_FAILURE;
    t.log("init glew");
#endif

#ifdef DEBUG_BUILD
    GHEnableDebugMode();
    t.log("enable debug mode");
#endif

    read_auth();

    t.log("read auth");

    switch (world.auth.state) {
    case AUTH_NOTHING: {
        world.auth.state = AUTH_TRIAL;
        world.auth.trial_start = get_unix_time();
        write_auth();
        tell_user("CodePerfect is free to evaluate for a 7-day trial, with access to full functionality. After that, you'll need a license for continued use.\n\nYou can buy a license at any time by selecting Help > Buy License.", "Trial");
        break;
    }

    case AUTH_TRIAL:
        if (get_unix_time() - world.auth.trial_start > 1000 * 60 * 60 * 24 * 7) {
            world.auth_error = true;
            auto res = ask_user_yes_no("Your trial has ended. A license is required for continued use.\n\nDo you want to buy one now?", NULL, "Purchase License", "No");
            if (res == ASKUSER_YES) {
                GHOpenURLInBrowser("https://codeperfect95.com/buy");
            }
        } else {
            GHAuth(NULL, NULL);
        }
        break;

    case AUTH_REGISTERED: {
        auto &auth = world.auth;
        cp_assert(auth.reg_email_len <= _countof(auth.reg_email));
        cp_assert(auth.reg_license_len <= _countof(auth.reg_license));

        auto email = cp_sprintf("%.*s", auth.reg_email_len, auth.reg_email);
        auto license = cp_sprintf("%.*s", auth.reg_license_len, auth.reg_license);
        cp_strcpy_fixed(world.authed_email, auth.reg_email);

        GHAuth((char*)email, (char*)license);
        break;
    }
    }

    {
        ccstr email = NULL;
        ccstr license = NULL;

        auto &a = world.auth;
        if (a.state == AUTH_REGISTERED) {
            email = cp_sprintf("%.*s", a.reg_email_len, a.reg_email);
            license = cp_sprintf("%.*s", a.reg_license_len, a.reg_license);
        }

        GHSendCrashReports((char*)email, (char*)license);
    }

    auto set_window_title = [&](ccstr note) {
        ccstr s = NULL;
        if (!note)
            s = cp_sprintf("%s - %s", WINDOW_TITLE, world.current_path);
        else
            s = cp_sprintf("%s (%s) - %s", WINDOW_TITLE, note, world.current_path);

        world.window->set_title(s);
    };

    auto get_window_note = [&]() -> ccstr {
        if (world.auth.state == AUTH_TRIAL) {
            auto time_elapsed = (get_unix_time() - world.auth.trial_start);
            auto days_left = 7 - floor((double)time_elapsed / (double)(1000 * 60 * 60 * 24));
            if (world.auth_error)
                return "trial expired";
            return cp_sprintf("%d days left in trial", (int)days_left);
        }
        return NULL;
    };

    set_window_title(get_window_note());
    world.window->swap_interval(0);

    t.log("set window title");

    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    {
        SCOPED_MEM(&world.world_mem);
        io.IniFilename = path_join(world.configdir, "imgui.ini");
    }

    // ImGui::StyleColorsLight();

    auto &style = ImGui::GetStyle();
    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.WindowPadding = ImVec2(7, 7);
    style.FramePadding = ImVec2(7, 2);
    style.CellPadding = ImVec2(4, 2);
    style.ItemSpacing = ImVec2(7, 3);
    style.ItemInnerSpacing = ImVec2(3, 3);
    style.TouchExtraPadding = ImVec2(0, 0);
    style.IndentSpacing = 21;
    style.ScrollbarSize = 11;
    style.GrabMinSize = 8;

    style.WindowRounding = 6;
    style.ChildRounding = 6;
    style.FrameRounding = 2;
    style.PopupRounding = 6;
    style.ScrollbarRounding = 2;
    style.GrabRounding = 2;
    style.LogSliderDeadzone = 4;
    style.TabRounding = 2;

    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    colors[ImGuiCol_Border]                 = ImVec4(0.62f, 0.62f, 0.62f, 0.50f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.22f, 0.22f, 0.22f, 0.54f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.07f, 0.07f, 0.07f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.19f, 0.40f, 0.68f, 0.96f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.17f, 0.43f, 0.78f, 0.96f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.16f, 0.40f, 0.73f, 0.96f);
    colors[ImGuiCol_Header]                 = ImVec4(0.36f, 0.36f, 0.36f, 0.31f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.36f, 0.36f, 0.36f, 0.69f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.36f, 0.36f, 0.36f, 0.50f);
    colors[ImGuiCol_Separator]              = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.28f, 0.41f, 0.58f, 0.49f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.20f, 0.41f, 0.68f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);
    colors[ImGuiCol_DockingPreview]         = ImVec4(0.26f, 0.59f, 0.98f, 0.18f);
    colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
    colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
    colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;

    io.KeyMap[ImGuiKey_LeftArrow] = CP_KEY_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = CP_KEY_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = CP_KEY_UP;
    io.KeyMap[ImGuiKey_DownArrow] = CP_KEY_DOWN;
    io.KeyMap[ImGuiKey_PageUp] = CP_KEY_PAGE_UP;
    io.KeyMap[ImGuiKey_PageDown] = CP_KEY_PAGE_DOWN;
    io.KeyMap[ImGuiKey_Home] = CP_KEY_HOME;
    io.KeyMap[ImGuiKey_End] = CP_KEY_END;
    io.KeyMap[ImGuiKey_Insert] = CP_KEY_INSERT;
    io.KeyMap[ImGuiKey_Delete] = CP_KEY_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = CP_KEY_BACKSPACE;
    io.KeyMap[ImGuiKey_Space] = CP_KEY_SPACE;
    io.KeyMap[ImGuiKey_Enter] = CP_KEY_ENTER;
    io.KeyMap[ImGuiKey_Escape] = CP_KEY_ESCAPE;
    io.KeyMap[ImGuiKey_Tab] = CP_KEY_TAB;
    io.KeyMap[ImGuiKey_A] = CP_KEY_A;
    io.KeyMap[ImGuiKey_C] = CP_KEY_C;
    io.KeyMap[ImGuiKey_V] = CP_KEY_V;
    io.KeyMap[ImGuiKey_X] = CP_KEY_X;
    io.KeyMap[ImGuiKey_Y] = CP_KEY_Y;
    io.KeyMap[ImGuiKey_Z] = CP_KEY_Z;

    io.SetClipboardTextFn = [](void*, ccstr s) { world.window->set_clipboard_string(s); };
    io.GetClipboardTextFn = [](void*) { return world.window->get_clipboard_string(); };

    t.log("set imgui vars");

    {
        struct Cursor_Pair { int x; Cursor_Type y; };

        Cursor_Pair pairs[] = {
            {ImGuiMouseCursor_Arrow, CP_CUR_ARROW},
            {ImGuiMouseCursor_TextInput, CP_CUR_IBEAM},
            {ImGuiMouseCursor_ResizeNS, CP_CUR_RESIZE_NS},
            {ImGuiMouseCursor_ResizeEW, CP_CUR_RESIZE_EW},
            {ImGuiMouseCursor_Hand, CP_CUR_POINTING_HAND},
            {ImGuiMouseCursor_ResizeAll, CP_CUR_RESIZE_ALL},
            {ImGuiMouseCursor_ResizeNESW, CP_CUR_RESIZE_NESW},
            {ImGuiMouseCursor_ResizeNWSE, CP_CUR_RESIZE_NWSE},
            {ImGuiMouseCursor_NotAllowed, CP_CUR_NOT_ALLOWED},
        };

        For (pairs)
            if (world.ui.cursors[it.x].init(it.y))
                world.ui.cursors_ready[it.x] = true;
    }

    world.ui.program = compile_program((char*)vert_glsl, vert_glsl_len, (char*)frag_glsl, frag_glsl_len);
    if (world.ui.program == -1)
        return error("could not compile shaders"), EXIT_FAILURE;

    world.ui.im_program = compile_program((char*)im_vert_glsl, im_vert_glsl_len, (char*)im_frag_glsl, im_frag_glsl_len);
    if (world.ui.im_program == -1)
        return error("could not compile imgui shaders"), EXIT_FAILURE;

    t.log("compile opengl programs");

    // grab window_size, frame_size, and display_scale
    world.window->get_size((int*)&world.window_size.x, (int*)&world.window_size.y);
    world.window->get_framebuffer_size((int*)&world.frame_size.x, (int*)&world.frame_size.y);
    world.window->get_content_scale(&world.display_scale.x, &world.display_scale.y);
    recalc_display_size();

    // now that we have world.display_size, we can call wksp.activate_pane_by_index
    activate_pane_by_index(0);

    t.log("initialize window & shit");

    // initialize & bind textures
    glGenTextures(__TEXTURE_COUNT__, world.ui.textures);
    for (u32 i = 0; i < __TEXTURE_COUNT__; i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, world.ui.textures[i]);
    }
    // from here, if we want to modify a texture, just set the active texture
    // are we going to run out of texture units?

    {
        SCOPED_FRAME();

        s32 len = 0;

        {
            ImFontConfig config;
            config.OversampleH = 3;
            config.OversampleV = 2;
            world.ui.im_font_ui = io.Fonts->AddFontFromMemoryTTF(open_sans_ttf, open_sans_ttf_len, UI_FONT_SIZE, &config);
            cp_assert(world.ui.im_font_ui);
        }

        {
            // merge font awesome into main font
            ImFontConfig config;
            config.MergeMode = true;
            config.GlyphMinAdvanceX = ICON_FONT_SIZE;
            config.GlyphOffset.y = 3;
            config.OversampleH = 3;
            config.OversampleV = 2;

            /*
            ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
            io.Fonts->AddFontFromMemoryTTF(fa_regular_400_ttf, fa_regular_400_ttf_len, ICON_FONT_SIZE, &config, icon_ranges);
            io.Fonts->AddFontFromMemoryTTF(fa_solid_900_ttf, fa_solid_900_ttf_len, ICON_FONT_SIZE, &config, icon_ranges);
            */

            ImWchar icon_ranges[] = { ICON_MIN_MD, ICON_MAX_MD, 0 };
            io.Fonts->AddFontFromMemoryTTF(material_icons_regular_ttf, material_icons_regular_ttf_len, ICON_FONT_SIZE, &config, icon_ranges);

            ImWchar icon_ranges2[] = { 0x21E7, 0x21E7+1, 0 };
            io.Fonts->AddFontFromMemoryTTF(open_sans_ttf, open_sans_ttf_len, UI_FONT_SIZE, &config, icon_ranges2);
        }

        {
            ImFontConfig config;
            config.OversampleH = 3;
            config.OversampleV = 2;
            world.ui.im_font_mono = io.Fonts->AddFontFromMemoryTTF(vera_mono_ttf, vera_mono_ttf_len, CODE_FONT_SIZE);
            cp_assert(world.ui.im_font_mono);
        }

        io.Fonts->Build();

        // init imgui texture
        u8* pixels;
        i32 width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

        glActiveTexture(GL_TEXTURE0 + TEXTURE_FONT_IMGUI);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);

    glGenVertexArrays(1, &world.ui.vao);
    glBindVertexArray(world.ui.vao);
    glGenBuffers(1, &world.ui.vbo);

    glGenVertexArrays(1, &world.ui.im_vao);
    glBindVertexArray(world.ui.im_vao);
    glGenBuffers(1, &world.ui.im_vbo);
    glGenBuffers(1, &world.ui.im_vebo);

    glGenFramebuffers(1, &world.ui.fbo);

    {
        // set opengl program attributes
        glUseProgram(world.ui.program);
        glBindVertexArray(world.ui.vao);
        glBindBuffer(GL_ARRAY_BUFFER, world.ui.vbo);

        i32 loc;

        loc = glGetAttribLocation(world.ui.program, "pos");
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, sizeof(Vert), (void*)_offsetof(Vert, x));

        loc = glGetAttribLocation(world.ui.program, "uv");
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, sizeof(Vert), (void*)_offsetof(Vert, u));

        loc = glGetAttribLocation(world.ui.program, "color");
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, sizeof(Vert), (void*)_offsetof(Vert, color));

        loc = glGetAttribLocation(world.ui.program, "mode");
        glEnableVertexAttribArray(loc);
        glVertexAttribIPointer(loc, 1, GL_INT, sizeof(Vert), (void*)_offsetof(Vert, mode));

        loc = glGetAttribLocation(world.ui.program, "texture_id");
        glEnableVertexAttribArray(loc);
        glVertexAttribIPointer(loc, 1, GL_INT, sizeof(Vert), (void*)_offsetof(Vert, texture_id));

        loc = glGetAttribLocation(world.ui.program, "round_w");
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 1, GL_FLOAT, GL_FALSE, sizeof(Vert), (void*)_offsetof(Vert, round_w));

        loc = glGetAttribLocation(world.ui.program, "round_h");
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 1, GL_FLOAT, GL_FALSE, sizeof(Vert), (void*)_offsetof(Vert, round_h));

        loc = glGetAttribLocation(world.ui.program, "round_r");
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 1, GL_FLOAT, GL_FALSE, sizeof(Vert), (void*)_offsetof(Vert, round_r));

        loc = glGetAttribLocation(world.ui.program, "round_flags");
        glEnableVertexAttribArray(loc);
        glVertexAttribIPointer(loc, 1, GL_INT, sizeof(Vert), (void*)_offsetof(Vert, round_flags));

        loc = glGetUniformLocation(world.ui.program, "projection");
        mat4f ortho_projection;
        new_ortho_matrix(ortho_projection, 0, world.frame_size.x, world.frame_size.y, 0);
        glUniformMatrix4fv(loc, 1, GL_FALSE, (float*)ortho_projection);

        for (u32 i = 0; i < __TEXTURE_COUNT__; i++) {
            loc = glGetUniformLocation(world.ui.program, cp_sprintf("tex%d", i));
            glUniform1i(loc, i);
        }
    }

    {
        // set imgui program attributes
        glUseProgram(world.ui.im_program);
        glBindVertexArray(world.ui.im_vao);
        glBindBuffer(GL_ARRAY_BUFFER, world.ui.im_vbo);

        i32 loc;

        loc = glGetAttribLocation(world.ui.im_program, "pos");
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, pos));

        loc = glGetAttribLocation(world.ui.im_program, "uv");
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, uv));

        loc = glGetAttribLocation(world.ui.im_program, "color");
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, col));

        loc = glGetUniformLocation(world.ui.im_program, "projection");
        mat4f ortho_projection;
        new_ortho_matrix(ortho_projection, 0, world.display_size.x, world.display_size.y, 0);
        glUniformMatrix4fv(loc, 1, GL_FALSE, (float*)ortho_projection);

        loc = glGetUniformLocation(world.ui.im_program, "tex");
        glUniform1i(loc, TEXTURE_FONT_IMGUI);
    }

    t.log("gl crap");

    world.start_background_threads();

    t.log("start background threads");

    auto last_frame_time = current_time_nano();

    if (world.testing.on) world.testing.ready = true;

    for (; !world.window->should_close; world.frame_index++) {
        bool was_trace_on = world.trace_next_frame;
        defer { if (was_trace_on) world.trace_next_frame = false; };

        Timer t;
        t.init("frame tracer", &world.trace_next_frame);

        if (world.auth.state == AUTH_REGISTERED && world.auth_status == GH_AUTH_WAITING) {
            auto &auth = world.auth;

            auto in_grace_period = [&](int days) {
                if (!auth.grace_period_start) {
                    auth.grace_period_start = get_unix_time();
                    write_auth();
                }
                return (get_unix_time() - auth.grace_period_start) < (1000 * 60 * 60 * 24 * days);
            };

            // TODO: timeout?
            world.auth_status = (GH_Auth_Status)GHGetAuthStatus();
            switch (world.auth_status) {
            case GH_AUTH_OK:
                auth.grace_period_start = get_unix_time();
                write_auth();
                // set_window_title(world.authed_email);
                break;
            case GH_AUTH_UNKNOWNERROR:
                // for now just do nothing, don't punish user for our fuckup
                break;
            case GH_AUTH_BADCREDS:
                set_window_title("unregistered");
                if (in_grace_period(3)) {
                    tell_user("We were unable to validate your license key. CodePerfect will continue to work for a short grace period. Please go to Help > License to enter a new license, or contact support@codeperfect95.com for help. Thanks!", "Invalid credentials");
                } else {
                    tell_user("We were unable to validate your license key. Unfortunately, the grace period has ended, so many features are now disabled. Please go to Help > License to enter a new license, or contact support@codeperfect95.com for help. Thanks!", "Invalid credentials");
                    world.auth_error = true;
                }
                break;
            case GH_AUTH_INTERNETERROR:
                set_window_title("unregistered");
                if (in_grace_period(7)) {
                    tell_user("We were unable to connect to the internet to validate your license key. CodePerfect will continue to work for a week; please connect to the internet at some point. Thanks!", "Unable to connect to internet");
                } else {
                    tell_user("We were unable to connect to the internet to validate your license key. Unfortunately, the grace period has ended, so many features are now disabled. Please connect to the internet, then restart CodePerfect.", "Unable to connect to internet");
                    world.auth_error = true;
                }
                break;
            }
        }

        t.log("auth");

        if (world.randomly_move_cursor_around) {
            if (get_current_editor()) {
                if (world.frame_index % 3 == 0) {
                    send_nvim_keys(rand() % 2 == 0 ? "{" : "}");
                }
            }
        }

        {
            GH_Message msg; ptr0(&msg);
            if (GHGetMessage(&msg)) {
                print("GHGetMessage returned: %s", msg.text);
                tell_user(msg.text, msg.title);
                if (msg.is_panic) return EXIT_FAILURE;
            }
        }

        auto frame_start_time = current_time_nano();

        world.frame_mem.reset();
        SCOPED_MEM(&world.frame_mem);

        {
            world.fst_mem.reset();
            SCOPED_MEM(&world.fst_mem);
            world.fst.init("frameskip");
            world.fst.log_output = alloc_list<char>();
        }

        {
            // Process message queue.
            auto messages = world.message_queue.start();
            defer { world.message_queue.end(); };

            For (*messages) {
                switch (it.type) {
                case MTM_FOCUS_APP_DEBUGGER:
                    if (it.focus_app_debugger_pid)
                        if (it.focus_app_debugger_pid == get_current_focused_window_pid())
                            if (!world.window->is_focused())
                                world.window->focus();
                    break;

                case MTM_EXIT:
                    exit(it.exit_code);
                    break;

                case MTM_PANIC: {
                    // When we get here, we already called cp_panic in the
                    // other thread. There's no point in throwing a new
                    // exception, since the stacktrace will just be this
                    // thread's, so just exit.
                    tell_user(NULL, it.panic_message);
                    write_stacktrace_to_file(it.panic_stacktrace);
                    exit(1);
                    break;
                }

                case MTM_TELL_USER:
                    tell_user(it.tell_user_text, it.tell_user_title);
                    break;

                case MTM_RELOAD_EDITOR: {
                    auto editor = find_editor_by_id(it.reload_editor_id);
                    if (editor) editor->reload_file(false);
                    break;
                }

                case MTM_NVIM_MESSAGE: {
                    auto &nv = world.nvim;
                    nv.handle_message_from_main_thread(&it.nvim_message);
                    break;
                }
                case MTM_GOTO_FILEPOS:
                    goto_file_and_pos(it.goto_file, it.goto_pos);
                    break;
                }

                fstlog("handle message of type %s", main_thread_message_type_str(it.type));
            }
        }

        fstlog("end message queue");
        t.log("message queue");

        // Process filesystem changes.

        {
            Fs_Event event;
            for (u32 items_processed = 0; items_processed < 10 && world.fswatch.next_event(&event); items_processed++) {
                if (is_git_folder(event.filepath)) continue;
                if (event.filepath[0] == '\0') continue;

                auto filepath = path_join(world.current_path, event.filepath);

                auto filedir = filepath;
                auto res = check_path(filedir);

                if (res == CPR_NONEXISTENT)
                    For (world.panes)
                        For (it.editors)
                            if (are_filepaths_equal(it.filepath, filepath))
                                it.file_was_deleted = true;

                if (res != CPR_DIRECTORY) filedir = cp_dirname(filedir);
                if (streq(filedir, ".")) filedir = "";

                reload_file_subtree(get_path_relative_to(filedir, world.current_path));

                auto editor = find_editor_by_filepath(filepath);
                if (editor) editor->reload_file(true);

                auto should_handle_fsevent = [&]() {
                    if (res == CPR_DIRECTORY) return true;
                    if (str_ends_with(filepath, ".go")) return true;
                    if (streq(cp_basename(filepath), "go.mod")) return true;

                    return false;
                };

                if (should_handle_fsevent()) {
                    world.indexer.message_queue.add([&](auto msg) {
                        msg->type = GOMSG_FSEVENT;
                        msg->fsevent_filepath = cp_strdup(event.filepath);
                    });
                }

                fstlog("fsevent %s", event.filepath);
            }
        }

        fstlog("end filesystem changes");
        t.log("filesystem changes");

        glDisable(GL_SCISSOR_TEST);
        {
            auto bg = global_colors.background;
            glClearColor(bg.r, bg.g, bg.b, 1.0);
        }
        glClear(GL_COLOR_BUFFER_BIT);

        {
            auto &io = ImGui::GetIO();
            // Send info to UI and ImGui.
            io.DisplaySize = ImVec2((float)world.display_size.x, (float)world.display_size.y);

            auto scale = world.get_display_scale();
            io.DisplayFramebufferScale = ImVec2(scale.x, scale.y);

            auto now = current_time_nano();
            io.DeltaTime = (double)(now - last_frame_time) / (double)1000000000;
            last_frame_time = now;

            io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
            if (world.window->is_focused()) {
                auto &pos = world.ui.mouse_pos;
                io.MousePos = ImVec2((float)pos.x, (float)pos.y);
            }

            for (i32 i = 0; i < IM_ARRAYSIZE(io.MouseDown); i++) {
                bool down = world.ui.mouse_just_pressed[i] || world.window->mouse_states[i];
                io.MouseDown[i] = down;
                world.ui.mouse_down[i] = down && !world.ui.mouse_captured_by_imgui;
                world.ui.mouse_just_pressed[i] = false;
                world.ui.mouse_just_released[i] = false;
            }

            if (!(io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)) {
                auto imgui_to_cp_cursor = [&](ImGuiMouseCursor cur) {
                    if (!world.ui.cursors_ready[cur])
                        cur = ImGuiMouseCursor_Arrow;
                    cp_assert(world.ui.cursors_ready[cur]);
                    return &world.ui.cursors[cur];
                };

                auto get_imgui_cursor = [&]() -> ImGuiMouseCursor {
                    ImGuiMouseCursor cur = ImGui::GetMouseCursor();
                    if (cur != ImGuiMouseCursor_Arrow)
                        return cur;
                    if (ui.hover.ready && ui.hover.cursor != ImGuiMouseCursor_Arrow)
                        return ui.hover.cursor;
                    return ImGuiMouseCursor_Arrow;
                };

                world.window->set_cursor(imgui_to_cp_cursor(get_imgui_cursor()));
            }
        }

        fstlog("send shit to ui");

        poll_window_events();

        fstlog("poll window events");
        For (world.window->events) {
            handle_window_event(&it);
            fstlog("handle window event %s", window_event_type_str(it.type));
        }

        do {
            auto &t = world.testing;
            if (!t.on) break;
            if (!t.inject_event) break;

            t.inject_event = false;
            handle_window_event(&t.event);
            t.processed_event = true;
        } while (0);

        fstlog("poll window events");
        world.window->events.len = 0;

        // cp_panic("leld0ingz");

        ui.draw_everything();
        fstlog("draw everything");
        ui.end_frame(); // end frame after polling events, so our event callbacks have access to imgui
        fstlog("end frame");

        world.window->swap_buffers();
        fstlog("swap buffers");

        if (!world.turn_off_framerate_cap) {
            auto timeleft = [&]() -> i32 {
                auto budget = (1000.f / FRAME_RATE_CAP);
                auto spent = (current_time_nano() - frame_start_time) / 1000000.f;
                return (i32)((i64)budget - (i64)spent);
            };

            while (true) {
                auto rem = timeleft();
                if (rem > 4 + 2) {
                    auto messages = world.message_queue.start();
                    defer {
                        if (messages->len)
                            world.message_queue.softend();
                        else
                            world.message_queue.end();
                    };

                    int i = 0;
                    while (i < messages->len) {
                        auto &it = messages->at(i);
                        if (it.type == MTM_NVIM_MESSAGE) {
                            auto &nv = world.nvim;
                            nv.handle_message_from_main_thread(&it.nvim_message);
                            messages->remove(i);
                        } else i++;
                    }

                    // sleep what's rest of the 4 milliseconds
                    auto rem2 = timeleft();
                    if (rem - rem2 < 4) sleep_milliseconds(4 - (rem - timeleft()));
                    continue;
                }

                if (rem > 0) {
                    sleep_milliseconds((u32)rem);
                    break;
                }

                auto fs = world.frameskips.append();
                fs->timestamp = current_time_milli();
                fs->ms_over = -rem;

#ifdef DEBUG_BUILD
                // print("frameskip!!!!!!!111111one =============");
                world.fst.log_output->append('\0');
                // print("%s", world.fst.log_output->items);
#endif

                break;
            }
        }

        {
            auto &fs = world.frameskips;
            auto now = current_time_milli();

            while (fs.len && fs[0].timestamp + 2000 < now)
                fs.remove((u32)0);
        }
    }

    return EXIT_SUCCESS;
}
