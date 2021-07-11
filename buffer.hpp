#pragma once

#include "common.hpp"
#include "list.hpp"
#include "mem.hpp"
#include "os.hpp"
#include "tree_sitter_crap.hpp"

struct Buffer;

struct Buffer_It {
    Buffer* buf;
    union {
        cur2 pos;
        struct {
            i32 x;
            i32 y;
        };
    };

    bool has_fake_end;
    cur2 fake_end;
    int fake_end_offset;
    bool append_chars_to_end;
    ccstr chars_to_append_to_end;

    bool eof();
    uchar peek();
    uchar prev();
    uchar next();
    bool bof() { return x == 0 && y == 0; }
    bool eol() { return peek() == '\n'; }
    uchar get(cur2 _pos);
};

typedef List<uchar> Line;

struct Cstr_To_Ustr {
    u8 buf[3];
    s32 buflen;
    s32 len;

    void init();
    s32 get_uchar_size(u8 first_char);
    void count(u8 ch);
    uchar feed(u8 ch, bool* found);

    // TODO: should write this, but i'm lazy and we need to ship
    // void read(u8 *chars, int len, uchar *buf, int buflen);
};

s32 uchar_to_cstr(uchar c, cstr out);

typedef fn<bool(char*)> Buffer_Read_Func;

struct Buffer {
    Pool *mem;

    List<Line> lines;
    List<u32> bytecounts;

    bool initialized;
    bool dirty;
    bool use_tree;
    TSTree *tree;
    TSTreeCursor cursor;
    bool tree_dirty;

    TSParser *parser;
    char tsinput_buffer[128];

    void copy_from(Buffer *other);
    void init(Pool *_mem, bool use_tree);
    void cleanup();
    void read(Buffer_Read_Func f);
    void read(File* f);
    void write(File* f);
    void clear();
    uchar* alloc_temp_array(s32 size);
    void free_temp_array(uchar* buf, s32 size);
    void insert(cur2 start, uchar* text, s32 len);
    void remove(cur2 start, cur2 end);
    void update_tree();

    void internal_append_line(uchar* text, s32 len);
    void internal_delete_lines(u32 y1, u32 y2);
    void internal_insert_line(u32 y, uchar* text, s32 len);

    Buffer_It iter(cur2 c);
    cur2 inc_cur(cur2 c);
    cur2 dec_cur(cur2 c);
    i32 cur_to_offset(cur2 c);
    cur2 offset_to_cur(i32 off);

    // this is so stupid lmao
    u32 idx_byte_to_gr(int y, int off);
    u32 idx_gr_to_cp(int y, int off);
    u32 idx_cp_to_byte(int y, int off);
    u32 idx_gr_to_byte(int y, int off) { return idx_cp_to_byte(y, idx_gr_to_cp(y, off)); }
    u32 idx_cp_to_gr(int y, int off) { return idx_byte_to_gr(y, idx_cp_to_byte(y, off)); }
    u32 idx_byte_to_cp(int y, int off);

    ccstr get_text(cur2 start, cur2 end);
};

s32 uchar_size(uchar c);
