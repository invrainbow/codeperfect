#include "buffer.hpp"
#include "common.hpp"
#include "world.hpp"
#include "unicode.hpp"
#include "diff.hpp"

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

ccstr Buffer::get_text(cur2 start, cur2 end) {
    auto ret = alloc_list<char>();
    char tmp[4];

    for (auto it = iter(start); it.pos < end; it.next())
        for (int i = 0, n = uchar_to_cstr(it.peek(), tmp); i < n; i++)
            ret->append(tmp[i]);

    ret->append('\0');
    return ret->items;
}

void Buffer::init(Pool *_mem, bool _use_tree) {
    ptr0(this);

    mem = _mem;

    {
        SCOPED_MEM(mem);
        lines.init(LIST_POOL, 128);
        bytecounts.init(LIST_POOL, 128);
        edit_buffer_old.init();
        edit_buffer_new.init();
    }

    initialized = true;
    dirty = false;

    if (_use_tree)
        enable_tree();

    mark_tree.init(this);
}

void Buffer::enable_tree() {
    if (use_tree) return; // already enabled

    use_tree = true;
    parser = new_ts_parser();
    update_tree();
}

void Buffer::cleanup() {
    if (!initialized) return;

    clear();
    if (parser != NULL)
        ts_parser_delete(parser);
    if (tree != NULL)
        ts_tree_delete(tree);

    mark_tree.cleanup();

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

bool Buffer::read(Buffer_Read_Func f) {
    // Expects buf to be empty.

    char ch;
    Cstr_To_Ustr conv;
    bool found;

    clear();

    Line *line = NULL;
    u32 *bc = NULL;

    auto insert_new_line = [&]() -> bool {
        line = lines.append();
        bc = bytecounts.append();

        // make this check more robust/at the right place?
        if (line == NULL) return false;

        line->init(LIST_CHUNK, CHUNK0);
        *bc = 0;
        return true;
    };

    if (!insert_new_line()) return false;

    conv.init();
    while (f(&ch)) {
        uchar uch = conv.feed(ch, &found);
        (*bc)++;

        if (found) {
            if (uch == '\n') { // TODO: handle \r
                if (!insert_new_line())
                    return false;
            } else {
                if (!line->append(uch))
                    return false;
            }
        }
    }

    (*bytecounts.last())++;
    dirty = false;

    if (use_tree) {
        if (tree != NULL) {
            ts_tree_delete(tree);
            tree = NULL;
        }
        update_tree();
    }
}

bool Buffer::read_data(char *data, int len) {
    if (len == -1) len = strlen(data);

    int i = 0;
    return read([&](char* out) {
        if (i < len) {
            *out = data[i++];
            return true;
        }
        return false;
    });
}

bool Buffer::read(File_Mapping *fm) {
    return read_data((char*)fm->data, fm->len);
}

void Buffer::write(File *f) {
    auto write_char = [&](uchar ch) {
        char buf[4];
        auto count = uchar_to_cstr(ch, buf);
        f->write(buf, count);
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

void Buffer::internal_delete_lines(u32 y1, u32 y2) {
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
    For (*line) bc += uchar_size(it);
    return bc + 1;
}

void Buffer::internal_insert_line(u32 y, uchar* text, s32 len) {
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
    internal_insert_line(lines.len, text, len);
    dirty = true;
}

void Buffer::insert(cur2 start, uchar* text, s32 len) {
    i32 x = start.x, y = start.y;

    internal_start_edit(start, start);

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

        if (x > 0)
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

    internal_finish_edit(end);
    dirty = true;
}

void Buffer::update_tree() {
    TSInput input;
    input.payload = this;
    input.encoding = TSInputEncodingUTF8;

    input.read = [](void *p, uint32_t off, TSPoint pos, uint32_t *read) -> const char* {
        auto buf = (Buffer*)p;
        auto it = buf->iter(buf->offset_to_cur(off));
        u32 n = 0;

        while (!it.eof()) {
            auto uch = it.next();
            if (uch == 0) break;

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
}

void Buffer::internal_start_edit(cur2 start, cur2 end) {
    // we use the tsedit for other things, right now for mark tree calculations.
    // so create it even if !use_tree, it's not only used for the tree.
    ptr0(&tsedit);
    tsedit.start_byte = cur_to_offset(start);
    tsedit.start_point = cur_to_tspoint(start);
    tsedit.old_end_byte = cur_to_offset(end);
    tsedit.old_end_point = cur_to_tspoint(end);

    edit_buffer_old.len = 0;
    edit_buffer_new.len = 0;

    auto it = iter(start);
    while (it.pos < end)
        edit_buffer_old.append(it.next());
}

void Buffer::internal_finish_edit(cur2 new_end) {
    tsedit.new_end_byte = cur_to_offset(new_end);
    tsedit.new_end_point = cur_to_tspoint(new_end);

    if (use_tree) {
        ts_tree_edit(tree, &tsedit);
        update_tree();
    }

    internal_update_mark_tree();
}

void Buffer::internal_update_mark_tree() {
    if (mark_tree.root == NULL) return;

    {
        auto start = tspoint_to_cur(tsedit.start_point);
        auto end = tspoint_to_cur(tsedit.new_end_point);

        auto it = iter(start);
        while (it.pos < end)
            edit_buffer_new.append(it.next());
    }

    auto a = new_dstr(&edit_buffer_old);
    auto b = new_dstr(&edit_buffer_new);

    auto start = tspoint_to_cur(tsedit.start_point);
    auto oldend = tspoint_to_cur(tsedit.old_end_point);
    auto newend = tspoint_to_cur(tsedit.new_end_point);

    auto diffs = diff_main(a, b);
    if (diffs == NULL) {
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

    For (*diffs) {
        switch (it.type) {
        case DIFF_DELETE:
            mark_tree.apply_edit(cur, advance_cur(cur, it.s), cur);
            break;
        case DIFF_INSERT:
            {
                auto end = advance_cur(cur, it.s);
                mark_tree.apply_edit(cur, cur, end);
                cur = end;
            }
            break;
        case DIFF_SAME:
            cur = advance_cur(cur, it.s);
            break;
        }
    }
}

void Buffer::remove(cur2 start, cur2 end) {
    i32 x1 = start.x, y1 = start.y;
    i32 x2 = end.x, y2 = end.y;

    internal_start_edit(start, end);

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

    for (int i = 0; i < off; i++)
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

    assert(off == 0);
    return x;
}

u32 Buffer::idx_byte_to_cp(int y, int off, bool nocrash) {
    auto &line = lines[y];
    for (u32 x = 0; x < line.len; x++) {
        auto size = uchar_size(line[x]);
        if (off < size) return x;
        off -= size;
    }

    if (!nocrash) assert(off == 0);
    return lines[y].len;
}

cur2 Buffer::offset_to_cur(i32 off) {
    cur2 ret;
    ret.x = -1;
    ret.y = -1;

    for (u32 y = 0; y < bytecounts.len; y++) {
        auto& it = bytecounts[y];
        if (off < it) {
            ret.y = y;
            ret.x = idx_byte_to_cp(y, off);
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

// ============
// mark_tree_marker

void cleanup_mark_node(Mark_Node *node) {
    if (node == NULL) return;

    cleanup_mark_node(node->left);
    cleanup_mark_node(node->right);

    for (auto it = node->marks; it != NULL; it = it->next) {
        if (it == it->next)
            our_panic("infinite loop");
        it->valid = false;
    }

    world.mark_node_fridge.free(node);
}

void Mark_Tree::cleanup() {
    cleanup_mark_node(root);
    mem.cleanup();
}

int Mark_Tree::get_height(Mark_Node *root) {
    return root == NULL ? 0 : root->height;
}

int Mark_Tree::get_balance(Mark_Node *root) {
    return root == NULL ? 0 : (get_height(root->left) - get_height(root->right));
}

Mark_Node *Mark_Tree::succ(Mark_Node *node) {
    if (node->right != NULL) {
        auto ret = node->right;
        while (ret->left != NULL)
            ret = ret->left;
        return ret;
    }

    for (; node->parent != NULL; node = node->parent)
        if (node->isleft)
            return node->parent;
    return NULL;
}

Mark_Node *Mark_Tree::find_node(Mark_Node *root, cur2 pos) {
    if (root == NULL) return NULL;
    if (root->pos == pos) return root;

    auto child = pos < root->pos ? root->left : root->right;
    if (child == NULL)
        return root;
    return find_node(child, pos);
}

Mark_Node *Mark_Tree::insert_node(cur2 pos) {
    auto node = find_node(root, pos);
    if (node != NULL && node->pos == pos)
        return node;

    node = world.mark_node_fridge.alloc();
    node->pos = pos;
    root = internal_insert_node(root, pos, node);

    check_tree_integrity();

    return node;
}

void Mark_Tree::insert_mark(Mark_Type type, cur2 pos, Mark *mark) {
    mark->type = type;
    mark->tree = this;
    mark->valid = true;

    check_tree_integrity();

    auto node = insert_node(pos);
    mark->node = node;
    mark->next = node->marks;
    if (mark->next == mark) our_panic("infinite loop");
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
    if (root->left != NULL) {
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
    if (root->right != NULL) {
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
    if (root == NULL) {
        recalc_height(node);
        return node;
    }

    if (root->pos == pos) our_panic("trying to insert duplicate pos");

    if (pos < root->pos) {
        auto old = root->left;
        root->left = internal_insert_node(root->left, pos, node);
        if (old == NULL) {
            node->parent = root;
            node->isleft = true;
        }
    } else {
        auto old = root->right;
        root->right = internal_insert_node(root->right, pos, node);
        if (old == NULL) {
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
    auto node = mark->node;
    bool found = false;

    check_tree_integrity();

    // remove `mark` from `node->marks`
    Mark *last = NULL;
    for (auto it = node->marks; it != NULL; it = it->next) {
        if (it == mark) {
            found = true;
            if (last == NULL)
               node->marks = mark->next;
            else {
               last->next = mark->next;
               if (last->next == last) our_panic("infinite loop");
            }
            break;
        }
        last = it;
    }

    check_tree_integrity();

    if (!found)
        our_panic("trying to delete mark not found in its node");

    if (node->marks == NULL)
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
    if (root == NULL) return root;

    if (pos < root->pos)
        root->left = internal_delete_node(root->left, pos);
    else if (pos > root->pos)
        root->right = internal_delete_node(root->right, pos);
    else {
        if (root->marks != NULL)
            our_panic("trying to delete a node that still has marks");

        if (root->left == NULL || root->right == NULL) {
            Mark_Node *ret = NULL;
            bool isleft = false;

            if (root->left == NULL) ret = root->right;
            if (root->right == NULL) ret = root->left;

            if (ret != NULL) {
                ret->parent = root->parent;
                ret->isleft = root->isleft;
            }

            world.mark_node_fridge.free(root);
            return ret;
        }

        auto min = root->right;
        while (min->left != NULL)
            min = min->left;

        root->marks = min->marks;
        for (auto it = root->marks; it != NULL; it = it->next)
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
    auto e = edits.append();
    e->start = start;
    e->old_end = old_end;
    e->new_end = new_end;

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
    if (nstart == NULL) return;

    auto it = nstart;
    if (it->pos < start) it = succ(it);

    Mark *orphan_marks = NULL;

    check_tree_integrity();

    while (it != NULL && it->pos < old_end) {
        if (it->pos < new_end) {
            it = succ(it);
        } else {
            for (auto mark = it->marks; mark != NULL; mark = mark->next)
                if (mark == mark->next)
                    our_panic("infinite loop");

            Mark *next = NULL;

            if (it->marks == orphan_marks)
                our_panic("why is this happening?");

            int i = 0;
            for (auto mark = it->marks; mark != NULL; mark = next) {
                if (orphan_marks == mark)
                    our_panic("why is this happening?");

                mark->node = NULL;
                next = mark->next;

                mark->next = orphan_marks;
                if (mark->next == mark) our_panic("infinite loop");

                orphan_marks = mark;
                i++;
            }
            it->marks = NULL;

            auto curr = it->pos;
            delete_node(curr);

            // after deleting the node, go to the next node
            it = find_node(root, curr);
            if (it != NULL && it->pos < curr)
                it = succ(it);
        }
    }

    check_tree_integrity();

    for (; it != NULL; it = succ(it)) {
        if (it->pos.y == old_end.y) {
            it->pos.y = new_end.y;
            it->pos.x = it->pos.x + new_end.x - old_end.x;
        } else {
            it->pos.y = it->pos.y + new_end.y - old_end.y;
        }
    }

    check_tree_integrity();

    if (orphan_marks != NULL) {
        auto nend = insert_node(new_end);
        Mark *next = NULL;
        for (auto mark = orphan_marks; mark != NULL; mark = next) {
            next = mark->next;
            mark->node = nend;
            mark->next = nend->marks;
            if (mark->next == mark) our_panic("infinite loop");
            nend->marks = mark;
        }
    }

    check_tree_integrity();
}

void Mark_Tree::check_mark_cycle(Mark_Node *root) {
    if (root == NULL) return;

    for (auto mark = root->marks; mark != NULL; mark = mark->next)
        if (mark == mark->next)
            our_panic("mark cycle detected");

    check_mark_cycle(root->left);
    check_mark_cycle(root->right);
}

void Mark_Tree::check_duplicate_marks_helper(Mark_Node *root, List<Mark*> *seen) {
    if (root == NULL) return;

    check_duplicate_marks_helper(root->left, seen);
    check_duplicate_marks_helper(root->right, seen);

    for (auto m = root->marks; m != NULL; m = m->next) {
        if (seen->find([&](auto it) { return *it == m; }) != NULL)
            our_panic("duplicate mark");
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
    if (min == NULL) return;

    while (min->left != NULL)
        min = min->left;

    // o(n). we need this for now, don't have this in final ver.
    cur2 last = {-1, -1};
    for (auto it = min; it != NULL; it = succ(it)) {
        if (last.x != -1) {
            if (last >= it->pos) {
                our_panic("out of order (or duplicate)");
            }
        }
        last = it->pos;
    }
}

cur2 Mark::pos() { return node->pos; }
void Mark::cleanup() { if (valid) tree->delete_mark(this); }

bool is_mark_valid(Mark *mark) {
    return mark != NULL && mark->valid;
}

// ============
// mark_tree_marker
