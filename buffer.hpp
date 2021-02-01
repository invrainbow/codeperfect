#pragma once

#include "common.hpp"
#include "list.hpp"
#include "mem.hpp"

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
};

void uchar_to_cstr(uchar c, cstr out, s32* pn);

struct Buffer {
    Pool *mem;
    List<Line> lines;
    bool initialized;
    bool dirty;

    void init(Pool *_mem);
    void cleanup();
    void read(FILE* f);
    void write(FILE* f);
    void delete_lines(u32 y1, u32 y2);
    void clear();
    void insert_line(u32 y, uchar* text, s32 len);
    void append_line(uchar* text, s32 len);
    uchar* alloc_temp_array(s32 size);
    void free_temp_array(uchar* buf, s32 size);
    void insert(cur2 start, uchar* text, s32 len);
    void remove(cur2 start, cur2 end);
    Buffer_It iter(cur2 c);
    cur2 inc_cur(cur2 c);
    cur2 dec_cur(cur2 c);
    i32 cur_to_offset(cur2 c);
    cur2 offset_to_cur(i32 off);
};
