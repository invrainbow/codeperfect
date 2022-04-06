#pragma once

#include "common.hpp"
#include "buffer.hpp"
#include "utils.hpp"
#include "mem.hpp"
#include "os.hpp"
#include "tree_sitter_crap.hpp"
#include "gohelper_shim.h"

extern "C" TSLanguage *tree_sitter_go();

extern const unsigned char GO_INDEX_MAGIC_BYTES[3];
extern const int GO_INDEX_VERSION;

enum {
    CUSTOM_HASH_BUILTINS = 1,
    // other custom packages? can't imagine there will be anything else
};

// mirrors tree-sitter-go/src/parser.c
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
    TSF_TYPE_ARGUMENTS = 32,
    TSF_TYPE_PARAMETERS = 33,
    TSF_UPDATE = 34,
    TSF_VALUE = 35,
};

// mirrors tree-sitter-go/src/parser.c
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
    TS_LBRACK = 15,
    TS_RBRACK = 16,
    TS_DOT_DOT_DOT = 17,
    TS_TYPE = 18,
    TS_STAR = 19,
    TS_STRUCT = 20,
    TS_LBRACE = 21,
    TS_RBRACE = 22,
    TS_INTERFACE = 23,
    TS_PIPE = 24,
    TS_TILDE = 25,
    TS_MAP = 26,
    TS_CHAN = 27,
    TS_LT_DASH = 28,
    TS_COLON_EQ = 29,
    TS_PLUS_PLUS = 30,
    TS_DASH_DASH = 31,
    TS_STAR_EQ = 32,
    TS_SLASH_EQ = 33,
    TS_PERCENT_EQ = 34,
    TS_LT_LT_EQ = 35,
    TS_GT_GT_EQ = 36,
    TS_AMP_EQ = 37,
    TS_AMP_CARET_EQ = 38,
    TS_PLUS_EQ = 39,
    TS_DASH_EQ = 40,
    TS_PIPE_EQ = 41,
    TS_CARET_EQ = 42,
    TS_COLON = 43,
    TS_FALLTHROUGH = 44,
    TS_BREAK = 45,
    TS_CONTINUE = 46,
    TS_GOTO = 47,
    TS_RETURN = 48,
    TS_GO = 49,
    TS_DEFER = 50,
    TS_IF = 51,
    TS_ELSE = 52,
    TS_FOR = 53,
    TS_RANGE = 54,
    TS_SWITCH = 55,
    TS_CASE = 56,
    TS_DEFAULT = 57,
    TS_SELECT = 58,
    TS_NEW = 59,
    TS_MAKE = 60,
    TS_PLUS = 61,
    TS_DASH = 62,
    TS_BANG = 63,
    TS_CARET = 64,
    TS_AMP = 65,
    TS_SLASH = 66,
    TS_PERCENT = 67,
    TS_LT_LT = 68,
    TS_GT_GT = 69,
    TS_AMP_CARET = 70,
    TS_EQ_EQ = 71,
    TS_BANG_EQ = 72,
    TS_LT = 73,
    TS_LT_EQ = 74,
    TS_GT = 75,
    TS_GT_EQ = 76,
    TS_AMP_AMP = 77,
    TS_PIPE_PIPE = 78,
    TS_INT_LITERAL = 79,
    TS_FLOAT_LITERAL = 80,
    TS_IMAGINARY_LITERAL = 81,
    TS_RUNE_LITERAL = 82,
    TS_NIL = 83,
    TS_TRUE = 84,
    TS_FALSE = 85,
    TS_IOTA = 86,
    TS_COMMENT = 87,
    TS_RAW_STRING_LITERAL = 88,
    TS_INTERPRETED_STRING_LITERAL = 89,
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
    TS_TYPE_PARAMETER_LIST = 103,
    TS_PARAMETER_LIST = 104,
    TS_PARAMETER_DECLARATION = 105,
    TS_VARIADIC_PARAMETER_DECLARATION = 106,
    TS_TYPE_ALIAS = 107,
    TS_TYPE_DECLARATION = 108,
    TS_TYPE_SPEC = 109,
    TS_EXPRESSION_LIST = 110,
    TS_PARENTHESIZED_TYPE = 111,
    TS_SIMPLE_TYPE = 112,
    TS_GENERIC_TYPE = 113,
    TS_TYPE_ARGUMENTS = 114,
    TS_POINTER_TYPE = 115,
    TS_ARRAY_TYPE = 116,
    TS_IMPLICIT_LENGTH_ARRAY_TYPE = 117,
    TS_SLICE_TYPE = 118,
    TS_STRUCT_TYPE = 119,
    TS_FIELD_DECLARATION_LIST = 120,
    TS_FIELD_DECLARATION = 121,
    TS_INTERFACE_TYPE = 122,
    TS_INTERFACE_BODY = 123,
    TS_INTERFACE_TYPE_NAME = 124,
    TS_CONSTRAINT_ELEM = 125,
    TS_CONSTRAINT_TERM = 126,
    TS_METHOD_SPEC = 127,
    TS_MAP_TYPE = 128,
    TS_CHANNEL_TYPE = 129,
    TS_FUNCTION_TYPE = 130,
    TS_BLOCK = 131,
    TS_STATEMENT_LIST = 132,
    TS_STATEMENT = 133,
    TS_EMPTY_STATEMENT = 134,
    TS_SIMPLE_STATEMENT = 135,
    TS_SEND_STATEMENT = 136,
    TS_RECEIVE_STATEMENT = 137,
    TS_INC_STATEMENT = 138,
    TS_DEC_STATEMENT = 139,
    TS_ASSIGNMENT_STATEMENT = 140,
    TS_SHORT_VAR_DECLARATION = 141,
    TS_LABELED_STATEMENT = 142,
    TS_EMPTY_LABELED_STATEMENT = 143,
    TS_FALLTHROUGH_STATEMENT = 144,
    TS_BREAK_STATEMENT = 145,
    TS_CONTINUE_STATEMENT = 146,
    TS_GOTO_STATEMENT = 147,
    TS_RETURN_STATEMENT = 148,
    TS_GO_STATEMENT = 149,
    TS_DEFER_STATEMENT = 150,
    TS_IF_STATEMENT = 151,
    TS_FOR_STATEMENT = 152,
    TS_FOR_CLAUSE = 153,
    TS_RANGE_CLAUSE = 154,
    TS_EXPRESSION_SWITCH_STATEMENT = 155,
    TS_EXPRESSION_CASE = 156,
    TS_DEFAULT_CASE = 157,
    TS_TYPE_SWITCH_STATEMENT = 158,
    TS_TYPE_SWITCH_HEADER = 159,
    TS_TYPE_CASE = 160,
    TS_SELECT_STATEMENT = 161,
    TS_COMMUNICATION_CASE = 162,
    TS_EXPRESSION = 163,
    TS_PARENTHESIZED_EXPRESSION = 164,
    TS_CALL_EXPRESSION = 165,
    TS_VARIADIC_ARGUMENT = 166,
    TS_SPECIAL_ARGUMENT_LIST = 167,
    TS_ARGUMENT_LIST = 168,
    TS_SELECTOR_EXPRESSION = 169,
    TS_INDEX_EXPRESSION = 170,
    TS_SLICE_EXPRESSION = 171,
    TS_TYPE_ASSERTION_EXPRESSION = 172,
    TS_TYPE_CONVERSION_EXPRESSION = 173,
    TS_COMPOSITE_LITERAL = 174,
    TS_LITERAL_VALUE = 175,
    TS_LITERAL_ELEMENT = 176,
    TS_KEYED_ELEMENT = 177,
    TS_FUNC_LITERAL = 178,
    TS_UNARY_EXPRESSION = 179,
    TS_BINARY_EXPRESSION = 180,
    TS_QUALIFIED_TYPE = 181,
    TS_SOURCE_FILE_REPEAT1 = 182,
    TS_IMPORT_SPEC_LIST_REPEAT1 = 183,
    TS_CONST_DECLARATION_REPEAT1 = 184,
    TS_CONST_SPEC_REPEAT1 = 185,
    TS_VAR_DECLARATION_REPEAT1 = 186,
    TS_TYPE_PARAMETER_LIST_REPEAT1 = 187,
    TS_PARAMETER_LIST_REPEAT1 = 188,
    TS_PARAMETER_DECLARATION_REPEAT1 = 189,
    TS_TYPE_DECLARATION_REPEAT1 = 190,
    TS_EXPRESSION_LIST_REPEAT1 = 191,
    TS_TYPE_ARGUMENTS_REPEAT1 = 192,
    TS_FIELD_DECLARATION_LIST_REPEAT1 = 193,
    TS_FIELD_DECLARATION_REPEAT1 = 194,
    TS_INTERFACE_TYPE_REPEAT1 = 195,
    TS_CONSTRAINT_ELEM_REPEAT1 = 196,
    TS_STATEMENT_LIST_REPEAT1 = 197,
    TS_EXPRESSION_SWITCH_STATEMENT_REPEAT1 = 198,
    TS_TYPE_SWITCH_STATEMENT_REPEAT1 = 199,
    TS_SELECT_STATEMENT_REPEAT1 = 200,
    TS_ARGUMENT_LIST_REPEAT1 = 201,
    TS_LITERAL_VALUE_REPEAT1 = 202,
    TS_FIELD_IDENTIFIER = 203,
    TS_LABEL_NAME = 204,
    TS_PACKAGE_IDENTIFIER = 205,
    TS_TYPE_IDENTIFIER = 206,
};

ccstr ts_field_type_str(Ts_Field_Type type);
ccstr ts_ast_type_str(Ts_Ast_Type type);

struct Go_Index;

struct Index_Stream {
    // File f;
    i64 offset;
    ccstr path;
    bool ok;
    File_Mapping *fm;

    bool open(ccstr _path, bool write = false);
    void cleanup();

    bool writen(void* buf, int n);
    bool write1(i8 x);
    bool write2(i16 x);
    bool write4(i32 x);
    bool writestr(ccstr s);
    void finish_writing();

    void readn(void* buf, s32 n);
    char read1();
    i16 read2();
    i32 read4();
    ccstr readstr();

    Go_Index *read_index();
    void write_index(Go_Index *index);
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
            File_Mapping *fm; // TODO: unicode
            cur2 pos;
        } mmap_params;

        struct {
            Buffer_It it;
        } buffer_params;
    };

    void init(File_Mapping *fm) {
        ptr0(this);
        type = IT_MMAP;
        mmap_params.fm = fm;
        mmap_params.pos = new_cur2(0, 0);
    }

    void init(Buffer* buf) {
        ptr0(this);
        type = IT_BUFFER;
        buffer_params.it.buf = buf;
    }

    void cleanup() {
        if (type == IT_MMAP) mmap_params.fm->cleanup();
    }

    uchar peek() {
        switch (type) {
        case IT_MMAP: return mmap_params.fm->data[mmap_params.pos.x];
        case IT_BUFFER: return buffer_params.it.peek();
        }
        return 0;
    }

    uchar next() {
        switch (type) {
        case IT_MMAP:
            {
                auto ret = peek();
                mmap_params.pos.x++;
                return ret;
            }
        case IT_BUFFER:
            return buffer_params.it.next();
        }
        return 0;
    }

    uchar prev() {
        switch (type) {
        case IT_MMAP:
            {
                auto ret = peek();
                mmap_params.pos.x--;
                return ret;
            }
        case IT_BUFFER:
            return buffer_params.it.prev();
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
        case IT_MMAP: return (mmap_params.pos.x == mmap_params.fm->len);
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

enum AC_Result_Type {
    ACR_DECLARATION,
    ACR_KEYWORD,
    ACR_POSTFIX,
    ACR_IMPORT,
    // TODO: other types, like autocompleting "fmt.Printf" when only "ftP" has been typed as a lone keyword
};

enum Postfix_Completion_Type {
    PFC_APPEND,
    PFC_LEN,
    PFC_CAP,

    PFC_ASSIGNAPPEND,

    PFC_FOR,
    PFC_FORKEY,
    PFC_FORVALUE,

    PFC_NIL,
    PFC_NOTNIL,
    PFC_NOT,

    PFC_EMPTY,
    PFC_IFEMPTY,
    PFC_IFNOTEMPTY,

    PFC_IF,
    PFC_IFNOT,
    PFC_IFNIL,
    PFC_IFNOTNIL,

    PFC_CHECK,

    PFC_DEFSTRUCT,
    PFC_DEFINTERFACE,
    PFC_SWITCH,
};

struct Gotype;
struct Godecl;

struct AC_Result {
    ccstr name;
    AC_Result_Type type;

    union {
        Postfix_Completion_Type postfix_operation;

        struct {
            Godecl *declaration_godecl;
            Gotype *declaration_evaluated_gotype;
            ccstr declaration_import_path;
            ccstr declaration_filename;
            bool declaration_is_builtin;
            ccstr declaration_package; // if the decl is "foo.bar", this will be "foo"
            bool declaration_is_struct_literal_field;
        };

        struct {
            ccstr import_path;
            bool import_is_existing;
        };
    };

    AC_Result* copy();
};

enum Autocomplete_Type {
    AUTOCOMPLETE_NONE = 0,
    AUTOCOMPLETE_DOT_COMPLETE,
    AUTOCOMPLETE_IDENTIFIER,
};

struct Autocomplete {
    Autocomplete_Type type;
    List<AC_Result>* results;
    ccstr prefix;
    cur2 keyword_start;
    cur2 keyword_end;

    // only for AUTOCOMPLETE_DOT_COMPLETE
    cur2 operand_start;
    cur2 operand_end;
    Gotype *operand_gotype;
    bool operand_is_error_type; // fix this once we're checking too many individual cases
};

enum Walk_Action {
    WALK_CONTINUE,
    WALK_ABORT,
    WALK_SKIP_CHILDREN,
};

ccstr _path_join(ccstr a, ...);
#define path_join(...) _path_join(__VA_ARGS__, NULL)

enum Index_Event_Type {
    INDEX_EVENT_FETCH_IMPORTS,
    INDEX_EVENT_REINDEX_PACKAGE,
};

struct Index_Event {
    Index_Event_Type type;
    char import_path[MAX_PATH]; // for reindex_package
    u64 time;
};

TSPoint cur_to_tspoint(cur2 c);
cur2 tspoint_to_cur(TSPoint p);

struct Go_Indexer;

// wrapper around TS_Node, honestly fuck getter patterns
struct Ast_Node {
    TSNode node;
    Parser_It *it;
    bool null;

    Ast_Node *dup(TSNode new_node);
    ccstr string();

    void init(TSNode _node, Parser_It *_it) {
        ptr0(this);

        node = _node;
        it = _it;
        null = ts_node_is_null(node);
    }

    cur2 start() { return null ? new_cur2(0, 0) : tspoint_to_cur(ts_node_start_point(node)); }
    cur2 end() { return null ? new_cur2(0, 0) : tspoint_to_cur(ts_node_end_point(node)); }
    u32 start_byte() { return null ? 0 : ts_node_start_byte(node); }
    u32 end_byte() { return null ? 0 : ts_node_end_byte(node); }
    Ts_Ast_Type type() { return (Ts_Ast_Type)(null ? 0 : ts_node_symbol(node)); }
    ccstr name() { return null ? NULL : ts_node_type(node); }
    const void* id() { return null ? NULL : node.id; }
    bool anon() { return null ? false : !ts_node_is_named(node); }
    int child_count() { return null ? 0 : ts_node_named_child_count(node); }
    int all_child_count() { return null ? 0 : ts_node_child_count(node); }
    bool is_missing() { return null ? false : ts_node_is_missing(node); }
    Ast_Node *parent() { return null ? NULL : dup(ts_node_parent(node)); }

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

    TSNode _skip_comment(TSNode x, bool forward, bool named) {
        auto next_func = forward
            ? (named ? ts_node_next_named_sibling : ts_node_next_sibling)
            : (named ? ts_node_prev_named_sibling : ts_node_prev_sibling);

        while (!ts_node_is_null(x) && ts_node_symbol(x) == TS_COMMENT)
            x = next_func(x);
        return x;
    }

    Ast_Node *child(bool skip_comment = true) {
        auto ret = ts_node_named_child(node, 0);
        if (skip_comment)
            ret = _skip_comment(ret, true, true);
        return dup(ret);
    }

    Ast_Node *next(bool skip_comment = true) {
        auto ret = ts_node_next_named_sibling(node);
        if (skip_comment)
            ret = _skip_comment(ret, true, true);
        return dup(ret);
    }

    Ast_Node *prev(bool skip_comment = true) {
        auto ret = ts_node_prev_named_sibling(node);
        if (skip_comment)
            ret = _skip_comment(ret, false, true);
        return dup(ret);
    }

    Ast_Node *child_all(bool skip_comment = true) {
        auto ret = ts_node_child(node, 0);
        if (skip_comment)
            ret = _skip_comment(ret, true, false);
        return dup(ret);
    }

    Ast_Node *next_all(bool skip_comment = true) {
        auto ret = ts_node_next_sibling(node);
        if (skip_comment)
            ret = _skip_comment(ret, true, false);
        return dup(ret);
    }

    Ast_Node *prev_all(bool skip_comment = true) {
        auto ret = ts_node_prev_sibling(node);
        if (skip_comment)
            ret = _skip_comment(ret, false, false);
        return dup(ret);
    }
};

#define MAX_INDEX_EVENTS 1024 // don't let file change dos us

struct Gotype;

enum Godecl_Type {
    GODECL_IMPORT,
    GODECL_VAR,
    GODECL_CONST,
    GODECL_TYPE,
    GODECL_FUNC, // should we have GODECL_METHOD too? can just check gotype->func_recv
    GODECL_FIELD,
    GODECL_PARAM,
    GODECL_SHORTVAR,
    GODECL_TYPECASE,
    // GODECL_RANGE,
};

ccstr godecl_type_str(Godecl_Type type);

struct Godecl {
    // A decl (TS_XXX_DECLARATION) contains multiple specs (TS_XXX_SPEC), each of which
    // might contain multiple IDs (TS_IDENTIFIER). Each Godecl corresponds to one of those
    // IDs.

    Godecl_Type type;

    // ccstr file; // only guaranteed to be set on toplevels
    cur2 decl_start;
    cur2 decl_end;
    cur2 spec_start;
    cur2 name_start;
    cur2 name_end;
    ccstr name;
    bool is_embedded; // for GODECL_FIELD
    bool is_toplevel;

    union {
        Gotype *gotype;
        ccstr import_path; // for GODECL_IMPORT
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
    ccstr tag;
    Godecl *field;

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
    // ccstr resolved_path;
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

    Goresult *copy_decl();
    Goresult *copy_gotype();
    Goresult *wrap(Godecl *new_decl);
    Goresult *wrap(Gotype *new_gotype);
};

enum Chan_Direction { CHAN_RECV, CHAN_SEND, CHAN_BI };

enum Range_Type {
    RANGE_LIST,
    RANGE_MAP,
};

enum Gotype_Builtin_Type {
    // functions
    GO_BUILTIN_APPEND,  // func append(slice []Type, elems ...Type) []Type
    GO_BUILTIN_CAP,     // func cap(v Type) int
    GO_BUILTIN_CLOSE,   // func close(c chan<- Type)
    GO_BUILTIN_COMPLEX, // func complex(r, i FloatType) ComplexType
    GO_BUILTIN_COPY,    // func copy(dst, src []Type) int
    GO_BUILTIN_DELETE,  // func delete(m map[Type]Type1, key Type)
    GO_BUILTIN_IMAG,    // func imag(c ComplexType) FloatType
    GO_BUILTIN_LEN,     // func len(v Type) int
    GO_BUILTIN_MAKE,    // func make(t Type, size ...IntegerType) Type
    GO_BUILTIN_NEW,     // func new(Type) *Type
    GO_BUILTIN_PANIC,   // func panic(v interface{})
    GO_BUILTIN_PRINT,   // func print(args ...Type)
    GO_BUILTIN_PRINTLN, // func println(args ...Type)
    GO_BUILTIN_REAL,    // func real(c ComplexType) FloatType
    GO_BUILTIN_RECOVER, // func recover() interface{}

    // types
    GO_BUILTIN_COMPLEXTYPE,
    GO_BUILTIN_FLOATTYPE,
    GO_BUILTIN_INTEGERTYPE,
    GO_BUILTIN_TYPE,
    GO_BUILTIN_TYPE1,
    GO_BUILTIN_BOOL,
    GO_BUILTIN_BYTE,
    GO_BUILTIN_COMPLEX128,
    GO_BUILTIN_COMPLEX64,
    GO_BUILTIN_ERROR,
    GO_BUILTIN_FLOAT32,
    GO_BUILTIN_FLOAT64,
    GO_BUILTIN_INT,
    GO_BUILTIN_INT16,
    GO_BUILTIN_INT32,
    GO_BUILTIN_INT64,
    GO_BUILTIN_INT8,
    GO_BUILTIN_RUNE,
    GO_BUILTIN_STRING,
    GO_BUILTIN_UINT,
    GO_BUILTIN_UINT16,
    GO_BUILTIN_UINT32,
    GO_BUILTIN_UINT64,
    GO_BUILTIN_UINT8,
    GO_BUILTIN_UINTPTR,
};

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
    GOTYPE_ASSERTION,
    GOTYPE_RANGE,

    GOTYPE_BUILTIN,

    _GOTYPE_LAZY_MARKER_, // #define IS_LAZY_TYPE(x) (x > _GOTYPE_LAZY_MARKER_)

    // "lazy" types
    GOTYPE_LAZY_INDEX,
    GOTYPE_LAZY_CALL,
    GOTYPE_LAZY_DEREFERENCE,
    GOTYPE_LAZY_REFERENCE,
    GOTYPE_LAZY_ARROW,
    GOTYPE_LAZY_ID,
    GOTYPE_LAZY_SEL,
    GOTYPE_LAZY_ONE_OF_MULTI,
    GOTYPE_LAZY_RANGE,
};

ccstr gotype_type_str(Gotype_Type type);

struct Gotype {
    Gotype_Type type;

    union {
        struct {
            Gotype_Builtin_Type builtin_type;
            Gotype *builtin_underlying_type;
        };

        struct {
            ccstr id_name;
            cur2 id_pos;
        };

        struct {
            ccstr sel_name; // now that we have pkg, do we need this?
            ccstr sel_sel;
        };

        struct {
            Gotype *map_key;
            Gotype *map_value;
        };

        List<Go_Struct_Spec> *struct_specs;
        List<Go_Struct_Spec> *interface_specs;
        Gotype *pointer_base;

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
        Gotype *assertion_base;

        struct {
            // This is the type of the thing in range, not the type of the returned item.
            // So if x is an []int and we have range x, range_base is []int, not int.
            Gotype *range_base;
            Range_Type range_type;
        };

        struct {
            ccstr lazy_id_name;
            cur2 lazy_id_pos;
        };

        struct {
            Gotype *lazy_sel_base;
            ccstr lazy_sel_sel;
        };

        Gotype *lazy_index_base;
        Gotype *lazy_call_base;
        Gotype *lazy_dereference_base;
        Gotype *lazy_reference_base;
        Gotype *lazy_arrow_base;

        struct {
            Gotype *lazy_range_base;
            bool lazy_range_is_index;
        };

        struct {
            Gotype *lazy_one_of_multi_base;
            int lazy_one_of_multi_index;
            bool lazy_one_of_multi_is_single; // lhs.len == 1 && rhs.len == 1
        };
    };

    Gotype *copy();
    void read(Index_Stream *s);
    void write(Index_Stream *s);
    bool equals_in_go_sense(Gotype *other);
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

struct Go_Import {
    ccstr package_name;
    Go_Package_Name_Type package_name_type;
    ccstr import_path;
    Godecl *decl;

    Go_Import *copy();
    void read(Index_Stream *s);
    void write(Index_Stream *s);
};

enum Go_Scope_Op_Type {
    GSOP_OPEN_SCOPE,
    GSOP_CLOSE_SCOPE,
    GSOP_DECL,
    GSOP_OPEN_SWITCH_TYPE_SCOPE,
};

struct Go_Scope_Op {
    Go_Scope_Op_Type type;
    cur2 pos;
    union {
        struct {
            Godecl *decl;
            int decl_scope_depth;
        };

        Gotype *gotype;
    };

    Go_Scope_Op *copy();
    void read(Index_Stream *s);
    void write(Index_Stream *s);
};

struct Go_Reference {
    bool is_sel;
    union {
        struct {
            Gotype *x;
            cur2 x_start;
            cur2 x_end;

            ccstr sel;
            cur2 sel_start;
            cur2 sel_end;
        };
        struct {
            ccstr name;
            cur2 start;
            cur2 end;
        };
    };

    Go_Reference *copy();
    void read(Index_Stream *s);
    void write(Index_Stream *s);

    cur2 true_start() { return is_sel ? x_start : start; }
};

struct Go_File {
    Pool pool;

    ccstr filename;
    List<Go_Scope_Op> *scope_ops;
    List<Godecl> *decls;
    List<Go_Import> *imports;
    List<Go_Reference> *references;

    u64 hash;

    void cleanup() {
        pool.cleanup();
    }

    // Go_File *copy();
    void read(Index_Stream *s);
    void write(Index_Stream *s);
};

struct Go_Package {
    Go_Package_Status status;
    ccstr import_path;
    // ccstr resolved_path?
    ccstr package_name;
    List<Go_File> *files;
    u64 hash;
    bool checked_for_outdated_hash;

    void cleanup_files() {
        if (!files) return;
        For (*files) it.cleanup();
        files->len = 0;
    }

    Go_Package *copy();
    void read(Index_Stream *s);
    void write(Index_Stream *s);
};

struct Go_Index {
    ccstr current_path;
    ccstr current_import_path;
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
    TSParser *editor_parser;
};

typedef fn<Walk_Action(Ast_Node *node, Ts_Field_Type field_type, int depth)> Walk_TS_Callback;
void walk_ts_cursor(TSTreeCursor *curr, bool abstract_only, Walk_TS_Callback cb);

struct Parameter_Hint {
    Gotype *gotype;
    cur2 call_args_start;
    int current_param;
};

struct Jump_To_Definition_Result {
    ccstr file;
    cur2 pos;
    Goresult *decl; // can be null

    Jump_To_Definition_Result *copy();
};

typedef fn<Godecl*()> New_Godecl_Func;

struct Module_Resolver {
    struct Node {
        ccstr name;
        Node *children;
        Node *next;
        ccstr value; // only leaves have values
    };

    ccstr gomodcache;

    Pool mem;
    Node *root_import_to_resolved;
    Node *root_resolved_to_import;
    ccstr module_path;

    void init(ccstr current_module_filepath, ccstr _gomodcache);
    void cleanup();

    Node *goto_child(Node *node, ccstr name, bool create_if_not_found) {
        for (auto it = node->children; it; it = it->next)
            if (streqi(it->name, name))
                return it;

        if (create_if_not_found) {
            auto ret = alloc_object(Node);
            ret->next = node->children;
            ret->name = cp_strcpy(name);
            node->children = ret;
            return ret;
        }

        return NULL;
    }

    void add_to_root(Node *root, ccstr key, ccstr value) {
        //print("adding %s -> %s", key, value);

        auto path = make_path(key);
        auto curr = root;
        For (*path->parts) curr = goto_child(curr, it, true);
        curr->value = value;
    }

    void add_path(ccstr import_path, ccstr resolved_path) {
        resolved_path = normalize_path_sep(cp_strcpy(resolved_path));
        add_to_root(root_import_to_resolved, import_path, resolved_path);
        add_to_root(root_resolved_to_import, resolved_path, import_path);
    }

    ccstr normalize_path_in_module_cache(ccstr import_path) {
        u32 len = 0;
        u32 slen = strlen(import_path);

        for (u32 i = 0; i < slen; i++)
            len += (isupper(import_path[i]) ? 2 : 1);

        auto new_filepath = alloc_array(char, len+1);
        for (u32 i = 0, j = 0; i < slen; i++) {
            auto ch = import_path[i];
            if (isupper(ch)) {
                new_filepath[j++] = '!';
                new_filepath[j++] = tolower(ch);
            } else {
                new_filepath[j++] = ch;
            }
        }
        new_filepath[len] = '\0';
        return new_filepath;
    }

    ccstr convert_path(Node *root, ccstr path, char sep) {
        auto parts = make_path(path)->parts;

        Node *curr = root;
        ccstr last_value = NULL;
        i32 last_index = -1;

        for (u32 i = 0; i < parts->len; i++) {
            auto part = parts->at(i);

            curr = goto_child(curr, part, false);
            if (!curr) break;

            if (curr->value) {
                last_value = curr->value;
                last_index = i + 1;
            }
        }

        if (!last_value) return NULL;

        auto ret = alloc_list<ccstr>(parts->len - last_index + 1);
        ret->append(last_value);
        for (u32 i = last_index; i < parts->len; i++)
            ret->append(parts->at(i));

        Path p;
        p.init(ret);
        return normalize_path_sep(p.str(), sep);
    }

    ccstr resolve_import(ccstr import_path) {
        if (streq(import_path, "@builtins"))
            return import_path;

        return convert_path(root_import_to_resolved, import_path, PATH_SEP);
    }

    ccstr resolved_path_to_import_path(ccstr resolved_path) {
        if (streq(resolved_path, "@builtins"))
            return resolved_path;

        return convert_path(root_resolved_to_import, resolved_path, '/');
    }
};

enum Tok_Type {
    TOK_ILLEGAL,
    TOK_EOF,
    TOK_COMMENT,
    TOK_ID,
};

struct Token {
    Tok_Type type;
    cur2 start;
    cur2 end;
};

bool isident(int c);

enum {
    LISTDECLS_PUBLIC_ONLY = 1 << 0,
    LISTDECLS_EXCLUDE_METHODS = 1 << 1,
};

Go_File *get_ready_file_in_package(Go_Package *pkg, ccstr filename);

enum Go_Message_Type {
    GOMSG_RESCAN_INDEX,
    GOMSG_OBLITERATE_AND_RECREATE_INDEX,
    GOMSG_CLEANUP_UNUSED_MEMORY,
    GOMSG_FSEVENT,
};

struct Go_Message {
    Go_Message_Type type;

    union {
        ccstr fsevent_filepath; // for GOMSG_FSEVENT
    };
};

struct Find_References_File {
    ccstr filepath;
    List<Go_Reference> *references;

    Find_References_File* copy();
};

struct Find_Decl {
    ccstr filepath;
    Goresult *decl;
    ccstr package_name;

    Find_Decl* copy();
};

enum Indexer_Status {
    IND_READY,
    IND_WRITING,
    IND_READING,
};

// a name & godecl (goresult) pair
struct Go_Symbol {
    ccstr pkgname;
    ccstr filepath;
    ccstr name;
    Goresult *decl;
    u64 filehash;

    ccstr full_name() { return cp_sprintf("%s.%s", pkgname, name); }
    Go_Symbol* copy();
};

struct Call_Hier_Node {
    Find_Decl *decl;
    Go_Reference *ref;
    // TODO: what else
    List<Call_Hier_Node> *children;

    Call_Hier_Node *copy();
};

struct Seen_Callee_Entry {
    Goresult *declres;
    Call_Hier_Node node;
};

struct Go_Indexer {
    ccstr goroot;
    // ccstr gopath;
    ccstr gomodcache;

    Pool mem;        // mem that exists for lifetime of Go_Indexer
    Pool final_mem;  // memory that holds the final value of `this->index`
    Pool ui_mem;     // memory used by UI when it calls jump to definition, etc.

    Pool scoped_table_mem;

    Go_Index index;

    char current_exe_path[MAX_PATH];

    Thread_Handle bgthread;

    Module_Resolver module_resolver;

    Table<int> package_lookup;

    Message_Queue<Go_Message> message_queue;

    Lock lock;
    Indexer_Status status;
    int reacquires;

    // ---

    void background_thread();
    bool start_background_thread();

    void init();
    void cleanup();

    Jump_To_Definition_Result* jump_to_symbol(ccstr symbol);
    Jump_To_Definition_Result* jump_to_definition(ccstr filepath, cur2 pos);
    bool autocomplete(ccstr filepath, cur2 pos, bool triggered_by_period, Autocomplete *out);
    Parameter_Hint *parameter_hint(ccstr filepath, cur2 pos);
    List<Go_Import> *optimize_imports(ccstr filepath);
    ccstr find_best_import(ccstr package_name, List<ccstr> *identifiers);

    ccstr filepath_to_import_path(ccstr filepath);
    void process_package(ccstr import_path);
    bool is_file_included_in_build(ccstr path);
    List<ccstr>* list_source_files(ccstr dirpath, bool include_tests);
    ccstr get_package_name(ccstr path);
    ccstr get_package_path(ccstr import_path);
    Parsed_File *parse_file(ccstr filepath, bool use_latest = false);
    void free_parsed_file(Parsed_File *file);
    void handle_error(ccstr err);
    u64 hash_package(ccstr resolved_package_path);
    ccstr ctx_to_filepath(Go_Ctx *ctx);
    Go_Ctx *filepath_to_ctx(ccstr filepath);
    Goresult *resolve_type(Gotype *type, Go_Ctx *ctx);
    Goresult *resolve_type_to_decl(Gotype *type, Go_Ctx *ctx);
    Goresult *unpointer_type(Gotype *type, Go_Ctx *ctx);
    List<Godecl> *parameter_list_to_fields(Ast_Node *params);
    Gotype *node_to_gotype(Ast_Node *node, bool toplevel = false);
    Goresult *find_decl_of_id(ccstr id, cur2 id_pos, Go_Ctx *ctx, Go_Import **single_import = NULL);
    void list_struct_fields(Goresult *type, List<Goresult> *ret);
    void list_dotprops(Goresult *type_res, Goresult *resolved_type_res, List<Goresult> *ret);
    void actually_list_dotprops(Goresult *type_res, Goresult *resolved_type_res, List<Goresult> *ret);
    bool node_func_to_gotype_sig(Ast_Node *params, Ast_Node *result, Go_Func_Sig *sig);
    void node_to_decls(Ast_Node *node, List<Godecl> *results, ccstr filename, Pool *target_pool = NULL);
    Gotype *new_gotype(Gotype_Type type);
    Goresult *find_decl_in_package(ccstr id, ccstr import_path);
    List<Goresult> *list_package_decls(ccstr import_path, int flags = 0);
    Go_Package *find_package_in_index(ccstr import_path);
    ccstr get_import_package_name(Go_Import *it);
    ccstr get_package_referred_to_by_ast(Ast_Node *node, Go_Ctx *ctx);
    ccstr find_import_path_referred_to_by_id(ccstr id, Go_Ctx *ctx);
    Pool *get_final_mem();
    Go_Package *find_up_to_date_package(ccstr import_path);
    void import_spec_to_decl(Ast_Node *spec_node, Godecl *decl);
    List<Postfix_Completion_Type> *get_postfix_completions(Ast_Node *operand_node, Go_Ctx *ctx);
    List<Goresult> *get_node_dotprops(Ast_Node *operand_node, bool *was_package, Go_Ctx *ctx);
    bool assignment_to_decls(List<Ast_Node*> *lhs, List<Ast_Node*> *rhs, New_Godecl_Func new_godecl, bool range = false);
    Gotype *new_primitive_type(ccstr name);
    Goresult *evaluate_type(Gotype *gotype, Go_Ctx *ctx);
    Gotype *expr_to_gotype(Ast_Node *expr);
    void process_tree_into_gofile(
        Go_File *file,
        Ast_Node *root,
        ccstr filename,
        ccstr *package_name,
        bool time = false
    );
    void iterate_over_scope_ops(Ast_Node *root, fn<bool(Go_Scope_Op*)> cb, ccstr filename);
    void reload_all_dirty_files();
    void reload_editor_if_dirty(void *editor);
    void reload_single_file(ccstr path);
    Go_Package_Status get_package_status(ccstr import_path);
    void replace_package_name(Go_Package *pkg, ccstr package_name);
    u64 hash_file(ccstr filepath);
    void start_writing(bool skip_if_already_started = false);
    void stop_writing();
    bool truncate_parsed_file(Parsed_File *pf, cur2 end_pos, ccstr chars_to_append);
    Gotype *get_closest_function(ccstr filepath, cur2 pos);

    void fill_goto_symbol(List<Go_Symbol> *out);
    void init_builtins(Go_Package *pkg);
    void import_decl_to_goimports(Ast_Node *decl_node, ccstr filename, List<Go_Import> *out);
    bool check_if_still_in_parameter_hint(ccstr filepath, cur2 cur, cur2 hint_start);
    Go_File *find_gofile_from_ctx(Go_Ctx *ctx, Go_Package **out = NULL);

    List<Find_References_File>* find_references(ccstr filepath, cur2 pos, bool include_self);
    List<Find_References_File>* find_references(Goresult *declres, bool include_self);
    List<Find_References_File>* actually_find_references(Goresult *declres, bool include_self);
    List<Goresult> *list_lazy_type_dotprops(Gotype *type, Go_Ctx *ctx);

    bool acquire_lock(Indexer_Status new_status, bool just_try = false);
    bool release_lock(Indexer_Status expected_status);

    bool try_acquire_lock(Indexer_Status new_status) {
        return acquire_lock(new_status, true);
    }

    bool are_gotypes_equal(Goresult *ra, Goresult *rb);
    bool are_decls_equal(Goresult *adecl, Goresult *bdecl);

    List<Find_Decl> *find_implementations(Goresult *target, bool search_everywhere);
    List<Find_Decl> *find_interfaces(Goresult *target, bool search_everywhere);
    List<Goresult> *list_interface_methods(Goresult *interface);
    bool list_interface_methods(Goresult *interface, List<Goresult> *out);

    void fill_generate_implementation(List<Go_Symbol> *out, bool selected_interface);
    bool list_type_methods(ccstr type_name, ccstr import_path, List<Goresult> *out);
    bool is_gotype_error(Goresult *res);
    bool is_import_path_internal(ccstr import_path);

    List<Call_Hier_Node>* generate_caller_hierarchy(Goresult *declres);
    void actually_generate_caller_hierarchy(Goresult *declres, List<Call_Hier_Node> *out);

    List<Call_Hier_Node>* generate_callee_hierarchy(Goresult *declres);
    void actually_generate_callee_hierarchy(Goresult *declres, List<Call_Hier_Node> *out, List<Seen_Callee_Entry> *seen = NULL);
    ccstr get_godecl_recvname(Godecl *it);
    Goresult *get_reference_decl(Go_Reference *it, Go_Ctx *ctx);
    Godecl *find_toplevel_containing(Go_File *file, cur2 start, cur2 end);
    Goresult *find_enclosing_toplevel(ccstr filepath, cur2 pos);
};

void walk_ast_node(Ast_Node *node, bool abstract_only, Walk_TS_Callback cb);
void find_nodes_containing_pos(Ast_Node *root, cur2 pos, bool abstract_only, fn<Walk_Action(Ast_Node *it)> callback, bool end_inclusive = false);

Ast_Node *new_ast_node(TSNode node, Parser_It *it);

#define FOR_NODE_CHILDREN(node) for (auto it = (node)->child(); !it->null; it = it->next())
#define FOR_ALL_NODE_CHILDREN(node) for (auto it = (node)->child_all(); !it->null; it = it->next_all())

Goresult *make_goresult(Gotype *gotype, Go_Ctx *ctx);
Goresult *make_goresult(Godecl *decl, Go_Ctx *ctx);

TSParser *new_ts_parser();

template<typename T>
T *read_object(Index_Stream *s) {
    auto size = s->read2();
    if (!size) return NULL;

    // TODO: i mean, don't literally crash the program, show an error and
    // rebuild the index or something
    cp_assert(size == sizeof(T), "size mismatch while reading object from index");

    auto obj = alloc_object(T);
    s->readn(obj, size);
    obj->read(s);
    return obj;
}

template<typename T>
List<T> *read_list(Index_Stream *s) {
    auto len = s->read4();
    auto ret = alloc_object(List<T>);
    ret->init(LIST_POOL, len);
    for (u32 i = 0; i < len; i++)
        ret->append(read_object<T>(s));
    return ret;
}

template<typename T>
void write_object(T *obj, Index_Stream *s) {
    if (!obj) {
        s->write2(0);
        return;
    }

    s->write2(sizeof(T));
    s->writen(obj, sizeof(T));
    obj->write(s);
}

template<typename L>
void write_list(L arr, Index_Stream *s) {
    if (!arr) {
        s->write4(0);
        return;
    }
    s->write4(arr->len);
    For (*arr) write_object(&it, s);
}

ccstr format_cur(cur2 c);

struct Type_Renderer;
typedef fn<bool(Type_Renderer*, Gotype*)> Type_Renderer_Handler;

struct Type_Renderer : public Text_Renderer {
    bool full;

    void write_type(Gotype *t, Type_Renderer_Handler custom_handler, bool omit_func_keyword = false);

    void write_type(Gotype *t, bool omit_func_keyword = false) {
        write_type(t, [&](auto, auto) -> bool { return false; }, omit_func_keyword);
    }
};

bool is_name_special_function(ccstr name);
