#include "nvim.hpp"
#include "buffer.hpp"
#include "world.hpp"
#include "utils.hpp"
#include "go.hpp"
#include "settings.hpp"
// #include <strsafe.h>

#define NVIM_DEBUG 1

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

void Nvim::assoc_grid_with_window(u32 grid, u32 win) {
    auto& table = grid_to_window;

    auto pair = table.find_or_append([&](Grid_Window_Pair* it) { return it->grid == grid; });
    assert(pair != NULL);

    pair->grid = grid;
    pair->win = win;
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
    define_str_case(NVIM_REQ_CREATE_WIN);
    define_str_case(NVIM_REQ_BUF_ATTACH);
    define_str_case(NVIM_REQ_UI_ATTACH);
    define_str_case(NVIM_REQ_SET_CURRENT_WIN);
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
            auto req = find_request_by_msgid(event->response.msgid);
            if (req == NULL) {
                nvim_print("handle_message_from_main_thread: couldn't find request for msgid %d", event->response.msgid);
                break;
            }

            defer { delete_request_by_msgid(req->msgid); };

            nvim_print("received RESPONSE event, request type = %s", nvim_request_type_str(req->type));

            // grab associated editor, break if we can't find it
            Editor* editor = NULL;
            if (req->editor_id != 0) {
                editor = world.find_editor_by_id(req->editor_id);
                if (editor == NULL) break;
            }

            switch (req->type) {
            case NVIM_REQ_DOTREPEAT_CREATE_BUF:
                {
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
                }
                break;

            case NVIM_REQ_DOTREPEAT_CREATE_WIN:
                dotrepeat_win_id = event->response.win.object_id;
                break;

            case NVIM_REQ_GOTO_EXTMARK:
                {
                    auto &data = event->response.goto_extmark;
                    if (!data.ok) break;
                    editor->move_cursor(data.pos);
                }
                break;

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
                    auto old_cur = editor->cur;
                    {
                        auto cur = editor->cur;
                        if (cur.x > 0) cur.x--;
                        editor->raw_move_cursor(cur);
                    }

                    u32 delete_len = 0;
                    {
                        auto del_start = editor->nvim_insert.backspaced_to;
                        auto del_end = editor->nvim_insert.start;
                        auto& buf = editor->buf;

                        for (u32 y = del_start.y; y <= del_end.y; y++) {
                            delete_len += buf.lines[y].len + 1;
                            if (y == del_start.y)
                                delete_len -= del_start.x;
                            if (y == del_end.y)
                                delete_len -= (buf.lines[y].len + 1 - del_end.x);
                        }
                    }

                    auto msgid = start_request_message("nvim_call_atomic", 1);
                    save_request(NVIM_REQ_POST_INSERT_DOTREPEAT_REPLAY, msgid, editor->id);
                    {
                        writer.write_array(5);

                        {
                            writer.write_array(2);
                            writer.write_string("nvim_win_set_cursor");
                            {
                                writer.write_array(2);
                                writer.write_int(editor->nvim_data.win_id);
                                {
                                    writer.write_array(2);
                                    writer.write_int(editor->cur.y + 1);
                                    writer.write_int(editor->cur.x);
                                }
                            }
                        }

                        {
                            writer.write_array(2);
                            writer.write_string("nvim_set_current_win");
                            {
                                writer.write_array(1);
                                writer.write_int(dotrepeat_win_id);
                            }
                        }

                        {
                            writer.write_array(2);
                            writer.write_string("nvim_buf_set_lines");
                            {
                                writer.write_array(5);
                                writer.write_int(dotrepeat_buf_id);
                                writer.write_int(0);
                                writer.write_int(-1);
                                writer.write_bool(false);
                                {
                                    writer.write_array(1);
                                    writer.write1(MP_OP_STRING);
                                    writer.write4(delete_len);
                                    for (u32 i = 0; i < delete_len; i++)
                                        writer.write1('x');
                                }
                            }
                        }

                        {
                            writer.write_array(2);
                            writer.write_string("nvim_win_set_cursor");
                            {
                                writer.write_array(2);
                                writer.write_int(dotrepeat_win_id);
                                {
                                    writer.write_array(2);
                                    writer.write_int(1);
                                    writer.write_int(delete_len);
                                }
                            }
                        }

                        {
                            Text_Renderer r;
                            r.init();
                            for (u32 i = 0; i < delete_len; i++)
                                r.writestr("<BS>");

                            auto it = editor->buf.iter(editor->nvim_insert.backspaced_to);
                            while (it.pos < old_cur) {
                                // wait, does this take utf-8?
                                auto ch = it.next();
                                if (ch == '<')
                                    r.writestr("<LT>");
                                else
                                    r.writechar((char)ch);
                            }

                            writer.write_array(2);
                            writer.write_string("nvim_input");
                            {
                                writer.write_array(1);
                                writer.write_string(r.finish());
                            }
                        }
                    }

                    end_message();
                }
                break;

            case NVIM_REQ_POST_INSERT_DOTREPEAT_REPLAY:
                start_request_message("nvim_call_atomic", 1);
                writer.write_array(2);
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
                        writer.write_string("<C-O>:stopinsert<CR>");
                    }
                }
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

            case NVIM_REQ_CREATE_BUF:
                editor->nvim_data.buf_id = event->response.buf.object_id;

                {
                    auto msgid = start_request_message("nvim_call_atomic", 1);
                    save_request(NVIM_REQ_BUF_ATTACH, msgid, editor->id);

                    writer.write_array(7); // TODO

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

                            writer.write_array(editor->buf.lines.len);
                            For (editor->buf.lines) {
                                writer.write1(MP_OP_STRING);
                                writer.write4(it.len);
                                For (it) writer.write1(it);
                            }
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

                    set_option("shiftwidth", [&]() { writer.write_int(4); });
                    set_option("tabstop",    [&]() { writer.write_int(4); });
                    set_option("expandtab",  [&]() { writer.write_bool(false); });
                    set_option("autoindent", [&]() { writer.write_bool(true); });
                    set_option("filetype",   [&]() { writer.write_string("go"); });
                    // how to do equiv. of `filetype indent plugin on'?

                    end_message();
                }

                {
                    auto msgid = start_request_message("nvim_open_win", 3);
                    save_request(NVIM_REQ_CREATE_WIN, msgid, editor->id);

                    writer.write_int(event->response.buf.object_id);
                    writer.write_bool(false);
                    writer.write_map(5);
                    writer.write_string("relative"); writer.write_string("win");
                    writer.write_string("row"); writer.write_int(0);
                    writer.write_string("col"); writer.write_int(0);
                    writer.write_string("width"); writer.write_int(NVIM_DEFAULT_WIDTH);
                    writer.write_string("height"); writer.write_int(NVIM_DEFAULT_HEIGHT);
                    end_message();
                }
                break;
            case NVIM_REQ_SET_CURRENT_WIN:
                if (editor->id == waiting_focus_window)
                    waiting_focus_window = 0;
                break;
            case NVIM_REQ_CREATE_WIN:
                editor->nvim_data.win_id = event->response.win.object_id;
                if (waiting_focus_window == editor->id)
                    set_current_window(editor);

                {
                    auto &b = world.build;
                    if (!b.ready()) break;

                    List<u32> *error_indexes = NULL;
                    {
                        SCOPED_MEM(&requests_mem);
                        error_indexes = alloc_list<u32>();
                    }

                    struct Call {
                        u32 buf_id;
                        cur2 pos;
                    };

                    auto calls = alloc_list<Call>();

                    auto editor_path = get_path_relative_to(editor->filepath, world.current_path);
                    for (u32 i = 0; i < b.errors.len; i++) {
                        auto &it = b.errors[i];

                        if (it.nvim_extmark != 0) continue;
                        if (!it.valid) continue;
                        if (!are_filepaths_equal(editor_path, it.file)) continue;

                        error_indexes->append(i);

                        auto call = calls->append();
                        call->buf_id = editor->nvim_data.buf_id;
                        call->pos = new_cur2(it.col - 1, it.row - 1);
                    }

                    auto msgid = start_request_message("nvim_call_atomic", 1);
                    auto req = save_request(NVIM_REQ_CREATE_EDITOR_EXTMARKS, msgid, 0);
                    defer { end_message(); };

                    writer.write_array(calls->len);
                    For (*calls) {
                        writer.write_array(2);
                        writer.write_string("nvim_buf_set_extmark");
                        {
                            writer.write_array(5);
                            writer.write_int(it.buf_id);
                            writer.write_int(b.nvim_namespace_id);
                            writer.write_int(it.pos.y);
                            writer.write_int(it.pos.x);
                            writer.write_map(0);
                        }
                    }

                    req->create_extmarks.error_indexes = error_indexes;
                }
                break;
            case NVIM_REQ_BUF_ATTACH:
                editor->nvim_data.is_buf_attached = true;
                break;
            case NVIM_REQ_UI_ATTACH:
                is_ui_attached = true;
                break;

            case NVIM_REQ_CREATE_EXTMARKS_CREATE_NAMESPACE:
                {
                    auto &b = world.build;
                    b.nvim_namespace_id = event->response.namespace_id;

                    List<u32> *error_indexes = NULL;
                    {
                        SCOPED_MEM(&requests_mem);
                        error_indexes = alloc_list<u32>();
                    }

                    struct Call {
                        cur2 pos;
                        u32 buf_id;
                    };

                    auto calls = alloc_list<Call>();
                    For (world.panes) {
                        For (it.editors) {
                            auto editor = it;
                            auto path = get_path_relative_to(it.filepath, world.current_path);
                            for (u32 i = 0; i < b.errors.len; i++) {
                                auto &it = b.errors[i];
                                if (!it.valid) continue;
                                if (!are_filepaths_equal(path, it.file)) continue;

                                error_indexes->append(i);
                                auto call = calls->append();
                                call->pos = new_cur2(it.col - 1, it.row - 1);
                                call->buf_id = editor.nvim_data.buf_id;
                            }
                        }
                    }

                    auto msgid = start_request_message("nvim_call_atomic", 1);
                    auto req2 = save_request(NVIM_REQ_CREATE_EXTMARKS_SET_EXTMARKS, msgid, 0);
                    defer { end_message(); };

                    writer.write_array(calls->len);
                    For (*calls) {
                        writer.write_array(2);
                        writer.write_string("nvim_buf_set_extmark");
                        {
                            writer.write_array(5);
                            writer.write_int(it.buf_id);
                            writer.write_int(b.nvim_namespace_id);
                            writer.write_int(it.pos.y);
                            writer.write_int(it.pos.x);
                            writer.write_map(0);
                        }
                    }

                    req2->create_extmarks.error_indexes = error_indexes;
                }
                break;

            case NVIM_REQ_CREATE_EXTMARKS_SET_EXTMARKS:
            case NVIM_REQ_CREATE_EDITOR_EXTMARKS:
                {
                    auto error_indexes = req->create_extmarks.error_indexes;
                    auto items_to_process = min(error_indexes->len, event->response.extmarks->len);

                    for (u32 i = 0; i < items_to_process; i++) {
                        auto &err = world.build.errors[error_indexes->at(i)];
                        auto extmark = event->response.extmarks->at(i);
                        err.nvim_extmark = extmark;
                    }

                    if (req->type == NVIM_REQ_CREATE_EXTMARKS_SET_EXTMARKS)
                        world.build.creating_extmarks = false;
                }
                break;
            }
        }
        break;
    case MPRPC_NOTIFICATION:
        nvim_print("received NOTIFICATION event, notification type = %s", nvim_notification_type_str(event->notification.type));
        switch (event->notification.type) {
        case NVIM_NOTIF_GRID_CLEAR:
            {
                auto &args = event->notification.grid_line;
                auto editor = find_editor_by_grid(args.grid);
                if (editor == NULL) break;
                mem0(editor->highlights, sizeof(editor->highlights));
            }
            break;
        case NVIM_NOTIF_GRID_LINE:
            {
                auto &args = event->notification.grid_line;

                auto editor = find_editor_by_grid(args.grid);
                if (editor == NULL) break;

                // args.row, args.col

                i32 last_hl = -1;
                Hl_Type last_hltype = HL_NONE;
                auto col = args.col;

                For (*args.cells) {
                    if (it.hl != -1 && it.hl != last_hl) {
                        last_hl = it.hl;
                        auto def = hl_defs.find([&](Hl_Def *it) { return it->id == last_hl; });
                        last_hltype = def == NULL ? HL_NONE : def->type;
                    }
                    for (u32 i = 0; (i < (it.reps != 0 ? it.reps : 1)) && col < NVIM_DEFAULT_WIDTH; i++) {
                        editor->highlights[args.row][col] = last_hltype;
                        col++;
                    }
                }
            }
            break;
        case NVIM_NOTIF_GRID_SCROLL:
            {
                auto &args = event->notification.grid_scroll;

                auto editor = find_editor_by_grid(args.grid);
                if (editor == NULL) break;

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
            }
            break;
        case NVIM_NOTIF_HL_ATTR_DEFINE:
            {
                auto &args = event->notification.hl_attr_define;

                nvim_print("got hl def for: %s", args.hi_name);

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
            }
            break;
        case NVIM_NOTIF_CUSTOM_MOVE_CURSOR:
            {
                auto editor = world.get_current_editor();
                if (editor == NULL) break;

                auto &view = editor->view;
                u32 y = 0;

                switch (event->notification.custom_move_cursor.screen_pos) {
                case SCREEN_POS_TOP:
                    y = view.y + min(view.h - 1, settings.scrolloff);
                    break;
                case SCREEN_POS_MIDDLE:
                    y = view.y + (view.h / 2);
                    break;
                case SCREEN_POS_BOTTOM:
                    y = view.y + relu_sub(view.h, 1 + settings.scrolloff);
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
                    view.y = relu_sub(y, settings.scrolloff);
                    break;
                case SCREEN_POS_MIDDLE:
                    view.y = relu_sub(y, view.h / 2);
                    break;
                case SCREEN_POS_BOTTOM:
                    view.y = relu_sub(y + settings.scrolloff + 1, view.h);
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

        case NVIM_NOTIF_CMDLINE_SHOW:
            {
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
            }
            break;

        case NVIM_NOTIF_MODE_CHANGE:
            {
                auto &args = event->notification.mode_change;

                nvim_print("mode: %s", args.mode_name);

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

                editor->nvim_data.grid_topline = args.topline;

                nvim_print("got cursor change to %s", format_pos(new_cur2((u32)args.curcol, (u32)args.curline)));
                if (mode != VI_INSERT)
                    editor->raw_move_cursor(new_cur2((u32)args.curcol, (u32)args.curline));

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

    if (started_messages.len > 0) {
        print("---");
        for (i32 i = started_messages.len - 1; i >= 0; i--)
            print("%s", started_messages[i]);
        panic("message not closed");
    }
}

void Nvim::run_event_loop() {
#define ASSERT(x) if (!(x)) { panic("nvim crashed"); }
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
                        add_event([&](Nvim_Message *msg) {
                            msg->notification.type = NVIM_NOTIF_CUSTOM_REVEAL_LINE;
                            msg->notification.custom_reveal_line.screen_pos = screen_pos;
                            msg->notification.custom_reveal_line.reset_cursor = reset_cursor;
                        });
                    } else if (streq(cmd, "move_cursor")) {
                        ASSERT(num_args == 1);
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

                                add_event([&](Nvim_Message *m) {
                                    m->notification.type = NVIM_NOTIF_CMDLINE_SHOW;
                                    m->notification.cmdline_show.content = our_strcpy(content);
                                    m->notification.cmdline_show.firstc = our_strcpy(firstc);
                                    m->notification.cmdline_show.prompt = our_strcpy(prompt);
                                });
                            } else if (streq(op, "mode_change")) {
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
                            } else if (streq(op, "grid_clear")) {
                                ASSERT(args_len == 1);
                                auto grid = reader.read_int(); CHECKOK();
                                add_event([&](Nvim_Message *m) {
                                    m->notification.type = NVIM_NOTIF_GRID_CLEAR;
                                    m->notification.grid_clear.grid = grid;
                                });
                            } else if (streq(op, "grid_line")) {
                                ASSERT(args_len == 4);

                                auto grid = reader.read_int(); CHECKOK();
                                auto row = reader.read_int(); CHECKOK();
                                auto col = reader.read_int(); CHECKOK();
                                auto num_cells = reader.read_array(); CHECKOK();

                                add_event([&](Nvim_Message *m) {
                                    m->notification.type = NVIM_NOTIF_GRID_LINE;
                                    m->notification.grid_line.grid = grid;
                                    m->notification.grid_line.row = row;
                                    m->notification.grid_line.col = col;

                                    auto cells = alloc_list<Grid_Cell>();
                                    for (u32 k = 0; k < num_cells; k++) {
                                        auto cell_len = reader.read_array();
                                        ASSERT(1 <= cell_len && cell_len <= 3);

                                        // skip character (we don't care)
                                        ASSERT(reader.peek_type() == MP_STRING);
                                        reader.skip_object(); CHECKOK();

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

                                add_event([&](Nvim_Message* m) {
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
                                        if (hi_name != NULL) {
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

                                if (hi_name != NULL) {
                                    add_event([&](Nvim_Message *m) {
                                        m->notification.type = NVIM_NOTIF_HL_ATTR_DEFINE;
                                        m->notification.hl_attr_define.id = id;
                                        m->notification.hl_attr_define.hi_name = our_strcpy(hi_name);
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
            }
            break;
        case MPRPC_REQUEST:
            {
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
            }
            break;
        case MPRPC_RESPONSE:
            {
                auto msgid = reader.read_int(); CHECKOK();
                nvim_print("[raw] got response with msgid %d", msgid);

                u32 req_editor_id;
                Nvim_Request_Type req_type;

                {
                    SCOPED_LOCK(&requests_lock);
                    auto req = find_request_by_msgid(msgid);
                    if (req == NULL) {
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
                if (req_editor_id != 0) {
                    auto is_match = [&](Editor* it) -> bool { return it->id == req_editor_id; };
                    editor = world.find_editor(is_match);
                    if (editor == NULL) {
                        reader.skip_object();
                        delete_request_by_msgid(msgid);
                        break;
                    }
                }

                auto add_response_event = [&](fn<void(Nvim_Message*)> f) {
                    add_event([&](Nvim_Message *it) {
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
                case NVIM_REQ_DOTREPEAT_CREATE_BUF:
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

                case NVIM_REQ_GOTO_EXTMARK:
                    {
                        bool ok = true;
                        u32 row = 0, col = 0;

                        auto arr_len = reader.read_array(); CHECKOK();
                        if (arr_len == 0) {
                            ok = false;
                        } else if (arr_len == 2) {
                            row = reader.read_int(); CHECKOK();
                            col = reader.read_int(); CHECKOK();
                        } else {
                            panic(our_sprintf("got array with %d items from nvim_buf_get_extmark_by_id", arr_len));
                        }

                        add_response_event([&](Nvim_Message *m) {
                            m->response.goto_extmark.ok = ok;
                            m->response.goto_extmark.pos = new_cur2(col, row);
                        });
                    }
                    break;

                case NVIM_REQ_CREATE_EXTMARKS_CREATE_NAMESPACE:
                    {
                        auto namespace_id = reader.read_int(); CHECKOK();
                        add_response_event([&](Nvim_Message *m) {
                            m->response.namespace_id = namespace_id;
                        });
                    }
                    break;

                case NVIM_REQ_CREATE_EXTMARKS_SET_EXTMARKS:
                case NVIM_REQ_CREATE_EDITOR_EXTMARKS:
                    read_atomic_call_response([&]() {
                        auto responses_len = reader.read_array(); CHECKOK();
                        add_response_event([&](Nvim_Message *m) {
                            auto extmarks = alloc_list<u32>(responses_len);
                            for (u32 i = 0; i < responses_len; i++)
                                extmarks->append(reader.read_int());
                            m->response.extmarks = extmarks;
                        });
                    });
                    break;

                case NVIM_REQ_POST_INSERT_DOTREPEAT_REPLAY:
                case NVIM_REQ_BUF_ATTACH:
                    read_atomic_call_response([&]() {
                        reader.skip_object(); CHECKOK();
                        add_response_event([&](Nvim_Message*) {});
                    });
                    break;

                case NVIM_REQ_CREATE_WIN:
                case NVIM_REQ_DOTREPEAT_CREATE_WIN:
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

void Nvim::init() {
    ptr0(this);

    mem.init();
    loop_mem.init();
    started_messages_mem.init();

    request_id = 0;

    SCOPED_MEM(&mem);

    requests_lock.init();
    requests.init();
    requests_mem.init();

    messages_lock.init();
    message_queue.init();
    messages_mem.init();

    grid_to_window.init();
    chars_after_exiting_insert_mode.init();

    cmdline.content.init();
    cmdline.firstc.init();
    cmdline.prompt.init();

    started_messages.init();

    hl_defs.init();
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
