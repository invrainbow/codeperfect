#pragma once

#include "common.hpp"
#include "list.hpp"
#include "mem.hpp"
#include "os.hpp"
#include "tree_sitter_crap.hpp"

typedef List<uchar> *Grapheme;

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
    bool bof() { return !x && !y; }
    bool bol() { return !x; }
    bool eol() { return peek() == '\n' || eof(); }
    uchar get(cur2 _pos);

    Grapheme gr_peek(cur2 *end = NULL);
    Grapheme gr_next();
    Grapheme gr_prev();
};

typedef List<uchar> Line;

struct Cstr_To_Ustr {
    u8 buf[3];
    s32 buflen;
    s32 len;
    uchar uch;

    void init();
    s32 get_uchar_size(u8 first_char);
    void count(u8 ch);
    bool feed(u8 ch);

    // TODO: should write this, but i'm lazy and we need to ship
    // void read(u8 *chars, int len, uchar *buf, int buflen);
};

List<uchar>* cstr_to_ustr(ccstr s, int len = -1);

s32 uchar_to_cstr(uchar c, cstr out);
char* uchar_to_cstr(uchar c);

typedef fn<bool(char*)> Buffer_Read_Func;

// actually, should we just build in a way where this doesn't need to be known
enum Mark_Type {
    MARK_BUILD_ERROR,
    MARK_SEARCH_RESULT,
    MARK_HISTORY,
    MARK_TEST,
    MARK_VIM_MARK,
};

struct Mark_Tree;
struct Mark_Node;

struct Mark {
    Mark_Type type;
    Mark_Tree *tree;
    Mark_Node *node;
    Mark *next;
    bool valid;

    cur2 pos();
    void cleanup();
};

bool is_mark_valid(Mark *mark);

struct Mark_Node {
    // data assoc'd with each node
    cur2 pos;
    Mark *marks; // linked list of mark pointers

    // internal
    Mark_Node *left;
    Mark_Node *right;
    Mark_Node *parent;
    bool isleft;
    int height;
};

struct Buffer;

// where do i use this?
// c-o/i
// build errors
// search results

// AVL tree for keeping track of marks.
struct Mark_Tree {
    Mark_Node *root;
    Buffer *buf;

    struct Edit {
        cur2 start;
        cur2 old_end;
        cur2 new_end;
    };

    void init(Buffer *_buf) {
        ptr0(this);
        buf = _buf;
    }

    void cleanup();

    // public api
    void insert_mark(Mark_Type type, cur2 pos, Mark *out);
    void delete_mark(Mark *mark);
    void apply_edit(cur2 start, cur2 old_end, cur2 new_end);

    // internal shit
    Mark_Node *find_node(Mark_Node *root, cur2 pos);
    Mark_Node *insert_node(cur2 pos);
    void delete_node(cur2 pos);
    Mark_Node *successor(Mark_Node *node);
    int get_height(Mark_Node *root);
    int get_balance(Mark_Node *root);
    void recalc_height(Mark_Node *root);
    Mark_Node* rotate_right(Mark_Node *root);
    Mark_Node* rotate_left(Mark_Node *root);
    Mark_Node *internal_insert_node(Mark_Node *root, cur2 pos, Mark_Node *node);
    Mark_Node *internal_delete_node(Mark_Node *root, cur2 pos);

    void check_ordering();
    void check_mark_cycle(Mark_Node *root);
    void check_duplicate_marks();
    void check_tree_integrity();
    void check_duplicate_marks_helper(Mark_Node *root, List<Mark*> *seen);
};

// to apply, remove start to old_end & insert what's in new_text
// to undo, remove start to new_end & insert what's in old_text
struct Change {
    cur2 start;
    cur2 old_end;
    cur2 new_end;
    // uchar _old_text[64];
    // uchar _new_text[64];
    List<uchar> old_text;
    List<uchar> new_text;
    Change *next;
};

struct Treap {
    int val; // Bytecount of given line that node represents
    int size; // Size of subtree
    int sum; // Total bytecount of whole subtree
    int priority; // Unimportant random value that treaps need to work
    Treap *left;
    Treap *right;
};

struct Treap_Split {
    Treap *left;
    Treap *right;
};

int treap_node_size(Treap *t);
int treap_node_sum(Treap *t);
void treap_node_update_stats(Treap *t);

Treap_Split *treap_split(Treap *t, int idx, int add = 0);
Treap *treap_merge(Treap *l, Treap *r);
Treap *treap_insert_node(Treap *t, int idx, Treap *node);
void treap_free(Treap *t);
Treap *treap_delete(Treap *t, int idx, int add = 0);
int treap_get(Treap *t, int idx, int add = 0);
bool treap_set(Treap *t, int idx, int val, int add = 0);

struct Bytecounts_Tree {
    Treap *root;

    void init() { ptr0(this); }

    int size() { return treap_node_size(root); }
    void append(int val) { insert(treap_node_size(root), val); }
    void remove(int index) { root = treap_delete(root, index); }
    void set(int idx, int val) { treap_set(root, idx, val); }
    int get(int idx) { return treap_get(root, idx); }

    int sum(int until);
    int internal_offset_to_line(Treap *t, int limit, int *rem);
    void insert(int idx, int val);

    int offset_to_line(int limit, int *rem) {
        *rem = 0;
        return internal_offset_to_line(root, limit, rem);
    }
};

struct Buffer {
    Pool *mem;

    List<Line> lines;
    Bytecounts_Tree bctree;
    Mark_Tree mark_tree;

    bool initialized;
    bool dirty;
    TSTree *tree;
    int tree_version;
    int buf_version;
    TSTreeCursor cursor;
    bool tree_dirty;
    int lang;

    TSParser *parser;
    char tsinput_buffer[1024];
    TSInputEdit tsedit;
    List<uchar> edit_buffer_old;
    List<uchar> edit_buffer_new;
    bool editable_from_main_thread_only;

    bool tree_batch_mode;
    int tree_batch_refs;
    List<TSInputEdit> tree_batch_edits;

    void tree_batch_start();
    void tree_batch_end();

    // TODO: if we have any more ring buffers, consider refactor

    // honestly, should we just pull this out into a Buffer_History class
    // so we don't need to keep writing `hist_` everywhere...
    bool use_history;
    Change* history[256];
    int hist_start;
    int hist_top;
    int hist_curr;

    bool hist_batch_mode;
    bool hist_force_push_next_change;
    int hist_batch_refs;

    void hist_batch_start() {
        hist_force_push_next_change = true;
        hist_batch_mode = true;
        hist_batch_refs++;
    }

    void hist_batch_end() {
        hist_batch_refs--;
        if (hist_batch_refs == 0) {
            hist_batch_mode = false;
            hist_force_push_next_change = true;
        }
    }

    int hist_inc(int i) { return i == _countof(history) - 1 ? 0 : i + 1; }
    int hist_dec(int i) { return !i ? _countof(history) - 1 : i - 1; }
    Change* hist_alloc();
    void hist_free(int i);
    Change* hist_push();
    Change* hist_get_latest_change_for_append();
    cur2 hist_undo();
    cur2 hist_redo();
    void hist_apply_change(Change *change, bool undo);

    void init(Pool *_mem, int _lang, bool use_history);
    void cleanup();
    void read(char *data, int len);
    void read(File_Mapping* fm);
    void write(File* f);
    void clear();
    uchar* alloc_temp_array(s32 size);
    void free_temp_array(uchar* buf, s32 size);
    void enable_tree(int _lang);
    void update_tree();

    void internal_append_line(uchar* text, s32 len);
    void internal_delete_lines(u32 y1, u32 y2);
    void internal_insert_line(u32 y, uchar* text, s32 len);
    void internal_start_edit(cur2 start, cur2 end);
    void internal_finish_edit(cur2 new_end);
    void internal_update_mark_tree();
    int internal_distance_between(cur2 a, cur2 b);

    cur2 insert(cur2 start, uchar* text, s32 len, bool applying_change = false);
    void remove(cur2 start, cur2 end, bool applying_change = false);
    void remove_lines(u32 y1, u32 y2);

    cur2 insert(cur2 start, uchar uch) { return insert(start, &uch, 1); }

    void internal_commit_remove_to_history(cur2 start, cur2 end);
    void internal_commit_insert_to_history(cur2 start, cur2 end);

    Buffer_It iter(cur2 c);
    cur2 inc_cur(cur2 c);
    cur2 dec_cur(cur2 c);
    i32 cur_to_offset(cur2 c);
    cur2 offset_to_cur(i32 off, bool *overflow = NULL);
    cur2 inc_gr(cur2 c);
    cur2 dec_gr(cur2 c);

    // this is so stupid lmao
    u32 idx_byte_to_gr(int y, int off);
    u32 idx_gr_to_cp(int y, int off);
    u32 idx_cp_to_byte(int y, int off);
    u32 idx_gr_to_byte(int y, int off) { return idx_cp_to_byte(y, idx_gr_to_cp(y, off)); }
    u32 idx_cp_to_gr(int y, int off) { return idx_byte_to_gr(y, idx_cp_to_byte(y, off)); }
    u32 idx_byte_to_cp(int y, int off, bool nocrash = false);

    // ???
    u32 internal_convert_x_vx(int y, int off, bool to_vx);
    u32 idx_vcp_to_cp(int y, int off) { return internal_convert_x_vx(y, off, false); }
    u32 idx_cp_to_vcp(int y, int off) { return internal_convert_x_vx(y, off, true); }

    ccstr get_text(cur2 start, cur2 end);
    List<uchar>* get_uchars(cur2 start, cur2 end, int limit = -1, cur2 *actual_end = NULL);

    bool is_valid(cur2 c);
    cur2 fix_cur(cur2 c);

    cur2 end_pos() {
        int y = lines.len-1;
        return new_cur2(lines[y].len, y);
    }
};

struct Scoped_Batch_Change {
    Buffer *buf;
    Scoped_Batch_Change(Buffer *_buf) {
        buf = _buf;
        buf->hist_batch_start();
        buf->tree_batch_start();
    }
    ~Scoped_Batch_Change() {
        buf->tree_batch_end();
        buf->hist_batch_end();
    }
};

#define SCOPED_BATCH_CHANGE(buf) Scoped_Batch_Change GENSYM(SCOPED_BATCH_CHANGE)(buf)

s32 uchar_size(uchar c);
