#include "nvim.hpp"
#include "buffer.hpp"
#include "world.hpp"
#include "utils.hpp"
// #include <strsafe.h>

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
    auto idx = world.nvim_data.grid_to_window.find([&](Grid_Window_Pair* it) {
        return it->win == editor->nvim_data.win_id;
    });

    if (idx == -1) return false;

    auto& pair = world.nvim_data.grid_to_window[idx];

    editor->nvim_data.is_resizing = true;

    auto msgid = start_request_message("nvim_ui_try_resize_grid", 3);
    save_request(NVIM_REQ_RESIZE, msgid, editor->id);

    writer.write_int(pair.grid);
    writer.write_int(editor->view.w);
    writer.write_int(editor->view.h);
    end_message();
    return true;
}

void assoc_grid_with_window(u32 grid, u32 win) {
    auto& table = world.nvim_data.grid_to_window;

    auto pair = table.find_or_append([&](Grid_Window_Pair* it) { return it->grid == grid; });
    assert(pair != NULL);

    pair->grid = grid;
    pair->win = win;

    auto editor = find_editor_by_window(win);
    if (editor != NULL) {
        if (editor->nvim_data.need_initial_resize) {
            world.nvim.resize_editor(editor);
            editor->nvim_data.need_initial_resize = false;
        }
    }
}

Editor* find_editor_by_grid(u32 grid) {
    auto& table = world.nvim_data.grid_to_window;

    auto idx = table.find([&](Grid_Window_Pair* it) { return it->grid == grid; });
    if (idx == -1) return NULL;

    return find_editor_by_window(table[idx].win);
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

                    if (streq(method, "nvim_buf_lines_event")) {
                        SCOPED_FRAME();

                        auto buf_ext = reader.read_ext(); CHECKOK();
                        auto buf_id = buf_ext->object_id;

                        auto changedtick = reader.read_int(); CHECKOK();
                        auto firstline = reader.read_int(); CHECKOK();
                        auto lastline = reader.read_int(); CHECKOK();
                        auto num_lines = reader.read_array(); CHECKOK();

                        auto editor = find_editor_by_buffer(buf_id);
                        if (editor == NULL) {
                            for (u32 i = 0; i < num_lines; i++) {
                                SCOPED_FRAME();
                                reader.skip_object(); CHECKOK();
                            }
                        } else {
                            if (lastline == -1)
                                lastline = editor->buf.lines.len;

                            editor->buf.delete_lines(firstline, lastline);

                            for (u32 i = 0; i < num_lines; i++) {
                                SCOPED_FRAME();

                                auto line = reader.read_string(); CHECKOK();
                                if (editor != NULL) {
                                    auto len = strlen(line);
                                    auto unicode_line = alloc_array(uchar, len);
                                    for (u32 i = 0; i < len; i++)
                                        unicode_line[i] = line[i];
                                    editor->buf.insert_line(firstline + i, unicode_line, len);
                                }
                            }
                        }

                        // skip {more} param. we don't care, we'll just handle the next
                        // notif when it comes
                        reader.skip_object(); CHECKOK();

                        if (editor->nvim_data.need_initial_pos_set) {
                            editor->nvim_data.need_initial_pos_set = false;

                            auto pos = editor->nvim_data.initial_pos;
                            if (pos.y == -1)
                                pos = editor->offset_to_cur(pos.x);
                            editor->move_cursor(pos);
                        }
                    } else if (streq(method, "redraw")) {
                        for (u32 i = 0; i < params_length; i++) {
                            auto argsets_len = reader.read_array(); CHECKOK();
                            auto op = reader.read_string(); CHECKOK();

                            for (u32 j = 1; j < argsets_len; j++) {
                                auto args_len = reader.read_array(); CHECKOK();

                                if (streq(op, "mode_change")) {
                                    // i think this could cause race condition
                                    SCOPED_FRAME();
                                    ASSERT(args_len == 2);

                                    auto mode_name = reader.read_string(); CHECKOK();
                                    auto mode_index = reader.read_int(); CHECKOK();

                                    auto editor = world.get_current_editor();
                                    if (editor == NULL) break;

                                    if (streq(mode_name, "normal"))
                                        editor->nvim_data.vimode = VIMODE_NORMAL;
                                    else if (streq(mode_name, "insert"))
                                        editor->nvim_data.vimode = VIMODE_INSERT;
                                    else if (streq(mode_name, "replace"))
                                        editor->nvim_data.vimode = VIMODE_REPLACE;
                                    else if (streq(mode_name, "visual"))
                                        editor->nvim_data.vimode = VIMODE_VISUAL;
                                } else if (streq(op, "win_viewport")) {
                                    SCOPED_FRAME();
                                    ASSERT(args_len == 6);

                                    auto grid = reader.read_int(); CHECKOK();
                                    auto window = reader.read_ext(); CHECKOK();
                                    auto topline = reader.read_int(); CHECKOK();
                                    auto botline = reader.read_int(); CHECKOK();
                                    auto curline = reader.read_int(); CHECKOK();
                                    auto curcol = reader.read_int(); CHECKOK();

                                    assoc_grid_with_window(grid, window->object_id);

                                    auto editor = find_editor_by_window(window->object_id);
                                    if (editor == NULL)
                                        continue; // TODO: handle us still receiving notifications for nonexistent window

                                    editor->raw_move_cursor(new_cur2((u32)curcol, (u32)curline));
                                } else if (streq(op, "win_pos")) {
                                    ASSERT(args_len == 6);
                                    auto grid = reader.read_int(); CHECKOK();
                                    auto window = reader.read_ext(); CHECKOK();
                                    /* auto start_row = */ reader.read_int(); CHECKOK();
                                    /* auto start_col = */ reader.read_int(); CHECKOK();
                                    /* auto width = */ reader.read_int(); CHECKOK();
                                    /* auto height = */ reader.read_int(); CHECKOK();
                                    assoc_grid_with_window(grid, window->object_id);
                                } else if (streq(op, "win_external_pos")) {
                                    ASSERT(args_len == 2);
                                    auto grid = reader.read_int(); CHECKOK();
                                    auto window = reader.read_ext(); CHECKOK();
                                    assoc_grid_with_window(grid, window->object_id);
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

                    i32 idx = find_request_by_msgid(msgid);
                    if (idx == -1) {
                        reader.skip_object(); CHECKOK(); // error
                        reader.skip_object(); CHECKOK(); // result
                        break;
                    }

                    bool error = false;
                    if (reader.peek_type() != MP_NIL) {
                        error = true;
                        if (reader.peek_type() == MP_STRING) {
                            auto error_str = reader.read_string(); CHECKOK();
                            SCOPED_FRAME();
                            // print("error in response for msgid %d: %d", msgid, error_str);
                        } else {
                            auto type = reader.peek_type();
                            reader.skip_object(); CHECKOK();
                            // print("error in response for msgid %d: (error was not a string, instead was %s)", msgid, mptype_str(type));
                        }
                    } else {
                        reader.read_nil(); CHECKOK();
                    }

                    bool unhandled = false;

                    /*
                    At this point, we have read the error and printed it out if there was one.
                    If there was, `error` will be `true`.

                    For 'handled' requests (meaning it's a request type with a `case`
                    below), it's the handler's job (below) to both read the response object, AND
                    determine what action to take if there was an error.

                    For 'unhandled' requests, we just skip over the response.
                    */

                    auto req = &requests[idx];

                    Editor* editor = NULL;
                    if (req->editor_id != 0) {
                        auto is_match = [&](Editor* it) -> bool { return it->id == req->editor_id; };
                        editor = world.find_editor(is_match);
                        if (editor == NULL) {
                            reader.skip_object();
                            break;
                        }
                    }

                    switch (req->type) {
                        case NVIM_REQ_AUTOCOMPLETE_SETBUF:
                            {
                                ASSERT(editor != NULL);
                                reader.skip_object(); CHECKOK();
                                if (!error) {
                                    editor->move_cursor(req->autocomplete_setbuf.target_cursor);
                                }
                            }
                            break;

                        case NVIM_REQ_GET_API_INFO:
                            {
                                if (error) {
                                    reader.skip_object(); CHECKOK();
                                    break;
                                }

                                auto len = reader.read_array(); CHECKOK();
                                ASSERT(len == 2);

                                auto channel_id = reader.read_int(); CHECKOK();
                                reader.skip_object(); CHECKOK();

                                // pass channel id to neovim so we can send it to rpcrequest
                                start_request_message("nvim_set_var", 2);
                                writer.write_string("channel_id");
                                writer.write_int(channel_id);
                                end_message();
                            }
                            break;
#if 0
                        case NVIM_REQ_REPLACE_TERMCODES:
                            {
                                if (error) {
                                    reader.skip_object(); CHECKOK();
                                    break;
                                }

                                ASSERT(editor != NULL);
                                auto s = reader.read_string(); CHECKOK();

                                {
                                    auto msgid = start_request_message("nvim_feedkeys", 3);

                                    auto newreq = save_request(NVIM_REQ_FEEDKEYS, msgid, editor->id);
                                    newreq->feedkeys.key = req->replace_termcodes.key;

                                    writer.write_string(s);
                                    writer.write_string("n");
                                    writer.write_bool(true);
                                    end_message();
                                }
                            }
                            break;
                        case NVIM_REQ_FEEDKEYS:
                            {
                                ASSERT(editor != NULL);
                                editor->process_key_after_nvim_finishes(req->feedkeys.key);
                                reader.skip_object(); CHECKOK();
                            }
                            break;
#endif
                        case NVIM_REQ_SET_CURSOR:
                            {
                                ASSERT(editor != NULL);
                                if (error) {
                                    // print("error");
                                }
                                reader.skip_object(); CHECKOK();
                            }
                            break;
                        case NVIM_REQ_RESIZE:
                            {
                                ASSERT(editor != NULL);
                                editor->nvim_data.is_resizing = false;
                                if (error) {
                                    // print("error");
                                }
                                reader.skip_object(); CHECKOK();
                            }
                            break;
                        case NVIM_REQ_CREATE_BUF:
                            {
                                u32 bufid = 0;
                                if (!error && reader.peek_type() == MP_EXT) {
                                    auto ext = reader.read_ext();
                                    bufid = ext->object_id;
                                } else {
                                    reader.skip_object(); CHECKOK();
                                }

                                if (bufid == 0) {
                                    // TODO: mark the editor as having an error
                                    break;
                                }

                                ASSERT(editor != NULL);

                                editor->nvim_data.buf_id = bufid;
                                // editor->nvim_data.status = ENS_WIN_PENDING;

                                {
                                    auto msgid = start_request_message("nvim_buf_attach", 3);
                                    save_request(NVIM_REQ_BUF_ATTACH, msgid, editor->id);

                                    writer.write_int(bufid);
                                    writer.write_bool(false);
                                    writer.write_map(0);
                                    end_message();
                                }

                                {
                                    start_request_message("nvim_buf_set_lines", 5);

                                    writer.write_int(bufid);
                                    writer.write_int(0);
                                    writer.write_int(-1);
                                    writer.write_bool(false);

                                    if (editor->nvim_data.file_handle != NULL) {
                                        Buffer tmpbuf;
                                        tmpbuf.init();
                                        defer { tmpbuf.cleanup(); };

                                        tmpbuf.read(editor->nvim_data.file_handle);
                                        fclose(editor->nvim_data.file_handle);
                                        editor->nvim_data.file_handle = NULL;

                                        writer.write_array(tmpbuf.lines.len);
                                        For (tmpbuf.lines) {
                                            writer.write1(MP_OP_STRING);
                                            writer.write4(it.len);
                                            For (it)
                                                writer.write1(it);
                                        }
                                    }

                                    end_message();
                                }

                                {
                                    typedef fn<void()> write_value_func;
                                    auto set_option = [&](ccstr key, write_value_func f) {
                                        start_request_message("nvim_buf_set_option", 3);
                                        writer.write_int(bufid);
                                        writer.write_string(key);
                                        f();
                                        end_message();
                                    };

                                    set_option("shiftwidth", [&]() { writer.write_int(2); });
                                    set_option("tabstop", [&]() { writer.write_int(2); });
                                    set_option("expandtab", [&]() { writer.write_bool(false); });
                                    set_option("wrap", [&]() { writer.write_bool(false); });
                                    set_option("autoindent", [&]() { writer.write_bool(true); });
                                    set_option("filetype", [&]() { writer.write_string("go"); });
                                    // how to do eq. of `filetype indent plugin on'?
                                }

                                {
                                    auto msgid = start_request_message("nvim_open_win", 3);
                                    save_request(NVIM_REQ_OPEN_WIN, msgid, editor->id);

                                    writer.write_int(bufid);
                                    writer.write_bool(false);
                                    writer.write_map(5);
                                    writer.write_string("relative"); writer.write_string("win");
                                    writer.write_string("row"); writer.write_int(0);
                                    writer.write_string("col"); writer.write_int(0);
                                    writer.write_string("width"); writer.write_int(200);
                                    writer.write_string("height"); writer.write_int(100);
                                    end_message();
                                }
                            }
                            break;
                        case NVIM_REQ_SET_CURRENT_WIN:
                            if (error) {
                                // TODO
                                reader.skip_object();
                                break;
                            }
                            reader.read_nil();
                            if (editor->id == world.nvim_data.waiting_focus_window) {
                                world.nvim_data.waiting_focus_window = 0;
                            }
                            break;
                        case NVIM_REQ_OPEN_WIN:
                            {
                                auto run = [&]() -> bool {
                                    if (error) return false;
                                    if (reader.peek_type() != MP_EXT) return false;

                                    auto ext = reader.read_ext();
                                    if (!reader.ok) return false;

                                    editor->nvim_data.win_id = ext->object_id;
                                    if (world.nvim_data.waiting_focus_window == editor->id)
                                        set_current_window(editor);
                                    return true;
                                };

                                if (!run()) {
                                    reader.skip_object();
                                    // TODO: mark error
                                }
                            }
                            break;
                        case NVIM_REQ_BUF_ATTACH:
                            {
                                reader.skip_object();
                                if (error) {
                                    // TODO: mark error
                                    break;
                                }
                                editor->nvim_data.is_buf_attached = true;
                            }
                            break;
                        case NVIM_REQ_UI_ATTACH:
                            {
                                reader.skip_object();
                                if (error) {
                                    // TODO: mark error
                                    break;
                                }
                                world.nvim_data.is_ui_attached = true;
                            }
                            break;
                        default:
                            unhandled = true;
                            break;
                    }

                    if (unhandled) {
                        reader.skip_object();
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

    request_id = 0;

    send_lock.init();
    print_lock.init();
    requests_lock.init();

    requests.init(LIST_POOL, 32);
}

void Nvim::start_running() {
    nvim_proc.init();
    nvim_proc.use_stdin = true;
    nvim_proc.dir = "c:/users/brandon/ide"; // TODO

    // TODO: get full path of init.vim
    nvim_proc.run("nvim -u ./init.vim -i NONE -N --embed --headless");

    reader.read_mode = MP_READ_PROC;
    reader.read_proc.proc = &nvim_proc;
    reader.offset = 0;
    writer.proc = &nvim_proc;

    auto nvim_event_loop = [](void *param) {
        SCOPED_MEM(&world.nvim_loop_mem);
        ((Nvim*)param)->run_event_loop();
    };

    event_loop_thread = create_thread(nvim_event_loop, this);
    if (event_loop_thread == NULL) return;
}

void Nvim::cleanup() {
    send_lock.cleanup();
    print_lock.cleanup();
    requests_lock.cleanup();

    if (event_loop_thread != NULL) {
        kill_thread(event_loop_thread);
        close_thread_handle(event_loop_thread);
    }

    nvim_proc.cleanup();
    requests.cleanup();
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
