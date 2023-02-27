#include "buffer.hpp"
#include "common.hpp"
#include "world.hpp"
#include "unicode.hpp"
#include "diff.hpp"
#include "defer.hpp"

s32 uchar_to_cstr(uchar c, cstr out) {
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
        out[k++] = (u8)(0b10000000 | ((c & 0b111111000000000000) >> 12));
        out[k++] = (u8)(0b10000000 | ((c & 0b111111000000) >> 6));
        out[k++] = (u8)(0b10000000 | (c & 0b111111));
    }
    return k;
}

char* uchar_to_cstr(uchar c) {
    auto ret = new_array(char, 5);
    auto len = uchar_to_cstr(c, ret);
    ret[len] = '\0';
    return ret;
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

    if (!buf->lines.len) return true;
    if (y == buf->lines.len - 1 && x >= buf->lines[y].len) return true;
    if (y > buf->lines.len - 1) return true;
    return false;
}

uchar Buffer_It::peek() {
    if (has_fake_end && append_chars_to_end && pos >= fake_end)
        return chars_to_append_to_end[pos.x - fake_end.x];

    if (!buf->lines.len)
        cp_panic(cp_sprintf("cannot peek on empty buffer, x = %d, y = %d", x, y));

    if (x > buf->lines[y].len)
        cp_panic(cp_sprintf("cannot access x = %d on line y = %d, length of line is %d", x, y, buf->lines[y].len));

    if (x == buf->lines[y].len)
        return '\n';
    return buf->lines[y][x];
}

uchar Buffer_It::prev() {
    if (bof()) return 0;

    if (x-- == 0) {
        y--;
        x = buf->lines[y].len;
    }
    return peek();
}

uchar Buffer_It::next() {
    if (eof()) return 0;

    auto ret = peek();
    if (has_fake_end && pos >= fake_end) {
        pos.x++;
    } else {
        if (++x > buf->lines[y].len) {
            y++;
            x = 0;
        }
    }
    return ret;
}

Grapheme Gr_Iter::read_grapheme_from_current_pos(cur2 *end) {
    if (eof()) return NULL;

    cur2 start = it.pos;

    Grapheme_Clusterer gc;
    gc.init();
    gc.feed(it.peek());

    auto ret = new_list(uchar);
    do {
        ret->append(it.next());
    } while (!eof() && !gc.feed(it.peek()));

    *end = it.pos;
    it.pos = start;
    return ret;
}

Grapheme Gr_Iter::peek() {
    return read_grapheme_from_current_pos(&saved_next_from_peek);
}

Grapheme Gr_Iter::prev() {
    saved_next_from_peek = NULL_CUR;

    auto current_pos = it.pos;

    // i guess the thing i don't know is... let's say you have a grapheme
    // consisting of codepoints A B C. if you start at C, will it read just C
    // as a grapheme? what if you start at B?

    Grapheme last_gr = NULL;
    while (!it.bof()) {
        auto old = it.pos;
        it.prev();

        cur2 end = NULL_CUR;
        auto gr = read_grapheme_from_current_pos(&end);

        // the grapheme starting at this pos no longer ends at current_pos
        if (!gr || end != current_pos) {
            // if old == current_pos it means that somehow the codepoint prior
            // to current_pos does not result in a grapheme that ends at
            // current_pos. that is very weird, but we need to move backward,
            // or we will probably have an infinite loop in downstream code. so
            // just keep the prev(), and get out.
            if (old != current_pos)
                it.pos = old;
            return last_gr;
        }
        last_gr = gr;
    }
    return last_gr;
}

void Gr_Iter::next() {
    if (saved_next_from_peek == NULL_CUR) {
        peek();
        cp_assert(saved_next_from_peek != NULL_CUR);
    }

    it.pos = saved_next_from_peek;
    saved_next_from_peek = NULL_CUR;
}

void Cstr_To_Ustr::init() {
    len = 0;
    buflen = 0;
}

s32 Cstr_To_Ustr::get_uchar_size(u8 first_char) {
    if (first_char < 0b10000000) return 1;
    if (first_char < 0b11100000) return 2;
    if (first_char < 0b11110000) return 3;
    return 4;
}

void Cstr_To_Ustr::count(u8 ch) {
    if (buflen) {
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

bool Cstr_To_Ustr::feed(u8 ch) {
    if (buflen) {
        auto needed = get_uchar_size(buf[0]);
        if (buflen + 1 == needed) {
            buflen = 0;
            auto b1 = buf[0];
            switch (needed) {
            case 2: {
                b1 &= 0b11111;
                auto b2 = ch & 0b111111;
                uch = (b1 << 6) | b2;
                break;
            }
            case 3: {
                b1 &= 0b1111;
                auto b2 = buf[1] & 0b111111;
                auto b3 = ch & 0b111111;
                uch = (b1 << 12) | (b2 << 6) | b3;
                break;
            }
            case 4: {
                b1 &= 0b111;
                auto b2 = buf[1] & 0b111111;
                auto b3 = buf[2] & 0b111111;
                auto b4 = ch & 0b111111;
                uch = (b1 << 18) | (b2 << 12) | (b3 << 6) | b4;
                break;
            }
            }
            return true;
        } else {
            buf[buflen++] = ch;
        }
    } else if (ch < 0b10000000) {
        uch = ch;
        return true;
    } else {
        buf[buflen++] = ch;
    }

    return false;
}

List<uchar>* cstr_to_ustr(ccstr s) {
    Cstr_To_Ustr conv; conv.init();

    auto ret = new_list(uchar);
    for (int i = 0, len = strlen(s); i < len; i++) {
        if (conv.feed(s[i]))
            ret->append(conv.uch);
    }
    return ret;
}

void Buffer::hist_apply_change(Change *change, bool undo) {
    auto start = change->start;
    cur2 old_end;
    List<uchar> *new_text;

    if (undo) {
        old_end = change->new_end;
        new_text = &change->old_text;
    } else {
        old_end = change->old_end;
        new_text = &change->new_text;
    }

    if (start != old_end) remove(start, old_end, true);

    insert(start, new_text->items, new_text->len, true);
}

cur2 Buffer::hist_undo() {
    if (hist_curr == hist_start) return NULL_CUR;

    hist_curr = hist_dec(hist_curr);

    auto arr = new_list(Change*);
    for (auto it = history[hist_curr]; it; it = it->next)
        arr->append(it);

    auto ret = NULL_CUR;

    for (int i = 0; i < arr->len; i++) {
        auto it = arr->at(arr->len - i - 1);
        hist_apply_change(it, true);

        if (i == arr->len-1) ret = it->start;
    }

    return ret;
}

cur2 Buffer::hist_redo() {
    if (hist_curr == hist_top) return NULL_CUR;

    auto ret = NULL_CUR;

    for (auto it = history[hist_curr]; it; it = it->next){
        hist_apply_change(it, false);
        if (!it->next) ret = it->start;
    }

    hist_curr = hist_inc(hist_curr);
    return ret;
}

ccstr Buffer::get_text(cur2 start, cur2 end) {
    auto ret = new_list(char);
    char tmp[4];

    // make sure start is valid
    if (start.x > lines[start.y].len)
        start.x = lines[start.y].len;

    for (auto it = iter(start); it.pos < end && !it.eof(); it.next())
        for (int i = 0, n = uchar_to_cstr(it.peek(), tmp); i < n; i++)
            ret->append(tmp[i]);

    ret->append('\0');
    return ret->items;
}

bool Buffer::is_valid(cur2 c) {
    if (0 <= c.y && c.y < lines.len)
        if (0 <= c.x && c.x <= lines[c.y].len)
            return true;
    return false;
}

cur2 Buffer::fix_cur(cur2 c) {
    if (c.y < 0) return new_cur2(0, 0);

    if (c.y >= lines.len) {
        int y = lines.len-1;
        return new_cur2(lines[y].len, y);
    }

    if (c.x < 0) c.x = 0;
    if (c.x > lines[c.y].len) c.x = lines[c.y].len;
    return c;
}

void Buffer::init(Pool *_mem, int _lang, bool _use_history) {
    ptr0(this);

    mem = _mem;
    use_history = _use_history;

    {
        SCOPED_MEM(mem);
        lines.init(LIST_POOL, 128);
        bytecounts.init(LIST_POOL, 128);
        edit_buffer_old.init();
        edit_buffer_new.init();
    }

    initialized = true;
    dirty = false;

    if (_lang != LANG_NONE)
        enable_tree(_lang);

    mark_tree.init(this);
}

void Buffer::enable_tree(int _lang) {
    if (lang != LANG_NONE) return; // already enabled

    lang = (int)_lang;
    parser = new_ts_parser((Parse_Lang)lang);

    if (lines.len) update_tree();
}

void Buffer::cleanup() {
    if (!initialized) return;

    clear();
    if (parser) ts_parser_delete(parser);
    if (tree) ts_tree_delete(tree);

    mark_tree.cleanup();

    for (int i = hist_start; i != hist_top; i = hist_inc(i))
        hist_free(i);

    initialized = false;
}

void Buffer::copy_from(Buffer *other) {
    Buffer_It it; ptr0(&it);
    it.buf = other;

    char tmp[4];
    s32 count = 0;
    u32 pos = 0;

    read([&](char *out) -> bool {
        if (pos >= count) {
            if (it.eof()) return false;
            count = uchar_to_cstr(it.next(), tmp);
            pos = 0;
        }
        *out = tmp[pos++];
        return true;
    });
}

bool Buffer::read(Buffer_Read_Func f, bool reread) {
    cp_assert(!editable_from_main_thread_only || is_main_thread);

    // Expects buf to be empty.

    if (reread) {
        if (lines.len) {
            u32 y = lines.len-1;
            internal_start_edit(new_cur2(0, 0), new_cur2(lines[y].len, y));
        } else {
            internal_start_edit(new_cur2(0, 0), new_cur2(0, 0));
        }
    }

    clear();

    Line *line = NULL;
    u32 *bc = NULL;

    auto insert_new_line = [&]() -> bool {
        line = lines.append();
        bc = bytecounts.append();

        // make this check more robust/at the right place?
        if (!line) return false;

        line->init(LIST_CHUNK, CHUNK0);
        *bc = 0;
        return true;
    };

    if (!insert_new_line()) return false;

    Cstr_To_Ustr conv; conv.init();
    bool last_was_cr = false;
    char ch;

    while (f(&ch)) {
        (*bc)++;

        if (conv.feed(ch)) {
            if (conv.uch == '\n') {
                if (last_was_cr) {
                    (*bc)--;
                    last_was_cr = false;
                }
                if (!insert_new_line()) return false;
            } else {
                if (last_was_cr) {
                    if (!line->append('\r')) return false;
                    last_was_cr = false;
                } else if (conv.uch == '\r') {
                    last_was_cr = true;
                    continue;
                }
                if (!line->append(conv.uch)) return false;
            }
        }
    }

    (*bytecounts.last())++;
    dirty = false;

    if (lang != LANG_NONE && !reread) {
        if (tree) {
            ts_tree_delete(tree);
            tree = NULL;
        }
        update_tree();
    }

    if (reread) {
        u32 y = lines.len-1;
        internal_finish_edit(new_cur2(lines[y].len, y));
    }

    return true;
}

bool Buffer::read_data(char *data, int len, bool reread) {
    if (len == -1) len = strlen(data);

    int i = 0;
    return read([&](char* out) {
        if (i < len) {
            *out = data[i++];
            return true;
        }
        return false;
    }, reread);
}

bool Buffer::read(File_Mapping *fm, bool reread) {
    return read_data((char*)fm->data, fm->len, reread);
}

void Buffer::write(File *f) {
    auto write_char = [&](uchar ch) {
        char buf[4];
        auto count = uchar_to_cstr(ch, buf);
        f->write(buf, count);
    };

    bool first = true;
    For (&lines) {
        if (first)
            first = false;
        else
            write_char('\n');
        for (auto ch : it)
            write_char(ch);
    }
}

// This function produces the same result as internal_delete_lines(), except it
// mimics the behavior through remove(). This is because doing this via
// .remove() has been tested to work -- the entire reason we made
// internal_...() functions was to stop us from calling them directly, because
// it would lead to bugs.
//
// Update: Also, .remove() handles tree updating correctly.
//
// Also, y1 and y2 are inclusive, whereas internal_delete_lines() is exclusive.
void Buffer::remove_lines(u32 y1, u32 y2) {
    auto start = new_cur2(0, y1);
    auto end = new_cur2(0, y2+1);

    if (y2 == lines.len-1) {
        // when would this happen? and is this how we should handle?
        // if (!y1) return;

        start = dec_cur(start);
        end = new_cur2(lines[lines.len-1].len, lines.len-1);
    } else {
        end = new_cur2(0, y2+1);
    }

    remove(start, end);
}

void Buffer::internal_delete_lines(u32 y1, u32 y2) {
    cp_assert(!editable_from_main_thread_only || is_main_thread);

    // holy shit, is this the end of a two year long bug????????
    if (y1 > lines.len) y1 = lines.len;
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
    internal_delete_lines(0, lines.len);
    dirty = true;
}

s32 get_bytecount(Line *line) {
    s32 bc = 0;
    For (line) bc += uchar_size(it);
    return bc + 1;
}

void Buffer::internal_insert_line(u32 y, uchar* text, s32 len) {
    cp_assert(!editable_from_main_thread_only || is_main_thread);

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

void Buffer::internal_append_line(uchar* text, s32 len) {
    cp_assert(!editable_from_main_thread_only || is_main_thread);

    internal_insert_line(lines.len, text, len);
    dirty = true;
}

void Buffer::insert(cur2 start, uchar* text, s32 len, bool applying_change) {
    i32 x = start.x, y = start.y;

    internal_start_edit(start, start);

    Change *change = NULL;
    bool need_new_change = false;

    if (use_history && !applying_change) {
        auto c = hist_get_latest_change_for_append();
        if (c) {
            if (start == c->new_end || hist_batch_mode) {
                change = c;
                if (start != c->new_end)
                    need_new_change = true;
            }
        }
    }

    bool has_newline = false;
    for (u32 i = 0; i < len; i++) {
        if (text[i] == '\n') {
            has_newline = true;
            break;
        }
    }

    if (y == lines.len)
        internal_append_line(NULL, 0);

    auto line = &lines[y];
    s32 total_len = line->len + len;

    if (has_newline) {
        uchar* buf = alloc_temp_array(total_len);
        defer { free_temp_array(buf, total_len); };

        if (x)
            memcpy(buf, line->items, sizeof(uchar) * x);
        memcpy(&buf[x], text, sizeof(uchar) * len);
        if (line->len > x)
            memcpy(&buf[x+len], &line->items[x], sizeof(uchar) * (line->len - x));

        internal_delete_lines(y, y + 1);

        u32 last = 0;
        for (u32 i = 0; i <= total_len; i++) {
            if (i == total_len || buf[i] == '\n') {
                internal_insert_line(y++, buf + last, i - last);
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

    auto end = start;
    for (int i = 0; i < len; i++) {
        if (text[i] == '\n') {
            end.x = 0;
            end.y++;
        } else {
            end.x++;
        }
    }

    if (use_history && !applying_change) {
        auto start_of_chars_to_copy = start;
        while (start_of_chars_to_copy < end) {
            if (!change || change->new_text.len == change->new_text.cap || need_new_change) {
                need_new_change = false;

                if (!change) {
                    change = hist_push();
                } else {
                    change->next = hist_alloc();
                    change = change->next;
                }
                change->start = start_of_chars_to_copy;
                change->old_end = start_of_chars_to_copy;
            }

            auto it = iter(start_of_chars_to_copy);
            while (it.pos != end && change->new_text.len < change->new_text.cap)
                change->new_text.append(it.next());

            change->new_end = it.pos;
            start_of_chars_to_copy = it.pos;
        }
    }

    internal_finish_edit(end);
    dirty = true;
}

void Buffer::update_tree() {
    TSInput input;
    input.payload = this;
    input.encoding = TSInputEncodingUTF8;

    input.read = [](void *p, uint32_t off, TSPoint pos, uint32_t *read) -> const char* {
        auto buf = (Buffer*)p;

        if (pos.row >= buf->lines.len) {
            buf->tsinput_buffer[0] = '\0';
            *read = 0;
            return buf->tsinput_buffer;
        }

        auto it = buf->iter(new_cur2(buf->idx_byte_to_cp(pos.row, pos.column, true), pos.row));
        u32 n = 0;

        while (!it.eof()) {
            auto uch = it.next();
            if (!uch) break;

            auto size = uchar_size(uch);
            if (n + size + 1 > _countof(buf->tsinput_buffer)) break;

            n += uchar_to_cstr(uch, &buf->tsinput_buffer[n]);
        }

        *read = n;
        buf->tsinput_buffer[n] = '\0';
        return buf->tsinput_buffer;
    };

    tree = ts_parser_parse(parser, tree, input);
    tree_dirty = true;
    tree_version++;
}

void Buffer::internal_start_edit(cur2 start, cur2 end) {
    cp_assert(!editable_from_main_thread_only || is_main_thread);

    // we use the tsedit for other things, right now for mark tree calculations.
    // so create it even if lang == LANG_NONE, it's not only used for the tree.
    ptr0(&tsedit);
    tsedit.start_byte = cur_to_offset(start);
    tsedit.start_point = cur_to_tspoint(start);
    tsedit.old_end_byte = cur_to_offset(end);
    tsedit.old_end_point = cur_to_tspoint(end);

    edit_buffer_old.len = 0;
    edit_buffer_new.len = 0;

    for (auto it = iter(start); it.pos < end && !it.eof(); it.next())
        edit_buffer_old.append(it.peek());
}

void Buffer::internal_finish_edit(cur2 new_end) {
    cp_assert(!editable_from_main_thread_only || is_main_thread);

    tsedit.new_end_byte = cur_to_offset(new_end);
    tsedit.new_end_point = cur_to_tspoint(new_end);

    if (lang != LANG_NONE) {
        ts_tree_edit(tree, &tsedit);
        update_tree();
    }

    internal_update_mark_tree();
}

void Buffer::internal_update_mark_tree() {
    cp_assert(!editable_from_main_thread_only || is_main_thread);

    if (!mark_tree.root) return;

    {
        auto start = tspoint_to_cur(tsedit.start_point);
        auto end = tspoint_to_cur(tsedit.new_end_point);

        for (auto it = iter(start); it.pos < end && !it.eof(); it.next())
            edit_buffer_new.append(it.peek());
    }

    auto a = new_dstr(&edit_buffer_old);
    auto b = new_dstr(&edit_buffer_new);

    auto start = tspoint_to_cur(tsedit.start_point);
    auto oldend = tspoint_to_cur(tsedit.old_end_point);
    auto newend = tspoint_to_cur(tsedit.new_end_point);

    auto diffs = diff_main(a, b);
    if (!diffs) {
        mark_tree.apply_edit(start, oldend, newend);
        return;
    }

    auto advance_cur = [&](cur2 c, DString s) -> cur2 {
        for (int i = 0, len = s.len(); i < len; i++) {
            if (s.get(i) == '\n') {
                c.y++;
                c.x = 0;
            } else {
                c.x++;
            }
        }
        return c;
    };

    auto cur = start;

    For (diffs) {
        switch (it.type) {
        case DIFF_DELETE:
            mark_tree.apply_edit(cur, advance_cur(cur, it.s), cur);
            break;
        case DIFF_INSERT: {
            auto end = advance_cur(cur, it.s);
            mark_tree.apply_edit(cur, cur, end);
            cur = end;
            break;
        }
        case DIFF_SAME:
            cur = advance_cur(cur, it.s);
            break;
        }
    }
}

Change* Buffer::hist_alloc() {
    auto ret = world.change_fridge.alloc();
    ret->old_text.init(LIST_FIXED, _countof(ret->_old_text), ret->_old_text);
    ret->new_text.init(LIST_FIXED, _countof(ret->_new_text), ret->_new_text);
    return ret;
}

void Buffer::hist_free(int i) {
    auto change = history[i];
    while (change) {
        auto next = change->next;
        world.change_fridge.free(change);
        change = next;
    }
}

// This doesn't fill out start, old_end, or new_end. Caller has to.
Change* Buffer::hist_push() {
    hist_force_push_next_change = false;

    auto curr = hist_curr;

    for (int i = hist_curr; i != hist_top; i = hist_inc(i))
        hist_free(i);

    hist_top = hist_curr = hist_inc(hist_curr);
    if (hist_curr == hist_start) {
        hist_free(hist_start);
        hist_start = hist_inc(hist_start);
    }

    auto ret = hist_alloc();
    history[curr] = ret;
    return ret;
}

Change* Buffer::hist_get_latest_change_for_append() {
    if (hist_curr == hist_start) return NULL;
    if (hist_force_push_next_change) return NULL;

    auto c = history[hist_dec(hist_curr)];
    if (!c) return NULL; // when would this happen?

    while (c->next) c = c->next;

    return c;
}

/*
void Buffer::hist_start_batch() {
    hist_batch_mode = true;
    hist_force_push_next_change = true;
}

void Buffer::hist_end_batch() {
    hist_batch_mode = false;
    hist_force_push_next_change = false;
}
*/

int Buffer::internal_distance_between(cur2 a, cur2 b) {
    cp_assert(!editable_from_main_thread_only || is_main_thread);

    if (a.y == b.y) return b.x - a.x;

    int total = 0;
    for (int y = a.y+1; y <= b.y-1; y++)
        total += lines[y].len;
    return total + (lines[a.y].len - a.x + 1) + b.x;
}

void Buffer::remove(cur2 start, cur2 end, bool applying_change) {
    i32 x1 = start.x, y1 = start.y;
    i32 x2 = end.x, y2 = end.y;

    internal_start_edit(start, end);

    do {
        if (!use_history) break;
        if (applying_change) break;

        Change *change = NULL;
        bool need_new_change = false;

        auto c = hist_get_latest_change_for_append();
        if (c) {
            if (c->new_end == end || hist_batch_mode) {
                change = c;
                if (c->new_end != end)
                    need_new_change = true;
            }
        }

        if (change && !need_new_change) {
            if (change->start <= start) {
                change->new_end = start;
                change->new_text.len -= internal_distance_between(start, end);
                break;
            }

            // we have an existing change and we're deleting past the start,
            // this means we're completing wiping out any text we've written
            change->new_text.len = 0;
        }

        auto end_of_chars_to_copy = end;

        // if we are going to be appending to this change
        if (change && !need_new_change)
            end_of_chars_to_copy = change->start;

        while (start < end_of_chars_to_copy) {
            if (!change || change->old_text.len == change->old_text.cap || need_new_change) {
                need_new_change = false;
                Change *new_change = NULL;
                if (!change) {
                    change = hist_push();
                } else {
                    change->next = hist_alloc();
                    change = change->next;
                }
                change->old_end = end_of_chars_to_copy;
            }

            int rem = change->old_text.cap - change->old_text.len;
            int chars = 0;
            auto it = iter(end_of_chars_to_copy);

            do {
                it.prev();
                chars++;
            } while (chars < rem && it.pos != start);

            change->start = it.pos;
            change->new_end = it.pos;

            {
                auto tmp = new_list(uchar);
                For (&change->old_text)
                    tmp->append(it);

                change->old_text.len = 0;
                for (; it.pos != end_of_chars_to_copy; it.next())
                    change->old_text.append(it.peek());
                For (tmp)
                    change->old_text.append(it);
            }

            end_of_chars_to_copy = change->start;
        }
    } while (0);

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
        internal_delete_lines(y1 + 1, min(lines.len, y2 + 1));
    }
    bytecounts[y1] = get_bytecount(&lines[y1]);
    dirty = true;

    internal_finish_edit(start);
}

Buffer_It Buffer::iter(cur2 c) {
    Buffer_It it; ptr0(&it);
    it.buf = this;
    it.pos = c;

    // TODO: should we assert or clamp here if c is out of bounds?
    return it;
}

cur2 Buffer::inc_cur(cur2 c) {
    auto it = iter(c);
    it.next();
    return it.pos;
}

Gr_Iter Buffer::gr_iter(cur2 c) {
    Gr_Iter ret;
    ret.init(iter(c));
    return ret;
}

cur2 Buffer::inc_gr(cur2 c) {
    auto it = gr_iter(c);
    if (!it.eof()) {
        it.peek();
        it.next();
    }
    return it.pos();
}

cur2 Buffer::dec_cur(cur2 c) {
    auto it = iter(c);
    it.prev();
    return it.pos;
}

uchar* Buffer::alloc_temp_array(s32 size) {
    if (size > 1024)
        return (uchar*)cp_malloc(sizeof(uchar) * size);
    return new_array(uchar, size);
}

void Buffer::free_temp_array(uchar* buf, s32 size) {
    if (size > 1024)
        cp_free(buf);
}

i32 Buffer::cur_to_offset(cur2 c) {
    i32 ret = 0;
    for (u32 y = 0; y < c.y; y++)
        ret += bytecounts[y];

    for (u32 x = 0; x < c.x; x++)
        ret += uchar_size(lines[c.y][x]);

    return ret;
}

u32 Buffer::idx_gr_to_cp(int y, int off) {
    auto &line = lines[y];

    Grapheme_Clusterer gc;
    gc.init();

    int idx = 0;
    gc.feed(line[idx]);

    for (int i = 0; i < off; i++) {
        idx++;
        while (idx < line.len && !gc.feed(line[idx]))
            idx++;
    }

    return idx; // is this right? lol
}

u32 Buffer::idx_cp_to_byte(int y, int off) {
    auto &line = lines[y];
    int ret = 0;

    cp_assert(off <= line.len);

    for (int i = 0; i < off && i < line.len; i++)
        ret += uchar_size(line[i]);
    return ret;
}

u32 Buffer::idx_byte_to_gr(int y, int off) {
    auto &line = lines[y];
    int k = 0;
    u32 x = 0;

    Grapheme_Clusterer gc;
    gc.init();
    gc.feed(line[k]);

    for (; k < line.len; x++) {
        int size = uchar_size(line[k++]);
        while (k < line.len && !gc.feed(line[k])) {
            size += uchar_size(line[k]);
            k++;
        }

        if (off < size) return x;
        off -= size;
    }

    cp_assert(!off);
    return x;
}

u32 Buffer::idx_byte_to_cp(int y, int off, bool nocrash) {
    auto &line = lines[y];
    for (u32 x = 0; x < line.len; x++) {
        auto size = uchar_size(line[x]);
        if (off < size) return x;
        off -= size;
    }

    if (!nocrash) {
        cp_assert(!off);
    }
    return lines[y].len;
}

u32 Buffer::internal_convert_x_vx(int y, int off, bool to_vx) {
    cp_assert(!editable_from_main_thread_only || is_main_thread);

    int x = 0;
    int vx = 0;
    auto &line = lines[y];

    Grapheme_Clusterer gc;
    gc.init();
    gc.feed(line[x]);

    while (true) {
        if (x >= line.len) break;

        int dvx = -1;
        int dx = 1;

        if (line[x] == '\t') {
            dvx = options.tabsize - (vx % options.tabsize);
        } else {
            dvx = cp_wcwidth(line[x]);
            if (dvx == -1) dvx = 1;
            while (x+dx < (to_vx ? off : line.len) && !gc.feed(line[x+dx]))
                dx++;
        }

        if (to_vx) {
            if (x + dx > off) break;
        } else {
            if (vx + dvx > off) break;
        }

        vx += dvx;
        x += dx;
    }

    return to_vx ? vx : x;
}

cur2 Buffer::offset_to_cur(i32 off, bool nothrow) {
    cur2 ret = NULL_CUR;

    Fori (&bytecounts) {
        if (off < it) {
            ret.y = i;
            ret.x = idx_byte_to_cp(i, off);
            break;
        }
        off -= it;
    }

    if (ret == NULL_CUR) {
        if (!nothrow) {
            cp_assert(!off);
        }
        ret.y = lines.len-1;
        ret.x = lines[ret.y].len;
    }

    return ret;
}

// ============
// mark_tree_marker

// if this gets to be too slow, switch to per-tree lock
Lock global_mark_tree_lock;

void cleanup_mark_node(Mark_Node *node) {
    if (!node) return;

    cleanup_mark_node(node->left);
    cleanup_mark_node(node->right);

    for (auto it = node->marks; it; it = it->next) {
        if (it == it->next)
            cp_panic("infinite loop");
        it->valid = false;
    }

    world.mark_node_fridge.free(node);
}

void Mark_Tree::cleanup() {
    cleanup_mark_node(root);
    // mem.cleanup();
}

int Mark_Tree::get_height(Mark_Node *root) {
    return !root ? 0 : root->height;
}

int Mark_Tree::get_balance(Mark_Node *root) {
    return !root ? 0 : (get_height(root->left) - get_height(root->right));
}

Mark_Node *Mark_Tree::succ(Mark_Node *node) {
    if (node->right) {
        auto ret = node->right;
        while (ret->left)
            ret = ret->left;
        return ret;
    }

    for (; node->parent; node = node->parent)
        if (node->isleft)
            return node->parent;
    return NULL;
}

Mark_Node *Mark_Tree::find_node(Mark_Node *root, cur2 pos) {
    if (!root) return NULL;
    if (root->pos == pos) return root;

    auto child = pos < root->pos ? root->left : root->right;
    if (!child)
        return root;
    return find_node(child, pos);
}

Mark_Node *Mark_Tree::insert_node(cur2 pos) {
    auto node = find_node(root, pos);
    if (node && node->pos == pos)
        return node;

    node = world.mark_node_fridge.alloc();
    node->pos = pos;
    root = internal_insert_node(root, pos, node);

    check_tree_integrity();

    return node;
}

void Mark_Tree::insert_mark(Mark_Type type, cur2 pos, Mark *mark) {
    world.global_mark_tree_lock.enter();
    defer { world.global_mark_tree_lock.leave(); };

    mark->type = type;
    mark->tree = this;
    mark->valid = true;

    check_tree_integrity();

    auto node = insert_node(pos);
    mark->node = node;
    mark->next = node->marks;
    if (mark->next == mark) cp_panic("infinite loop");
    node->marks = mark;

    check_tree_integrity();
}

void Mark_Tree::recalc_height(Mark_Node *root) {
    root->height = 1 + max(get_height(root->left), get_height(root->right));
}

Mark_Node* Mark_Tree::rotate_right(Mark_Node *root) {
    auto y = root->left;

    y->parent = root->parent;
    y->isleft = root->isleft;

    root->left = y->right;
    if (root->left) {
        root->left->parent = root;
        root->left->isleft = true;
    }

    y->right = root;
    y->right->parent = y;
    y->right->isleft = false;

    recalc_height(root);
    recalc_height(y);
    return y;
}

Mark_Node* Mark_Tree::rotate_left(Mark_Node *root) {
    auto y = root->right;

    y->parent = root->parent;
    y->isleft = root->isleft;

    root->right = y->left;
    if (root->right) {
        root->right->parent = root;
        root->right->isleft = false;
    }

    y->left = root;
    y->left->parent = y;
    y->left->isleft = true;

    recalc_height(root);
    recalc_height(y);
    return y;
}

// precond: pos doesn't exist in root
Mark_Node *Mark_Tree::internal_insert_node(Mark_Node *root, cur2 pos, Mark_Node *node) {
    if (!root) {
        recalc_height(node);
        return node;
    }

    if (root->pos == pos) cp_panic("trying to insert duplicate pos");

    if (pos < root->pos) {
        auto old = root->left;
        root->left = internal_insert_node(root->left, pos, node);
        if (!old) {
            node->parent = root;
            node->isleft = true;
        }
    } else {
        auto old = root->right;
        root->right = internal_insert_node(root->right, pos, node);
        if (!old) {
            node->parent = root;
            node->isleft = false;
        }
    }

    recalc_height(root);

    auto balance = get_balance(root);
    if (balance > 1) {
        if (pos > root->left->pos)
            root->left = rotate_left(root->left);
        return rotate_right(root);
    }
    if (balance < -1) {
        if (pos < root->right->pos)
            root->right = rotate_right(root->right);
        return rotate_left(root);
    }
    return root;
}

void Mark_Tree::delete_mark(Mark *mark) {
    world.global_mark_tree_lock.enter();
    defer { world.global_mark_tree_lock.leave(); };

    auto node = mark->node;
    bool found = false;

    check_tree_integrity();

    // remove `mark` from `node->marks`
    Mark *last = NULL;
    for (auto it = node->marks; it; it = it->next) {
        if (it == mark) {
            found = true;
            if (!last)
               node->marks = mark->next;
            else {
               last->next = mark->next;
               if (last->next == last) cp_panic("infinite loop");
            }
            break;
        }
        last = it;
    }

    check_tree_integrity();

    if (!found)
        cp_panic("trying to delete mark not found in its node");

    if (!node->marks)
        delete_node(node->pos);

    mark->valid = false;
    // ptr0(mark); // surface the error earlier
    // world.mark_fridge.free(mark);
}

void Mark_Tree::delete_node(cur2 pos) {
    check_tree_integrity();
    root = internal_delete_node(root, pos);
    check_tree_integrity();
}

Mark_Node *Mark_Tree::internal_delete_node(Mark_Node *root, cur2 pos) {
    if (!root) return root;

    if (pos < root->pos)
        root->left = internal_delete_node(root->left, pos);
    else if (pos > root->pos)
        root->right = internal_delete_node(root->right, pos);
    else {
        if (root->marks)
            cp_panic("trying to delete a node that still has marks");

        if (!root->left || !root->right) {
            Mark_Node *ret = NULL;
            bool isleft = false;

            if (!root->left) ret = root->right;
            if (!root->right) ret = root->left;

            if (ret) {
                ret->parent = root->parent;
                ret->isleft = root->isleft;
            }

            world.mark_node_fridge.free(root);
            return ret;
        }

        auto min = root->right;
        while (min->left)
            min = min->left;

        root->marks = min->marks;
        for (auto it = root->marks; it; it = it->next)
            it->node = root;
        root->pos = min->pos;
        min->marks = NULL;

        root->right = internal_delete_node(root->right, min->pos);
    }

    recalc_height(root);

    auto balance = get_balance(root);
    if (balance > 1) {
        if (get_balance(root->left) < 0)
            root->left = rotate_left(root->left);
        return rotate_right(root);
    }
    if (balance < -1) {
        if (get_balance(root->right) > 0)
            root->right = rotate_right(root->right);
        return rotate_left(root);
    }
    return root;
}

void Mark_Tree::apply_edit(cur2 start, cur2 old_end, cur2 new_end) {
    world.global_mark_tree_lock.enter();
    defer { world.global_mark_tree_lock.leave(); };

    if (start == old_end && old_end == new_end)
        return;

    /*
     *   for pos where (start < pos < old_end)
     *      if pos < new_end
     *          invalidate marks
     *      else
     *          set pos to new_end
     *
     *   for pos where (pos >= old_end)
     *      if pos.y == old_end.y
     *         pos.y = new_end.y
     *         pos.x = pos.x - old_end.x + new_end.x
     *      else
     *         pos.y += (new_end.y - old_end.y)
     */

    auto nstart = find_node(root, start);
    if (!nstart) return;

    auto it = nstart;
    if (it->pos < start) it = succ(it);

    Mark *orphan_marks = NULL;

    check_tree_integrity();

    while (it && it->pos < old_end) {
        if (it->pos < new_end) {
            it = succ(it);
        } else {
            for (auto mark = it->marks; mark; mark = mark->next)
                if (mark == mark->next)
                    cp_panic("infinite loop");

            Mark *next = NULL;

            if (it->marks == orphan_marks)
                cp_panic("why is this happening?");

            int i = 0;
            for (auto mark = it->marks; mark; mark = next) {
                if (orphan_marks == mark)
                    cp_panic("why is this happening?");

                mark->node = NULL;
                next = mark->next;

                mark->next = orphan_marks;
                if (mark->next == mark) cp_panic("infinite loop");

                orphan_marks = mark;
                i++;
            }
            it->marks = NULL;

            auto curr = it->pos;
            delete_node(curr);

            // after deleting the node, go to the next node
            it = find_node(root, curr);
            if (it && it->pos < curr)
                it = succ(it);
        }
    }

    check_tree_integrity();

    for (; it; it = succ(it)) {
        if (it->pos.y == old_end.y) {
            it->pos.y = new_end.y;
            it->pos.x = it->pos.x + new_end.x - old_end.x;
        } else {
            it->pos.y = it->pos.y + new_end.y - old_end.y;
        }
    }

    check_tree_integrity();

    if (orphan_marks) {
        auto nend = insert_node(new_end);
        Mark *next = NULL;
        for (auto mark = orphan_marks; mark; mark = next) {
            next = mark->next;
            mark->node = nend;
            mark->next = nend->marks;
            if (mark->next == mark) cp_panic("infinite loop");
            nend->marks = mark;
        }
    }

    check_tree_integrity();
}

void Mark_Tree::check_mark_cycle(Mark_Node *root) {
    if (!root) return;

    for (auto mark = root->marks; mark; mark = mark->next)
        if (mark == mark->next)
            cp_panic("mark cycle detected");

    check_mark_cycle(root->left);
    check_mark_cycle(root->right);
}

void Mark_Tree::check_duplicate_marks_helper(Mark_Node *root, List<Mark*> *seen) {
    if (!root) return;

    check_duplicate_marks_helper(root->left, seen);
    check_duplicate_marks_helper(root->right, seen);

    for (auto m = root->marks; m; m = m->next) {
        if (seen->find([&](auto it) { return *it == m; }))
            cp_panic("duplicate mark");
        seen->append(m);
    }
}

void Mark_Tree::check_duplicate_marks() {
    SCOPED_FRAME();

    // if this gets too slow, use a set
    List<Mark*> seen; seen.init();

    check_duplicate_marks_helper(root, &seen);
}

void Mark_Tree::check_tree_integrity() {
    check_ordering();
    check_mark_cycle(root);
    check_duplicate_marks();
}

void Mark_Tree::check_ordering() {
    auto min = root;
    if (!min) return;

    while (min->left)
        min = min->left;

    // o(n). we need this for now, don't have this in final ver.
    cur2 last = {-1, -1};
    for (auto it = min; it; it = succ(it)) {
        if (last.x != -1) {
            if (last >= it->pos) {
                cp_panic("out of order (or duplicate)");
            }
        }
        last = it->pos;
    }
}

cur2 Mark::pos() { return node->pos; }
void Mark::cleanup() { if (valid) tree->delete_mark(this); }

bool is_mark_valid(Mark *mark) {
    return mark && mark->valid;
}

// ============
// mark_tree_marker
