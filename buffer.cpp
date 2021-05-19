#include "buffer.hpp"
#include "common.hpp"
#include "world.hpp"

void uchar_to_cstr(uchar c, cstr out, s32* pn) {
    u32 k = 0;
    if (c <= 0x7f) {
        out[k++] = (u8)c;
    } else if (c <= 0x7ff) {
        out[k++] = (u8)(0b11000000 | (c >> 6));
        out[k++] = (u8)(0b10000000 | (c & 0b111111));
    } else if (c <= 0xffff) {
        out[k++] = (u8)(0b11100000 | (c >> 12));
        out[k++] = (u8)(0b10000000 | ((c & 0b111111000000) >> 6));
        out[k++] = (u8)(0b10000000 | (c & 0b111111));
    } else {
        out[k++] = (u8)(0b11110000 | (c >> 18));
        out[k++] = (u8)(0b10000000 | ((c % 0b111111000000000000) >> 12));
        out[k++] = (u8)(0b10000000 | ((c % 0b111111000000) >> 6));
        out[k++] = (u8)(0b10000000 | (c & 0b111111));
    }
    *pn = k;
}

s32 uchar_size(uchar c) {
    if (c <= 0x7f) return 1;
    if (c <= 0x7ff) return 2;
    if (c <= 0xffff) return 3;
    return 4;
}
uchar Buffer_It::get(cur2 _pos) {
    auto old = pos;
    pos = _pos;
    defer { pos = old; };
    return peek();
}

bool Buffer_It::eof() {
    if (has_fake_end) {
        auto new_end = fake_end;
        if (append_chars_to_end)
            new_end.x += strlen(chars_to_append_to_end);
        return pos >= new_end;
    }

    if (buf->lines.len == 0) return true;
    if (y == buf->lines.len - 1 && x == buf->lines[y].len) return true;
    if (y > buf->lines.len - 1) return true;
    return false;
}

uchar Buffer_It::peek() {
    if (has_fake_end && append_chars_to_end && pos >= fake_end)
        return chars_to_append_to_end[pos.x - fake_end.x];
    if (x == buf->lines[y].len)
        return '\n';
    return buf->lines[y][x];
}

uchar Buffer_It::prev() {
    if (bof())
        return 0;

    if (x-- == 0) {
        y--;
        x = buf->lines[y].len;
    }
    return peek();
}

uchar Buffer_It::next() {
    if (eof())
        return 0;

    auto ret = peek();
    if (++x > buf->lines[y].len) {
        y++;
        x = 0;
    }
    return ret;
}

void Cstr_To_Ustr::init() {
    len = 0;
    buflen = 0;
}

s32 Cstr_To_Ustr::get_uchar_size(u8 first_char) {
    if (first_char < 0b10000000)
        return 1;
    if (first_char < 0b11100000)
        return 2;
    if (first_char < 0b11110000)
        return 3;
    return 4;
}

void Cstr_To_Ustr::count(u8 ch) {
    if (buflen > 0) {
        if (buflen + 1 == get_uchar_size(buf[0])) {
            len++;
            buflen = 0;
        } else {
            buf[buflen++] = ch;
        }
    } else if (ch < 0b10000000) {
        len++;
    } else {
        buf[buflen++] = ch;
    }
}

uchar Cstr_To_Ustr::feed(u8 ch, bool* found) {
    if (buflen > 0) {
        auto needed = get_uchar_size(buf[0]);
        if (buflen + 1 == needed) {
            buflen = 0;
            auto b1 = buf[0];
            *found = true;
            switch (needed) {
                case 2:
                    {
                        b1 &= 0b11111;
                        auto b2 = ch & 0b111111;
                        return (b1 << 6) | b2;
                    }
                case 3:
                    {
                        b1 &= 0b1111;
                        auto b2 = buf[1] & 0b111111;
                        auto b3 = ch & 0b111111;
                        return (b1 << 12) | (b2 << 6) | b3;
                    }
                case 4:
                    {
                        b1 &= 0b111;
                        auto b2 = buf[1] & 0b111111;
                        auto b3 = buf[2] & 0b111111;
                        auto b4 = ch & 0b111111;
                        return (b1 << 18) | (b2 << 12) | (b3 << 6) | b4;
                    }
            }
        } else {
            buf[buflen++] = ch;
        }
    } else if (ch < 0b10000000) {
        *found = true;
        return ch;
    } else {
        buf[buflen++] = ch;
    }

    *found = false;
    return 0;
}

void Buffer::init(Pool *_mem) {
    mem = _mem;
    {
        SCOPED_MEM(mem);
        lines.init(LIST_POOL, 128);
        bytecounts.init(LIST_POOL, 128);
    }
    initialized = true;
    dirty = false;
}

void Buffer::cleanup() {
    if (initialized) {
        clear();
        initialized = false;
    }
}

void Buffer::copy_from(Buffer *other) {
    Buffer_It it = {0};
    it.buf = other;

    char tmp[4];
    s32 count = 0;
    u32 pos = 0;

    read([&](char *out) -> bool {
        if (pos >= count) {
            if (it.eof()) return false;
            uchar_to_cstr(it.next(), tmp, &count);
            pos = 0;
        }
        *out = tmp[pos++];
        return true;
    });
}

void Buffer::read(Buffer_Read_Func f) {
    // Expects buf to be empty.

    char ch;
    Cstr_To_Ustr conv;
    bool found;

    clear();

    Line *line = NULL;
    u32 *bc = NULL;

    auto insert_new_line = [&]() {
        line = lines.append();
        bc = bytecounts.append();

        if (line == NULL) panic("unable to insert new line");

        line->init(LIST_CHUNK, CHUNK0);
        *bc = 0;
    };

    insert_new_line();
    conv.init();
    while (f(&ch)) {
        uchar uch = conv.feed(ch, &found);
        (*bc)++;

        if (found) {
            if (uch == '\n') // TODO: handle \r
                insert_new_line();
            else
                line->append(uch);
        }
    }

    (*bytecounts.last())++;
    dirty = false;
}

void Buffer::read(FILE* f) {
    read([&](char* out) -> bool {
        return !feof(f) && fread(out, 1, 1, f) == 1;
    });
}

void Buffer::write(FILE* f) {
    auto write_char = [&](uchar ch) {
        char buf[4];
        s32 count;

        uchar_to_cstr(ch, buf, &count);
        fwrite(buf, 1, count, f);
    };

    bool first = true;
    For (lines) {
        if (first)
            first = false;
        else
            write_char('\n');
        for (auto ch : it)
            write_char(ch);
    }
}

void Buffer::delete_lines(u32 y1, u32 y2) {
    if (y2 > lines.len) y2 = lines.len;

    for (u32 y = y1; y < y2; y++)
        lines[y].cleanup();

    if (y2 < lines.len) {
        memmove(&lines[y1], &lines[y2], sizeof(Line) * (lines.len - y2));
        memmove(&bytecounts[y1], &bytecounts[y2], sizeof(u32) * (bytecounts.len - y2));
    }

    lines.len -= (y2 - y1);
    bytecounts.len -= (y2 - y1);
    dirty = true;
}

void Buffer::clear() {
    delete_lines(0, lines.len);
    dirty = true;
}

s32 get_bytecount(Line *line) {
    s32 bc = 0;
    For (*line) bc += uchar_size(it);
    return bc + 1;
}

void Buffer::insert_line(u32 y, uchar* text, s32 len) {
    lines.ensure_cap(lines.len + 1);
    bytecounts.ensure_cap(bytecounts.len + 1);

    if (y > lines.len) {
        y = lines.len;
    } else if (y < lines.len) {
        memmove(&lines[y + 1], &lines[y], sizeof(Line) * (lines.len - y));
        memmove(&bytecounts[y + 1], &bytecounts[y], sizeof(u32) * (bytecounts.len - y));
    }

    lines[y].init(LIST_CHUNK, len);
    lines[y].len = len;
    memcpy(lines[y].items, text, sizeof(uchar) * len);

    bytecounts[y] = get_bytecount(&lines[y]);

    lines.len++;
    bytecounts.len++;
    dirty = true;
}

void Buffer::append_line(uchar* text, s32 len) {
    insert_line(lines.len, text, len);
    dirty = true;
}

void Buffer::insert(cur2 start, uchar* text, s32 len) {
    i32 x = start.x, y = start.y;

    bool has_newline = false;
    for (u32 i = 0; i < len; i++) {
        if (text[i] == '\n') {
            has_newline = true;
            break;
        }
    }

    if (y == lines.len)
        append_line(NULL, 0);

    auto line = &lines[y];
    s32 total_len = line->len + len;

    if (has_newline) {
        uchar* buf = alloc_temp_array(total_len);
        defer { free_temp_array(buf, total_len); };

        if (x > 0)
            memcpy(buf, line->items, sizeof(uchar) * x);
        memcpy(&buf[x], text, sizeof(uchar) * len);
        if (line->len > x)
            memcpy(&buf[x+len], &line->items[x], sizeof(uchar) * (line->len - x));

        delete_lines(y, y + 1);

        u32 last = 0;
        for (u32 i = 0; i <= total_len; i++) {
            if (i == total_len || buf[i] == '\n') {
                insert_line(y++, buf + last, i - last);
                last = i + 1;
            }
        }
    } else {
        line->ensure_cap(total_len);
        memmove(&line->items[x + len], &line->items[x], sizeof(uchar) * (line->len - x));
        memcpy(&line->items[x], text, sizeof(uchar) * len);
        line->len = total_len;

        u32 bc = 0;
        for (u32 i = 0; i < len; i++)
            bc += uchar_size(text[i]);
        bytecounts[y] += bc;
    }

    dirty = true;
}

void Buffer::remove(cur2 start, cur2 end) {
    i32 x1 = start.x, y1 = start.y;
    i32 x2 = end.x, y2 = end.y;

    if (y1 == y2) {
        // TODO: If we can shrink the line, should we?
        if (lines[y1].len > x2)
            memcpy(lines[y1].items + x1, lines[y1].items + x2, sizeof(uchar) * (lines[y1].len - x2));
        lines[y1].len -= (x2 - x1);
    } else {
        s32 new_len = x1 + (y2 < lines.len ? lines[y2].len : 0) - x2;
        lines[y1].ensure_cap(new_len);
        lines[y1].len = new_len;
        if (y2 < lines.len)
            memcpy(lines[y1].items + x1, lines[y2].items + x2, sizeof(uchar) * (lines[y2].len - x2));
        delete_lines(y1 + 1, min(lines.len, y2 + 1));
    }
    bytecounts[y1] = get_bytecount(&lines[y1]);
    dirty = true;
}

Buffer_It Buffer::iter(cur2 c) {
    Buffer_It it = {0};
    it.buf = this;
    it.pos = c;
    return it;
}

cur2 Buffer::inc_cur(cur2 c) {
    auto it = iter(c);
    it.next();
    return it.pos;
}

cur2 Buffer::dec_cur(cur2 c) {
    auto it = iter(c);
    it.prev();
    return it.pos;
}

uchar* Buffer::alloc_temp_array(s32 size) {
    if (size > 1024)
        return (uchar*)our_malloc(sizeof(uchar) * size);
    return alloc_array(uchar, size);
}

void Buffer::free_temp_array(uchar* buf, s32 size) {
    if (size > 1024)
        our_free(buf);
}

i32 Buffer::cur_to_offset(cur2 c) {
    i32 ret = 0;
    for (u32 y = 0; y < c.y; y++)
        ret += bytecounts[y];

    for (u32 x = 0; x < c.x; x++)
        ret += uchar_size(lines[c.y][x]);

    return ret;
}

cur2 Buffer::offset_to_cur(i32 off) {
    cur2 ret;
    ret.x = -1;
    ret.y = -1;

    for (u32 y = 0; y < bytecounts.len; y++) {
        auto& it = bytecounts[y];
        if (off < it) {
            ret.y = y;
            for (u32 x = 0; x < lines[y].len; x++) {
                auto size = uchar_size(lines[y][x]);
                if (off < size) {
                    ret.x = x;
                    break;
                }
                off -= size;
            }
            if (ret.x == -1) {
                assert(off == 0);
                ret.x = lines[y].len;
            }
            break;
        }
        off -= it;
    }

    if (ret.x == -1 || ret.y == -1) {
        assert(ret.x == -1 && ret.y == -1);
        assert(off == 0);
        ret.y = lines.len-1;
        ret.x = lines[ret.y].len;
    }

    return ret;
}
