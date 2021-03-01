#pragma once

#include "common.hpp"
#include "buffer.hpp"
#include "utils.hpp"
#include "mem.hpp"
#include "os.hpp"
#include "uthash.h"
#include "tree_sitter_crap.hpp"

#undef uthash_malloc
#undef uthash_free
#define uthash_malloc(sz) alloc_memory(sz)
#define uthash_free(ptr, sz)

extern "C" TSLanguage *tree_sitter_go();

// mirrors tree-sitter/src/go.h
enum Ts_Field_Type {
    TSF_ALIAS = 1,
    TSF_ALTERNATIVE = 2,
    TSF_ARGUMENTS = 3,
    TSF_BODY = 4,
    TSF_CAPACITY = 5,
    TSF_CHANNEL = 6,
    TSF_COMMUNICATION = 7,
    TSF_CONDITION = 8,
    TSF_CONSEQUENCE = 9,
    TSF_ELEMENT = 10,
    TSF_END = 11,
    TSF_FIELD = 12,
    TSF_FUNCTION = 13,
    TSF_INDEX = 14,
    TSF_INITIALIZER = 15,
    TSF_KEY = 16,
    TSF_LABEL = 17,
    TSF_LEFT = 18,
    TSF_LENGTH = 19,
    TSF_NAME = 20,
    TSF_OPERAND = 21,
    TSF_OPERATOR = 22,
    TSF_PACKAGE = 23,
    TSF_PARAMETERS = 24,
    TSF_PATH = 25,
    TSF_RECEIVER = 26,
    TSF_RESULT = 27,
    TSF_RIGHT = 28,
    TSF_START = 29,
    TSF_TAG = 30,
    TSF_TYPE = 31,
    TSF_UPDATE = 32,
    TSF_VALUE = 33,
};

// mirrors tree-sitter/src/go.h
enum Ts_Ast_Type {
    TS_ERROR = ((TSSymbol)-1),
    TS_IDENTIFIER = 1,
    TS_LF = 2,
    TS_SEMI = 3,
    TS_PACKAGE = 4,
    TS_IMPORT = 5,
    TS_ANON_DOT = 6,
    TS_BLANK_IDENTIFIER = 7,
    TS_LPAREN = 8,
    TS_RPAREN = 9,
    TS_CONST = 10,
    TS_COMMA = 11,
    TS_EQ = 12,
    TS_VAR = 13,
    TS_FUNC = 14,
    TS_DOT_DOT_DOT = 15,
    TS_TYPE = 16,
    TS_STAR = 17,
    TS_LBRACK = 18,
    TS_RBRACK = 19,
    TS_STRUCT = 20,
    TS_LBRACE = 21,
    TS_RBRACE = 22,
    TS_INTERFACE = 23,
    TS_MAP = 24,
    TS_CHAN = 25,
    TS_LT_DASH = 26,
    TS_COLON_EQ = 27,
    TS_PLUS_PLUS = 28,
    TS_DASH_DASH = 29,
    TS_STAR_EQ = 30,
    TS_SLASH_EQ = 31,
    TS_PERCENT_EQ = 32,
    TS_LT_LT_EQ = 33,
    TS_GT_GT_EQ = 34,
    TS_AMP_EQ = 35,
    TS_AMP_CARET_EQ = 36,
    TS_PLUS_EQ = 37,
    TS_DASH_EQ = 38,
    TS_PIPE_EQ = 39,
    TS_CARET_EQ = 40,
    TS_COLON = 41,
    TS_FALLTHROUGH = 42,
    TS_BREAK = 43,
    TS_CONTINUE = 44,
    TS_GOTO = 45,
    TS_RETURN = 46,
    TS_GO = 47,
    TS_DEFER = 48,
    TS_IF = 49,
    TS_ELSE = 50,
    TS_FOR = 51,
    TS_RANGE = 52,
    TS_SWITCH = 53,
    TS_CASE = 54,
    TS_DEFAULT = 55,
    TS_SELECT = 56,
    TS_NEW = 57,
    TS_MAKE = 58,
    TS_PLUS = 59,
    TS_DASH = 60,
    TS_BANG = 61,
    TS_CARET = 62,
    TS_AMP = 63,
    TS_SLASH = 64,
    TS_PERCENT = 65,
    TS_LT_LT = 66,
    TS_GT_GT = 67,
    TS_AMP_CARET = 68,
    TS_PIPE = 69,
    TS_EQ_EQ = 70,
    TS_BANG_EQ = 71,
    TS_LT = 72,
    TS_LT_EQ = 73,
    TS_GT = 74,
    TS_GT_EQ = 75,
    TS_AMP_AMP = 76,
    TS_PIPE_PIPE = 77,
    TS_RAW_STRING_LITERAL = 78,
    TS_DQUOTE = 79,
    TS_INTERPRETED_STRING_LITERAL_TOKEN1 = 80,
    TS_ESCAPE_SEQUENCE = 81,
    TS_INT_LITERAL = 82,
    TS_FLOAT_LITERAL = 83,
    TS_IMAGINARY_LITERAL = 84,
    TS_RUNE_LITERAL = 85,
    TS_NIL = 86,
    TS_TRUE = 87,
    TS_FALSE = 88,
    TS_COMMENT = 89,
    TS_SOURCE_FILE = 90,
    TS_PACKAGE_CLAUSE = 91,
    TS_IMPORT_DECLARATION = 92,
    TS_IMPORT_SPEC = 93,
    TS_DOT = 94,
    TS_IMPORT_SPEC_LIST = 95,
    TS_DECLARATION = 96,
    TS_CONST_DECLARATION = 97,
    TS_CONST_SPEC = 98,
    TS_VAR_DECLARATION = 99,
    TS_VAR_SPEC = 100,
    TS_FUNCTION_DECLARATION = 101,
    TS_METHOD_DECLARATION = 102,
    TS_PARAMETER_LIST = 103,
    TS_PARAMETER_DECLARATION = 104,
    TS_VARIADIC_PARAMETER_DECLARATION = 105,
    TS_TYPE_ALIAS = 106,
    TS_TYPE_DECLARATION = 107,
    TS_TYPE_SPEC = 108,
    TS_EXPRESSION_LIST = 109,
    TS_PARENTHESIZED_TYPE = 110,
    TS_SIMPLE_TYPE = 111,
    TS_POINTER_TYPE = 112,
    TS_ARRAY_TYPE = 113,
    TS_IMPLICIT_LENGTH_ARRAY_TYPE = 114,
    TS_SLICE_TYPE = 115,
    TS_STRUCT_TYPE = 116,
    TS_FIELD_DECLARATION_LIST = 117,
    TS_FIELD_DECLARATION = 118,
    TS_INTERFACE_TYPE = 119,
    TS_METHOD_SPEC_LIST = 120,
    TS_METHOD_SPEC = 121,
    TS_MAP_TYPE = 122,
    TS_CHANNEL_TYPE = 123,
    TS_FUNCTION_TYPE = 124,
    TS_BLOCK = 125,
    TS_STATEMENT_LIST = 126,
    TS_STATEMENT = 127,
    TS_EMPTY_STATEMENT = 128,
    TS_SIMPLE_STATEMENT = 129,
    TS_SEND_STATEMENT = 130,
    TS_RECEIVE_STATEMENT = 131,
    TS_INC_STATEMENT = 132,
    TS_DEC_STATEMENT = 133,
    TS_ASSIGNMENT_STATEMENT = 134,
    TS_SHORT_VAR_DECLARATION = 135,
    TS_LABELED_STATEMENT = 136,
    TS_EMPTY_LABELED_STATEMENT = 137,
    TS_FALLTHROUGH_STATEMENT = 138,
    TS_BREAK_STATEMENT = 139,
    TS_CONTINUE_STATEMENT = 140,
    TS_GOTO_STATEMENT = 141,
    TS_RETURN_STATEMENT = 142,
    TS_GO_STATEMENT = 143,
    TS_DEFER_STATEMENT = 144,
    TS_IF_STATEMENT = 145,
    TS_FOR_STATEMENT = 146,
    TS_FOR_CLAUSE = 147,
    TS_RANGE_CLAUSE = 148,
    TS_EXPRESSION_SWITCH_STATEMENT = 149,
    TS_EXPRESSION_CASE = 150,
    TS_DEFAULT_CASE = 151,
    TS_TYPE_SWITCH_STATEMENT = 152,
    TS_TYPE_SWITCH_HEADER = 153,
    TS_TYPE_CASE = 154,
    TS_SELECT_STATEMENT = 155,
    TS_COMMUNICATION_CASE = 156,
    TS_EXPRESSION = 157,
    TS_PARENTHESIZED_EXPRESSION = 158,
    TS_CALL_EXPRESSION = 159,
    TS_VARIADIC_ARGUMENT = 160,
    TS_SPECIAL_ARGUMENT_LIST = 161,
    TS_ARGUMENT_LIST = 162,
    TS_SELECTOR_EXPRESSION = 163,
    TS_INDEX_EXPRESSION = 164,
    TS_SLICE_EXPRESSION = 165,
    TS_TYPE_ASSERTION_EXPRESSION = 166,
    TS_TYPE_CONVERSION_EXPRESSION = 167,
    TS_COMPOSITE_LITERAL = 168,
    TS_LITERAL_VALUE = 169,
    TS_KEYED_ELEMENT = 170,
    TS_ELEMENT = 171,
    TS_FUNC_LITERAL = 172,
    TS_UNARY_EXPRESSION = 173,
    TS_BINARY_EXPRESSION = 174,
    TS_QUALIFIED_TYPE = 175,
    TS_INTERPRETED_STRING_LITERAL = 176,
    TS_SOURCE_FILE_REPEAT1 = 177,
    TS_IMPORT_SPEC_LIST_REPEAT1 = 178,
    TS_CONST_DECLARATION_REPEAT1 = 179,
    TS_CONST_SPEC_REPEAT1 = 180,
    TS_VAR_DECLARATION_REPEAT1 = 181,
    TS_PARAMETER_LIST_REPEAT1 = 182,
    TS_TYPE_DECLARATION_REPEAT1 = 183,
    TS_FIELD_NAME_LIST_REPEAT1 = 184,
    TS_EXPRESSION_LIST_REPEAT1 = 185,
    TS_FIELD_DECLARATION_LIST_REPEAT1 = 186,
    TS_METHOD_SPEC_LIST_REPEAT1 = 187,
    TS_STATEMENT_LIST_REPEAT1 = 188,
    TS_EXPRESSION_SWITCH_STATEMENT_REPEAT1 = 189,
    TS_TYPE_SWITCH_STATEMENT_REPEAT1 = 190,
    TS_TYPE_CASE_REPEAT1 = 191,
    TS_SELECT_STATEMENT_REPEAT1 = 192,
    TS_ARGUMENT_LIST_REPEAT1 = 193,
    TS_LITERAL_VALUE_REPEAT1 = 194,
    TS_INTERPRETED_STRING_LITERAL_REPEAT1 = 195,
    TS_FIELD_IDENTIFIER = 196,
    TS_LABEL_NAME = 197,
    TS_PACKAGE_IDENTIFIER = 198,
    TS_TYPE_IDENTIFIER = 199,
};

ccstr ts_field_type_str(Ts_Field_Type type);
ccstr ts_ast_type_str(Ts_Ast_Type type);

struct Index_Stream {
    File f;
    u32 offset;
    ccstr path;
    bool ok;

    File_Result open(ccstr _path, u32 access, File_Open_Mode open_mode);
    bool seek(u32 offset);
    void cleanup();
    bool writen(void* buf, int n);
    bool write1(i8 x);
    bool write2(i16 x);
    bool write4(i32 x);
    bool writestr(ccstr s);
    void readn(void* buf, s32 n);
    char read1();
    i16 read2();
    i32 read4();
    ccstr readstr();
};

enum It_Type {
    IT_INVALID = 0,
    IT_MMAP,
    IT_BUFFER,
};

struct Parser_It {
    It_Type type;

    union {
        struct {
            Entire_File *ef; // TODO: unicode
            cur2 pos;
        } mmap_params;

        struct {
            Buffer_It it;
        } buffer_params;
    };

    void init(Entire_File *ef) {
        ptr0(this);
        type = IT_MMAP;
        mmap_params.ef = ef;
        mmap_params.pos = new_cur2(0, 0);
    }

    void init(Buffer* buf) {
        ptr0(this);
        type = IT_BUFFER;
        buffer_params.it.buf = buf;
    }

    void cleanup() {
        if (type == IT_MMAP) free_entire_file(mmap_params.ef);
    }

    u8 peek() {
        switch (type) {
        case IT_MMAP: return mmap_params.ef->data[mmap_params.pos.x];
        case IT_BUFFER: return (u8)buffer_params.it.peek();
        }
        return 0;
    }

    u8 next() {
        switch (type) {
        case IT_MMAP:
            {
                auto ret = peek();
                mmap_params.pos.x++;
                return ret;
            }
        case IT_BUFFER:
            return (u8)buffer_params.it.next();
        }
        return 0;
    }

    u8 prev() {
        switch (type) {
        case IT_MMAP:
            {
                auto ret = peek();
                mmap_params.pos.x--;
                return ret;
            }
        case IT_BUFFER:
            return (u8)buffer_params.it.prev();
        }
        return 0;
    }

    bool bof() {
        switch (type) {
        case IT_MMAP: return (mmap_params.pos.x == 0);
        case IT_BUFFER: return buffer_params.it.bof();
        }
    }
    
    bool eof() {
        switch (type) {
        case IT_MMAP: return (mmap_params.pos.x == mmap_params.ef->len);
        case IT_BUFFER: return buffer_params.it.eof();
        }
        return false;
    }

    cur2 get_pos() {
        // TODO: to convert between pos types based on this->type
        switch (type) {
        case IT_MMAP:
            return new_cur2(mmap_params.pos.x, -1);
        case IT_BUFFER:
            return buffer_params.it.pos;
        }
        return new_cur2(0, 0);
    }

    void set_pos(cur2 pos) {
        // TODO: to convert between pos types based on this->type
        switch (type) {
        case IT_MMAP:
            mmap_params.pos = pos;
            break;
        case IT_BUFFER:
            buffer_params.it.pos = pos;
            break;
        }
    }
};

struct AC_Result {
    ccstr name;
};

enum Autocomplete_Type {
    AUTOCOMPLETE_NONE = 0,
    AUTOCOMPLETE_STRUCT_FIELDS,
    AUTOCOMPLETE_PACKAGE_EXPORTS,
    AUTOCOMPLETE_IDENTIFIER,
};

struct Autocomplete {
    List<AC_Result>* results;
    Autocomplete_Type type;
    cur2 keyword_start_position;
};

enum Walk_Action {
    WALK_CONTINUE,
    WALK_ABORT,
    WALK_SKIP_CHILDREN,
};

enum Gomod_DirectiveType {
    GOMOD_DIRECTIVE_REQUIRE,
    GOMOD_DIRECTIVE_REPLACE,
};

struct Gomod_Directive {
    Gomod_DirectiveType type;
    ccstr module_path;
    ccstr module_version;

    // for replace
    ccstr replace_path;
    ccstr replace_version;

    Gomod_Directive *copy();
    void read(Index_Stream *s);
    void write(Index_Stream *s);
};

struct Gomod_Info {
    ccstr path;
    bool exists;
    u64 hash;

    ccstr module_path;
    ccstr go_version;
    List<Gomod_Directive> *directives;

    Gomod_Info *copy();
    void read(Index_Stream *s);
    void write(Index_Stream *s);
};

enum Gomod_Tok_Type {
    GOMOD_TOK_ILLEGAL,
    GOMOD_TOK_EOF,

    // comment
    GOMOD_TOK_COMMENT,

    // punctuation
    GOMOD_TOK_LPAREN,
    GOMOD_TOK_RPAREN,
    GOMOD_TOK_ARROW,

    // whitespace
    GOMOD_TOK_NEWLINE,

    // keywords
    GOMOD_TOK_MODULE,
    GOMOD_TOK_GO,
    GOMOD_TOK_REQUIRE,
    GOMOD_TOK_REPLACE,
    GOMOD_TOK_EXCLUDE,

    // strings & identifiers
    GOMOD_TOK_STRIDENT,
};

ccstr gomod_tok_type_str(Gomod_Tok_Type t);

struct Gomod_Token {
    Gomod_Tok_Type type;
    ccstr val;
    bool val_truncated;
};

ccstr _path_join(ccstr a, ...);
#define path_join(...) _path_join(__VA_ARGS__, NULL)

ccstr get_package_name_from_file(ccstr filepath);

/*
enum Import_Location {
    IMPLOC_GOPATH = 0,
    IMPLOC_GOROOT = 1,
    IMPLOC_GOMOD = 2,
    IMPLOC_VENDOR = 3,
};
*/

struct Resolved_Import {
    ccstr path;
    ccstr package_name;
    // Import_Location location_type;
};

enum Index_Event_Type {
    INDEX_EVENT_FETCH_IMPORTS,
    INDEX_EVENT_REINDEX_PACKAGE,
};

struct Index_Event {
    Index_Event_Type type;
    char import_path[MAX_PATH]; // for reindex_package
    u64 time;
};

enum Go_Watch_Type {
    WATCH_WKSP,
    WATCH_GOPATH,
    WATCH_PKGMOD,
    WATCH_GOROOT,
};

TSPoint cur_to_tspoint(cur2 c);
cur2 tspoint_to_cur(TSPoint p);

struct Go_Indexer;

// wrapper around TS_Node, honestly fuck getter patterns
struct Ast_Node {
    TSNode node;

    Go_Indexer *indexer;
    cur2 start;
    cur2 end;
    u32 start_byte;
    u32 end_byte;
    Ts_Ast_Type type;
    bool null;
    ccstr name;
    const void* id;
    bool anon;
    int child_count;

    Ast_Node *dup(TSNode new_node);
    ccstr string();

    void init(TSNode _node) {
        ptr0(this);
        node = _node;

        null = ts_node_is_null(node);
        if (!null) {
            start = tspoint_to_cur(ts_node_start_point(node));
            end = tspoint_to_cur(ts_node_end_point(node));
            start_byte = ts_node_start_byte(node);
            end_byte = ts_node_end_byte(node);
            type = (Ts_Ast_Type)ts_node_symbol(node);
            name = ts_node_type(node);
            id = node.id;
            anon = !ts_node_is_named(node);
            child_count = ts_node_named_child_count(node);
        }
    }

    Ast_Node *source_node() {
        auto curr = node;
        while (true) {
            if ((Ts_Ast_Type)ts_node_symbol(curr) == TS_SOURCE_FILE)
                break;
            if (ts_node_is_null(curr))
                break;
            curr = ts_node_parent(curr);
        }
        return dup(curr);
    }

    bool eq(Ast_Node *other) { return ts_node_eq(node, other->node); }
    Ast_Node *field(Ts_Field_Type f) { return dup(ts_node_child_by_field_id(node, f)); }

    TSNode _skip_comment(TSNode x, bool forward) {
        auto next_func = forward ? ts_node_next_named_sibling : ts_node_prev_named_sibling;
        while (!ts_node_is_null(x) && ts_node_symbol(x) == TS_COMMENT)
            x = next_func(x);
        return x;
    }

    Ast_Node *child() {
        auto ret = ts_node_named_child(node, 0);
        ret = _skip_comment(ret, true);
        return dup(ret);
    }

    Ast_Node *parent() { return dup(ts_node_parent(node)); }

    Ast_Node *next() {
        auto ret = ts_node_next_named_sibling(node);
        ret = _skip_comment(ret, true);
        return dup(ret);
    }

    Ast_Node *prev() {
        auto ret = ts_node_prev_named_sibling(node);
        ret = _skip_comment(ret, false);
        return dup(ret);
    }
};

struct Go_Index_Watcher {
    Go_Indexer *_this;
    Go_Watch_Type type;
    Thread_Handle thread;
    Fs_Watcher watch;
};

#define MAX_INDEX_EVENTS 1024 // don't let file change dos us

struct Gotype;

enum Godecl_Type {
    GODECL_IMPORT,
    GODECL_VAR,
    GODECL_CONST,
    GODECL_TYPE,
    GODECL_FUNC,
    GODECL_FIELD,
    GODECL_SHORTVAR,
};

struct Godecl {
    // A decl (TS_XXX_DECLARATION) contains multiple specs (TS_XXX_SPEC), each of which
    // might contain multiple IDs (TS_IDENTIFIER). Each Godecl corresponds to one of those
    // IDs.

    Godecl_Type type;

    ccstr file; // only guaranteed to be set on toplevels
    cur2 decl_start;
    cur2 spec_start;
    cur2 name_start;
    ccstr name;
    bool incomplete;

    union {
        Gotype *gotype;
        ccstr import_path; // for GOTYPE_IMPORT
    };

    Godecl *copy();
    void read(Index_Stream *s);
    void write(Index_Stream *s);
};

struct Go_Func_Sig {
    List<Godecl> *params;
    List<Godecl> *result;
};

// used for or interfaces too
struct Go_Struct_Spec {
    bool is_embedded;
    ccstr tag;

    union {
        Godecl *field;
        Gotype *embedded_type;
    };

    Go_Struct_Spec *copy();
    void read(Index_Stream *s);
    void write(Index_Stream *s);
};

enum Goresult_Type {
    GORESULT_DECL,
    GORESULT_GOTYPE,
};

struct Go_Ctx {
    ccstr import_path;
    ccstr resolved_path;
    ccstr filename;
};

struct Goresult {
    // we should just know this from context (the context this shows up in, not
    // the Go_Ctx).
    // Goresult_Type type; 
    Go_Ctx *ctx;
    union {
        void *ptr;
        Godecl *decl;
        Gotype *gotype;
    };
};

enum Chan_Direction { CHAN_RECV, CHAN_SEND, CHAN_BI };

enum Gotype_Type {
    GOTYPE_ID,
    GOTYPE_SEL,
    GOTYPE_MAP,
    GOTYPE_STRUCT,
    GOTYPE_INTERFACE,
    GOTYPE_POINTER,
    GOTYPE_FUNC,
    GOTYPE_SLICE,
    GOTYPE_ARRAY,
    GOTYPE_CHAN,
    GOTYPE_MULTI,
    GOTYPE_VARIADIC,
};

struct Gotype {
    Gotype_Type type;

    union {
        struct {
            ccstr id_name;
            cur2 id_pos;
        };

        struct {
            ccstr sel_name; // now that we have pkg, do we need this?
            ccstr sel_sel;
        };

        List<Go_Struct_Spec> *struct_specs;
        List<Go_Struct_Spec> *interface_specs;
        Gotype *pointer_base;

        struct {
            Gotype *map_key;
            Gotype *map_value;
        };

        struct {
            Go_Func_Sig func_sig;
            Gotype *func_recv;
        };

        Gotype *slice_base;
        Gotype *array_base;

        struct {
            Gotype *chan_base;
            Chan_Direction chan_direction;
        };

        List<Gotype*> *multi_types;
        Gotype *variadic_base;
    };

    Gotype *copy();
    void read(Index_Stream *s);
    void write(Index_Stream *s);
};

enum Go_Package_Status {
    GPS_OUTDATED,
    GPS_UPDATING,
    GPS_READY,
};

enum Go_Package_Name_Type {
    GPN_IMPLICIT,
    GPN_EXPLICIT,
    GPN_BLANK,
    GPN_DOT,
};

struct Go_Single_Import {
    ccstr file;
    ccstr package_name;
    Go_Package_Name_Type package_name_type;
    ccstr import_path;
    ccstr resolved_path;
    Godecl *decl;

    Go_Single_Import *copy();
    void read(Index_Stream *s);
    void write(Index_Stream *s);
};

struct Go_Dependency {
    ccstr import_path;
    ccstr resolved_path;
    ccstr package_name;

    Go_Dependency *copy();
    void read(Index_Stream *s);
    void write(Index_Stream *s);
};

enum Go_Scope_Op_Type {
    GSOP_OPEN_SCOPE,
    GSOP_CLOSE_SCOPE,
    GSOP_DECL,
};

struct Go_Scope_Op {
    Go_Scope_Op_Type type; 
    cur2 pos;
    Godecl *decl;

    Go_Scope_Op *copy();
    void read(Index_Stream *s);
    void write(Index_Stream *s);
};

struct Go_Package_File_Info {
    ccstr filename;
    List<Go_Scope_Op> *scope_ops;

    Go_Package_File_Info *copy();
    void read(Index_Stream *s);
    void write(Index_Stream *s);
};

struct Go_Package {
    Go_Package_Status status;

    // Together, these two uniquely identify a Go_Package.
    ccstr import_path;
    ccstr resolved_path;

    List<Go_Single_Import> *individual_imports;
    List<Go_Dependency> *dependencies;

    ccstr package_name;
    List<Godecl> *decls;
    List<Go_Package_File_Info> *files;
    bool is_hash_ready;
    u64 hash;

    Go_Package *copy();
    void read(Index_Stream *s);
    void write(Index_Stream *s);
};

struct Go_Index {
    ccstr current_path;
    ccstr current_import_path;
    Gomod_Info *gomod;
    List<Go_Package> *packages;

    Go_Index *copy();
    void read(Index_Stream *s);
    void write(Index_Stream *s);
};

struct Parsed_File {
    Ast_Node *root;
    TSTree *tree;
    Parser_It *it;
    bool tree_belongs_to_editor;
};

typedef fn<Walk_Action(Ast_Node *node, Ts_Field_Type field_type, int depth)> Walk_TS_Callback;
void walk_ts_cursor(TSTreeCursor *curr, bool abstract_only, Walk_TS_Callback cb);

struct Parameter_Hint {
    Gotype *gotype;
    cur2 call_args_start;
};

struct Jump_To_Definition_Result {
    ccstr file;
    cur2 pos;
};

typedef fn<Godecl*(Godecl_Type type, cur2 spec_start, ccstr name)> Node_To_Decls_Callback;

struct Go_Indexer {
    Pool mem;        // mem that exists for lifetime of Go_Indexer
    Pool final_mem;  // memory that holds the final value of this->index`
    Pool ui_mem;     // memory used by UI when it calls jump to definition, etc.

    Pool scoped_table_mem;

    Go_Index index;

    char current_exe_path[MAX_PATH];
    Process buildparser_proc;

    List<Parsed_File*> current_parsed_files;

    Thread_Handle bgthread;

    Scoped_Table<Godecl*> *local_decls;

    // ---

    void background_thread();
    bool start_background_thread();

    void init();
    void cleanup();

    Jump_To_Definition_Result* jump_to_definition(ccstr filepath, cur2 pos);
    bool autocomplete(ccstr filepath, cur2 pos, bool triggered_by_period, Autocomplete *out);
    Parameter_Hint *parameter_hint(ccstr filepath, cur2 pos, bool triggered_by_paren);

    void run_background_thread2();
    ccstr file_to_import_path(ccstr filepath);
    ccstr directory_to_import_path(ccstr path_str);
    void handle_fs_event(Go_Index_Watcher *w, Fs_Event *event);
    void temp();
    void process_package(ccstr import_path);

    bool is_file_included_in_build(ccstr path);
    List<ccstr>* list_source_files(ccstr dirpath, bool include_tests);
    ccstr get_package_name(ccstr path);
    Resolved_Import* resolve_import_from_filesystem(ccstr import_path, Go_Ctx *ctx);
    Resolved_Import* resolve_import(ccstr import_path, Go_Ctx *ctx);
    Parsed_File *parse_file(ccstr filepath);
    void free_parsed_file(Parsed_File *file);
    ccstr get_workspace_import_path();
    void handle_error(ccstr err);
    u64 hash_package(ccstr resolved_package_path);
    void read_index_from_filesystem();
    ccstr get_package_name_from_file(ccstr filepath);
    ccstr get_filepath_from_ctx(Go_Ctx *ctx);
    Goresult *infer_type(Ast_Node *expr, Go_Ctx *ctx);
    Goresult *resolve_type(Gotype *type, Go_Ctx *ctx);
    Goresult *unpointer_type(Gotype *type, Go_Ctx *ctx);
    List<Godecl> *parameter_list_to_fields(Ast_Node *params);
    Gotype *node_to_gotype(Ast_Node *node);
    Goresult *find_decl_of_id(ccstr id, cur2 id_pos, Go_Ctx *ctx, Go_Single_Import **single_import = NULL);
    void list_fields_and_methods(Goresult *type_res, Goresult *resolved_type_res, List<Goresult> *ret);
    bool node_func_to_gotype_sig(Ast_Node *params, Ast_Node *result, Go_Func_Sig *sig);
    void node_to_decls(Ast_Node *node, List<Goresult> *results, Go_Ctx *ctx);
    Gotype *new_gotype(Gotype_Type type);
    Goresult *find_decl_in_package(ccstr id, ccstr import_path, ccstr resolved_path);
    List<Godecl> *get_package_decls(ccstr import_path, ccstr resolved_path);
    Resolved_Import *resolve_import_from_gomod(ccstr import_path, Gomod_Info *info, Go_Ctx *ctx);
    Resolved_Import *check_potential_resolved_import(ccstr filepath);
    Go_Package *find_package_in_index(ccstr import_path, ccstr resolved_path);
    ccstr find_import_path_referred_to_by_id(ccstr id, Go_Ctx *ctx, ccstr *resolved_path);
    void crawl_index();
    Ast_Node *new_ast_node(TSNode node);
    Pool *get_final_mem();
    void walk_ast_node(Ast_Node *node, bool abstract_only, Walk_TS_Callback cb);
    Go_Package *find_package(ccstr import_path, ccstr resolved_path);
    void import_spec_to_decl(Ast_Node *spec_node, Godecl *decl);
    void find_nodes_containing_pos(Ast_Node *root, cur2 pos, fn<Walk_Action(Ast_Node *it)> callback);
};

#define FOR_NODE_CHILDREN(node) for (auto it = (node)->child(); !it->null; it = it->next())

Goresult *make_goresult(Gotype *gotype, Go_Ctx *ctx);
Goresult *make_goresult(Godecl *decl, Go_Ctx *ctx);

Gomod_Info *parse_gomod_file(ccstr filepath);

TSParser *new_ts_parser();

template<typename T>
T *read_object(Index_Stream *s) {
    auto size = s->read2();
    if (size == 0) return NULL;

    // TODO: i mean, don't literally crash the program, show an error and
    // rebuild the index or something
    our_assert(size == sizeof(T), "size mismatch while reading object from index");

    auto obj = alloc_object(T);
    s->readn(obj, size);
    obj->read(s);
    return obj;
}

template<typename T>
List<T> *read_list(Index_Stream *s) {
    auto len = s->read4();
    auto specs = alloc_list<T>(len);
    for (u32 i = 0; i < len; i++)
        specs->append(read_object<T>(s));
    return specs;
}

template<typename T>
void write_object(T *obj, Index_Stream *s) {
    if (obj == NULL) {
        s->write2(0);
        return;
    }

    s->write2(sizeof(T));
    s->writen(obj, sizeof(T));
    obj->write(s);
}

template<typename L>
void write_list(L arr, Index_Stream *s) {
    if (arr == NULL) {
        s->write4(0);
        return;
    }
    s->write4(arr->len);
    For (*arr) write_object(&it, s);
}
