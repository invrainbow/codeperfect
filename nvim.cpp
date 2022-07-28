#include "nvim.hpp"
#include "buffer.hpp"
#include "world.hpp"
#include "utils.hpp"
#include "go.hpp"
#include "settings.hpp"
#include "unicode.hpp"
#include "defer.hpp"
// #include <strsafe.h>

#define NVIM_DEBUG 0

#if NVIM_DEBUG
#define nvim_print(fmt, ...) print("[nvim] " fmt, ##__VA_ARGS__)
#else
#define nvim_print(fmt, ...)
#endif

Editor* find_editor_by_window(u32 win_id) {
    return find_editor([&](auto it) {
        return it->nvim_data.win_id == win_id;
    });
}

Editor* find_editor_by_buffer(u32 buf_id) {
    return find_editor([&](auto it) {
        return it->nvim_data.buf_id == buf_id;
    });
}

void Nvim::assoc_grid_with_window(u32 grid, u32 win) {
    auto& table = grid_to_window;

    auto pair = table.find_or_append([&](auto it) { return it->grid == grid; });
    cp_assert(pair);

    pair->grid = grid;
    pair->win = win;
}

Editor* Nvim::find_editor_by_grid(u32 grid) {
    auto& table = grid_to_window;

    auto pair = table.find([&](auto it) { return it->grid == grid; });
    if (!pair) return NULL;

    return find_editor_by_window(pair->win);
}

void Nvim::handle_editor_on_ready(Editor *editor) {
    if (!editor->is_nvim_ready()) return;

    // clear dirty indicator (it'll be set after we filled buf during
    // nvim_buf_lines_event)
    editor->buf->dirty = false;

    {
        // clear undo history
        auto msgid = start_request_message("nvim_call_function", 2);
        save_request(NVIM_REQ_FILEOPEN_CLEAR_UNDO, msgid, editor->id);

        writer.write_string("CPClearUndo");
        writer.write_array(1);
        writer.write_int(editor->nvim_data.buf_id);
        end_message();
    }
}

void Nvim::write_line(Line *line) {
    writer.write1(MP_OP_STRING);

    int len = 0;
    For (*line)
        len += uchar_size(it);

    writer.write4(len);

    For (*line) {
        char buf[4];
        auto size = uchar_to_cstr(it, buf);
        for (int j = 0; j < size; j++)
            writer.write1(buf[j]);
    }
}

// TODO: also need to remove request from queue once it's been processed
void Nvim::handle_message_from_main_thread(Nvim_Message *event) {
    switch (event->type) {
    case MPRPC_RESPONSE: {
        auto req = find_request_by_msgid(event->response.msgid);
        if (!req) {
            nvim_print("handle_message_from_main_thread: couldn't find request for msgid %d", event->response.msgid);
            break;
        }

        defer { delete_request_by_msgid(req->msgid); };

        nvim_print("received RESPONSE event, request type = %s", nvim_request_type_str(req->type));

        // grab associated editor, break if we can't find it
        Editor* editor = NULL;
        if (req->editor_id) {
            editor = find_editor_by_id(req->editor_id);
            if (!editor) break;
        }

        switch (req->type) {
        case NVIM_REQ_DOTREPEAT_CREATE_BUF: {
            dotrepeat_buf_id = event->response.buf.object_id;

            auto msgid = start_request_message("nvim_open_win", 3);
            save_request(NVIM_REQ_DOTREPEAT_CREATE_WIN, msgid, 0);

            writer.write_int(event->response.buf.object_id);
            writer.write_bool(true);
            writer.write_map(3);
            writer.write_string("external"); writer.write_bool(true);
            writer.write_string("width"); writer.write_int(100);
            writer.write_string("height"); writer.write_int(100);
            end_message();
            break;
        }

        case NVIM_REQ_DOTREPEAT_CREATE_WIN:
            dotrepeat_win_id = event->response.win.object_id;
            break;

        case NVIM_REQ_FILEOPEN_CLEAR_UNDO:
            editor->buf->dirty = false;
            break;

        case NVIM_REQ_POST_SAVE_SETLINES: {
            auto was_saving = editor->saving;
            editor->saving = false;
            if (was_saving)
                editor->buf->dirty = false;

            /*
            auto cur = req->post_save_setlines.cur;
            if (cur.y >= editor->buf->lines.len) {
                cur.y = editor->buf->lines.len-1;
                cur.x = relu_sub(editor->buf->lines[cur.y].len, 1);
            }
            editor->move_cursor(cur);
            */
            break;
        }

        case NVIM_REQ_POST_INSERT_DOTREPEAT_REPLAY: {
            start_request_message("nvim_call_atomic", 1);

            u64 diff = current_time_nano() - post_insert_dotrepeat_time;
            print("postinsert dotrepeat took %d ms", (int)(diff / 1000000.0));
            post_insert_dotrepeat_time = current_time_nano();

            auto curr_editor = get_current_editor();

            writer.write_array(curr_editor != editor ? 3 : 2);

            {
                writer.write_array(2);
                writer.write_string("nvim_set_current_win");
                {
                    writer.write_array(1);
                    writer.write_int(editor->nvim_data.win_id);
                }
            }

            {
                writer.write_array(2);
                writer.write_string("nvim_input");
                {
                    writer.write_array(1);
                    // don't move the cursor back one; we handle that
                    // <F1> is remapped to ':', because ':' is mapped to <nop>
                    // writer.write_string("<C-O>:stopinsert<CR>");
                    writer.write_string("<Esc>");
                }
            }

            if (curr_editor != editor) {
                writer.write_array(2);
                writer.write_string("nvim_set_current_win");
                {
                    writer.write_array(1);
                    writer.write_int(curr_editor->nvim_data.win_id);
                }
            }

            end_message();
            break;
        }

        case NVIM_REQ_AUTOCOMPLETE_SETBUF:
            editor->move_cursor(req->autocomplete_setbuf.target_cursor);
            break;

        case NVIM_REQ_GET_API_INFO:
            // pass channel id to neovim so we can send it to rpcrequest
            start_request_message("nvim_set_var", 2);
            writer.write_string("channel_id");
            writer.write_int(event->response.channel_id);
            end_message();

            {
                start_request_message("nvim_input", 1);
                writer.write_string("<Enter>");
                end_message();
            }

            break;

        case NVIM_REQ_CREATE_BUF:
            editor->nvim_data.buf_id = event->response.buf.object_id;

            {
                auto msgid = start_request_message("nvim_call_atomic", 1);
                save_request(NVIM_REQ_BUF_ATTACH, msgid, editor->id);

                writer.write_array(8);

                {
                    writer.write_array(2);
                    writer.write_string("nvim_buf_attach");
                    {
                        writer.write_array(3);
                        writer.write_int(event->response.buf.object_id);
                        writer.write_bool(false);
                        writer.write_map(0);
                    }
                }

                {
                    writer.write_array(2);
                    writer.write_string("nvim_buf_set_lines");
                    {
                        writer.write_array(5);

                        writer.write_int(event->response.buf.object_id);
                        writer.write_int(0);
                        writer.write_int(-1);
                        writer.write_bool(false);

                        writer.write_array(editor->buf->lines.len);
                        For (editor->buf->lines) write_line(&it);
                    }
                }

                typedef fn<void()> write_value_func;
                auto set_option = [&](ccstr key, write_value_func f) {
                    writer.write_array(2);
                    writer.write_string("nvim_buf_set_option");
                    {
                        writer.write_array(3);
                        writer.write_int(event->response.buf.object_id);
                        writer.write_string(key);
                        f();
                    }
                };

                set_option("expandtab",  [&]() { writer.write_bool(false); });
                set_option("tabstop",    [&]() { writer.write_int(4); });
                set_option("shiftwidth", [&]() { writer.write_int(4); });
                set_option("modifiable", [&]() { writer.write_bool(editor->is_modifiable()); });
                set_option("buftype",    [&]() { writer.write_string("nofile"); });
                set_option("buflisted",  [&]() { writer.write_bool(true); });

                end_message();
            }

            {
                auto msgid = start_request_message("nvim_open_win", 3);
                save_request(NVIM_REQ_CREATE_WIN, msgid, editor->id);

                writer.write_int(event->response.buf.object_id);
                writer.write_bool(false);
                writer.write_map(3);
                // writer.write_string("relative"); writer.write_string("win");
                // writer.write_string("row"); writer.write_int(0);
                // writer.write_string("col"); writer.write_int(0);
                writer.write_string("external"); writer.write_bool(true);
                writer.write_string("width"); writer.write_int(NVIM_DEFAULT_WIDTH);
                writer.write_string("height"); writer.write_int(NVIM_DEFAULT_HEIGHT);
                end_message();
            }
            break;
        case NVIM_REQ_SET_CURRENT_WIN:
            if (editor->id == waiting_focus_window)
                waiting_focus_window = 0;
            current_win_id = editor->id;
            break;
        case NVIM_REQ_CREATE_WIN:
            editor->nvim_data.win_id = event->response.win.object_id;
            if (waiting_focus_window == editor->id)
                set_current_window(editor);

            if (editor->nvim_data.need_initial_pos_set) {
                nvim_print("need initial pos set, setting...");
                auto pos = editor->nvim_data.initial_pos;
                if (pos.y == -1)
                    pos = editor->offset_to_cur(pos.x);
                // editor->raw_move_cursor(pos);
                editor->move_cursor(pos);
            }

            {
                start_request_message("nvim_win_set_option", 3);
                writer.write_int(editor->nvim_data.win_id);
                writer.write_string("scroll");
                writer.write_int(editor->view.h / 2);
                end_message();
            }

            handle_editor_on_ready(editor);
            break;
        case NVIM_REQ_BUF_ATTACH:
            editor->nvim_data.is_buf_attached = true;
            break;

        case NVIM_REQ_UI_ATTACH:
            is_ui_attached = true;
            break;
        }
        break;
    }
    case MPRPC_NOTIFICATION:
        nvim_print("received NOTIFICATION event, notification type = %s", nvim_notification_type_str(event->notification.type));
        switch (event->notification.type) {
        case NVIM_NOTIF_GRID_CLEAR: {
            auto &args = event->notification.grid_line;
            auto editor = find_editor_by_grid(args.grid);
            if (!editor) break;
            mem0(editor->highlights, sizeof(editor->highlights));
            break;
        }
        case NVIM_NOTIF_GRID_LINE: {
            auto &args = event->notification.grid_line;

            {
                SCOPED_FRAME();

                Text_Renderer r;
                r.init();
                Fori (*args.cells) {
                    r.write("(%d x %d)", it.hl, it.reps);
                    if (i+1 < args.cells->len)
                        r.writestr(", ");
                }
                nvim_print("grid line %d:%d: %s", args.row, args.col, r.finish());
            }

            auto editor = find_editor_by_grid(args.grid);
            if (!editor) break;

            i32 last_hl = -1;
            Hl_Type last_hltype = HL_NONE;
            auto col = args.col;

            For (*args.cells) {
                if (streq(it.text, "")) continue;

                if (it.hl != -1 && it.hl != last_hl) {
                    last_hl = it.hl;
                    auto def = hl_defs.find([&](auto it) { return it->id == last_hl; });
                    last_hltype = !def ? HL_NONE : def->type;
                }

                int reps = (!streq(it.text, "\t") && it.reps) ? it.reps : 1;
                for (u32 i = 0; i < reps && col < NVIM_DEFAULT_WIDTH; i++) {
                    editor->highlights[args.row][col] = last_hltype;
                    col++;
                }
            }
            break;
        }
        case NVIM_NOTIF_GRID_SCROLL: {
            auto &args = event->notification.grid_scroll;

            auto editor = find_editor_by_grid(args.grid);
            if (!editor) break;

            auto move_row = [&](u32 dest, u32 src) {
                for (u32 i = 0; i < NVIM_DEFAULT_WIDTH; i++) {
                    editor->highlights[dest][i] = editor->highlights[src][i];
                    editor->highlights[src][i] = HL_NONE;
                }
            };

            auto top = args.top;
            auto bot = min(NVIM_DEFAULT_HEIGHT, args.bot);
            auto rows = args.rows;

            if (args.rows < 0) {
                rows = -rows;
                for (i32 k = bot - 1; k >= top + rows; k--)
                    move_row(k, k - rows);
            } else {
                for (u32 k = top; k < bot - rows; k++)
                    move_row(k, k + rows);
            }
            break;
        }
        case NVIM_NOTIF_HL_ATTR_DEFINE: {
            auto &args = event->notification.hl_attr_define;

            nvim_print("hl def: %d = %s", args.id, args.hi_name);

            auto get_hl_type = [&]() -> Hl_Type {
                if (streq(args.hi_name, "IncSearch")) return HL_INCSEARCH;
                if (streq(args.hi_name, "Search")) return HL_SEARCH;
                if (streq(args.hi_name, "Visual")) return HL_VISUAL;

                return HL_NONE;
            };

            auto def = hl_defs.find_or_append([&](Hl_Def *it) { return it->id == args.id; });
            auto hltype = get_hl_type();
            if (hltype != HL_NONE) {
                def->id = args.id;
                def->type = hltype;
            } else {
                hl_defs.remove(def);
            }
            break;
        }
        case NVIM_NOTIF_CUSTOM_GOTO_DEFINITION:
            handle_goto_definition();
            break;
        case NVIM_NOTIF_CUSTOM_GET_VISUAL: {
            auto &args = event->notification.custom_get_visual;

            auto editor = find_editor_by_buffer(args.bufid);
            if (!editor) break;

            auto fix_pos = [&](cur2 c) {
                auto newx = editor->buf->idx_byte_to_cp(c.y, c.x, true); // do we need/want nocrash?
                return new_cur2((int)newx, (int)c.y);
            };

            auto start = fix_pos(args.start);
            auto end = fix_pos(args.end);

            if (streq(args.for_what, "copy_visual")) {
                auto s = editor->buf->get_text(start, editor->buf->inc_cur(end));
                set_clipboard_string(s);
            }
            /*
            else if (streq(args.for_what, "search_in_visual")) {
                auto &wnd = world.wnd_current_file_search;

                auto editor = find_editor_by_buffer(args.bufid);
                if (!editor) break;

                trigger_file_search(
                    editor->cur_to_offset(start),
                    editor->cur_to_offset(end)
                );
            }
            */
            break;
        }
        case NVIM_NOTIF_CUSTOM_MOVE_CURSOR: {
            auto editor = get_current_editor();
            if (!editor) break;

            auto &view = editor->view;
            u32 y = 0;

            switch (event->notification.custom_move_cursor.screen_pos) {
            case SCREEN_POS_TOP:
                y = view.y + min(view.h - 1, options.scrolloff);
                break;
            case SCREEN_POS_MIDDLE:
                y = view.y + (view.h / 2);
                break;
            case SCREEN_POS_BOTTOM:
                y = view.y + relu_sub(view.h, 1 + options.scrolloff);
                break;
            }

            if (y >= editor->buf->lines.len)
                y = editor->buf->lines.len-1;

            auto &line = editor->buf->lines[y];
            u32 x = 0;
            while (x < line.len && isspace((char)line[x]))
                x++;
            if (x == line.len) x = 0;

            editor->move_cursor(new_cur2(x, y));
            break;
        }
        case NVIM_NOTIF_CUSTOM_JUMP: {
            auto forward = event->notification.custom_jump.forward;
            if (forward)
                world.history.go_forward();
            else
                world.history.go_backward();
            break;
        }
        case NVIM_NOTIF_CUSTOM_REVEAL_LINE: {
            auto editor = get_current_editor();
            if (!editor) break;

            u32 y = editor->cur.y;
            auto &view = editor->view;

            switch (event->notification.custom_reveal_line.screen_pos) {
            case SCREEN_POS_TOP:
                view.y = relu_sub(y, options.scrolloff);
                break;
            case SCREEN_POS_MIDDLE:
                view.y = relu_sub(y, view.h / 2);
                break;
            case SCREEN_POS_BOTTOM:
                view.y = relu_sub(y + options.scrolloff + 1, view.h);
                break;
            }

            if (event->notification.custom_reveal_line.reset_cursor) {
                auto &line = editor->buf->lines[y];
                u32 x = 0;
                while (x < line.len && isspace((char)line[x]))
                    x++;
                if (x == line.len) x = 0;

                editor->move_cursor(new_cur2(x, y));
            }
            break;
        }
        case NVIM_NOTIF_BUF_CHANGEDTICK: {
            auto &args = event->notification.buf_changedtick;

            auto editor = find_editor_by_buffer(args.buf.object_id);
            if (!editor) break;

            if (args.changedtick > editor->nvim_data.changedtick)
                editor->nvim_data.changedtick = args.changedtick;
            break;
        }
        case NVIM_NOTIF_BUF_LINES: {
            auto &args = event->notification.buf_lines;

            auto editor = find_editor_by_buffer(args.buf.object_id);
            if (!editor) break;

            if (args.changedtick > editor->nvim_data.changedtick)
                editor->nvim_data.changedtick = args.changedtick;

            auto is_change_empty = (args.firstline == args.lastline && !args.lines->len);

            bool skip = (
                !editor->nvim_data.got_initial_lines
                || args.changedtick <= editor->nvim_insert.skip_changedticks_until
                || is_change_empty
            );

            nvim_print("updating lines...");
            if (!skip) editor->update_lines(args.firstline, args.lastline, args.lines, args.line_lengths);

            if (!editor->nvim_data.got_initial_lines) {
                nvim_print("got_initial_lines = false, setting to true & calling handle_editor_on_ready()");
                editor->nvim_data.got_initial_lines = true;
                handle_editor_on_ready(editor);
            }
            break;
        }

        case NVIM_NOTIF_CMDLINE_SHOW: {
            auto &args = event->notification.cmdline_show;

            auto copy_str_into_list = [&](ccstr s, List<char> *out) {
                auto len = strlen(s);
                out->len = 0;
                for (u32 i = 0; i < len; i++)
                    out->append(s[i]);
                out->append('\0');
            };

            copy_str_into_list(args.content, &cmdline.content);
            copy_str_into_list(args.firstc, &cmdline.firstc);
            copy_str_into_list(args.prompt, &cmdline.prompt);

            nvim_print("cmdline.content = \"%s\", \"%s\"", args.content, cmdline.content.items);
            nvim_print("cmdline.firstc = \"%s\", \"%s\"", args.firstc, cmdline.firstc.items);
            nvim_print("cmdline.prompt = \"%s\", \"%s\"", args.prompt, cmdline.prompt.items);
            break;
        }

        case NVIM_NOTIF_MODE_CHANGE: {
            auto &args = event->notification.mode_change;

            nvim_print("mode: %s", args.mode_name);

            if (streq(args.mode_name, "normal")) {
                mode = VI_NORMAL;

                if (post_insert_dotrepeat_time) {
                    auto diff = current_time_nano() - post_insert_dotrepeat_time;
                    print("postinsert mode change took %d ms", (int)(diff / 1000000.0));
                    post_insert_dotrepeat_time = 0;
                }
            }
            else if (streq(args.mode_name, "insert"))
                mode = VI_INSERT;
            else if (streq(args.mode_name, "replace"))
                mode = VI_REPLACE;
            else if (streq(args.mode_name, "visual"))
                mode = VI_VISUAL;
            else if (streq(args.mode_name, "operator"))
                mode = VI_OPERATOR;
            else if (streq(args.mode_name, "cmdline_normal"))
                mode = VI_CMDLINE;
            else
                mode = VI_UNKNOWN;

            auto editor = get_current_editor();
            if (editor) {
                if (mode == VI_INSERT) {
                    editor->nvim_insert.start = editor->cur;
                    editor->nvim_insert.old_end = editor->cur;
                    editor->nvim_insert.deleted_graphemes = 0;
                } else if (mode == VI_NORMAL) {
                    if (editor->nvim_data.waiting_for_move_cursor) {
                        editor->nvim_data.waiting_for_move_cursor = false;
                        editor->move_cursor(editor->nvim_data.move_cursor_to);
                    }
                }
            }

            if (mode != VI_INSERT && exiting_insert_mode) {
                if (editor_that_triggered_escape) {
                    auto ed = find_editor_by_id(editor_that_triggered_escape);
                    if (ed)
                        editor = ed;
                    editor_that_triggered_escape = 0;
                }

                if (editor) {
                    auto &gohere = editor->go_here_after_escape;
                    if (gohere.x != -1 && gohere.y != -1) {
                        ccstr insert_cmd = "i";
                        if (gohere.x == editor->buf->lines[gohere.y].len)
                            insert_cmd = "a";

                        // gohere.x--;
                        editor->move_cursor(gohere);
                        gohere.x = -1;
                        gohere.y = -1;

                        // enter insert mode after
                        start_request_message("nvim_input", 1);
                        writer.write_string(insert_cmd);
                        end_message();

                        // just lose the extra chars
                    } else if (chars_after_exiting_insert_mode.len > 0) {
                        start_request_message("nvim_input", 1);

                        Text_Renderer rend;
                        rend.init();

                        For (chars_after_exiting_insert_mode) {
                            if (it == '\b')
                                rend.write("<Backspace>");
                            else
                                rend.writechar(it);
                        }

                        writer.write_string(rend.chars.items, rend.chars.len);
                        end_message();
                    }
                }

                chars_after_exiting_insert_mode.len = 0;
                exiting_insert_mode = false;
            }
            break;
        }

        case NVIM_NOTIF_WIN_VIEWPORT: {
            auto &args = event->notification.win_viewport;

            assoc_grid_with_window(args.grid, args.window.object_id);

            auto editor = find_editor_by_window(args.window.object_id);
            if (!editor)
                break; // TODO: handle us still receiving notifications for nonexistent window

            editor->nvim_data.grid_topline = args.topline;

            /*
            auto should_move_cursor = [&]() -> bool {
                if (mode != VI_INSERT) return true;

                auto &cur = editor->cur;
                // auto buf = editor->buf;

                // if this a necessary post-insert corrective cursor change
                if (cur.x > buf->lines[cur.y].len)
                    if (args.curline == cur.y && args.curcol <= buf->lines[cur.y].len)
                        return true;

                return false;
            };

            if (should_move_cursor())
                editor->raw_move_cursor(new_cur2((u32)args.curcol, (u32)args.curline));
            */

            auto y = (u32)args.curline;
            auto x = editor->buf->idx_byte_to_cp(y, args.curcol, true);
            auto new_cur = new_cur2(x, y);

            if (!editor->nvim_data.got_initial_cur) {
                bool set = false;

                if (editor->nvim_data.need_initial_pos_set) {
                    auto pos = editor->nvim_data.initial_pos;
                    if (pos.y == -1)
                        pos = editor->offset_to_cur(pos.x);

                    auto is_pos_even_valid = [&]() -> bool {
                        auto buf = editor->buf;
                        if (0 <= pos.y && pos.y < buf->lines.len)
                            if (0 <= pos.x && pos.x < buf->lines[pos.y].len)
                                return true;
                        return false;
                    };

                    if (new_cur == pos || !is_pos_even_valid())
                        set = true;
                } else {
                    set = true;
                }

                if (set) {
                    editor->nvim_data.got_initial_cur = true;
                    handle_editor_on_ready(editor);
                }
            }

            if (editor->is_nvim_ready()) {
                // if (mode != VI_INSERT || new_cur != editor->cur)
                editor->raw_move_cursor(new_cur);
            }
            break;
        }

        case NVIM_NOTIF_WIN_POS: {
            auto &args = event->notification.win_pos;
            assoc_grid_with_window(args.grid, args.window.object_id);
            break;
        }
        }
        break;
    }
}

void Nvim::run_event_loop() {
#define ASSERT(x) if (!(x)) { cp_panic("The Vim plugin has crashed."); }
#define CHECKOK() ASSERT(reader.ok)

    {
        auto msgid = start_request_message("nvim_ui_attach", 3);
        save_request(NVIM_REQ_UI_ATTACH, msgid, 0);

        writer.write_int(NVIM_DEFAULT_WIDTH);
        writer.write_int(NVIM_DEFAULT_HEIGHT);
        {
            writer.write_map(4);
            writer.write_string("ext_linegrid"); writer.write_bool(true);
            writer.write_string("ext_multigrid"); writer.write_bool(true);
            writer.write_string("ext_cmdline"); writer.write_bool(true);
            writer.write_string("ext_hlstate"); writer.write_bool(true);
        }
        end_message();
    }

    {
        auto msgid = start_request_message("nvim_get_api_info", 0);
        save_request(NVIM_REQ_GET_API_INFO, msgid, 0);
        end_message();
    }

    {
        auto msgid = start_request_message("nvim_create_buf", 2);
        save_request(NVIM_REQ_DOTREPEAT_CREATE_BUF, msgid, 0);
        writer.write_bool(false);
        writer.write_bool(true);
        end_message();
    }

    while (true) {
        s32 msglen = reader.read_array(); CHECKOK();
        auto msgtype = (MprpcMessageType)reader.read_int(); CHECKOK();

        auto expected_len = (msgtype == MPRPC_NOTIFICATION ? 3 : 4);
        ASSERT(msglen == expected_len);

        auto add_event = [&](fn<void(Nvim_Message*)> f) {
            world.message_queue.add([&](auto msg) {
                msg->type = MTM_NVIM_MESSAGE;
                msg->nvim_message.type = msgtype;
                f(&msg->nvim_message);
            });
        };

        switch (msgtype) {
        case MPRPC_REQUEST: nvim_print("[received] MPRPC_REQUEST"); break;
        case MPRPC_RESPONSE: nvim_print("[received] MPRPC_RESPONSE"); break;
        case MPRPC_NOTIFICATION: nvim_print("[received] MPRPC_NOTIFICATION"); break;
        }

        switch (msgtype) {
        case MPRPC_NOTIFICATION: {
            auto method = reader.read_string(); CHECKOK();
            // nvim_print("got notification with method %s", method);
            auto params_length = reader.read_array(); CHECKOK();

            auto just_skip_params = [&]() {
                for (u32 i = 0; i < params_length; i++) {
                    reader.skip_object();
                    CHECKOK();
                }
            };

            nvim_print("method: %s", method);

            if (streq(method, "custom_notification")) {
                SCOPED_FRAME();

                auto cmd = reader.read_string(); CHECKOK();
                auto num_args = reader.read_array(); CHECKOK();
                if (streq(cmd, "reveal_line")) {
                    ASSERT(num_args == 2);
                    auto screen_pos = (Screen_Pos)reader.read_int(); CHECKOK();
                    auto reset_cursor = (bool)reader.read_int();
                    add_event([&](auto msg) {
                        msg->notification.type = NVIM_NOTIF_CUSTOM_REVEAL_LINE;
                        msg->notification.custom_reveal_line.screen_pos = screen_pos;
                        msg->notification.custom_reveal_line.reset_cursor = reset_cursor;
                    });
                } else if (streq(cmd, "get_visual")) {
                    ASSERT(num_args == 6);
                    auto for_what = reader.read_string(); CHECKOK();
                    auto ys = reader.read_int(); CHECKOK();
                    auto xs = reader.read_int(); CHECKOK();
                    auto ye = reader.read_int(); CHECKOK();
                    auto xe = reader.read_int(); CHECKOK();
                    auto bufid = reader.read_int(); CHECKOK();
                    add_event([&](auto msg) {
                        msg->notification.type = NVIM_NOTIF_CUSTOM_GET_VISUAL;
                        msg->notification.custom_get_visual.for_what = cp_strdup(for_what);
                        msg->notification.custom_get_visual.start = new_cur2((i32)xs-1, (i32)ys-1);
                        msg->notification.custom_get_visual.end = new_cur2((i32)xe-1, (i32)ye-1);
                        msg->notification.custom_get_visual.bufid = bufid;
                    });
                } else if (streq(cmd, "goto_definition")) {
                    ASSERT(!num_args);
                    add_event([&](auto msg) {
                        msg->notification.type = NVIM_NOTIF_CUSTOM_GOTO_DEFINITION;
                    });
                } else if (streq(cmd, "jump")) {
                    ASSERT(num_args == 1);
                    auto forward = (bool)reader.read_int();
                    add_event([&](auto msg) {
                        msg->notification.type = NVIM_NOTIF_CUSTOM_JUMP;
                        msg->notification.custom_jump.forward = forward;
                    });
                } else if (streq(cmd, "move_cursor")) {
                    ASSERT(num_args == 1);
                    auto screen_pos = (Screen_Pos)reader.read_int(); CHECKOK();
                    add_event([&](auto msg) {
                        msg->notification.type = NVIM_NOTIF_CUSTOM_MOVE_CURSOR;
                        msg->notification.custom_move_cursor.screen_pos = screen_pos;
                    });
                }
            } else if (streq(method, "nvim_buf_changedtick_event")) {
                SCOPED_FRAME();

                auto buf = reader.read_ext(); CHECKOK();
                auto changedtick = reader.read_int(); CHECKOK();

                add_event([&](auto msg) {
                    msg->notification.type = NVIM_NOTIF_BUF_CHANGEDTICK;
                    msg->notification.buf_changedtick.buf = *buf;
                    msg->notification.buf_changedtick.changedtick = changedtick;
                });
            } else if (streq(method, "nvim_buf_lines_event")) {
                SCOPED_FRAME();

                auto buf = reader.read_ext(); CHECKOK();
                auto changedtick = reader.read_int(); CHECKOK();
                auto firstline = reader.read_int(); CHECKOK();
                auto lastline = reader.read_int(); CHECKOK();
                auto num_lines = reader.read_array(); CHECKOK();

                add_event([&](auto msg) {
                    msg->notification.type = NVIM_NOTIF_BUF_LINES;
                    msg->notification.buf_lines.buf = *buf;
                    msg->notification.buf_lines.changedtick = changedtick;
                    msg->notification.buf_lines.firstline = firstline;
                    msg->notification.buf_lines.lastline = lastline;

                    auto lines = alloc_list<uchar*>();
                    auto line_lengths = alloc_list<s32>();

                    for (u32 i = 0; i < num_lines; i++) {
                        auto line = reader.read_string(); CHECKOK();
                        {
                            auto uline = cstr_to_ustr(line);
                            lines->append(uline->items);
                            line_lengths->append(uline->len);
                        }
                    }

                    msg->notification.buf_lines.lines = lines;
                    msg->notification.buf_lines.line_lengths = line_lengths;
                });

                // skip {more} param. we don't care, we'll just handle the next
                // notif when it comes
                reader.skip_object(); CHECKOK();
            } else if (streq(method, "redraw")) {
                for (u32 i = 0; i < params_length; i++) {
                    auto argsets_len = reader.read_array(); CHECKOK();
                    auto op = reader.read_string(); CHECKOK();

                    nvim_print("redraw op: %s", op);

                    for (u32 j = 1; j < argsets_len; j++) {
                        auto args_len = reader.read_array(); CHECKOK();

                        if (streq(op, "cmdline_show")) {
                            SCOPED_FRAME();
                            ASSERT(args_len == 6);

                            Text_Renderer r;
                            r.init();
                            auto num_content_parts = reader.read_array(); CHECKOK();
                            for (u32 i = 0; i < num_content_parts; i++) {
                                auto partsize = reader.read_array(); CHECKOK();
                                ASSERT(partsize == 2);
                                reader.skip_object(); CHECKOK(); // skip attrs, we don't care
                                auto s = reader.read_string(); CHECKOK();
                                r.writestr(s);
                            }
                            auto content = r.finish();

                            /* auto pos = */ reader.read_int(); CHECKOK();
                            auto firstc = reader.read_string(); CHECKOK();
                            auto prompt = reader.read_string(); CHECKOK();
                            /* auto indent = */ reader.read_int(); CHECKOK();
                            /* auto level = */ reader.read_int(); CHECKOK();

                            add_event([&](auto m) {
                                m->notification.type = NVIM_NOTIF_CMDLINE_SHOW;
                                m->notification.cmdline_show.content = cp_strdup(content);
                                m->notification.cmdline_show.firstc = cp_strdup(firstc);
                                m->notification.cmdline_show.prompt = cp_strdup(prompt);
                            });
                        } else if (streq(op, "mode_change")) {
                            SCOPED_FRAME();
                            ASSERT(args_len == 2);

                            auto mode_name = reader.read_string(); CHECKOK();
                            auto mode_index = reader.read_int(); CHECKOK();

                            add_event([&](auto m) {
                                m->notification.type = NVIM_NOTIF_MODE_CHANGE;
                                m->notification.mode_change.mode_name = cp_strdup(mode_name);
                                m->notification.mode_change.mode_index = mode_index;
                            });
                        } else if (streq(op, "win_viewport")) {
                            SCOPED_FRAME();
                            ASSERT(args_len == 6);

                            auto grid = reader.read_int(); CHECKOK();
                            auto window = reader.read_ext(); CHECKOK();
                            auto topline = reader.read_int(); CHECKOK();
                            auto botline = reader.read_int(); CHECKOK();
                            auto curline = reader.read_int(); CHECKOK();
                            auto curcol = reader.read_int(); CHECKOK();

                            add_event([&](auto m) {
                                m->notification.type = NVIM_NOTIF_WIN_VIEWPORT;
                                m->notification.win_viewport.grid = grid;
                                m->notification.win_viewport.window = *window;
                                m->notification.win_viewport.topline = topline;
                                m->notification.win_viewport.botline = botline;
                                m->notification.win_viewport.curline = curline;
                                m->notification.win_viewport.curcol = curcol;
                            });
                        } else if (streq(op, "win_pos")) {
                            ASSERT(args_len == 6);
                            auto grid = reader.read_int(); CHECKOK();
                            auto window = reader.read_ext(); CHECKOK();
                            /* auto start_row = */ reader.read_int(); CHECKOK();
                            /* auto start_col = */ reader.read_int(); CHECKOK();
                            /* auto width = */ reader.read_int(); CHECKOK();
                            /* auto height = */ reader.read_int(); CHECKOK();

                            add_event([&](auto m) {
                                m->notification.type = NVIM_NOTIF_WIN_POS;
                                m->notification.win_pos.grid = grid;
                                m->notification.win_pos.window = *window;
                            });
                        } else if (streq(op, "win_external_pos")) {
                            ASSERT(args_len == 2);
                            auto grid = reader.read_int(); CHECKOK();
                            auto window = reader.read_ext(); CHECKOK();

                            add_event([&](auto m) {
                                m->notification.type = NVIM_NOTIF_WIN_POS;
                                m->notification.win_pos.grid = grid;
                                m->notification.win_pos.window = *window;
                            });
                        } else if (streq(op, "grid_clear")) {
                            ASSERT(args_len == 1);
                            auto grid = reader.read_int(); CHECKOK();
                            add_event([&](auto m) {
                                m->notification.type = NVIM_NOTIF_GRID_CLEAR;
                                m->notification.grid_clear.grid = grid;
                            });
                        } else if (streq(op, "grid_line")) {
                            ASSERT(args_len == 4);

                            auto grid = reader.read_int(); CHECKOK();
                            auto row = reader.read_int(); CHECKOK();
                            auto col = reader.read_int(); CHECKOK();
                            auto num_cells = reader.read_array(); CHECKOK();

                            add_event([&](auto m) {
                                m->notification.type = NVIM_NOTIF_GRID_LINE;
                                m->notification.grid_line.grid = grid;
                                m->notification.grid_line.row = row;
                                m->notification.grid_line.col = col;

                                auto cells = alloc_list<Grid_Cell>();
                                for (u32 k = 0; k < num_cells; k++) {
                                    auto cell_len = reader.read_array();
                                    ASSERT(1 <= cell_len && cell_len <= 3);

                                    auto text = reader.read_string(); CHECKOK();

                                    // read highlight id & reps
                                    i32 hl = -1, reps = 0;
                                    if (cell_len == 2) {
                                        hl = reader.read_int(); CHECKOK();
                                    } else if (cell_len == 3) {
                                        hl = reader.read_int(); CHECKOK();
                                        reps = reader.read_int(); CHECKOK();
                                    }

                                    auto cell = cells->append();
                                    cell->hl = hl;
                                    cell->reps = reps;
                                    cell->text = text;
                                }

                                m->notification.grid_line.cells = cells;
                            });
                        } else if (streq(op, "grid_scroll")) {
                            ASSERT(args_len == 7);
                            auto grid = reader.read_int(); CHECKOK();
                            auto top = reader.read_int(); CHECKOK();
                            auto bot = reader.read_int(); CHECKOK();
                            auto left = reader.read_int(); CHECKOK();
                            auto right = reader.read_int(); CHECKOK();
                            auto rows = reader.read_int(); CHECKOK();
                            auto cols = reader.read_int(); CHECKOK();

                            add_event([&](auto m) {
                                m->notification.type = NVIM_NOTIF_GRID_SCROLL;
                                m->notification.grid_scroll.grid = grid;
                                m->notification.grid_scroll.top = top;
                                m->notification.grid_scroll.bot = bot;
                                m->notification.grid_scroll.left = left;
                                m->notification.grid_scroll.right = right;
                                m->notification.grid_scroll.rows = rows;
                                m->notification.grid_scroll.cols = cols;
                            });
                        } else if (streq(op, "hl_attr_define")) {
                            ASSERT(args_len == 4);
                            auto id = reader.read_int(); CHECKOK();
                            /* rgb_attr = */ reader.skip_object(); CHECKOK();
                            /* cterm_attr = */ reader.skip_object(); CHECKOK();
                            auto info_maps = reader.read_array(); CHECKOK();
                            ccstr hi_name = NULL;

                            if (info_maps > 0) {
                                for (u32 i = 0; i < info_maps - 1; i++) {
                                    reader.skip_object(); CHECKOK();
                                }

                                auto info_keys = reader.read_map(); CHECKOK();
                                for (u32 i = 0; i < info_keys; i++) {
                                    auto key = reader.read_string(); CHECKOK();
                                    if (hi_name) {
                                        reader.skip_object(); CHECKOK();
                                        continue;
                                    }

                                    if (streq(key, "hi_name")) {
                                        hi_name = reader.read_string(); CHECKOK();
                                    } else {
                                        reader.skip_object(); CHECKOK();
                                    }
                                }
                            }

                            if (hi_name) {
                                add_event([&](auto m) {
                                    m->notification.type = NVIM_NOTIF_HL_ATTR_DEFINE;
                                    m->notification.hl_attr_define.id = id;
                                    m->notification.hl_attr_define.hi_name = cp_strdup(hi_name);
                                });
                            }
                        } else {
                            for (u32 k = 0; k < args_len; k++) {
                                reader.skip_object();
                                CHECKOK();
                            }
                        }
                    }
                }
            } else {
                just_skip_params();
                CHECKOK();
            }
            break;
        }
        case MPRPC_REQUEST: {
            auto msgid = reader.read_int(); CHECKOK();
            auto method = reader.read_string(); CHECKOK();
            auto params_length = reader.read_array(); CHECKOK();

            // nvim_print("got request with method %s", method);
            // check `method` against names of rpcrequests

            // in the default case we just skip params
            for (u32 i = 0; i < params_length; i++) {
                reader.skip_object();
                CHECKOK();
            }

            start_response_message(msgid);
            writer.write_nil(); // no error
            writer.write_string(""); // empty response
            end_message();
            break;
        }
        case MPRPC_RESPONSE: {
            auto msgid = reader.read_int(); CHECKOK();
            nvim_print("[raw] got response with msgid %d", msgid);

            u32 req_editor_id;
            Nvim_Request_Type req_type;

            {
                SCOPED_LOCK(&requests_lock);
                auto req = find_request_by_msgid(msgid);
                if (!req) {
                    nvim_print("couldn't find request for msgid %d", msgid);
                    reader.skip_object(); CHECKOK(); // error
                    reader.skip_object(); CHECKOK(); // result
                    break;
                }

                req_editor_id = req->editor_id;
                req_type = req->type;
            }

            if (reader.peek_type() != MP_NIL) {
                if (reader.peek_type() == MP_STRING) {
                    auto error_str = reader.read_string(); CHECKOK();
                    SCOPED_FRAME();
                    nvim_print("error in response for msgid %d, reqtype = %d: %d", msgid, req_type, error_str);
                } else {
                    auto type = reader.peek_type();
                    reader.skip_object(); CHECKOK();
                    nvim_print("error in response for msgid %d, reqtype = %d: (error was not a string, instead was %s)", msgid, req_type, mptype_str(type));
                }
                reader.skip_object();
                delete_request_by_msgid(msgid);
                break;
            }

            // read the error
            reader.read_nil(); CHECKOK();

            // grab associated editor, break if we can't find it
            Editor* editor = NULL;
            if (req_editor_id) {
                auto is_match = [&](auto it) { return it->id == req_editor_id; };
                editor = find_editor(is_match);
                if (!editor) {
                    reader.skip_object();
                    delete_request_by_msgid(msgid);
                    break;
                }
            }

            auto add_response_event = [&](fn<void(Nvim_Message*)> f) {
                add_event([&](auto it) {
                    it->response.msgid = msgid;
                    f(it);
                });
            };

            auto read_atomic_call_response = [&](fn<void()> f) {
                auto arrlen = reader.read_array(); CHECKOK();
                ASSERT(arrlen == 2);

                f(); // read response

                if (reader.peek_type() == MP_NIL) {
                    reader.read_nil(); CHECKOK();
                } else {
                    auto arrlen = reader.read_array(); CHECKOK();
                    ASSERT(arrlen == 3);

                    auto index = reader.read_int(); CHECKOK();
                    reader.skip_object(); CHECKOK();
                    auto err = reader.read_string(); CHECKOK();

                    nvim_print("nvim_call_atomic call #%d had error: %s", index, err);
                }
            };

            switch (req_type) {
            case NVIM_REQ_GET_API_INFO: {
                auto len = reader.read_array(); CHECKOK();
                ASSERT(len == 2);

                auto channel_id = reader.read_int(); CHECKOK();
                reader.skip_object(); CHECKOK();

                add_response_event([&](auto m) {
                    m->response.channel_id = channel_id;
                });
                break;
            }

            case NVIM_REQ_CREATE_BUF:
            case NVIM_REQ_DOTREPEAT_CREATE_BUF: {
                if (reader.peek_type() != MP_EXT) {
                    reader.skip_object(); CHECKOK(); // check if this path ever gets hit
                    break;
                }

                auto buf = reader.read_ext(); CHECKOK();
                add_response_event([&](auto m) {
                    m->response.buf = *buf;
                });
                break;
            }

            case NVIM_REQ_POST_INSERT_DOTREPEAT_REPLAY:
            case NVIM_REQ_BUF_ATTACH:
                read_atomic_call_response([&]() {
                    reader.skip_object(); CHECKOK();
                    add_response_event([&](auto) {});
                });
                break;

            case NVIM_REQ_CREATE_WIN:
            case NVIM_REQ_DOTREPEAT_CREATE_WIN: {
                if (reader.peek_type() != MP_EXT) {
                    reader.skip_object(); CHECKOK(); // check if this path ever gets hit
                    break;
                }

                auto win = reader.read_ext(); CHECKOK();
                add_response_event([&](auto m) {
                    m->response.win = *win;
                });
                break;
            }

            default:
                reader.skip_object();
                add_response_event([&](auto) {});
                break;
            }
            break;
        }
        }
    }

#undef CHECKOK
#undef ASSERT
}

// caller is expected to write `params_length` params
void Nvim::write_request_header(u32 msgid, ccstr method, u32 params_length) {
    writer.write_array(4);
    writer.write_int(MPRPC_REQUEST); // 1
    writer.write_int(msgid); // 2
    writer.write_string(method); // 3
    writer.write_array(params_length); // 4
}

// caller is expected to write error and result
void Nvim::write_response_header(u32 msgid) {
    writer.write_array(4);
    writer.write_int(MPRPC_RESPONSE); // 1
    writer.write_int(msgid); // 2
}

// caller is expected to write `params_length` params
void Nvim::write_notification_header(ccstr method, u32 params_length) {
    writer.write_array(3);
    writer.write_int(MPRPC_NOTIFICATION); // 1
    writer.write_string(method); // 2
    writer.write_array(params_length); // 3
}

void Nvim::init() {
    ptr0(this);

    mem.init("nvim::mem");
    loop_mem.init("nvim::loop_mem");
    started_messages_mem.init();

    request_id = 0;

    SCOPED_MEM(&mem);

    requests_lock.init();
    requests.init();
    requests_mem.init("nvim::requests_mem");

    grid_to_window.init();
    chars_after_exiting_insert_mode.init();

    cmdline.content.init();
    cmdline.firstc.init();
    cmdline.prompt.init();

    hl_defs.init();
}

void Nvim::start_running() {
    SCOPED_MEM(&mem);

    nvim_proc.init();
    nvim_proc.use_stdin = true;
    nvim_proc.dir = cp_dirname(get_executable_path());

#if OS_WINBLOWS
    // for some reason skip_shell causes file not found on windows
    nvim_proc.run(".\\nvim\\bin\\nvim.exe -u init.vim -i NONE -N --embed --headless");
#elif OS_MAC
    nvim_proc.skip_shell = true;
    nvim_proc.run("./nvim/bin/nvim -u init.vim -i NONE -N --embed --headless");
#else
#error "only windows and mac supported right now"
#endif

    reader.proc = &nvim_proc;
    reader.offset = 0;
    writer.proc = &nvim_proc;

    auto func = [](void *p) {
        auto nvim = (Nvim*)p;
        SCOPED_MEM(&nvim->loop_mem);
        nvim->run_event_loop();
    };

    event_loop_thread = create_thread(func, this);
    if (!event_loop_thread) return;
}

void Nvim::cleanup() {
    requests_lock.cleanup();

    if (event_loop_thread) {
        kill_thread(event_loop_thread);
        close_thread_handle(event_loop_thread);
    }

    nvim_proc.cleanup();

    mem.cleanup();
    loop_mem.cleanup();
}

ccstr Mp_Reader::read_string() {
    auto read_length = [&]() -> i32 {
        auto b = read1();
        if (ok) {
            if (b >> 5 == 0b101) return b & 0b00011111;
            if (b == 0xd9) return read1();
            if (b == 0xda) return read2();
            if (b == 0xdb) return read4();
            ok = false;
        }
        return -1;
    };

    auto len = read_length();
    if (!ok) return NULL;

    Frame frame;
    Text_Renderer r;
    r.init();

    for (u32 i = 0; i < len; i++) {
        char ch = read1();
        if (!ok) {
            frame.restore();
            return NULL;
        }
        r.writechar(ch);
    }
    ok = true;
    return r.finish();
}

Ext_Info* Mp_Reader::read_ext() {
    u8 b = read1();
    if (!ok) return NULL;

    Nvim_Ext_Type type;
    s32 len = 0;

    switch (b) {
    case 0xd4:
    case 0xd5:
    case 0xd6:
    case 0xd7:
    case 0xd8:
        type = (Nvim_Ext_Type)read1(); if (!ok) return NULL;
        switch (b) {
        case 0xd4: len = 1; break;
        case 0xd5: len = 2; break;
        case 0xd6: len = 4; break;
        case 0xd7: len = 8; break;
        case 0xd8: len = 16; break;
        }
        break;
    case 0xc7:
        len = (s32)(u8)read1(); if (!ok) return NULL;
        type = (Nvim_Ext_Type)read1(); if (!ok) return NULL;
        break;
    case 0xc8:
        len = (s32)(u16)read2(); if (!ok) return NULL;
        type = (Nvim_Ext_Type)read1(); if (!ok) return NULL;
        break;
    case 0xc9:
        len = (s32)(u32)read4(); if (!ok) return NULL;
        type = (Nvim_Ext_Type)read1(); if (!ok) return NULL;
        break;
    default:
        ok = false;
        return NULL;
    }

    auto start_offset = offset;
    auto object_id = (u32)read_int();
    if (!ok) return NULL;

    while (offset < start_offset + len) {
        read1();
        if (!ok) return NULL;
    }

    auto ret = alloc_object(Ext_Info);
    ret->type = type;
    ret->object_id = object_id;

    ok = true;
    return ret;
}

void Mp_Reader::skip_object() {
    SCOPED_FRAME();

    u32 arrlen; // for array and map
    auto obj_type = peek_type();
    switch (obj_type) {
    case MP_NIL: read_nil(); return;
    case MP_BOOL: read_bool(); return;
    case MP_INT: read_int(); return;
    case MP_DOUBLE: read_double(); return;
    case MP_STRING: read_string(); return;
    case MP_EXT: read_ext(); return;
    case MP_ARRAY:
    case MP_MAP:
        arrlen = obj_type == MP_ARRAY ? read_array() : (read_map() * 2);
        if (ok) {
            for (u32 i = 0; i < arrlen; i++) {
                skip_object();
                if (!ok)
                    break;
            }
        }
        return;
    }
}
