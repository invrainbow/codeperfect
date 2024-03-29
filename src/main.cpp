#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <functional>
#include <inttypes.h>
#include <math.h>
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
#include "os.hpp"
#include "settings.hpp"
#include "ui.hpp"
#include "unicode.hpp"
#include "utils.hpp"
#include "world.hpp"
#include "copy.hpp"

#include "binaries.h"
#include "fzy_match.h"
#include "icons.h"
#include "imgui.h"
#include "tests.hpp"

static const char WINDOW_TITLE[] = "CodePerfect";

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

void im_update_keymod_states() {
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiMod_Ctrl, world.window->key_states[CP_KEY_LEFT_CONTROL] || world.window->key_states[CP_KEY_RIGHT_CONTROL]);
    io.AddKeyEvent(ImGuiMod_Shift, world.window->key_states[CP_KEY_LEFT_SHIFT] || world.window->key_states[CP_KEY_RIGHT_SHIFT]);
    io.AddKeyEvent(ImGuiMod_Alt,  world.window->key_states[CP_KEY_LEFT_ALT] || world.window->key_states[CP_KEY_RIGHT_ALT]);
    io.AddKeyEvent(ImGuiMod_Super, world.window->key_states[CP_KEY_LEFT_SUPER] || world.window->key_states[CP_KEY_RIGHT_SUPER]);
}

bool is_vim_macro_running() {
    return world.vim.on && world.vim.macro_state == MACRO_RUNNING;
}

// big handler, pulling out just to reduce indent
void handle_key_event(Window_Event *it) {
    Timer t; t.init("key callback", &world.trace_next_frame); defer { t.log("done"); };

    auto key = it->key.key;
    auto keymods = it->key.mods;
    auto press = it->key.press;
    // print("key = %d, mods = %d", key, keymods);

    // disable inputs if macro is running, this supercedes even imgui.
    if (is_vim_macro_running()) {
        // There should be only one editor with macro running, but just disable all.
        if (press && keymods == CP_MOD_CTRL && key == CP_KEY_C)
            world.vim.macro_state = MACRO_IDLE; // do we need to do anything else?
        return;
    }

    // imgui shit
    im_update_keymod_states();
    ImGui::GetIO().AddKeyEvent(cp_key_to_imgui_key((Key)key), press);

    if (!press) return;

    world.konami->append(key);
    while (world.konami->len > 10)
        world.konami->remove((int)0);

    auto is_konami = [&]() {
        if (world.konami->len != 10) return false;

        int secret[] = { CP_KEY_UP, CP_KEY_UP, CP_KEY_DOWN, CP_KEY_DOWN, CP_KEY_LEFT, CP_KEY_RIGHT, CP_KEY_LEFT, CP_KEY_RIGHT, CP_KEY_B, CP_KEY_A };
        for (int i = 0; i < 10; i++)
            if (world.konami->at(i) != secret[i])
                return false;
        return true;
    };

    if (is_konami())
        if (ask_user_yes_no("This is an easter egg that lets developers trigger a crash in release mode. Do you want to do that now?", "Easter egg", "Yes", "No") == ASKUSER_YES)
            cp_panic("panicking");

    // handle global keys

    if (keymods == CP_MOD_PRIMARY) {
        switch (key) {
        case CP_KEY_1:
        case CP_KEY_2:
        case CP_KEY_3:
        case CP_KEY_4:
            activate_pane_by_index(key - CP_KEY_1);
            world.cmd_unfocus_all_windows = true;
            return;
#ifndef RELEASE_MODE
        case CP_KEY_F12:
            world.windows_open.im_demo ^= 1;
            return;
#endif
        }
    }

    if (!keymods && key == CP_KEY_ESCAPE) {
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) {
            world.cmd_unfocus_all_windows = true; // see if this causes any sync problems
            return;
        }
    }

    // commands
    for (int i = 0; i < _CMD_COUNT_; i++) {
        auto cmd = (Command)i;
        auto info = command_info_table[cmd];
        if (!info.has_shortcut(keymods, key)) continue;
        if (!is_command_enabled(cmd)) continue;

        if (!info.allow_shortcut_when_imgui_focused)
            if (is_imgui_hogging_keyboard())
                continue;

        handle_command(cmd, false);
        return;
    }

    if (is_imgui_hogging_keyboard()) return;

    // handle editor-level shit now
    // ============================
    auto editor = get_current_editor();
    if (!editor) return;

    // handle ast navigation
    if (editor->ast_navigation.on) {
        switch (keymods) {
        case CP_MOD_SHIFT:
            switch (key) {
            case CP_KEY_DOWN:
            case CP_KEY_RIGHT:
            case CP_KEY_J:
            case CP_KEY_L:
                editor->ast_navigate_in();
                return;
            case CP_KEY_UP:
            case CP_KEY_LEFT:
            case CP_KEY_H:
            case CP_KEY_K:
                editor->ast_navigate_out();
                return;
            }
            break;
        case CP_MOD_NONE:
            switch (key) {
            case CP_KEY_DOWN:
            case CP_KEY_RIGHT:
            case CP_KEY_J:
            case CP_KEY_L:
                editor->ast_navigate_next();
                return;

            case CP_KEY_UP:
            case CP_KEY_LEFT:
            case CP_KEY_H:
            case CP_KEY_K:
                editor->ast_navigate_prev();
                return;

            case CP_KEY_ESCAPE:
                editor->ast_navigation.on = false;
                return;

            case CP_KEY_BACKSPACE: {
                auto node = editor->get_selected_ast_node();
                if (!node) break;

                auto a = node->start();
                auto b = node->end();
                if (a == b) break;

                editor->buf->remove(a, b);
                editor->ast_navigation.on = false;
                break;
            }

            }
            break;
        }
        return;
    }

    // always run before vim
    // ---------------------

    switch (key) {
    case CP_KEY_DOWN:
    case CP_KEY_UP:
        if (move_autocomplete_cursor(editor, key == CP_KEY_DOWN ? 1 : -1))
            return;
        // pass through
        break;
    }

    switch (keymods) {
    case CP_MOD_CTRL:
        switch (key) {
        case CP_KEY_SPACE:
            editor->trigger_autocomplete(false, false);
            return;
        }
        break;

    case CP_MOD_CTRL | CP_MOD_SHIFT:
        switch (key) {
        case CP_KEY_SPACE:
            editor->trigger_parameter_hint();
            return;
        }
        break;
    }

    if (keymods == CP_MOD_PRIMARY) {
        switch (key) {
        case CP_KEY_LEFT_BRACKET:
        case CP_KEY_RIGHT_BRACKET: {
            if (world.vim.on) {
                auto mode = world.vim_mode();
                if (mode == VI_INSERT || mode == VI_REPLACE)
                    break;
            }

            List<Selection_Range> *ranges = NULL;

            auto selection = editor->get_selection();
            if (!selection) {
                Selection_Range range; ptr0(&range);
                range.start = editor->cur;
                range.end = editor->buf->inc_gr(editor->cur);

                selection = new_object(Selection);
                selection->type = SEL_CHAR;
                selection->ranges = new_list(Selection_Range);
                selection->ranges->append(&range);
            }

            int y1 = selection->ranges->at(0).start.y;
            int y2 = selection->ranges->last()->end.y;
            editor->indent_block(y1, y2, key == CP_KEY_LEFT_BRACKET ? -1 : 1);
            return;
        }
        }
    }

    if (key == CP_KEY_TAB && (keymods == CP_MOD_NONE || keymods == CP_MOD_SHIFT)) {
        auto selection = editor->get_selection();
        if (selection) {
            bool dedent = (keymods & CP_MOD_SHIFT);
            int y1 = selection->ranges->at(0).start.y;
            int y2 = selection->ranges->last()->end.y;
            editor->indent_block(y1, y2, dedent ? -1 : 1);
            return;
        }
    }

    // send to vim
    //
    // always send to vim, even in insert mode, because vim needs to record
    // inputs. if vim didn't handle, such as in insert mode (which it defers to
    // us), it'll return false, and then we can manually handle below. vim will
    // simply ignore keys in insert mode
    //
    // the handlers above this run no matter what
    // the handlers below this don't run if vim intercepts first
    if (world.vim.on) {
        switch (key) {
        case CP_KEY_ENTER:
        case CP_KEY_BACKSPACE:
        case CP_KEY_ESCAPE:
            if (editor->vim_handle_key(key, 0))
                return;
            break;
        }

        switch (keymods) {
        case CP_MOD_NONE:
            switch (key) {
            case CP_KEY_TAB:
            case CP_KEY_UP:
            case CP_KEY_DOWN:
            case CP_KEY_LEFT:
            case CP_KEY_RIGHT:
                if (editor->vim_handle_key(key, 0))
                    return;
                break;
            }
            break;
        case CP_MOD_CTRL:
            if (key != CP_KEY_LEFT_CONTROL && key != CP_KEY_RIGHT_CONTROL)
                if (editor->vim_handle_key(key, keymods))
                    return;
            break;
        }
    }

    // run after vim
    // -------------
    switch (key) {
    case CP_KEY_ESCAPE:
        editor->handle_escape();
        return;
    }

    // now handle insert mode shit
    // ===========================

    if (world.vim.on) {
        switch (world.vim_mode()) {
        case VI_INSERT:
        case VI_REPLACE:
            break;
        default:
            return;
        }
    }

    switch (key) {
    case CP_KEY_ENTER:
        editor->handle_type_enter();
        break;
    case CP_KEY_BACKSPACE:
        editor->handle_type_backspace(keymods);
        break;
    case CP_KEY_TAB:
        editor->handle_type_tab(keymods);
        break;
    }

    // now handle insert mode non-vim movement
    // =======================================
    if (world.vim.on) return;

    bool handled = false;

    auto buf = editor->buf;
    auto cur = editor->cur;
    bool was_updown_movement;

    auto handle_cursor_left = [&]() {
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
    };

    auto handle_cursor_right = [&]() {
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
    };

    auto handle_cursor_updown = [&](bool down) {
        int lo = 0, hi = buf->lines.len-1;

        int y = (int)cur.y + (down ? 1 : -1);
        if (y > hi) y = hi;
        if (y < lo) y = lo;

        if (y != cur.y) {
            cur.y = y;
            cur.x = buf->idx_vcp_to_cp(cur.y, editor->savedvx);
        }

        was_updown_movement = true;
    };

    switch (keymods) {
    case CP_MOD_SHIFT:
    case CP_MOD_NONE:
        switch (key) {
        case CP_KEY_LEFT:
            handle_cursor_left();
            handled = true;
            break;

        case CP_KEY_RIGHT:
            handle_cursor_right();
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
                cur = buf->end_pos();
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
        case CP_KEY_DOWN:
        case CP_KEY_UP:
            handle_cursor_updown(key == CP_KEY_DOWN);
            handled = true;
            break;
        }
        break;

    case CP_MOD_TEXT | CP_MOD_SHIFT:
    case CP_MOD_TEXT:
        switch (key) {
        case CP_KEY_DOWN:
        case CP_KEY_UP:
            handle_cursor_updown(key == CP_KEY_DOWN);
            handled = true;
            break;
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
                cur = editor->handle_alt_move(key == CP_KEY_LEFT, false);
            }
            handled = true;
            break;
        }
        break;

    // emacs style keybindings on macos
    case CP_MOD_CTRL:
    case CP_MOD_CTRL | CP_MOD_SHIFT:
        switch (key) {
        case CP_KEY_E: {
            auto &line = buf->lines[editor->cur.y];
            cur.x = line.len;
            handled = true;
            break;
        }
        case CP_KEY_A:
            cur.x = 0;
            handled = true;
            break;
        case CP_KEY_N:
        case CP_KEY_P:
            handle_cursor_updown(key == CP_KEY_N);
            handled = true;
            break;
        case CP_KEY_B:
            handle_cursor_left();
            handled = true;
            break;
        case CP_KEY_F:
            handle_cursor_right();
            handled = true;
            break;
        }
        break;

    case CP_MOD_CTRL | CP_MOD_TEXT:
    case CP_MOD_CTRL | CP_MOD_TEXT | CP_MOD_SHIFT:
        switch (key) {
        case CP_KEY_B:
        case CP_KEY_F:
            cur = editor->handle_alt_move(key == CP_KEY_B, false);
            handled = true;
            break;
        }
        break;
    }

    if (!handled) return;

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

    if (was_updown_movement)
        editor->savedvx = old_savedvx;

    editor->update_autocomplete(false); // TODO: why call this here?
    editor->update_parameter_hint();
}

void handle_window_event(Window_Event *it) {
    switch (it->type) {
    case WINEV_FOCUS:
        handle_window_focus(true);
        break;

    case WINEV_BLUR:
        handle_window_focus(false);
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

        auto zs = world.get_zoom_scale();
        ImGui::GetIO().AddMousePosEvent((float)x / zs, (float)y / zs);
        break;
    }

    case WINEV_MOUSE: {
        auto button = it->mouse.button;
        auto press = it->mouse.press;
        auto mods = it->mouse.mods;

        if (is_vim_macro_running()) break;

        Timer t; t.init("mousebutton callback", &world.trace_next_frame); defer { t.log("done"); };

        // Don't set world.ui.mouse_down here. We set it based on
        // world.ui.mouse_just_pressed and some additional logic below, while
        // we're setting io.MouseDown for ImGui.

        im_update_keymod_states();

        if (button < 0 || button >= _countof(world.ui.mouse_just_pressed))
            break;

        if (press)
            world.ui.mouse_just_pressed[button] = true;
        else
            world.ui.mouse_just_released[button] = true;

        auto &io = ImGui::GetIO();
        io.AddMouseButtonEvent(button, press);
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

    case WINEV_KEY:
        handle_key_event(it);
        break;

    case WINEV_CHAR: {
        auto ch = it->character.ch;

        if (is_vim_macro_running()) break;

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

        if (mods == CP_MOD_CTRL) break;

        ImGuiIO& io = ImGui::GetIO();
        if (ch > 0 && ch < 0x10000)
            io.AddInputCharacter((u16)ch);

        if (is_imgui_hogging_keyboard()) break;
        if (!uni_isprint(ch)) break;

        auto editor = get_current_editor();
        if (!editor) break;

        auto &nav = editor->ast_navigation;
        if (nav.on) {
            if (world.vim.on) {
                switch (ch) {
                case 'y':
                case 'c':
                case 's':
                case 'd':
                case 'x': {
                    if (ch == 'c' || ch == 'd' || ch == 'x' || ch == 's')
                        if (!editor->is_modifiable())
                            break;

                    do {
                        auto node = editor->get_selected_ast_node();
                        if (!node) break;

                        auto a = node->start();
                        auto b = node->end();
                        if (a == b) break;

                        editor->vim_yank_text(editor->buf->get_text(a, b));

                        if (ch == 'c' || ch == 's') {
                            Vim_Command cmd;
                            cmd.o_count = 0;
                            cmd.m_count = 0;
                            cmd.motion = new_list(Vim_Command_Input);
                            cmd.op = new_list(Vim_Command_Input);

                            auto inp = cmd.op->append();
                            inp->is_key = false;
                            inp->ch = ch;

                            editor->vim_enter_insert_mode(&cmd, [&]() {
                                editor->buf->remove(a, b);
                            });
                        } else if (ch == 'd' || ch == 'x') {
                            editor->buf->remove(a, b);
                        }

                    } while (0);

                    editor->ast_navigation.on = false;
                    break;
                }
                }
            }
            break;
        }

        // when vim is on, it takes over insert mode completely
        if (world.vim.on) {
            editor->vim_handle_char(ch);
            break;
        }

        if (!editor->is_modifiable()) break;

        if (!world.vim.on)
            editor->delete_selection();

        Type_Char_Opts opts; ptr0(&opts);
        opts.replace_mode = world.vim_mode() == VI_REPLACE;
        editor->type_char(ch, &opts, world.time_type_char);
        break;
    }
    }
}

int main(int argc, char **argv) {
    is_main_thread = true;

#ifdef DEBUG_BUILD
    {
        Pool mem; mem.init("tmp");
        mem.owns_address(0);
        mem.cleanup();
    }
#endif

    gargc = argc;
    gargv = argv;

    Pool tmpmem;
    tmpmem.init("tmpmem");
    SCOPED_MEM(&tmpmem);

    Timer t;
    t.init(NULL, false);
    // t.always_log = true;

    init_platform_crap();

    t.log("init platform crap");
    if (!window_init_everything())
        return error("window init failed"), EXIT_FAILURE;
    t.log("init window everything");

    world.init();

    t.log("init world");
    SCOPED_MEM(&world.frame_mem);

    if (world.test_running) {
        run_tests(world.test_name);
        return 0;
    }

    {
        SCOPED_MEM(&world.world_mem);
        world.window = new_object(Window);
    }

    auto init_glew = []() -> bool {
        glewExperimental = GL_TRUE;
        Timer t; t.init("glewInit", false);
        auto err = glewInit();
        if (err != GLEW_OK) {
            error("unable to init GLEW: %s", glewGetErrorString(err));
            return false;
        }
        t.total();
        return true;
    };

    {
        // init glew using a dummy context
        make_bootstrap_context();
        t.log("make bootstrap context");

        defer {
            destroy_bootstrap_context();
            t.log("destroy bootstrap context");
        };

        if (!init_glew()) return EXIT_FAILURE;
        t.log("bootstrap context, init glew");
    }

    if (!world.window->init(1280, 720, WINDOW_TITLE))
        return error("could not create window"), EXIT_FAILURE;

    t.log("init window");
    world.window->make_context_current();
    t.log("make context current");

#ifdef DEBUG_BUILD
    GHEnableDebugMode();
    t.log("enable debug mode");
#endif

    world.window->set_title(cp_sprintf("%s - %s", WINDOW_TITLE, world.current_path));
    world.window->swap_interval(0);

    t.log("set window title");

    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    {
        SCOPED_MEM(&world.world_mem);
        io.IniFilename = path_join(world.configdir, "imgui.ini");
    }
    io.ConfigInputTrickleEventQueue = false;

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

        For (&pairs)
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

    t.log("generate textures");

    {
        SCOPED_FRAME();

        s32 len = 0;

        {
            ImFontConfig config;
            config.OversampleH = 3;
            config.OversampleV = 2;
            // config.GlyphExtraSpacing.x = 0.25;
            world.ui.im_font_ui = io.Fonts->AddFontFromMemoryTTF(ui.base_ui_font->data->get_data(), ui.base_ui_font->data->get_len(), UI_FONT_SIZE, &config);
            cp_assert(world.ui.im_font_ui);
        }

        t.log("load im_ui_font");

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

            t.log("load material icons font");

            ImWchar icon_ranges2[] = { 0x21E7, 0x21E7+1, 0 };
            io.Fonts->AddFontFromMemoryTTF(open_sans_ttf, open_sans_ttf_len, UI_FONT_SIZE, &config, icon_ranges2);

            t.log("load open sans font");
        }

        {
            ImFontConfig config;
            config.OversampleH = 3;
            config.OversampleV = 2;
            world.ui.im_font_mono = io.Fonts->AddFontFromMemoryTTF(ui.base_font->data->get_data(), ui.base_font->data->get_len(), CODE_FONT_SIZE, &config);
            cp_assert(world.ui.im_font_mono);

            t.log("load im_font_mono");
        }

        io.Fonts->Build();

        t.log("build fonts");

        // init imgui texture
        u8* pixels;
        i32 width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

        glActiveTexture(GL_TEXTURE0 + TEXTURE_FONT_IMGUI);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

        t.log("load texture data");
    }

    t.log("init fonts");

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

        loc = glGetUniformLocation(world.ui.program, "tex");
        glUniform1i(loc, TEXTURE_FONT);
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

    if (world.jblow_tests.on) {
        if (world.jblow_tests.headless)
            world.window->hide();
        world.jblow_tests.ready = true;
    }

    t.total();

    for (;; world.frame_index++) {
        if (world.window->should_close) {
            auto editor = get_current_editor();
            if (!editor) break;

            if (!world.dont_prompt_on_close_unsaved_tab)
                if (editor->ask_user_about_unsaved_changes())
                    break;

            world.window->should_close = false;
            // canceled close, keep going
        }

        // update various states
        world.dbg.copy_state_to_main_thread();
        world.searcher.copy_state_to_main_thread();

        // update local searcher marks
        do {
            auto &srch = world.searcher;
            if (srch.mt_state_id == world.search_marks_state_id) break;

            if (world.search_marks) {
                For (world.search_marks) {
                    For (it.mark_starts) it->cleanup();
                    For (it.mark_ends) it->cleanup();
                }
            }

            {
                world.search_marks_mem.reset();
                SCOPED_MEM(&world.search_marks_mem);
                world.search_marks = new_list(Search_Marks_File);
            }

            world.search_marks_state_id = srch.mt_state_id;

            bool has_results = false;
            switch (srch.mt_state.type) {
            case SEARCH_SEARCH_DONE:
            case SEARCH_REPLACE_IN_PROGRESS:
            case SEARCH_REPLACE_DONE:
                has_results = true;
                break;
            }

            if (!has_results) break;

            auto &state = world.searcher.mt_state;
            For (state.results) {
                auto editor = find_editor_by_filepath(it.filepath);
                if (editor)
                    create_search_marks_for_editor(&it, editor);
            }
        } while (0);

        // fill search results

        bool was_trace_on = world.trace_next_frame;
        defer { if (was_trace_on) world.trace_next_frame = false; };

        Timer t;
        t.init("frame tracer", &world.trace_next_frame);

        auto frame_start_time = current_time_nano();

        world.frame_mem.reset();
        SCOPED_MEM(&world.frame_mem);

        {
            world.fst_mem.reset();
            SCOPED_MEM(&world.fst_mem);
            world.fst.init("frameskip");
            world.fst.log_output = new_list(char);
        }

        bool reset_inputs_after_defocus = false;

        {
            // Process message queue.
            auto messages = world.message_queue.start();
            defer { world.message_queue.end(); };

            For (messages) {
                switch (it.type) {
                case MTM_EXIT:
                    if (it.exit_message)
                        tell_user_error(it.exit_message);
                    exit(it.exit_code);
                    break;

                case MTM_WRITE_LAST_FOLDER: {
                    File f;
                    if (f.init_write(path_join(world.configdir, ".last_folder")) == FILE_RESULT_OK) {
                        f.write(world.current_path, strlen(world.current_path));
                        f.cleanup();
                    }
                    break;
                }

                case MTM_RESET_AFTER_DEFOCUS:
                    reset_inputs_after_defocus = true;
                    break;

                case MTM_TEST_MOVE_CURSOR: {
                    auto editor = get_current_editor();
                    if (editor)
                        editor->move_cursor(it.test_move_cursor);
                    break;
                }

                case MTM_FOCUS_APP_DEBUGGER:
                    if (it.focus_app_debugger_pid)
                        if (it.focus_app_debugger_pid == get_current_focused_window_pid())
                            if (!world.window->is_focused())
                                world.window->focus();
                    break;

                case MTM_TELL_USER:
                    tell_user(it.tell_user_text, it.tell_user_title);
                    break;

                case MTM_RELOAD_EDITOR: {
                    auto editor = find_editor_by_id(it.reload_editor_id);
                    if (editor) editor->reload_file(false);
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
                    For (get_all_editors())
                        if (are_filepaths_equal(it->filepath, filepath))
                            it->file_was_deleted = true;

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
            // Send info to UI and ImGui.

            auto &io = ImGui::GetIO();
            io.DisplaySize = ImVec2((float)world.display_size.x, (float)world.display_size.y);

            auto scale = world.get_display_scale();
            io.DisplayFramebufferScale = ImVec2(scale.x, scale.y);

            auto now = current_time_nano();
            io.DeltaTime = (double)(now - last_frame_time) / (double)1000000000;
            last_frame_time = now;

            for (i32 i = 0; i < _countof(world.ui.mouse_just_pressed); i++) {
                bool down = world.ui.mouse_just_pressed[i] || world.window->mouse_states[i];
                world.ui.mouse_down[i] = down && !world.ui.mouse_captured_by_imgui;
                world.ui.mouse_just_pressed[i] = false;
                world.ui.mouse_just_released[i] = false;
            }

            auto get_imgui_cursor = [&]() -> ImGuiMouseCursor {
                ImGuiMouseCursor cur = ImGui::GetMouseCursor();
                if (cur != ImGuiMouseCursor_Arrow)
                    return cur;
                if (ui.hover.ready && ui.hover.cursor != ImGuiMouseCursor_Arrow)
                    return ui.hover.cursor;
                return ImGuiMouseCursor_Arrow;
            };

            auto cur = get_imgui_cursor();
            if (!world.ui.cursors_ready[cur])
                cur = ImGuiMouseCursor_Arrow;
            cp_assert(world.ui.cursors_ready[cur]);
            world.window->set_cursor(&world.ui.cursors[cur]);
        }

        fstlog("send shit to ui");

        poll_window_events();

        fstlog("poll window events");
        For (&world.window->events) {
            handle_window_event(&it);
            fstlog("handle window event %s", window_event_type_str(it.type));
        }

        do {
            auto &ref = world.jblow_tests;
            if (!ref.on) break;
            if (!ref.events.len) break;

            SCOPED_LOCK(&ref.lock);
            For (&ref.events) {
                if (it.handled) continue;
                handle_window_event(&it.event);
                it.handled = true;
            }
        } while (0);

        // this is called when we do something that defocuses window, like file
        // dialog or message box basically, the message box interrupts the main
        // UI loop and then quickly send a blur + focus in quick succession,
        // which cancels out inside imgui. we want to add a blur after those
        // two, in order to trigger the input reset, and we need to do it after
        // handling window events, which is why we do it here.
        if (reset_inputs_after_defocus)
            handle_window_focus(false);

        fstlog("poll window events");
        world.window->events.len = 0;

        ui.draw_everything();
        fstlog("draw everything");
        ui.end_frame(); // end frame after polling events, so our event callbacks have access to imgui
        fstlog("end frame");

        world.window->swap_buffers();
        fstlog("swap buffers");

        auto get_framecap = [&]() {
            if (world.jblow_tests.on) return 144;

            switch (options.fps_limit_enum) {
            case FPS_30: return 30;
            case FPS_60: return 60;
            case FPS_120: return 120;
            }
            return 60;
        };

        int framecap = get_framecap();

        auto timeleft = [&]() -> i32 {
            auto budget = 1000.f / framecap;
            auto spent = (current_time_nano() - frame_start_time) / 1000000.f;
            return (i32)((i64)budget - (i64)spent);
        };


        // run vim macros at the end of the frame, using
        // our knowledge of how much time we have remaining, to run as much of
        // the macro as possible.
        if (world.vim.macro_state == MACRO_RUNNING) {
            auto editor = get_current_editor();
            if (editor) {
                // run for a minimum of 3ms every frame even if we're about to run
                // out
                u64 deadline = current_time_nano() + (max(timeleft(), 3) * 1000000);
                editor->vim_execute_macro_little_bit(deadline);
            } else {
                world.vim.macro_state = MACRO_IDLE;
            }
        }

        if (!world.turn_off_framerate_cap) {
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

                    // sleep what's rest of the 4 milliseconds
                    auto rem2 = timeleft();
                    if (rem - rem2 < 4) sleep_milli(4 - (rem - timeleft()));
                    continue;
                }

                if (rem > 0) {
                    sleep_milli((u32)rem);
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
