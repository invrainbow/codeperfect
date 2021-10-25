#pragma once

#include "common.hpp"
#include "buffer.hpp"
#include "utils.hpp"
#include "mem.hpp"
#include "os.hpp"
#include "tree_sitter_crap.hpp"

extern "C" TSLanguage *tree_sitter_go();

extern const unsigned char GO_INDEX_MAGIC_BYTES[3];
extern const int GO_INDEX_VERSION;

enum {
    CUSTOM_HASH_BUILTINS = 1,
    // other custom packages? can't imagine there will be anything else
};

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
    TS_INT_LITERAL = 78,
    TS_FLOAT_LITERAL = 79,
    TS_IMAGINARY_LITERAL = 80,
    TS_RUNE_LITERAL = 81,
    TS_NIL = 82,
    TS_TRUE = 83,
    TS_FALSE = 84,
    TS_COMMENT = 85,
    TS_RAW_STRING_LITERAL = 86,
    TS_INTERPRETED_STRING_LITERAL = 87,
    TS_SOURCE_FILE = 88,
    TS_PACKAGE_CLAUSE = 89,
    TS_IMPORT_DECLARATION = 90,
    TS_IMPORT_SPEC = 91,
    TS_DOT = 92,
    TS_IMPORT_SPEC_LIST = 93,
    TS_DECLARATION = 94,
    TS_CONST_DECLARATION = 95,
    TS_CONST_SPEC = 96,
    TS_VAR_DECLARATION = 97,
    TS_VAR_SPEC = 98,
    TS_FUNCTION_DECLARATION = 99,
    TS_METHOD_DECLARATION = 100,
    TS_PARAMETER_LIST = 101,
    TS_PARAMETER_DECLARATION = 102,
    TS_VARIADIC_PARAMETER_DECLARATION = 103,
    TS_TYPE_ALIAS = 104,
    TS_TYPE_DECLARATION = 105,
    TS_TYPE_SPEC = 106,
    TS_EXPRESSION_LIST = 107,
    TS_PARENTHESIZED_TYPE = 108,
    TS_SIMPLE_TYPE = 109,
    TS_POINTER_TYPE = 110,
    TS_ARRAY_TYPE = 111,
    TS_IMPLICIT_LENGTH_ARRAY_TYPE = 112,
    TS_SLICE_TYPE = 113,
    TS_STRUCT_TYPE = 114,
    TS_FIELD_DECLARATION_LIST = 115,
    TS_FIELD_DECLARATION = 116,
    TS_INTERFACE_TYPE = 117,
    TS_METHOD_SPEC_LIST = 118,
    TS_METHOD_SPEC = 119,
    TS_MAP_TYPE = 120,
    TS_CHANNEL_TYPE = 121,
    TS_FUNCTION_TYPE = 122,
    TS_BLOCK = 123,
    TS_STATEMENT_LIST = 124,
    TS_STATEMENT = 125,
    TS_EMPTY_STATEMENT = 126,
    TS_SIMPLE_STATEMENT = 127,
    TS_SEND_STATEMENT = 128,
    TS_RECEIVE_STATEMENT = 129,
    TS_INC_STATEMENT = 130,
    TS_DEC_STATEMENT = 131,
    TS_ASSIGNMENT_STATEMENT = 132,
    TS_SHORT_VAR_DECLARATION = 133,
    TS_LABELED_STATEMENT = 134,
    TS_EMPTY_LABELED_STATEMENT = 135,
    TS_FALLTHROUGH_STATEMENT = 136,
    TS_BREAK_STATEMENT = 137,
    TS_CONTINUE_STATEMENT = 138,
    TS_GOTO_STATEMENT = 139,
    TS_RETURN_STATEMENT = 140,
    TS_GO_STATEMENT = 141,
    TS_DEFER_STATEMENT = 142,
    TS_IF_STATEMENT = 143,
    TS_FOR_STATEMENT = 144,
    TS_FOR_CLAUSE = 145,
    TS_RANGE_CLAUSE = 146,
    TS_EXPRESSION_SWITCH_STATEMENT = 147,
    TS_EXPRESSION_CASE = 148,
    TS_DEFAULT_CASE = 149,
    TS_TYPE_SWITCH_STATEMENT = 150,
    TS_TYPE_SWITCH_HEADER = 151,
    TS_TYPE_CASE = 152,
    TS_SELECT_STATEMENT = 153,
    TS_COMMUNICATION_CASE = 154,
    TS_EXPRESSION = 155,
    TS_PARENTHESIZED_EXPRESSION = 156,
    TS_CALL_EXPRESSION = 157,
    TS_VARIADIC_ARGUMENT = 158,
    TS_SPECIAL_ARGUMENT_LIST = 159,
    TS_ARGUMENT_LIST = 160,
    TS_SELECTOR_EXPRESSION = 161,
    TS_INDEX_EXPRESSION = 162,
    TS_SLICE_EXPRESSION = 163,
    TS_TYPE_ASSERTION_EXPRESSION = 164,
    TS_TYPE_CONVERSION_EXPRESSION = 165,
    TS_COMPOSITE_LITERAL = 166,
    TS_LITERAL_VALUE = 167,
    TS_KEYED_ELEMENT = 168,
    TS_ELEMENT = 169,
    TS_FUNC_LITERAL = 170,
    TS_UNARY_EXPRESSION = 171,
    TS_BINARY_EXPRESSION = 172,
    TS_QUALIFIED_TYPE = 173,
    TS_SOURCE_FILE_REPEAT1 = 174,
    TS_IMPORT_SPEC_LIST_REPEAT1 = 175,
    TS_CONST_DECLARATION_REPEAT1 = 176,
    TS_CONST_SPEC_REPEAT1 = 177,
    TS_VAR_DECLARATION_REPEAT1 = 178,
    TS_PARAMETER_LIST_REPEAT1 = 179,
    TS_TYPE_DECLARATION_REPEAT1 = 180,
    TS_FIELD_NAME_LIST_REPEAT1 = 181,
    TS_EXPRESSION_LIST_REPEAT1 = 182,
    TS_FIELD_DECLARATION_LIST_REPEAT1 = 183,
    TS_METHOD_SPEC_LIST_REPEAT1 = 184,
    TS_STATEMENT_LIST_REPEAT1 = 185,
    TS_EXPRESSION_SWITCH_STATEMENT_REPEAT1 = 186,
    TS_TYPE_SWITCH_STATEMENT_REPEAT1 = 187,
    TS_TYPE_CASE_REPEAT1 = 188,
    TS_SELECT_STATEMENT_REPEAT1 = 189,
    TS_ARGUMENT_LIST_REPEAT1 = 190,
    TS_LITERAL_VALUE_REPEAT1 = 191,
    TS_FIELD_IDENTIFIER = 192,
    TS_LABEL_NAME = 193,
    TS_PACKAGE_IDENTIFIER = 194,
    TS_TYPE_IDENTIFIER = 195,
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

        ccstr import_path;
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
    // bool is_embedded; // use field->name == NULL instead
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
            ccstr x;
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
        if (files == NULL) return;
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
        for (auto it = node->children; it != NULL; it = it->next)
            if (streqi(it->name, name))
                return it;

        if (create_if_not_found) {
            auto ret = alloc_object(Node);
            ret->next = node->children;
            ret->name = our_strcpy(name);
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
        resolved_path = normalize_path_sep(our_strcpy(resolved_path));
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
            if (curr == NULL) break;

            if (curr->value != NULL) {
                last_value = curr->value;
                last_index = i + 1;
            }
        }

        if (last_value == NULL) return NULL;

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

struct Gohelper {
    Process proc;
    bool returned_error;

    void init(ccstr cmd, ccstr path);
    void cleanup();
    ccstr readline();
    int readint();
    ccstr run(ccstr op, ...);
};

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
};

struct Go_Indexer {
    ccstr goroot;
    // ccstr gopath;
    ccstr gomodcache;

    Pool mem;        // mem that exists for lifetime of Go_Indexer
    Pool final_mem;  // memory that holds the final value of `this->index`
    Pool ui_mem;     // memory used by UI when it calls jump to definition, etc.

    Gohelper gohelper_dynamic;

    Pool scoped_table_mem;

    Go_Index index;

    char current_exe_path[MAX_PATH];

    Thread_Handle bgthread;

    Module_Resolver module_resolver;

    // why am i even using a scoped_table here?
    Scoped_Table<Go_Package*> package_lookup;

    Message_Queue<Go_Message> message_queue;

    Lock lock;
    bool ready;
    int open_starts;

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
    ccstr get_filepath_from_ctx(Go_Ctx *ctx);
    Goresult *resolve_type(Gotype *type, Go_Ctx *ctx);
    Goresult *unpointer_type(Gotype *type, Go_Ctx *ctx);
    List<Godecl> *parameter_list_to_fields(Ast_Node *params);
    Gotype *node_to_gotype(Ast_Node *node);
    Goresult *find_decl_of_id(ccstr id, cur2 id_pos, Go_Ctx *ctx, Go_Import **single_import = NULL);
    void list_struct_fields(Goresult *type, List<Goresult> *ret);
    void list_fields_and_methods(Goresult *type_res, Goresult *resolved_type_res, List<Goresult> *ret);
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
    List<Goresult> *get_dot_completions(Ast_Node *operand_node, bool *was_package, Go_Ctx *ctx);
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
    Go_Package_Status get_package_status(ccstr import_path);
    void replace_package_name(Go_Package *pkg, ccstr package_name);
    u64 hash_file(ccstr filepath);
    void start_writing();
    void stop_writing();
    bool truncate_parsed_file(Parsed_File *pf, cur2 end_pos, ccstr chars_to_append);
    Gotype *get_closest_function(ccstr filepath, cur2 pos);
    void fill_goto_symbol();
    void init_builtins(Go_Package *pkg);
    void import_decl_to_goimports(Ast_Node *decl_node, ccstr filename, List<Go_Import> *out);
    bool check_if_still_in_parameter_hint(ccstr filepath, cur2 cur, cur2 hint_start);
    Go_File *find_gofile_from_ctx(Go_Ctx *ctx);

    List<Find_References_File>* find_all_references(ccstr filepath, cur2 pos);
    List<Find_References_File>* find_all_references(Goresult *declres);
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
    auto ret = alloc_object(List<T>);
    ret->init(LIST_POOL, len);
    for (u32 i = 0; i < len; i++)
        ret->append(read_object<T>(s));
    return ret;
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

ccstr format_cur(cur2 c);

typedef struct _GH_Build_Error {
    char* text;
    int32_t is_valid;
    char *filename;
    int32_t line;
    int32_t col;
    int32_t is_vcol;
} GH_Build_Error;

typedef struct _GH_Message {
	char* text;
	char* title;
	int32_t is_panic;
} GH_Message;

typedef signed char GoInt8;
typedef unsigned char GoUint8;
typedef short GoInt16;
typedef unsigned short GoUint16;
typedef int GoInt32;
typedef unsigned int GoUint32;
typedef long long GoInt64;
typedef unsigned long long GoUint64;
typedef GoInt64 GoInt;
typedef GoUint64 GoUint;
typedef size_t GoUintptr;
typedef float GoFloat32;
typedef double GoFloat64;
typedef GoUint8 GoBool;

extern GoBool (*GHStartBuild)(char* cmdstr);
extern void (*GHStopBuild)();
extern void (*GHFreeBuildStatus)(void* p, GoInt lines);
extern GH_Build_Error* (*GHGetBuildStatus)(GoInt* pstatus, GoInt* plines);
extern char* (*GHGetGoEnv)(char* name);
extern void (*GHFmtStart)();
extern void (*GHFmtAddLine)(char* line);
extern char* (*GHFmtFinish)(GoBool sortImports);
extern void (*GHFree)(void* p);
extern GoBool (*GHGitIgnoreInit)(char* repo);
extern GoBool (*GHGitIgnoreCheckFile)(char* file);
extern void (*GHAuthAndUpdate)();
extern GoBool (*GHRenameFileOrDirectory)(char* oldpath, char* newpath);
extern void (*GHEnableDebugMode)();
extern GoInt (*GHGetVersion)();
extern char* (*GHGetGoBinaryPath)();
extern char* (*GHGetDelvePath)();
extern char* (*GHGetGoroot)();
// extern char* (*GHGetGopath)();
extern char* (*GHGetGomodcache)();
extern GoBool (*GHGetMessage)(void* p);
extern void (*GHFreeMessage)(void* p);
extern GoBool (*GHInitConfig)();

void load_gohelper();

extern int gh_version;

template<typename T>
T *clone(T *old) {
    auto ret = alloc_object(T);
    memcpy(ret, old, sizeof(T));
    return ret;
}
