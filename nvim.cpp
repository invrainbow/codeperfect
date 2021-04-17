#include "nvim.hpp"
#include "buffer.hpp"
#include "world.hpp"
#include "utils.hpp"
#include "go.hpp"
// #include <strsafe.h>

#define NVIM_DEBUG 0

#if NVIM_DEBUG
#define nvim_print(fmt, ...) print("[nvim] " fmt, ##__VA_ARGS__)
#else
#define nvim_print(fmt, ...)
#endif

Editor* find_editor_by_window(u32 win_id) {
    return world.find_editor([&](Editor* it) {
        return it->nvim_data.win_id == win_id;
    });
}

Editor* find_editor_by_buffer(u32 buf_id) {
    return world.find_editor([&](Editor* it) {
        return it->nvim_data.buf_id == buf_id;
    });
}

bool Nvim::resize_editor(Editor* editor) {
    auto pair = grid_to_window.find([&](Grid_Window_Pair* it) {
        return it->win == editor->nvim_data.win_id;
    });

    if (pair == NULL) return false;

    editor->nvim_data.is_resizing = true;

    auto msgid = start_request_message("nvim_ui_try_resize_grid", 3);
    save_request(NVIM_REQ_RESIZE, msgid, editor->id);

    writer.write_int(pair->grid);
    writer.write_int(editor->view.w);
    writer.write_int(editor->view.h);
    end_message();
    return true;
}

void Nvim::assoc_grid_with_window(u32 grid, u32 win) {
    auto& table = grid_to_window;

    auto pair = table.find_or_append([&](Grid_Window_Pair* it) { return it->grid == grid; });
    assert(pair != NULL);

    pair->grid = grid;
    pair->win = win;

    auto editor = find_editor_by_window(win);
    if (editor != NULL) {
        if (editor->nvim_data.need_initial_resize) {
            nvim_print("need initial resize, calling world.nvim.resize_editor()...");
            world.nvim.resize_editor(editor);
            editor->nvim_data.need_initial_resize = false;
        }
    }
}

Editor* Nvim::find_editor_by_grid(u32 grid) {
    auto& table = grid_to_window;

    auto pair = table.find([&](Grid_Window_Pair* it) { return it->grid == grid; });
    if (pair == NULL) return NULL;

    return find_editor_by_window(pair->win);
}

void Nvim::handle_editor_on_ready(Editor *editor) {
    nvim_print("handle_editor_on_ready() called...");

    if (!editor->is_nvim_ready()) return;

    nvim_print("nvim is ready!");

    if (editor->nvim_data.need_initial_pos_set) {
        nvim_print("need initial pos set, setting...");

        editor->nvim_data.need_initial_pos_set = false;
        auto pos = editor->nvim_data.initial_pos;
        if (pos.y == -1)
            pos = editor->offset_to_cur(pos.x);
        editor->move_cursor(pos);
    }

    // clear dirty indicator (it'll be set after we filled buf during
    // nvim_buf_lines_event)
    editor->buf.dirty = false;

    // clear undo history
    auto msgid = start_request_message("nvim_call_function", 2);
    save_request(NVIM_REQ_FILEOPEN_CLEAR_UNDO, msgid, editor->id);

    writer.write_string("IdeClearUndo");
    writer.write_array(1);
    writer.write_int(editor->nvim_data.buf_id);
    end_message();
}

ccstr nvim_request_type_str(Nvim_Request_Type type) {
    switch (type) {
    define_str_case(NVIM_REQ_GET_API_INFO);
    define_str_case(NVIM_REQ_CREATE_BUF);
    define_str_case(NVIM_REQ_OPEN_WIN);
    define_str_case(NVIM_REQ_BUF_ATTACH);
    define_str_case(NVIM_REQ_UI_ATTACH);
    define_str_case(NVIM_REQ_SET_CURRENT_WIN);
    define_str_case(NVIM_REQ_RESIZE);
    define_str_case(NVIM_REQ_AUTOCOMPLETE_SETBUF);
    define_str_case(NVIM_REQ_POST_INSERT_GETCHANGEDTICK);
    define_str_case(NVIM_REQ_FILEOPEN_CLEAR_UNDO);
    define_str_case(NVIM_REQ_POST_SAVE_GETCHANGEDTICK);
    define_str_case(NVIM_REQ_POST_SAVE_SETLINES);
    }
    return NULL;
}

ccstr nvim_notification_type_str(Nvim_Notification_Type type) {
    switch (type) {
        define_str_case(NVIM_NOTIF_BUF_LINES);
        define_str_case(NVIM_NOTIF_MODE_CHANGE);
        define_str_case(NVIM_NOTIF_WIN_VIEWPORT);
        define_str_case(NVIM_NOTIF_WIN_POS);
    }
    return NULL;
}

// TODO: also need to remove request from queue once it's been processed
void Nvim::handle_message_from_main_thread(Nvim_Message *event) {
    switch (event->type) {
    case MPRPC_RESPONSE:
        {
            auto req = event->response.original_request;
            nvim_print("received RESPONSE event, request type = %s", nvim_request_type_str(req->type));

            // grab associated editor, break if we can't find it
            Editor* editor = NULL;
            if (req->editor_id != 0) {
                editor = world.find_editor_by_id(req->editor_id);
                if (editor == NULL) {
                    reader.skip_object();
                    break;
                }
            }

            switch (req->type) {
            case NVIM_REQ_FILEOPEN_CLEAR_UNDO:
                editor->buf.dirty = false;
                break;

            case NVIM_REQ_POST_SAVE_SETLINES:
                editor->saving = false;
                editor->buf.dirty = false;
                {
                    auto cur = req->post_save_setlines.cur;
                    if (cur.y >= editor->buf.lines.len) {
                        cur.y = editor->buf.lines.len-1;
                        cur.x = relu_sub(editor->buf.lines[cur.y].len, 1);
                    }
                    editor->move_cursor(cur);
                }
                break;

            case NVIM_REQ_POST_SAVE_GETCHANGEDTICK:
                {
                    // skip next update from nvim
                    editor->nvim_insert.skip_changedticks_until = event->response.changedtick + 1;

                    auto msgid = start_request_message("nvim_buf_set_lines", 5);
                    auto req2 = save_request(NVIM_REQ_POST_SAVE_SETLINES, msgid, editor->id);
                    req2->post_save_setlines.cur = req->post_save_getchangedtick.cur;

                    writer.write_int(event->response.buf.object_id);
                    writer.write_int(0);
                    writer.write_int(-1);
                    writer.write_bool(false);

                    writer.write_array(editor->buf.lines.len);
                    For (editor->buf.lines) {
                        writer.write1(MP_OP_STRING);
                        writer.write4(it.len);
                        For (it) writer.write1(it);
                    }
                    end_message();
                }
                break;

            case NVIM_REQ_POST_INSERT_GETCHANGEDTICK:
                {
                    // skip next update from nvim
                    editor->nvim_insert.skip_changedticks_until = event->response.changedtick + 1;

                    auto cur = editor->cur;
                    auto backspaced_to = editor->nvim_insert.backspaced_to;
                    auto start = editor->nvim_insert.start;

                    // set new lines
                    start_request_message("nvim_buf_set_lines", 5);
                    writer.write_int(editor->nvim_data.buf_id);
                    writer.write_int(backspaced_to.y);
                    writer.write_int(start.y + 1);
                    writer.write_bool(false);
                    writer.write_array(cur.y - backspaced_to.y + 1);
                    for (u32 y = backspaced_to.y; y <= cur.y; y++) {
                        auto& line = editor->buf.lines[y];
                        writer.write1(MP_OP_STRING);
                        writer.write4(line.len);
                        For (line) writer.write1(it);
                    }
                    end_message();

                    // move cursor
                    {
                        auto cur = editor->cur;
                        if (cur.x > 0) cur.x--; // simulate the "back 1" that vim normally does when exiting insert mode
                        print("sending move_cursor to %s", format_pos(cur));
                        editor->raw_move_cursor(cur);
                    }

                    auto msgid = start_request_message("nvim_win_set_cursor", 2);
                    save_request(NVIM_REQ_POST_INSERT_MOVE_CURSOR, msgid, editor->id);
                    writer.write_int(editor->nvim_data.win_id);
                    {
                        writer.write_array(2);
                        writer.write_int(editor->cur.y + 1);
                        writer.write_int(editor->cur.x);
                    }
                    end_message();
                }
                break;

            case NVIM_REQ_POST_INSERT_MOVE_CURSOR:
                print("sending escape", format_pos(editor->cur));
                // send the <esc> key that we delayed
                start_request_message("nvim_input", 1);
                writer.write_string("<Esc>");
                end_message();
                break;

            case NVIM_REQ_AUTOCOMPLETE_SETBUF:
                editor->move_cursor(req->autocomplete_setbuf.target_cursor);
                break;

            case NVIM_REQ_GET_API_INFO:
                // pass channel id to neovim so we can send it to rpcrequest
                start_request_message("nvim_set_var", 2);
                writer.write_string("channel_id");
                writer.write_int(event->response.channel_id);
                end_message();
                break;

            case NVIM_REQ_RESIZE:
                editor->nvim_data.is_resizing = false;
                break;

            case NVIM_REQ_CREATE_BUF:
                editor->nvim_data.buf_id = event->response.buf.object_id;
                // editor->nvim_data.status = ENS_WIN_PENDING;

                {
                    auto msgid = start_request_message("nvim_buf_attach", 3);
                    save_request(NVIM_REQ_BUF_ATTACH, msgid, editor->id);

                    writer.write_int(event->response.buf.object_id);
                    writer.write_bool(false);
                    writer.write_map(0);
                    end_message();
                }

                {
                    start_request_message("nvim_buf_set_lines", 5);

                    writer.write_int(event->response.buf.object_id);
                    writer.write_int(0);
                    writer.write_int(-1);
                    writer.write_bool(false);

                    writer.write_array(editor->buf.lines.len);
                    For (editor->buf.lines) {
                        writer.write1(MP_OP_STRING);
                        writer.write4(it.len);
                        For (it) writer.write1(it);
                    }

                    // editor->buf.clear();
                    end_message();
                }

                {
                    typedef fn<void()> write_value_func;
                    auto set_option = [&](ccstr key, write_value_func f) {
                        start_request_message("nvim_buf_set_option", 3);
                        writer.write_int(event->response.buf.object_id);
                        writer.write_string(key);
                        f();
                        end_message();
                    };

                    set_option("shiftwidth", [&]() { writer.write_int(2); });
                    set_option("tabstop",    [&]() { writer.write_int(2); });
                    set_option("expandtab",  [&]() { writer.write_bool(false); });
                    set_option("wrap",       [&]() { writer.write_bool(false); });
                    set_option("autoindent", [&]() { writer.write_bool(true); });
                    set_option("filetype",   [&]() { writer.write_string("go"); });
                    // how to do equiv. of `filetype indent plugin on'?
                }

                {
                    auto msgid = start_request_message("nvim_open_win", 3);
                    save_request(NVIM_REQ_OPEN_WIN, msgid, editor->id);

                    writer.write_int(event->response.buf.object_id);
                    writer.write_bool(false);
                    writer.write_map(5);
                    writer.write_string("relative"); writer.write_string("win");
                    writer.write_string("row"); writer.write_int(0);
                    writer.write_string("col"); writer.write_int(0);
                    writer.write_string("width"); writer.write_int(200);
                    writer.write_string("height"); writer.write_int(100);
                    end_message();
                }
                break;
            case NVIM_REQ_SET_CURRENT_WIN:
                if (editor->id == waiting_focus_window)
                    waiting_focus_window = 0;
                break;
            case NVIM_REQ_OPEN_WIN:
                editor->nvim_data.win_id = event->response.win.object_id;
                if (waiting_focus_window == editor->id)
                    set_current_window(editor);
                break;
            case NVIM_REQ_BUF_ATTACH:
                editor->nvim_data.is_buf_attached = true;
                break;
            case NVIM_REQ_UI_ATTACH:
                is_ui_attached = true;
                break;
            }
        }
        break;
    case MPRPC_NOTIFICATION:
        nvim_print("received NOTIFICATION event, notification type = %s", nvim_notification_type_str(event->notification.type));
        switch (event->notification.type) {
        case NVIM_NOTIF_CUSTOM_MOVE_CURSOR:
            {
                auto editor = world.get_current_editor();
                if (editor == NULL) break;

                auto &view = editor->view;
                u32 y = 0;

                switch (event->notification.custom_move_cursor.screen_pos) {
                case SCREEN_POS_TOP:
                    y = view.y;
                    break;
                case SCREEN_POS_MIDDLE:
                    y = view.y + (view.h / 2);
                    break;
                case SCREEN_POS_BOTTOM:
                    y = view.y + view.h - 1;
                    break;
                }

                if (y >= editor->buf.lines.len)
                    y = editor->buf.lines.len-1;

                auto &line = editor->buf.lines[y];
                u32 x = 0;
                while (x < line.len && isspace((char)line[x]))
                    x++;
                if (x == line.len) x = 0;

                editor->move_cursor(new_cur2(x, y));
            }
            break;
        case NVIM_NOTIF_CUSTOM_REVEAL_LINE:
            {
                auto editor = world.get_current_editor();
                if (editor == NULL) break;

                u32 y = editor->cur.y;
                auto &view = editor->view;

                switch (event->notification.custom_reveal_line.screen_pos) {
                case SCREEN_POS_TOP:
                    view.y = y;
                    break;
                case SCREEN_POS_MIDDLE:
                    view.y = relu_sub(y, view.h / 2);
                    break;
                case SCREEN_POS_BOTTOM:
                    view.y = relu_sub(y + 1, view.h);
                    break;
                }

                if (event->notification.custom_reveal_line.reset_cursor) {
                    auto &line = editor->buf.lines[y];
                    u32 x = 0;
                    while (x < line.len && isspace((char)line[x]))
                        x++;
                    if (x == line.len) x = 0;

                    editor->move_cursor(new_cur2(x, y));
                }
            }
            break;
        case NVIM_NOTIF_BUF_LINES:
            {
                auto &args = event->notification.buf_lines;
                auto editor = find_editor_by_buffer(args.buf.object_id);

                auto is_change_empty = (args.firstline == args.lastline && args.lines->len == 0);

                bool skip = (
                    editor == NULL
                    || !editor->nvim_data.got_initial_lines
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
            }
            break;
        case NVIM_NOTIF_MODE_CHANGE:
            {
                auto &args = event->notification.mode_change;

                print("mode: %s", args.mode_name);

                if (streq(args.mode_name, "normal"))
                    mode = VI_NORMAL;
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

                auto editor = world.get_current_editor();
                if (editor != NULL) {
                    if (mode == VI_INSERT) {
                        editor->nvim_insert.start = editor->cur;
                        editor->nvim_insert.backspaced_to = editor->cur;
                    } else if (mode == VI_NORMAL) {
                        if (editor->nvim_data.waiting_for_move_cursor) {
                            editor->nvim_data.waiting_for_move_cursor = false;
                            editor->move_cursor(editor->nvim_data.move_cursor_to);
                        }
                    }
                }

                if (mode != VI_INSERT && exiting_insert_mode) {
                    start_request_message("nvim_input", 1);
                    writer.write_string(chars_after_exiting_insert_mode.items, chars_after_exiting_insert_mode.len);
                    end_message();

                    chars_after_exiting_insert_mode.len = 0;
                    exiting_insert_mode = false;
                }
            }
            break;

        case NVIM_NOTIF_WIN_VIEWPORT:
            {
                auto &args = event->notification.win_viewport;

                assoc_grid_with_window(args.grid, args.window.object_id);

                auto editor = find_editor_by_window(args.window.object_id);
                if (editor == NULL)
                    break; // TODO: handle us still receiving notifications for nonexistent window

                print("got cursor change to %s", format_pos(new_cur2((u32)args.curcol, (u32)args.curline)));

                /*
                Here is the situation. Through testing, I've found that when
                you exit insert mode, cursor change is sent before mode change.

                So we're going to keep ignoring cursor change notifications in
                insert mode, and the notification that moves the cursor 1 back
                after leaving insert mode will be ignored (since it comes
                before mode change, we will still be in insert mode).  Then we
                will move the cursor back 1 *manually.*

                If the cursor-before-mode order changes, we will need to fix
                this. It's also possible this is a race condition that so far
                has happened to put the cursor-change notification first?
                Fucking hell, this is why async APIs (and APIs in general) are
                gay.
                */
                if (mode != VI_INSERT)
                    editor->raw_move_cursor(new_cur2((u32)args.curcol, (u32)args.curline));
                else
                    print("in insert mode, ignoring cursor change");

                if (!editor->nvim_data.got_initial_cur) {
                    nvim_print("got_initial_cur = false, setting to true & calling handle_editor_on_ready()");
                    editor->nvim_data.got_initial_cur = true;
                    handle_editor_on_ready(editor);
                }
            }
            break;

        case NVIM_NOTIF_WIN_POS:
            {
                auto &args = event->notification.win_pos;
                assoc_grid_with_window(args.grid, args.window.object_id);
            }
            break;
        }
        break;
    }
}

void Nvim::run_event_loop() {
    auto before_exit = [&]() {
        print("exiting...");
    };

#define ASSERT(x) if (!(x)) { before_exit(); return; }
#define CHECKOK() ASSERT(reader.ok)

    {
        auto msgid = start_request_message("nvim_ui_attach", 3);
        save_request(NVIM_REQ_UI_ATTACH, msgid, 0);

        writer.write_int(200);
        writer.write_int(100);
        {
            writer.write_map(2);
            writer.write_string("ext_linegrid"); writer.write_bool(true);
            writer.write_string("ext_multigrid"); writer.write_bool(true);
        }
        end_message();
    }

    {
        auto msgid = start_request_message("nvim_get_api_info", 0);
        save_request(NVIM_REQ_GET_API_INFO, msgid, 0);
        end_message();
    }

    while (true) {
        auto type = reader.peek_type();
        s32 msglen = reader.read_array(); CHECKOK();
        auto msgtype = (MprpcMessageType)reader.read_int(); CHECKOK();

        auto expected_len = (msgtype == MPRPC_NOTIFICATION ? 3 : 4);
        ASSERT(msglen == expected_len);

        auto add_event = [&](fn<void(Nvim_Message*)> f) {
            world.add_event([&](Main_Thread_Message *msg) {
                msg->type = MTM_NVIM_MESSAGE;
                msg->nvim_message.type = msgtype;
                f(&msg->nvim_message);
            });
        };

        switch (msgtype) {
        case MPRPC_NOTIFICATION:
            {
                auto method = reader.read_string(); CHECKOK();
                // print("got notification with method %s", method);
                auto params_length = reader.read_array(); CHECKOK();

                auto just_skip_params = [&]() {
                    for (u32 i = 0; i < params_length; i++) {
                        reader.skip_object();
                        CHECKOK();
                    }
                };

                print("method: %s", method);

                if (streq(method, "custom_notification")) {
                    SCOPED_FRAME();

                    auto cmd = reader.read_string(); CHECKOK();
                    auto num_args = reader.read_array(); CHECKOK();
                    if (streq(cmd, "reveal_line")) {
                        assert(num_args == 2);
                        auto screen_pos = (Screen_Pos)reader.read_int(); CHECKOK();
                        auto reset_cursor = (bool)reader.read_int();
                        add_event([&](Nvim_Message *msg) {
                            msg->notification.type = NVIM_NOTIF_CUSTOM_REVEAL_LINE;
                            msg->notification.custom_reveal_line.screen_pos = screen_pos;
                            msg->notification.custom_reveal_line.reset_cursor = reset_cursor;
                        });
                    } else if (streq(cmd, "move_cursor")) {
                        assert(num_args == 1);
                        auto screen_pos = (Screen_Pos)reader.read_int(); CHECKOK();
                        add_event([&](Nvim_Message *msg) {
                            msg->notification.type = NVIM_NOTIF_CUSTOM_MOVE_CURSOR;
                            msg->notification.custom_move_cursor.screen_pos = screen_pos;
                        });
                    }
                } else if (streq(method, "nvim_buf_lines_event")) {
                    SCOPED_FRAME();

                    auto buf = reader.read_ext(); CHECKOK();
                    auto changedtick = reader.read_int(); CHECKOK();
                    auto firstline = reader.read_int(); CHECKOK();
                    auto lastline = reader.read_int(); CHECKOK();
                    auto num_lines = reader.read_array(); CHECKOK();

                    add_event([&](Nvim_Message *msg) {
                        msg->notification.type = NVIM_NOTIF_BUF_LINES;
                        msg->notification.buf_lines.buf = *buf;
                        msg->notification.buf_lines.changedtick = changedtick;
                        msg->notification.buf_lines.firstline = firstline;
                        msg->notification.buf_lines.lastline = lastline;

                        auto lines = alloc_list<uchar*>();
                        auto line_lengths = alloc_list<s32>();

                        for (u32 i = 0; i < num_lines; i++) {
                            auto line = reader.read_string(); CHECKOK();
                            auto len = strlen(line);
                            {
                                SCOPED_MEM(&messages_mem);
                                auto unicode_line = alloc_array(uchar, len);
                                for (u32 i = 0; i < len; i++)
                                    unicode_line[i] = line[i];
                                lines->append(unicode_line);
                                line_lengths->append(len);
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

                        for (u32 j = 1; j < argsets_len; j++) {
                            auto args_len = reader.read_array(); CHECKOK();

                            if (streq(op, "mode_change")) {
                                SCOPED_FRAME();
                                ASSERT(args_len == 2);

                                auto mode_name = reader.read_string(); CHECKOK();
                                auto mode_index = reader.read_int(); CHECKOK();

                                add_event([&](Nvim_Message *m) {
                                    m->notification.type = NVIM_NOTIF_MODE_CHANGE;
                                    m->notification.mode_change.mode_name = our_strcpy(mode_name);
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

                                add_event([&](Nvim_Message *m) {
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

                                add_event([&](Nvim_Message *m) {
                                    m->notification.type = NVIM_NOTIF_WIN_POS;
                                    m->notification.win_pos.grid = grid;
                                    m->notification.win_pos.window = *window;
                                });
                            } else if (streq(op, "win_external_pos")) {
                                ASSERT(args_len == 2);
                                auto grid = reader.read_int(); CHECKOK();
                                auto window = reader.read_ext(); CHECKOK();

                                add_event([&](Nvim_Message *m) {
                                    m->notification.type = NVIM_NOTIF_WIN_POS;
                                    m->notification.win_pos.grid = grid;
                                    m->notification.win_pos.window = *window;
                                });
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
            }
            break;
        case MPRPC_REQUEST:
            {
                auto msgid = reader.read_int(); CHECKOK();
                auto method = reader.read_string(); CHECKOK();
                auto params_length = reader.read_array(); CHECKOK();

                // print("got request with method %s", method);
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
            }
            break;
        case MPRPC_RESPONSE:
            {
                auto msgid = reader.read_int(); CHECKOK();
                nvim_print("[raw] got response with msgid %d", msgid);

                auto req = find_request_by_msgid(msgid);
                if (req == NULL) {
                    nvim_print("couldn't find request for msgid %d", msgid);
                    reader.skip_object(); CHECKOK(); // error
                    reader.skip_object(); CHECKOK(); // result
                    break;
                }
                defer { requests.remove(req); };

                if (reader.peek_type() != MP_NIL) {
                    if (reader.peek_type() == MP_STRING) {
                        auto error_str = reader.read_string(); CHECKOK();
                        SCOPED_FRAME();
                        print("error in response for msgid %d: %d", msgid, error_str);
                    } else {
                        auto type = reader.peek_type();
                        reader.skip_object(); CHECKOK();
                        print("error in response for msgid %d: (error was not a string, instead was %s)", msgid, mptype_str(type));
                    }
                    reader.skip_object();
                    break;
                }

                // read the error
                reader.read_nil(); CHECKOK();

                // grab associated editor, break if we can't find it
                Editor* editor = NULL;
                if (req->editor_id != 0) {
                    auto is_match = [&](Editor* it) -> bool { return it->id == req->editor_id; };
                    editor = world.find_editor(is_match);
                    if (editor == NULL) {
                        reader.skip_object();
                        break;
                    }
                }

                auto add_response_event = [&](fn<void(Nvim_Message*)> f) {
                    add_event([&](Nvim_Message *it) {
                        auto req_copy = alloc_object(Nvim_Request);
                        memcpy(req_copy, req, sizeof(Nvim_Request));
                        it->response.original_request = req_copy;
                        f(it);
                    });
                };

                switch (req->type) {
                case NVIM_REQ_POST_INSERT_GETCHANGEDTICK:
                    {
                        auto changedtick = reader.read_int(); CHECKOK();
                        add_response_event([&](Nvim_Message *m) {
                            m->response.changedtick = changedtick;
                        });
                    }
                    break;
                case NVIM_REQ_GET_API_INFO:
                    {
                        auto len = reader.read_array(); CHECKOK();
                        ASSERT(len == 2);

                        auto channel_id = reader.read_int(); CHECKOK();
                        reader.skip_object(); CHECKOK();

                        add_response_event([&](Nvim_Message *m) {
                            m->response.channel_id = channel_id;
                        });
                    }
                    break;
                case NVIM_REQ_CREATE_BUF:
                    {
                        if (reader.peek_type() != MP_EXT) {
                            reader.skip_object(); CHECKOK(); // check if this path ever gets hit
                            break;
                        }

                        auto buf = reader.read_ext(); CHECKOK();
                        add_response_event([&](Nvim_Message *m) {
                            m->response.buf = *buf;
                        });
                    }
                    break;

                case NVIM_REQ_OPEN_WIN:
                    {
                        if (reader.peek_type() != MP_EXT) {
                            reader.skip_object(); CHECKOK(); // check if this path ever gets hit
                            break;
                        }

                        auto win = reader.read_ext(); CHECKOK();
                        add_response_event([&](Nvim_Message *m) {
                            m->response.win = *win;
                        });
                    }
                    break;

                default:
                    reader.skip_object();
                    add_response_event([&](Nvim_Message*) {});
                    break;
                }
            }
            break;
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

#define NVIM_DEFAULT_WIDTH 200
#define NVIM_DEFAULT_HEIGHT 100

void Nvim::init() {
    ptr0(this);

    mem.init();
    loop_mem.init();

    request_id = 0;

    SCOPED_MEM(&mem);

    send_lock.init();
    requests_lock.init();
    requests.init();
    messages_lock.init();
    message_queue.init();
    messages_mem.init();
    grid_to_window.init();
    chars_after_exiting_insert_mode.init();
}

void Nvim::start_running() {
    SCOPED_MEM(&mem);

    nvim_proc.init();
    nvim_proc.use_stdin = true;
    nvim_proc.dir = "c:/users/brandon/ide"; // TODO

    // TODO: get full path of init.vim
    nvim_proc.run("nvim -u ./init.vim -i NONE -N --embed --headless");

    reader.proc = &nvim_proc;
    reader.offset = 0;
    writer.proc = &nvim_proc;

    auto func = [](void *p) {
        auto nvim = (Nvim*)p;
        SCOPED_MEM(&nvim->loop_mem);
        nvim->run_event_loop();
    };

    event_loop_thread = create_thread(func, this);
    if (event_loop_thread == NULL) return;
}

void Nvim::cleanup() {
    send_lock.cleanup();
    requests_lock.cleanup();

    if (event_loop_thread != NULL) {
        kill_thread(event_loop_thread);
        close_thread_handle(event_loop_thread);
    }

    nvim_proc.cleanup();

    mem.cleanup();
    loop_mem.cleanup();
    messages_mem.cleanup();
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

    NvimExtType type;
    s32 len = 0;

    switch (b) {
        case 0xd4:
        case 0xd5:
        case 0xd6:
        case 0xd7:
        case 0xd8:
            type = (NvimExtType)read1(); if (!ok) return NULL;
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
            type = (NvimExtType)read1(); if (!ok) return NULL;
            break;
        case 0xc8:
            len = (s32)(u16)read2(); if (!ok) return NULL;
            type = (NvimExtType)read1(); if (!ok) return NULL;
            break;
        case 0xc9:
            len = (s32)(u32)read4(); if (!ok) return NULL;
            type = (NvimExtType)read1(); if (!ok) return NULL;
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

ccstr mptype_str(MpType type) {
    switch (type) {
    define_str_case(MP_UNKNOWN);
    define_str_case(MP_BOOL);
    define_str_case(MP_INT);
    define_str_case(MP_DOUBLE);
    define_str_case(MP_STRING);
    define_str_case(MP_NIL);
    define_str_case(MP_ARRAY);
    define_str_case(MP_MAP);
    define_str_case(MP_EXT);
    }
    return NULL;
}
