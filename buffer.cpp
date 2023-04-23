#include "buffer.hpp"
#include "common.hpp"
#include "world.hpp"
#include "unicode.hpp"
#include "diff.hpp"
#include "defer.hpp"

int Bytecounts_Tree::sum(int until) {
    auto s = treap_split(root, until);
    int sum = treap_node_sum(s->left);
    root = treap_merge(s->left, s->right);
    return sum;
}

int Bytecounts_Tree::internal_offset_to_line(Treap *t, int limit, int *rem) {
    if (!t) return 0;

    if (limit >= treap_node_sum(t))
        return treap_node_size(t);

    int left_sum = treap_node_sum(t->left);
    if (limit < left_sum)
        return internal_offset_to_line(t->left, limit, rem);

    if (limit == left_sum)
        return treap_node_size(t->left);

    if (limit - left_sum < t->val) {
        *rem = limit - left_sum;
        return treap_node_size(t->left);
    }

    return treap_node_size(t->left) + 1
        + internal_offset_to_line(t->right, limit - left_sum - t->val, rem);
}

void Bytecounts_Tree::insert(int idx, int val) {
    auto node = world.treap_fridge.alloc();
    node->val = val;
    node->size = 1;
    node->sum = val;
    node->priority = rand();
    root = treap_insert_node(root, idx, node);
}

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

Grapheme Buffer_It::gr_peek(cur2 *end) {
    if (eof()) return NULL;

    cur2 start = pos;

    Grapheme_Clusterer gc;
    gc.init();
    gc.feed(peek());

    auto ret = new_list(uchar);
    do {
        ret->append(next());
    } while (!eof() && !gc.feed(peek()));

    if (end) *end = pos;
    pos = start;
    return ret;
}

Grapheme Buffer_It::gr_prev() {
    auto current_pos = pos;

    // i guess the thing i don't know is... let's say you have a grapheme
    // consisting of codepoints A B C. if you start at C, will it read just C
    // as a grapheme? what if you start at B?

    Grapheme last_gr = NULL;
    while (!bof()) {
        auto old = pos;
        prev();

        cur2 end = NULL_CUR;
        auto gr = gr_peek(&end);

        // the grapheme starting at this pos no longer ends at current_pos
        if (!gr || end != current_pos) {
            // if old == current_pos it means that somehow the codepoint prior
            // to current_pos does not result in a grapheme that ends at
            // current_pos. that is very weird, but we need to move backward,
            // or we will probably have an infinite loop in downstream code. so
            // just keep the prev(), and get out.
            if (old != current_pos)
                pos = old;
            return last_gr;
        }
        last_gr = gr;
    }
    return last_gr;
}

Grapheme Buffer_It::gr_next() {
    cur2 newpos;
    auto ret = gr_peek(&newpos);
    if (ret) pos = newpos;
    return ret;
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

List<uchar>* cstr_to_ustr(ccstr s, int len) {
    if (len == -1) len = strlen(s);

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

    edit_text(start, old_end, new_text->items, new_text->len, true);
}

void Buffer::tree_batch_start() {
    if (lang == LANG_NONE) return;

    tree_batch_mode = true;
    if (!tree_batch_refs)
        tree_batch_edits.len = 0;
    tree_batch_refs++;
}

void Buffer::tree_batch_end() {
    if (lang == LANG_NONE) return;

    Timer t; t.init("tree_batch_end"); t.always_log = true;

    tree_batch_refs--;
    if (tree_batch_refs != 0) return;

    tree_batch_mode = false;

    if (tree) {
        TSInputEdit curr;
        memcpy(&curr, &tree_batch_edits[0], sizeof(TSInputEdit));

        Fori (&tree_batch_edits) {
            if (i == 0) continue;

            // if we can merge edit and it, do so
            // otherwise, apply curr and set curr to it
            if (curr.new_end_byte == it.old_end_byte) {
                if (it.start_byte < curr.start_byte) {
                    curr.start_byte = it.start_byte;
                    curr.start_point = it.start_point;
                }
                curr.new_end_byte = it.new_end_byte;
                curr.new_end_point = it.new_end_point;
            } else {
                ts_tree_edit(tree, &curr);
                memcpy(&curr, &it, sizeof(TSInputEdit));
            }
        }
        ts_tree_edit(tree, &curr);
        t.log("apply tsedits");
    }

    update_tree();
    t.log("call update_tree");
}

cur2 Buffer::hist_undo() {
    if (hist_curr == hist_start) return NULL_CUR;

    tree_batch_start();
    defer { tree_batch_end(); };

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

    tree_batch_start();
    defer { tree_batch_end(); };

    auto ret = NULL_CUR;

    for (auto it = history[hist_curr]; it; it = it->next) {
        hist_apply_change(it, false);
        if (!it->next) ret = it->start;
    }

    hist_curr = hist_inc(hist_curr);
    return ret;
}

List<uchar>* Buffer::get_uchars(cur2 start, cur2 end, int limit, cur2 *actual_end) {
    // make sure start and end are valid
    if (start.x > lines[start.y].len) start.x = lines[start.y].len;
    if (end.y >= lines.len)           end.y = lines.len-1;
    if (end.x > lines[end.y].len)     end.x = lines[end.y].len;

    auto ret = new_list(uchar);

    if (limit != -1)
        *actual_end = end;

    for (int y = start.y; y <= end.y; y++) {
        int xstart = y == start.y ? start.x : 0;
        int xend = y == end.y ? end.x : lines[y].len;

        int count = xend - xstart;
        if (y < end.y) count++; // '\n'

        if (limit != -1 && count > limit) {
            ret->concat(lines[y].items + xstart, limit);
            *actual_end = new_cur2(xstart + limit, y);
            break;
        } else {
            ret->concat(lines[y].items + xstart, xend - xstart);
            if (y < end.y)
                ret->append('\n');
        }

        if (limit != -1)
            limit -= count;
    }

    return ret;
}

ccstr Buffer::get_text(cur2 start, cur2 end) {
    auto ret = new_list(char);

    For (get_uchars(start, end)) {
        char tmp[4];
        int count = uchar_to_cstr(it, tmp);
        ret->concat(tmp, count);
    }

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
    if (c.y >= lines.len) return end_pos();

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
        edit_buffer_old.init();
        edit_buffer_new.init();
        tree_batch_edits.init();
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

void Buffer::read(char *data, int len) {
    SCOPED_BATCH_CHANGE(this);

    cp_assert(!editable_from_main_thread_only || is_main_thread);

    auto start = new_cur2(0, 0);

    if (lines.len > 1 || lines[0].len)
        remove(start, end_pos());

    if (len == 0) {
        if (!lines.len) {
            // insert a space & remove it right away, to ensure we have at
            // least one empty
            uchar tmp = (uchar)' ';
            insert(start, &tmp, 1);
            remove(start, end_pos());
        }
        return;
    }

    if (len == -1) len = strlen(data);

    auto uchars = cstr_to_ustr(data, len);
    insert(start, uchars->items, uchars->len);
}

void Buffer::read(File_Mapping *fm) {
    read((char*)fm->data, fm->len);
}

void Buffer::write(File *f) {
    Fori (&lines) {
        auto uchars = &it;
        auto chars = new_list(char);

        For (uchars) {
            char buf[4];
            auto count = uchar_to_cstr(it, buf);
            chars->concat(buf, count);
        }

        if (i != lines.len-1)
            chars->append('\n');

        f->write(chars->items, chars->len);
    }
}

// This function basically produces the result internal_delete_lines() does,
// except it mimics the outcome through remove(). All changes
// must happen through .insert() and .remove(), or a whole bunch of shit
// breaks, including history and tree handling.
//
// Note: here y1 and y2 are inclusive, whereas internal_delete_lines() is exclusive.
void Buffer::remove_lines(u32 y1, u32 y2) {
    auto start = new_cur2(0, y1);
    auto end = new_cur2(0, y2+1);

    if (y2 == lines.len-1) {
        // when would this happen? and is this how we should handle?
        // if (!y1) return;

        start = dec_cur(start);
        end = end_pos();
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
    if (y2 < lines.len)
        memmove(&lines[y1], &lines[y2], sizeof(Line) * (lines.len - y2));
    lines.len -= (y2 - y1);

    for (int i = y1; i < y2; i++)
        bctree.remove(y1);

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

    lines.append();

    if (y > lines.len)
        y = lines.len;
    else if (y < lines.len)
        memmove(&lines[y + 1], &lines[y], sizeof(Line) * (lines.len - y));

    lines[y].init(LIST_CHUNK, len);
    lines[y].len = len;
    memcpy(lines[y].items, text, sizeof(uchar) * len);

    bctree.insert(y, get_bytecount(&lines[y]));

    dirty = true;
}

void Buffer::internal_append_line(uchar* text, s32 len) {
    cp_assert(!editable_from_main_thread_only || is_main_thread);

    internal_insert_line(lines.len, text, len);
    dirty = true;
}

cur2 Buffer::edit_text(cur2 start, cur2 old_end, uchar *text, s32 len, bool applying_change) {
    cp_assert(!editable_from_main_thread_only || is_main_thread);

    SCOPED_BATCH_CHANGE(this);

    TSInputEdit edit;
    edit.start_byte = cur_to_offset(start);
    edit.start_point = cur_to_tspoint(start);
    edit.old_end_byte = cur_to_offset(old_end);
    edit.old_end_point = cur_to_tspoint(old_end);

    edit_buffer_old.len = 0;
    edit_buffer_old.concat(get_uchars(start, old_end));

    // do the remove
    if (start != old_end) {
        if (use_history && !applying_change)
            internal_commit_remove_to_history(start, old_end);
        internal_do_actual_remove(start, old_end);
    }

    // do the insert
    cur2 new_end = start;
    if (text && len) {
        internal_do_actual_insert(start, text, len);

        // easier/better way to calculate this?
        new_end = start;
        for (int i = 0; i < len; i++) {
            if (text[i] == '\n') {
                new_end.x = 0;
                new_end.y++;
            } else {
                new_end.x++;
            }
        }

        if (use_history && !applying_change)
            internal_commit_insert_to_history(start, new_end);
    }

    edit.new_end_byte = cur_to_offset(new_end);
    edit.new_end_point = cur_to_tspoint(new_end);

    if (lang != LANG_NONE) {
        if (!tree_batch_mode) {
            ts_tree_edit(tree, &edit);
            update_tree();
        } else {
            tree_batch_edits.append(&edit);
        }
    }

    internal_update_mark_tree(&edit);

    // set this here or in .remove() and .insert()?
    buf_version++;
    dirty = true;
    return new_end;
}

cur2 Buffer::insert(cur2 start, uchar* text, s32 len, bool applying_change) {
    return edit_text(start, start, text, len, applying_change);
}

void Buffer::remove(cur2 start, cur2 end, bool applying_change) {
    edit_text(start, end, NULL, 0, applying_change);
}

void Buffer::internal_do_actual_insert(cur2 start, uchar* text, s32 len) {
    i32 x = start.x, y = start.y;

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

    // honestly this routine is probably pretty slow, i've been hesitant to
    // fuck with it because it's been reliable but some good perf gains to be
    // had here, especially with large undos etc
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

        bctree.set(y, bctree.get(y) + bc);
    }
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

        auto start = new_cur2(buf->idx_byte_to_cp(pos.row, pos.column, true), pos.row);
        auto end = buf->end_pos();

        cur2 junk;
        auto uchars = buf->get_uchars(start, end, _countof(buf->tsinput_buffer), &junk);

        u32 n = 0;
        For (uchars) {
            auto size = uchar_size(it);
            if (n + size + 1 > _countof(buf->tsinput_buffer))
                break;
            n += uchar_to_cstr(it, &buf->tsinput_buffer[n]);
        }

        *read = n;
        buf->tsinput_buffer[n] = '\0';
        return buf->tsinput_buffer;
    };

    tree = ts_parser_parse(parser, tree, input);
    tree_dirty = true;
    tree_version++;
}

void Buffer::internal_update_mark_tree(TSInputEdit *edit) {
    cp_assert(!editable_from_main_thread_only || is_main_thread);

    if (!mark_tree.root) return;

    {
        auto start = tspoint_to_cur(edit->start_point);
        auto end = tspoint_to_cur(edit->new_end_point);

        edit_buffer_new.len = 0;
        edit_buffer_new.concat(get_uchars(start, end));
    }

    auto a = new_dstr(&edit_buffer_old);
    auto b = new_dstr(&edit_buffer_new);

    auto start = tspoint_to_cur(edit->start_point);
    auto oldend = tspoint_to_cur(edit->old_end_point);
    auto newend = tspoint_to_cur(edit->new_end_point);

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
    ret->old_text.init(LIST_CHUNK, CHUNK0);
    ret->new_text.init(LIST_CHUNK, CHUNK0);
    return ret;
}

void Buffer::hist_free(int i) {
    auto change = history[i];
    while (change) {
        auto next = change->next;

        change->old_text.cleanup();
        change->new_text.cleanup();
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

int Buffer::internal_distance_between(cur2 a, cur2 b) {
    cp_assert(!editable_from_main_thread_only || is_main_thread);

    if (a.y == b.y) return b.x - a.x;

    int total = 0;
    for (int y = a.y+1; y <= b.y-1; y++)
        total += lines[y].len;
    return total + (lines[a.y].len - a.x + 1) + b.x;
}

void Buffer::internal_commit_insert_to_history(cur2 start, cur2 end) {
    Change *change = NULL;

    auto c = hist_get_latest_change_for_append();
    if (c) {
        if (start == c->new_end || hist_batch_mode)
            change = c;

        if (start == c->new_end) {
            if (c->new_text.len + internal_distance_between(start, end) < CHUNKMAX) {
                c->new_end = end;
                change->new_text.concat(get_uchars(start, end));
                return;
            }
        }
    }

    while (start < end) {
        if (change) {
            change->next = hist_alloc();
            change = change->next;
        } else {
            change = hist_push();
        }

        change->start = start;
        change->old_end = start;

        cur2 real_end;
        auto uchars = get_uchars(start, end, CHUNKMAX - change->new_text.len, &real_end);
        change->new_text.concat(uchars);
        change->new_end = real_end;
        start = real_end;
    }
}

void Buffer::internal_commit_remove_to_history(cur2 start, cur2 end) {
    Change *change = NULL;

    auto c = hist_get_latest_change_for_append();
    if (c) {
        if (c->new_end == end || hist_batch_mode)
            change = c;

        // Up here in the next block is where we do various forms of
        // "surgery" on the existing change with stupid math. If we don't
        // find a reason to break out in this block here, below we will
        // only be adding on to change->next.

        if (c->new_end == end) {
            if (c->start <= start) {
                c->new_end = start;
                c->new_text.len -= internal_distance_between(start, end);
                return;
            }

            // handle the "backspace repeatedly" optimization
            if (internal_distance_between(start, end) + c->old_text.len < CHUNKMAX) {
                // we have an existing change and we're deleting past the start,
                // this means we're completing wiping out any text we've written
                auto new_end = c->start;

                c->new_text.len = 0;
                c->start = start;
                c->new_end = start;

                // basically, prepend to old_text
                auto tmp = new_list(uchar);
                tmp->concat(&c->old_text);
                c->old_text.len = 0;
                c->old_text.concat(get_uchars(start, new_end));
                c->old_text.concat(tmp);
                return;
            }
        }
    }

    auto old_start = start;

    while (start < end) {
        // grab our next change...
        // if change != NULL, i.e. we're tacking on, then alloc and set its next
        // otherwise, create a new entry
        if (change) {
            change->next = hist_alloc();
            change = change->next;
        } else {
            change = hist_push();
        }

        change->start = old_start;
        change->new_end = old_start;
        change->old_text.len = 0;

        cur2 real_end;
        auto uchars = get_uchars(start, end, CHUNKMAX - change->old_text.len, &real_end);
        change->old_text.concat(uchars);

        // take old_start + (real_end - start)
        auto adjusted_end = old_start;
        if (real_end.y != start.y) {
            adjusted_end.x = real_end.x;
            adjusted_end.y += (real_end.y - start.y);
        } else {
            adjusted_end.x += (real_end.x - start.x);
        }
        change->old_end = adjusted_end;

        start = real_end;
    }
}

void Buffer::internal_do_actual_remove(cur2 start, cur2 end) {
    if (start == end) return;

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
        internal_delete_lines(y1 + 1, min(lines.len, y2 + 1));
    }
    bctree.set(y1, get_bytecount(&lines[y1]));
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

cur2 Buffer::inc_gr(cur2 c) {
    auto it = iter(c);
    if (!it.eof())
        it.gr_next();
    return it.pos;
}

cur2 Buffer::dec_gr(cur2 c) {
    auto it = iter(c);
    if (!it.bof())
        it.gr_prev();
    return it.pos;
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
    i32 ret = bctree.sum(c.y);
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

cur2 Buffer::offset_to_cur(i32 off, bool *overflow) {
    if (overflow) *overflow = false;

    int rem = 0;
    int y = bctree.offset_to_line(off, &rem);

    if (y < bctree.size())
        return new_cur2(idx_byte_to_cp(y, rem), y);

    if (overflow) *overflow = true;
    return end_pos();
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

Mark_Node *Mark_Tree::successor(Mark_Node *node) {
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
    if (it->pos < start) it = successor(it);

    Mark *orphan_marks = NULL;

    check_tree_integrity();

    while (it && it->pos < old_end) {
        if (it->pos < new_end) {
            it = successor(it);
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
                it = successor(it);
        }
    }

    check_tree_integrity();

    for (; it; it = successor(it)) {
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
#ifdef DEBUG_BUILD
    check_ordering();
    check_mark_cycle(root);
    check_duplicate_marks();
#endif
}

void Mark_Tree::check_ordering() {
    auto min = root;
    if (!min) return;

    while (min->left)
        min = min->left;

    // o(n). we need this for now, don't have this in final ver.
    cur2 last = {-1, -1};
    for (auto it = min; it; it = successor(it)) {
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
// treap_marker

int treap_node_size(Treap *t) { return t ? t->size : 0; }
int treap_node_sum(Treap *t) { return t ? t->sum : 0; }

void treap_node_update_stats(Treap *t) {
    if (!t) return;

    t->size = 1 + treap_node_size(t->left) + treap_node_size(t->right);
    t->sum = t->val + treap_node_sum(t->left) + treap_node_sum(t->right);
}

Treap_Split *treap_split(Treap *t, int idx, int add) {
    auto ret = new_object(Treap_Split);

    if (!t) {
        ret->left = NULL;
        ret->right = NULL;
        return ret;
    }

    int curr = add + treap_node_size(t->left);
    if (idx <= curr) {
        auto split = treap_split(t->left, idx, add);
        ret->left = split->left;
        t->left = split->right;
        ret->right = t;
    } else {
        auto split = treap_split(t->right, idx, curr + 1);
        t->right = split->left;
        ret->right = split->right;
        ret->left = t;
    }

    treap_node_update_stats(t);
    return ret;
}

Treap *treap_merge(Treap *l, Treap *r) {
    Treap *ret = NULL;

    if (!l || !r) {
        ret = l ? l : r;
    } else if (l->priority > r->priority) {
        l->right = treap_merge(l->right, r);
        ret = l;
    } else {
        r->left = treap_merge(l, r->left);
        ret = r;
    }

    treap_node_update_stats(ret);
    return ret;
}

Treap *treap_insert_node(Treap *t, int idx, Treap *node) {
    auto split = treap_split(t, idx);
    return treap_merge(treap_merge(split->left, node), split->right);
}

void treap_free(Treap *t) {
    if (!t) return;

    treap_free(t->left);
    treap_free(t->right);
    world.treap_fridge.free(t);
}

Treap *treap_delete(Treap *t, int idx, int add) {
    int curr = add + treap_node_size(t->left);
    if (idx == curr) {
        auto ret = treap_merge(t->left, t->right);
        world.treap_fridge.free(t);
        return ret;
    }

    if (idx > curr)
        t->right = treap_delete(t->right, idx, curr + 1);
    else
        t->left = treap_delete(t->left, idx, add);

    treap_node_update_stats(t);
    return t;
}

int treap_get(Treap *t, int idx, int add) {
    if (!t) return -1;

    int curr = add + treap_node_size(t->left);
    if (idx == curr) return t->val;

    if (idx < curr)
        return treap_get(t->left, idx, add);
    return treap_get(t->right, idx, curr + 1);
}

bool treap_set(Treap *t, int idx, int val, int add) {
    if (!t) return false;

    int curr = add + treap_node_size(t->left);
    if (idx == curr) {
        t->val = val;
        return true;
    }

    if (idx < curr)
        return treap_set(t->left, idx, val, add);
    return treap_set(t->right, idx, val, curr + 1);
}
