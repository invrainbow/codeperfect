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

#include "os.hpp"
#include "common.hpp"
#include "debugger.hpp"
#include "editor.hpp"
#include "list.hpp"
#include "go.hpp"
#include "utils.hpp"
#include "world.hpp"
#include "nvim.hpp"
#include "ui.hpp"
#include "fzy_match.h"
#include "settings.hpp"
#include "unicode.hpp"
#include "defer.hpp"

#include "imgui.h"
#include "fonts.hpp"
#include "icons.h"
#include "binaries.h"

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

void handle_window_event(Window_Event *it) {
    switch (it->type) {
    case WINEV_FOCUS:
        break;

    case WINEV_BLUR: {
        auto &io = ImGui::GetIO();

        ptr0(&io.KeysDown);
        io.KeyCtrl = false;
        io.KeyShift = false;
        io.KeyAlt = false;
        io.KeySuper = false;

        break;
    }

    case WINEV_WINDOW_SIZE: {
        auto w = it->window_size.w;
        auto h = it->window_size.h;

        Timer t; t.init("windowsize callback", &world.trace_next_frame); defer { t.log("done"); };

        world.window_size.x = w;
        world.window_size.y = h;

        mat4f projection;
        new_ortho_matrix(projection, 0, w, h, 0);
        glUseProgram(world.ui.im_program);
        glUniformMatrix4fv(glGetUniformLocation(world.ui.im_program, "projection"), 1, GL_FALSE, (float*)projection);

        // clear frame
        auto bgcolor = global_colors.background;
        glClearColor(bgcolor.r, bgcolor.g, bgcolor.b, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        world.window->swap_buffers();
        break;
    }

    case WINEV_FRAME_SIZE: {
        auto w = it->frame_size.w;
        auto h = it->frame_size.h;

        Timer t; t.init("framebuffersize callback", &world.trace_next_frame); defer { t.log("done"); };

        world.display_size.x = w;
        world.display_size.y = h;

        mat4f projection;
        new_ortho_matrix(projection, 0, w, h, 0);
        glUseProgram(world.ui.program);
        glUniformMatrix4fv(glGetUniformLocation(world.ui.program, "projection"), 1, GL_FALSE, (float*)projection);

        // clear frame
        auto bgcolor = global_colors.background;
        glClearColor(bgcolor.r, bgcolor.g, bgcolor.b, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        world.window->swap_buffers();
        break;
    }

    case WINEV_MOUSE_MOVE: {
        auto x = it->mouse_move.x;
        auto y = it->mouse_move.y;

        Timer t; t.init("cursorpos callback", &world.trace_next_frame); defer { t.log("done"); };

        // world.ui.mouse_delta.x = x - world.ui.mouse_pos.x;
        // world.ui.mouse_delta.y = y - world.ui.mouse_pos.y;
        world.ui.mouse_pos.x = x;
        world.ui.mouse_pos.y = y;

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

            if (delta < (100 - pane1.width))
                delta = 100 - pane1.width;
            if (delta > pane2.width - 100)
                delta = pane2.width - 100;

            pane1.width += delta;
            pane2.width -= delta;
        }
        break;
    }

    case WINEV_MOUSE: {
        auto button = it->mouse.button;
        auto action = it->mouse.action;
        auto mods = it->mouse.mods;

        Timer t; t.init("mousebutton callback", &world.trace_next_frame); defer { t.log("done"); };

        // Don't set world.ui.mouse_down here. We set it based on
        // world.ui.mouse_just_pressed and some additional logic below, while
        // we're setting io.MouseDown for ImGui.

        if (button < 0 || button >= IM_ARRAYSIZE(world.ui.mouse_just_pressed))
            return;

        switch (action) {
        case CP_ACTION_PRESS:
            world.ui.mouse_just_pressed[button] = true;
            break;
        case CP_ACTION_RELEASE:
            world.ui.mouse_just_released[button] = true;
            break;
        }
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
        break;
    }

    case WINEV_KEY: {
        auto key = it->key.key;
        auto action = it->key.action;

        Timer t; t.init("key callback", &world.trace_next_frame); defer { t.log("done"); };

        ImGuiIO& io = ImGui::GetIO();
        if (action == CP_ACTION_PRESS) io.KeysDown[key] = true;
        if (action == CP_ACTION_RELEASE) io.KeysDown[key] = false;

        io.KeyCtrl = io.KeysDown[CP_KEY_LEFT_CONTROL] || io.KeysDown[CP_KEY_RIGHT_CONTROL];
        io.KeyShift = io.KeysDown[CP_KEY_LEFT_SHIFT] || io.KeysDown[CP_KEY_RIGHT_SHIFT];
        io.KeyAlt = io.KeysDown[CP_KEY_LEFT_ALT] || io.KeysDown[CP_KEY_RIGHT_ALT];
        io.KeySuper = io.KeysDown[CP_KEY_LEFT_SUPER] || io.KeysDown[CP_KEY_RIGHT_SUPER];

        if (action != CP_ACTION_PRESS && action != CP_ACTION_REPEAT) return;

        // handle global keys

        auto keymods = ui.imgui_get_keymods();

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

            case CP_KEY_K: {
                SCOPED_MEM(&world.run_command_mem);
                world.run_command_mem.reset();

                auto &wnd = world.wnd_command;
                wnd.query[0] = '\0';
                wnd.actions = alloc_list<Command>();
                wnd.filtered_results = alloc_list<int>();
                wnd.selection = 0;

                for (int i = 0; i < _CMD_COUNT_; i++) {
                    auto fuck_cpp = (Command)i;
                    if (is_command_enabled(fuck_cpp))
                        wnd.actions->append(fuck_cpp);
                }

                world.wnd_command.show = true;
                break;
            }
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

        // handle non-global keys

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

            editor->delete_selection();
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

        auto alt_move = [&](bool back) -> cur2 {
            auto it = editor->iter();

            if (back) {
                if (it.bof()) return it.pos;
                it.prev();
            }

            auto done = [&]() { return back ? it.bof() : it.eof(); };
            auto advance = [&]() { back ? it.prev() : it.next(); };

            for (; !done(); advance())
                if (!isspace(it.peek()))
                    break;

            bool isid = isident(it.peek());
            for (; !done(); advance()) {
                if (isident(it.peek()) != isid || isspace(it.peek())) {
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
                auto new_cur = alt_move(true);
                while (editor->cur > new_cur)
                    editor->backspace_in_insert_mode(1, 0);
            } else {
                // if we're at beginning of line
                if (!editor->cur.x) {
                    auto back1 = editor->buf->dec_cur(editor->cur);
                    editor->buf->remove(back1, editor->cur);
                    if (world.use_nvim) {
                        if (back1 < editor->nvim_insert.start) {
                            editor->nvim_insert.start = back1;
                            editor->nvim_insert.deleted_graphemes++;
                        }
                    }
                    editor->raw_move_cursor(back1);
                } else {
                    editor->backspace_in_insert_mode(1, 0); // erase one grapheme
                }
            }

            editor->update_autocomplete(false);
            editor->update_parameter_hint();
        };

        // handle movement
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
                        cur = alt_move(key == CP_KEY_LEFT);
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

        // handle enter, backspace, tab
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
                break;
#else
                handle_tab();
                return;
#endif
            }
        }

        switch (keymods) {
        case CP_MOD_SHIFT:
            switch (key) {
            case CP_KEY_ESCAPE:
                if (!editor->trigger_escape())
                    send_nvim_keys("<S-Esc>");
                break;
            }
            break;

        case CP_MOD_CTRL: {
            bool handled = false;

            switch (key) {
            case CP_KEY_ENTER:
                /*
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
                if (!editor->trigger_escape())
                    send_nvim_keys("<C-Esc>");
                break;

            case CP_KEY_TAB:
                goto_next_tab();
                break;

            case CP_KEY_R:
            case CP_KEY_O:
            case CP_KEY_I:
            case CP_KEY_D:
            case CP_KEY_U:
                if (world.use_nvim) {
                    if (world.nvim.mode != VI_INSERT) {
                        SCOPED_FRAME();
                        send_nvim_keys(cp_sprintf("<C-%c>", tolower((char)key)));
                    }
                }
                break;
            case CP_KEY_Y:
                if (!editor) break;
                if (world.nvim.mode == VI_INSERT) break;
                if (editor->view.y > 0) {
                    editor->view.y--;
                    editor->ensure_cursor_on_screen();
                }
                break;
            case CP_KEY_E:
                if (!editor) break;
                if (world.nvim.mode == VI_INSERT) break;
                if (editor->view.y + 1 < editor->buf->lines.len) {
                    editor->view.y++;
                    editor->ensure_cursor_on_screen();
                }
                break;
            case CP_KEY_V:
                if (world.use_nvim)
                    if (world.nvim.mode != VI_INSERT)
                        send_nvim_keys("<C-v>");
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
                if (!editor->trigger_escape())
                    send_nvim_keys("<C-S-Esc>");
                break;
            case CP_KEY_TAB:
                goto_previous_tab();
                break;
            case CP_KEY_SPACE: {
                auto ed = get_current_editor();
                if (!ed) break;
                ed->trigger_parameter_hint();
                break;
            }
            }
            break;

        case CP_MOD_NONE: {
            if (!editor) break;

            switch (key) {
            case CP_KEY_LEFT:
            case CP_KEY_RIGHT:
                if (world.use_nvim && world.nvim.mode != VI_INSERT)
                    send_nvim_keys(key == CP_KEY_LEFT ? "<Left>" : "<Right>");
                break;

            case CP_KEY_DOWN:
            case CP_KEY_UP:
                if (world.use_nvim) {
                    if (world.nvim.mode == VI_INSERT)
                        move_autocomplete_cursor(editor, key == CP_KEY_DOWN ? 1 : -1);
                    else
                        send_nvim_keys(key == CP_KEY_DOWN ? "<Down>" : "<Up>");
                }
                break;

            case CP_KEY_ESCAPE:
                if (editor->trigger_escape()) break;
                if (world.use_nvim)
                    send_nvim_keys("<Esc>");
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
                    // clear undo history
                    nv.start_request_message("nvim_call_function", 2);
                    nv.writer.write_string("CPCopyVisual");
                    nv.writer.write_array(0);
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
                    set_clipboard_string(s);

                    if (key == CP_KEY_X) {
                        editor->buf->remove(a, b);
                        editor->selecting = false;
                        editor->move_cursor(a);
                    }
                }
                break;

            case CP_KEY_V:
                if (!world.use_nvim || world.nvim.mode == VI_INSERT) {
                    auto clipboard_contents = get_clipboard_string();
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

            /*
            // We need to rethink this, because cmd+k is now used for the
            // command palette.

            case CP_KEY_J:
            case CP_KEY_K: {
                auto ed = get_current_editor();
                if (!ed) return;
                move_autocomplete_cursor(ed, key == CP_KEY_J ? 1 : -1);
                break;
            }
            */

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
                    if (!editor->ask_user_about_unsaved_changes())
                        break;

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

                    if (world.use_nvim)
                        send_nvim_keys("<Esc>");
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
        } else {
            ed->delete_selection();
            ed->type_char_in_insert_mode(ch);
        }
        break;
    }

    }
}


#if OS_WINBLOWS
// i can't believe this shit is necessary:
// https://github.com/golang/go/issues/42190#issuecomment-1114628523
//
// and what happens if (after god intervenes and injects some iq points into
// them) they decide to fix it, and call it themselves? will we be
// double-calling? will that break things?
extern "C" {
    extern __declspec(dllexport) void _rt0_amd64_windows_lib();
}
#else
#define _rt0_amd64_windows_lib()
#endif

int main(int argc, char **argv) {
    _rt0_amd64_windows_lib();

    gargc = argc;
    gargv = argv;

    is_main_thread = true;

    Timer t;
    t.init();

    init_platform_specific_crap();

    if (!window_init_everything())
        return error("window init failed"), EXIT_FAILURE;

#if 0
    {
        Pool pool;
        pool.init();
        SCOPED_MEM(&pool);
        random_macos_tests();
    }
#endif

    world.init();
    SCOPED_MEM(&world.frame_mem);

    {
        SCOPED_MEM(&world.world_mem);
        world.window = alloc_object(Window);
    }

    {
        // init glew using a dummy context
        make_bootstrap_context();
        defer { destroy_bootstrap_context(); };

        glewExperimental = GL_TRUE;
        auto err = glewInit();
        if (err != GLEW_OK)
            return error("unable to init GLEW: %s", glewGetErrorString(err)), EXIT_FAILURE;
    }

    if (!world.window->init(1280, 720, WINDOW_TITLE))
        return error("could not create window"), EXIT_FAILURE;

#ifdef DEBUG_BUILD
    GHEnableDebugMode();
#endif

    read_auth();

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
            auto res = ask_user_yes_no(NULL, "Your trial has ended. A license is required for continued use.\n\nWould you like to purchase one now?", "Purchase License", "No");
            if (res == ASKUSER_YES) {
                open_webbrowser("https://codeperfect95.com/buy-license");
            }
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

    GHUpdate();

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
    world.window->make_context_current();
    world.window->swap_interval(0);

    auto configdir = GHGetConfigDir();
    if (!configdir)
        return error("unable to get config dir"), EXIT_FAILURE;

    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    {
        SCOPED_MEM(&world.world_mem);
        io.IniFilename = path_join(configdir, "imgui.ini");
    }

    // ImGui::StyleColorsLight();

    auto &style = ImGui::GetStyle();
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
    io.KeyMap[ImGuiKey_A] = CP_KEY_A;
    io.KeyMap[ImGuiKey_C] = CP_KEY_C;
    io.KeyMap[ImGuiKey_V] = CP_KEY_V;
    io.KeyMap[ImGuiKey_X] = CP_KEY_X;
    io.KeyMap[ImGuiKey_Y] = CP_KEY_Y;
    io.KeyMap[ImGuiKey_Z] = CP_KEY_Z;

    io.SetClipboardTextFn = [](void*, ccstr s) { set_clipboard_string(s); };
    io.GetClipboardTextFn = [](void*) { return get_clipboard_string(); };

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

        For (pairs) {
            if (!world.ui.cursors[it.x].init(it.y))
                return error("could not initialize cursor"), EXIT_FAILURE;
            world.ui.cursors_ready[it.x] = true;
        }
    }

    world.ui.program = compile_program((char*)vert_glsl, vert_glsl_len, (char*)frag_glsl, frag_glsl_len);
    if (world.ui.program == -1)
        return error("could not compile shaders"), EXIT_FAILURE;

    world.ui.im_program = compile_program((char*)im_vert_glsl, im_vert_glsl_len, (char*)im_frag_glsl, im_frag_glsl_len);
    if (world.ui.im_program == -1)
        return error("could not compile imgui shaders"), EXIT_FAILURE;

    // grab window_size, display_size, and display_scale
    world.window->get_size((int*)&world.window_size.x, (int*)&world.window_size.y);
    world.window->get_framebuffer_size((int*)&world.display_size.x, (int*)&world.display_size.y);
    world.window->get_content_scale(&world.display_scale.x, &world.display_scale.y);

    // now that we have world.display_size, we can call wksp.activate_pane_by_index
    activate_pane_by_index(0);

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

        world.ui.im_font_ui = io.Fonts->AddFontFromMemoryTTF(open_sans_ttf, open_sans_ttf_len, UI_FONT_SIZE);
        cp_assert(world.ui.im_font_ui);

        {
            // merge font awesome into main font
            ImFontConfig config;
            config.MergeMode = true;
            config.GlyphMinAdvanceX = ICON_FONT_SIZE;
            config.GlyphOffset.y = 3;

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

        world.ui.im_font_mono = io.Fonts->AddFontFromMemoryTTF(vera_mono_ttf, vera_mono_ttf_len, CODE_FONT_SIZE);
        cp_assert(world.ui.im_font_mono);

        /*
        if (!world.font.init((u8*)vera_mono_ttf, CODE_FONT_SIZE, TEXTURE_FONT))
            cp_panic("unable to load code font");
        */

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

        loc = glGetUniformLocation(world.ui.program, "projection");
        mat4f ortho_projection;
        new_ortho_matrix(ortho_projection, 0, world.display_size.x, world.display_size.y, 0);
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
        new_ortho_matrix(ortho_projection, 0, world.window_size.x, world.window_size.y, 0);
        glUniformMatrix4fv(loc, 1, GL_FALSE, (float*)ortho_projection);

        loc = glGetUniformLocation(world.ui.im_program, "tex");
        glUniform1i(loc, TEXTURE_FONT_IMGUI);
    }

    world.start_background_threads();

    t.log("initialize everything");

    auto last_frame_time = current_time_nano();

    for (; !world.window->should_close; world.frame_index++) {
        bool was_trace_on = world.trace_next_frame;
        defer { if (was_trace_on) world.trace_next_frame = false; };

        Timer t;
        t.init("frame tracer", &world.trace_next_frame);

        if (world.auth_status == GH_AUTH_WAITING) {
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
                if (msg.is_panic)
                    return EXIT_FAILURE;
            }
        }

        auto frame_start_time = current_time_nano();

        world.frame_mem.reset();

        SCOPED_MEM(&world.frame_mem);

        {
            // Process message queue.
            auto messages = world.message_queue.start();
            defer { world.message_queue.end(); };

            For (*messages) {
                switch (it.type) {
                case MTM_PANIC:
                    cp_panic(it.panic_message);
                    break;

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
            }
        }

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

                if (res != CPR_DIRECTORY)
                    filedir = cp_dirname(filedir);
                if (streq(filedir, "."))
                    filedir = "";

                reload_file_subtree(get_path_relative_to(filedir, world.current_path));

                auto editor = find_editor_by_filepath(filepath);
                if (editor)
                    editor->reload_file(true);

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
            }
        }

        t.log("filesystem changes");

        glDisable(GL_SCISSOR_TEST);
        {
            auto bg = global_colors.background;
            glClearColor(bg.r, bg.g, bg.b, 1.0);
        }
        glClear(GL_COLOR_BUFFER_BIT);

        {
            // Send info to UI and ImGui.
            io.DisplaySize = ImVec2((float)world.window_size.x, (float)world.window_size.y);
            io.DisplayFramebufferScale = ImVec2(world.display_scale.x, world.display_scale.y);

            auto now = current_time_nano();
            io.DeltaTime = (double)(now - last_frame_time) / (double)1000000000;
            last_frame_time = now;

            io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
            if (world.window->is_focused()) {
                double x, y;
                world.window->get_cursor_pos(&x, &y);
                io.MousePos = ImVec2((float)x, (float)y);
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

        poll_window_events();
        For (world.window->events)
            handle_window_event(&it);
        world.window->events.len = 0;

        ui.draw_everything();
        ui.end_frame(); // end frame after polling events, so our event callbacks have access to imgui

        world.window->swap_buffers();

        if (!world.turn_off_framerate_cap) {
            // wait until next frame
            auto budget = (1000.f / FRAME_RATE_CAP);
            auto spent = (current_time_nano() - frame_start_time) / 1000000.f;
            if (budget > spent) {
                sleep_milliseconds((u32)(budget - spent));
            } else {
                auto fs = world.frameskips.append();
                fs->timestamp = current_time_milli();
                fs->ms_over = spent - budget;
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
