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

// actually, should we just build in a way where this doesn't need to be known
enum Mark_Type {
    MARK_BUILD_ERROR,
    MARK_SEARCH_RESULT,
    MARK_HISTORY,
    MARK_TEST,
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
    Mark_Node *succ(Mark_Node *node);
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

struct Buffer {
    Pool *mem;

    List<Line> lines;
    List<u32> bytecounts;
    Mark_Tree mark_tree;

    bool initialized;
    bool dirty;
    bool use_tree;
    TSTree *tree;
    TSTreeCursor cursor;
    bool tree_dirty;

    TSParser *parser;
    char tsinput_buffer[128];
    TSInputEdit tsedit;
    List<uchar> edit_buffer_old;
    List<uchar> edit_buffer_new;

    void copy_from(Buffer *other);
    void init(Pool *_mem, bool use_tree);
    void cleanup();
    bool read(Buffer_Read_Func f);
    bool read_data(char *data, int len);
    bool read(File_Mapping* fm);
    void write(File* f);
    void clear();
    uchar* alloc_temp_array(s32 size);
    void free_temp_array(uchar* buf, s32 size);
    void enable_tree();
    void update_tree();

    void internal_append_line(uchar* text, s32 len);
    void internal_delete_lines(u32 y1, u32 y2);
    void internal_insert_line(u32 y, uchar* text, s32 len);
    void internal_start_edit(cur2 start, cur2 end);
    void internal_finish_edit(cur2 new_end);
    void internal_update_mark_tree();

    void insert(cur2 start, uchar* text, s32 len);
    void remove(cur2 start, cur2 end);

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
    u32 idx_byte_to_cp(int y, int off, bool nocrash = false);

    ccstr get_text(cur2 start, cur2 end);
};

s32 uchar_size(uchar c);
