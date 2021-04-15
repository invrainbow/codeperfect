#pragma once

// NOTE: convert to unsigned before doing bit twiddling

#include "list.hpp"
#include "os.hpp"
#include "editor.hpp"

enum NvimExtType {
    NVIM_EXT_BUFFER,
    NVIM_EXT_WINDOW,
    NVIM_EXT_TABPAGE,
};

enum MpType {
    MP_UNKNOWN = -1,
    MP_BOOL,
    MP_INT,
    MP_DOUBLE,
    MP_STRING,
    MP_NIL,
    MP_ARRAY,
    MP_MAP,
    MP_EXT,
};

ccstr mptype_str(MpType type);

enum MpOpcode {
    MP_OP_NIL = 0xc0,
    MP_OP_TRUE = 0xc3,
    MP_OP_FALSE = 0xc2,
    MP_OP_INT = 0xd3,
    MP_OP_DOUBLE = 0xcb,
    MP_OP_STRING = 0xdb,
    MP_OP_ARRAY = 0xdd,
    MP_OP_MAP = 0xdf,
};

struct Mp_Writer {
    Process* proc;

    // "private" interface

    void flush() { proc->flush(); };
    void write1(char ch) { proc->write1(ch); }

    void write4(u32 n) {
        for (i32 i = 3; i >= 0; i--)
            write1((n >> (8 * i)) & 0xff);
    }

    void write_raw_array(u32 len, char opcode) {
        write1(opcode);
        write4(len);
    }

    // "public" interface

    void write_nil() { write1(MP_OP_NIL); }
    void write_bool(bool b) { write1(b ? MP_OP_TRUE : MP_OP_FALSE); }
    void write_int(int num) { write_i64((i64)num); }

    void write_i64(i64 num) {
        write1(MP_OP_INT);
        for (i32 i = 7; i >= 0; i--)
            write1((num >> (8 * i)) & 0xff);
    }

    void write_double(double num) {
        write1(MP_OP_DOUBLE);
        auto p = (char*)(&num);
        for (u32 i = 0; i < 8; i++)
            write1(p[i]);
    }

    void write_string(ccstr s) {
        write_string(s, strlen(s));
    }

    void write_string(ccstr s, s32 len) {
        write1(MP_OP_STRING);
        write4(len);
        for (u32 i = 0; i < len; i++)
            write1(s[i]);
    }

    void write_array(u32 len) { write_raw_array(len, MP_OP_ARRAY); };
    void write_map(u32 len) { write_raw_array(len, MP_OP_MAP); }
};

struct Ext_Info {
    NvimExtType type;
    // for now all the extension types just have an int
    u32 object_id;
};

struct Mp_Reader {
    u64 offset;
    Process* proc;
    bool ok; // success status of last read

    /*
    Ok, so I suck at C and the whole sign/unsigned thing. Basically, the only
    time there's a problem is when you cast from small -> big (e.g. char -> int)
    and the sign gets extended, e.g.:

        char bytes[2] = { 0x03, 0xe8 };

        // read i16 from bytes as big endian
        i16 num = (bytes[0] << 8) | bytes[1];

    This produces some weird negative number, instead of the expected 0x03e8,
    because 0xe8 = -24 when signed, and gets sign-extended to 0xffe8 when converted to
    i16. The solution is to use `unsigned char bytes`.

    Otherwise, regardless of type, the bits in memory are the same, and you can
    always just cast with no problem.
    */

    u8 _read1() {
        u8 ret = 0;
        ok = proc->read1((char*)&ret);
        return ret;
    }

    u8 read1() {
        auto ret = _read1();
        if (ok) offset++;
        return ret;
    }

    i16 read2() {
        do {
            u8 a = read1(); if (!ok) break;
            u8 b = read1(); if (!ok) break;

            ok = true;
            return (a << 8) | b;
        } while (0);

        ok = false;
        return 0;
    }

    i32 read4() {
        do {
            u8 a = read1(); if (!ok) break;
            u8 b = read1(); if (!ok) break;
            u8 c = read1(); if (!ok) break;
            u8 d = read1(); if (!ok) break;

            ok = true;
            return (a << 24) | (b << 16) | (c << 8) | d;
        } while (0);

        ok = false;
        return 0;
    }

    i64 read8() {
        do {
            i64 a = (i64)read1(); if (!ok) break;
            i64 b = (i64)read1(); if (!ok) break;
            i64 c = (i64)read1(); if (!ok) break;
            i64 d = (i64)read1(); if (!ok) break;
            i64 e = (i64)read1(); if (!ok) break;
            i64 f = (i64)read1(); if (!ok) break;
            i64 g = (i64)read1(); if (!ok) break;
            i64 h = (i64)read1(); if (!ok) break;

            ok = true;
            return (a << 56) | (b << 48) | (c << 40) | (d << 32) | (e << 24) | (f << 16) | (g << 8) | h;
        } while (0);

        ok = false;
        return 0;
    }

    u8 peek() {
        u8 ch = 0;
        ok = proc->peek((char*)&ch);
        return ch;
    }

    MpType peek_type() {
        u8 b = peek();
        if (!ok)
            return MP_UNKNOWN;

        switch (b) {
            case 0xc0: return MP_NIL;
            case 0xc2: return MP_BOOL;
            case 0xc3: return MP_BOOL;
            case 0xcc: return MP_INT;
            case 0xcd: return MP_INT;
            case 0xce: return MP_INT;
            case 0xcf: return MP_INT;
            case 0xd0: return MP_INT;
            case 0xd1: return MP_INT;
            case 0xd2: return MP_INT;
            case 0xd3: return MP_INT;
            case 0xca: return MP_DOUBLE;
            case 0xcb: return MP_DOUBLE;
            case 0xd9: return MP_STRING;
            case 0xda: return MP_STRING;
            case 0xdb: return MP_STRING;
            case 0xdc: return MP_ARRAY;
            case 0xdd: return MP_ARRAY;
            case 0xde: return MP_MAP;
            case 0xdf: return MP_MAP;
            case 0xc7: return MP_EXT;
            case 0xc8: return MP_EXT;
            case 0xc9: return MP_EXT;
            case 0xd4: return MP_EXT;
            case 0xd5: return MP_EXT;
            case 0xd6: return MP_EXT;
            case 0xd7: return MP_EXT;
            case 0xd8: return MP_EXT;
        }

        if (b >> 4 == 0b1001) return MP_ARRAY;
        if (b >> 4 == 0b1000) return MP_MAP;
        if (b >> 5 == 0b101) return MP_STRING;
        if (b >> 5 == 0b111) return MP_INT;
        if (b >> 7 == 0b0) return MP_INT;

        return MP_UNKNOWN;
    }

    s32 read_array() {
        auto b = read1();
        if (ok) {
            if (b >> 4 == 0b1001) return b & 0b00001111;
            if (b == 0xdc) return read2();
            if (b == 0xdd) return read4();
            ok = false;
        }
        return 0;
    }

    s32 read_map() {
        auto b = read1();
        if (ok) {
            if (b >> 4 == 0b1000) return b & 0b00001111;
            if (b == 0xde) return read2();
            if (b == 0xdf) return read4();
            ok = false;
        }
        return 0;
    }

    i64 read_int() {
        u8 b = read1();
        if (ok) {
            if (b >> 7 == 0b0) return b;
            if (b >> 5 == 0b111) return (i8)b;
            if (b == 0xcc || b == 0xd0) return b == 0xcc ? (u8)read1() : (i8)read1();
            if (b == 0xcd || b == 0xd1) return b == 0xcd ? (u16)read2() : (i16)read2();
            if (b == 0xce || b == 0xd2) return b == 0xce ? (u32)read4() : (i32)read4();

            // we can't return a u64, so have caller cast
            if (b == 0xd3 || b == 0xcf) return (i64)read8();
            ok = false;
        }
        return 0;
    }

    void skip_object();
    ccstr read_string();
    Ext_Info* read_ext();

    double read_double() {
        auto b = read1();
        if (!ok) return 0;

        char buf[8];
        u32 bytes = (b == 0xca ? 4 : 8);

        for (u32 i = 0; i < bytes; i++) {
            buf[i] = read1();
            if (!ok) return 0;
        }

        ok = true;
        return (bytes == 4 ? (double)(*(float*)buf) : *(double*)buf);
    }

    bool read_bool() {
        auto b = read1();
        if (ok) {
            if (b == 0xc2) return false;
            if (b == 0xc3) return true;
            ok = false;
        }
        return false;
    }

    void read_nil() {
        auto b = read1();
        ok = ok && (b == 0xc0);
    }
};

enum MprpcMessageType {
    MPRPC_REQUEST = 0,
    MPRPC_RESPONSE = 1,
    MPRPC_NOTIFICATION = 2,
};

enum Nvim_Request_Type {
    NVIM_REQ_NONE = 0,
    NVIM_REQ_GET_API_INFO,
    NVIM_REQ_CREATE_BUF,
    NVIM_REQ_OPEN_WIN,
    NVIM_REQ_BUF_ATTACH,
    NVIM_REQ_UI_ATTACH,
    NVIM_REQ_SET_CURRENT_WIN,
    NVIM_REQ_RESIZE,
    NVIM_REQ_AUTOCOMPLETE_SETBUF,
    NVIM_REQ_POST_INSERT_GETCHANGEDTICK,
    NVIM_REQ_POST_INSERT_MOVE_CURSOR,
    NVIM_REQ_FILEOPEN_CLEAR_UNDO,
    NVIM_REQ_POST_SAVE_GETCHANGEDTICK,
    NVIM_REQ_POST_SAVE_SETLINES,
};

struct Nvim_Request {
    u32 msgid;
    Nvim_Request_Type type;
    u32 editor_id;

    union {
        struct {
            u32 to_width;
            u32 to_height;
        } resize;

        struct {
            cur2 target_cursor;
        } autocomplete_setbuf;

        struct {
            cur2 cur;
        } post_save_setlines;

        struct {
            cur2 cur;
        } post_save_getchangedtick;
    };
};

enum Nvim_Notification_Type {
    NVIM_NOTIF_BUF_LINES,
    NVIM_NOTIF_MODE_CHANGE,
    NVIM_NOTIF_WIN_VIEWPORT,
    NVIM_NOTIF_WIN_POS,
};

struct Nvim_Message {
    MprpcMessageType type;
    union {
        struct {
            Nvim_Request *original_request;
            union {
                int changedtick;
                int channel_id;
                Ext_Info buf;
                Ext_Info win;
            };
        } response;

        struct {
            Nvim_Notification_Type type;
            union {
                struct {
                    Ext_Info buf;
                    int changedtick;
                    int firstline;
                    int lastline;
                    List<uchar*> *lines;
                    List<s32> *line_lengths;
                } buf_lines;

                struct {
                    ccstr mode_name;
                    int mode_index;
                } mode_change;

                struct {
                    int grid;
                    Ext_Info window;
                    int topline;
                    int botline;
                    int curline;
                    int curcol;
                } win_viewport;

                struct {
                    int grid;
                    Ext_Info window;
                } win_pos;
            };
        } notification;
    };
};

struct Grid_Window_Pair {
    u32 grid;
    u32 win;
};

enum Vi_Mode {
    VI_NONE,
    VI_NORMAL,
    VI_VISUAL,
    VI_INSERT,
    VI_REPLACE,
    VI_UNKNOWN,
};

struct Nvim {
    // memory
    Pool mem;
    Pool loop_mem;
    Pool messages_mem;

    // orchestration
    Process nvim_proc;
    Mp_Reader reader;
    Mp_Writer writer;
    Lock send_lock;
    Lock requests_lock;
    u32 request_id;
    Thread_Handle event_loop_thread;
    List<Nvim_Request> requests;
    List<Nvim_Message> message_queue;
    Lock messages_lock;

    // state
    List<Grid_Window_Pair> grid_to_window;
    bool is_ui_attached;
    u32 waiting_focus_window;
    Vi_Mode mode;

    void init();
    void start_running();
    void cleanup();

    void write_request_header(u32 msgid, ccstr method, u32 params_length);
    void write_response_header(u32 msgid);
    void write_notification_header(ccstr method, u32 params_length);

    Nvim_Request* save_request(Nvim_Request_Type type, u32 msgid, u32 editor_id) {
        auto req = requests.append();
        req->type = type;
        req->msgid = msgid;
        req->editor_id = editor_id;
        return req;
    }

    // TODO: document the request lifecycle
    u32 start_request_message(ccstr method, u32 params_length) {
        send_lock.enter();

        auto msgid = request_id++;
        write_request_header(msgid, method, params_length);
        return msgid;
    }

    void start_response_message(u32 msgid) {
        send_lock.enter();
        write_response_header(msgid);
    }

    void end_message() {
        writer.flush();
        send_lock.leave();
    }

    Nvim_Request *find_request_by_msgid(u32 msgid) {
        return requests.find([&](Nvim_Request* it) -> bool {
            return it->msgid == msgid;
        });
    }

    void run_event_loop();
    void run_test_event_loop();

    void set_current_window(Editor* editor) {
        auto msgid = start_request_message("nvim_set_current_win", 1);
        save_request(NVIM_REQ_SET_CURRENT_WIN, msgid, editor->id);
        writer.write_int(editor->nvim_data.win_id);
        end_message();
    }

    bool resize_editor(Editor* editor);
    void handle_editor_on_ready(Editor *editor);
    void handle_message_from_main_thread(Nvim_Message *event);
    void assoc_grid_with_window(u32 grid, u32 win);
    Editor* find_editor_by_grid(u32 grid);
};

