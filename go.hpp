#pragma once

#include "common.hpp"
#include "buffer.hpp"
#include "utils.hpp"

#include "os.hpp"
#include "uthash.h"

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

    void cleanup() {
        if (type == IT_MMAP) free_entire_file(mmap_params.ef);
    }

    void init(Buffer* buf) {
        ptr0(this);
        type = IT_BUFFER;

        buffer_params.it.buf = buf;
    }

    u8 peek() {
        switch (type) {
            case IT_MMAP:
                return mmap_params.ef->data[mmap_params.pos.x];
            case IT_BUFFER:
                return (u8)buffer_params.it.peek();
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

    bool eof() {
        switch (type) {
            case IT_MMAP:
                return (mmap_params.pos.x == mmap_params.ef->len);
            case IT_BUFFER:
                return buffer_params.it.eof();
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

bool isid(int c);
bool ishex(int c);
bool isoct(int c);

enum Tok_Type {
    TOK_ILLEGAL,
    TOK_EOF,
    TOK_COMMENT,
    TOK_ID,                         // main
    TOK_INT,                        // 12345
    TOK_FLOAT,                    // 123.45
    TOK_IMAG,                     // 123.45i
    TOK_RUNE,                     // 'a'
    TOK_STRING,                 // "abc"
    TOK_ADD,                        // +
    TOK_SUB,                        // -
    TOK_MUL,                        // *
    TOK_QUO,                        // /
    TOK_REM,                        // %
    TOK_AND,                        // &
    TOK_OR,                         // |
    TOK_XOR,                        // ^
    TOK_SHL,                        // <<
    TOK_SHR,                        // >>
    TOK_AND_NOT,                // &^
    TOK_ADD_ASSIGN,         // +=
    TOK_SUB_ASSIGN,         // -=
    TOK_MUL_ASSIGN,         // *=
    TOK_QUO_ASSIGN,         // /=
    TOK_REM_ASSIGN,         // %=
    TOK_AND_ASSIGN,         //, &=
    TOK_OR_ASSIGN,            // |=
    TOK_XOR_ASSIGN,         // ^=
    TOK_SHL_ASSIGN,         // <<=
    TOK_SHR_ASSIGN,         // >>=
    TOK_AND_NOT_ASSIGN, // &^=
    TOK_LAND,                     // &&
    TOK_LOR,                        // ||
    TOK_ARROW,                    // <-
    TOK_INC,                        // ++
    TOK_DEC,                        // --
    TOK_EQL,                        // ==
    TOK_LSS,                        // <
    TOK_GTR,                        // >
    TOK_ASSIGN,                 // =
    TOK_NOT,                        // !
    TOK_NEQ,                        // !=
    TOK_LEQ,                        // <=
    TOK_GEQ,                        // >=
    TOK_DEFINE,                 // :=
    TOK_ELLIPSIS,             // ...
    TOK_LPAREN,                 // (
    TOK_LBRACK,                 // [
    TOK_LBRACE,                 // {
    TOK_COMMA,                    // ,
    TOK_PERIOD,                 // .
    TOK_RPAREN,                 // )
    TOK_RBRACK,                 // ]
    TOK_RBRACE,                 // }
    TOK_SEMICOLON,            // ;
    TOK_COLON,                    // :
    TOK_BREAK,
    TOK_CASE,
    TOK_CHAN,
    TOK_CONST,
    TOK_CONTINUE,
    TOK_DEFAULT,
    TOK_DEFER,
    TOK_ELSE,
    TOK_FALLTHROUGH,
    TOK_FOR,
    TOK_FUNC,
    TOK_GO,
    TOK_GOTO,
    TOK_IF,
    TOK_IMPORT,
    TOK_INTERFACE,
    TOK_MAP,
    TOK_PACKAGE,
    TOK_RANGE,
    TOK_RETURN,
    TOK_SELECT,
    TOK_STRUCT,
    TOK_SWITCH,
    TOK_TYPE,
    TOK_VAR,
};

ccstr tok_type_str(Tok_Type type);

enum Tok_Imag_Type {
    IMAG_INT,
    IMAG_FLOAT,
};

union TokenVal {
    u32 int_val;
    float float_val;
    struct {
        Tok_Imag_Type type;
        union {
            int int_val;
            float float_val;
        };
    } imag_val;
    char char_val; // TODO: unicode
    ccstr string_val;
};

struct Token {
    Tok_Type type;
    ccstr lit;
    cur2 start_before_leading_whitespace;
    cur2 start;
    cur2 end;
    TokenVal val;
};

enum Ast_Type {
    AST_ILLEGAL,
    AST_BASIC_LIT,
    AST_ID,
    AST_UNARY_EXPR,
    AST_BINARY_EXPR,
    AST_ARRAY_TYPE,
    AST_POINTER_TYPE,
    AST_SLICE_TYPE,
    AST_MAP_TYPE,
    AST_STRUCT_TYPE,
    AST_INTERFACE_TYPE,
    AST_CHAN_TYPE,
    AST_FUNC_TYPE,
    AST_PAREN,
    AST_SELECTOR_EXPR,
    AST_SLICE_EXPR,
    AST_INDEX_EXPR,
    AST_TYPE_ASSERTION_EXPR,
    AST_CALL_EXPR,
    AST_CALL_ARGS,
    AST_LITERAL_ELEM,
    AST_LITERAL_VALUE,
    AST_LIST,
    AST_ELLIPSIS,
    AST_FIELD,
    AST_PARAMETERS,
    AST_FUNC_LIT,
    AST_BLOCK,
    AST_SIGNATURE,
    AST_COMPOSITE_LIT,
    AST_GO_STMT,
    AST_DEFER_STMT,
    AST_SELECT_STMT,
    AST_SWITCH_STMT,
    AST_IF_STMT,
    AST_FOR_STMT,
    AST_RETURN_STMT,
    AST_BRANCH_STMT,
    AST_EMPTY_STMT,
    AST_DECL,
    AST_ASSIGN_STMT,
    AST_VALUE_SPEC,
    AST_IMPORT_SPEC,
    AST_TYPE_SPEC,
    AST_INC_DEC_STMT,
    AST_SEND_STMT,
    AST_LABELED_STMT,
    AST_EXPR_STMT,
    AST_PACKAGE_DECL,
    AST_SOURCE,
    AST_STRUCT_FIELD,
    AST_INTERFACE_SPEC,
    AST_CASE_CLAUSE,
    AST_COMM_CLAUSE,
    AST_FUNC_DECL,
    AST_EMPTY,

    _AST_TYPES_COUNT_,
};

ccstr ast_type_str(Ast_Type type);

enum Ast_Chan_Direction {
    AST_CHAN_RECV,
    AST_CHAN_SEND,
    AST_CHAN_BI,
};

ccstr ast_chan_direction_str(Ast_Chan_Direction dir);

struct Ast {
    Ast_Type type;
    cur2 start;
    cur2 end;

    union {
        int extra_data_start;

        struct {
            Ast* signature;
            Ast* body;
        } func_lit;

        struct {
            Tok_Type type;
            Ast* specs;
        } decl;

        struct {
            Ast* params;
            Ast* result;
        } signature;

        struct {
            TokenVal val;
            ccstr lit;
        } basic_lit;

        struct {
            Tok_Type op;
            Ast* rhs;
        } unary_expr;

        List<Ast*> list;

        struct {
            Tok_Type op;
            Ast* lhs;
            Ast* rhs;
        } binary_expr;

        struct {
            ccstr lit;
            Ast* decl;
            Ast* decl_id;
            bool bad;
        } id;

        struct {
            Ast* base_type;
        } slice_type;

        struct {
            Ast* key_type;
            Ast* value_type;
        } map_type;

        struct {
            Ast_Chan_Direction direction;
            Ast* base_type;
        } chan_type;

        struct {
            Ast* base_type;
        } pointer_type;

        struct {
            Ast* length;
            Ast* base_type;
        } array_type;

        struct {
            Ast* x;
        } paren;

        struct {
            Ast* x;
            Ast* sel;
            cur2 period_pos;
        } selector_expr;

        struct {
            Ast* x;
            Ast* s1;
            Ast* s2;
            Ast* s3;
        } slice_expr;

        struct {
            Ast* x;
            Ast* key;
        } index_expr;

        struct {
            Ast* x;
            Ast* type;
        } type_assertion_expr;

        struct {
            Ast* func;
            Ast* call_args;
        } call_expr;

        struct {
            Ast* args;
            bool ellip;
        } call_args;

        struct {
            Ast* key;
            Ast* elem;
        } literal_elem;

        struct {
            Ast* elems;
        } literal_value;

        struct {
            Ast* type;
        } ellipsis;

        struct {
            Ast* ids;
            Ast* type;
        } field;

        struct {
            Ast* fields;
        } parameters;

        struct {
            Ast* stmts;
        } block;

        struct {
            Ast* base_type;
            Ast* literal_value;
        } composite_lit;

        struct {
            Ast* x;
        } go_stmt;

        struct {
            Ast* x;
        } defer_stmt;

        struct {
            Ast* init;
            Ast* cond;
            Ast* body;
            Ast* else_;
        } if_stmt;

        struct {
            Ast* exprs;
        } return_stmt;

        struct {
            Tok_Type branch_type;
            Ast* label;
        } branch_stmt;

        struct {
            Ast* fields;
        } struct_type;

        struct {
            Ast* specs;
        } interface_type;

        struct {
            Ast* signature;
        } func_type;

        struct {
            Ast* ids;
            Ast* type;
            Ast* vals;
            Tok_Type spec_type;
        } value_spec;

        struct {
            Ast* package_name;
            Ast* path;
        } import_spec;

        struct {
            Ast* id;
            Ast* type;
            bool is_alias;
        } type_spec;

        struct {
            Ast* lhs;
            Tok_Type op;
            Ast* rhs;
        } assign_stmt;

        struct {
            Tok_Type op;
            Ast* x;
        } inc_dec_stmt;

        struct {
            Ast* chan;
            Ast* value;
        } send_stmt;

        struct {
            Ast* label;
            Ast* stmt;
        } labeled_stmt;

        struct {
            Ast* x;
        } expr_stmt;

        struct {
            Ast* name;
        } package_decl;

        struct {
            Ast* package_decl;
            Ast* imports;
            Ast* decls;
        } source;

        struct {
            Ast* ids; // NULL means embedded field
            Ast* type;
            Ast* tag;
        } struct_field;

        struct {
            Ast* name;
            Ast* signature;
            Ast* type;
        } interface_spec;

        struct {
            Ast* s1;
            Ast* s2;
            Ast* body;
            bool is_type_switch;
        } switch_stmt;

        struct {
            Ast* clauses;
        } select_stmt;

        struct {
            Ast* s1;
            Ast* s2;
            Ast* s3;
            Ast* body;
        } for_stmt;

        struct {
            Ast* vals;
            Ast* stmts;
        } case_clause;

        struct {
            Ast* comm;
            Ast* stmts;
        } comm_clause;

        struct {
            Ast* recv;
            Ast* name;
            Ast* signature;
            Ast* body;
        } func_decl;
    };
};

// an Ast contextualized within a file (full file path) and package (import path)
struct File_Ast {
    Ast* ast;
    ccstr file;
    ccstr import_path;

    File_Ast *dup(Ast *new_ast);
};

struct AC_Result {
    ccstr name;
    // TODO: what else here?
    // File_Ast* decl;
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

struct Parameter_Hint {
    Ast* signature;
    Ast* call_args;
};

enum Walk_Action {
    WALK_CONTINUE,
    WALK_ABORT,
    WALK_SKIP_CHILDREN,
};

typedef fn<Walk_Action(Ast* ast, ccstr name, int depth)> WalkAstFn;

void walk_ast(Ast* root, WalkAstFn fn);

enum OpPrec {
    PREC_LOWEST = 0,
    PREC_LOR,
    PREC_LAND,
    PREC_COMPARE,
    PREC_ADD,
    PREC_MUL,
    PREC_UNARY,
    PREC_HIGHEST,
};

OpPrec op_to_prec(Tok_Type type);

typedef fn<bool(Tok_Type)> Sync_Lookup_Func;

bool lookup_stmt_start(Tok_Type type);
bool lookup_decl_start(Tok_Type type);
bool lookup_expr_end(Tok_Type type);

struct Parse_Error {
    ccstr error;

    Parse_Error(ccstr _error) { error = _error; }
};

const auto INVALID_POS = new_cur2((u32)-1, (u32)-1);

#define PRIME1 151
#define PRIME2 163
#define ITEM_DELETED 0xdeadbeef

u32 next_prime(u32 x);

template <typename T>
struct Scoped_Table {
    struct Overwrite {
        ccstr name;
        T value;
        bool restore;
    };

    struct Entry {
        ccstr key;
        T value;
        UT_hash_handle hh;
    };

    Pool mem;
    Entry* table;
    List<Overwrite> overwrites;
    List<u32> scopes;

    void init() {
        table = NULL;
        mem.init("scoped table mem");
        overwrites.init(LIST_MALLOC, 100);
        scopes.init(LIST_MALLOC, 100);
    }

    void cleanup() {
        Entry* curr;
        Entry* tmp;

        HASH_ITER(hh, table, curr, tmp) {
            HASH_DEL(table, curr);
        }

        mem.cleanup();
        overwrites.cleanup();
        scopes.cleanup();
    }

    void open_scope() { scopes.append(overwrites.len); }

    Entry* alloc_entry() {
        return (Entry*)mem.alloc(sizeof(Entry));
    }

    bool close_scope() {
        if (scopes.len == 0) {
            error("No scopes are open.");
            return false;
        }

        auto off = scopes[scopes.len - 1];

        for (i32 i = overwrites.len; i > off; i--) {
            auto&& it = overwrites[i - 1];
            auto name = it.name;
            if (it.restore) {
                auto entry = alloc_entry();
                entry->key = name;
                entry->value = it.value;
                HASH_ADD_KEYPTR(hh, table, entry->key, strlen(entry->key), entry);
            } else {
                Entry* entry = NULL;
                HASH_FIND_STR(table, name, entry);
                if (entry != NULL) {
                    HASH_DEL(table, entry);
                }
            }
        }

        overwrites.len = off;
        scopes.len--;
        return true;
    }

    T* set(ccstr name) {
        auto p = overwrites.append();
        p->name = name;

        // do we even need to save overwrite if entry == NULL? I forget how this works
        // TODO: investigate

        Entry* entry = NULL;
        HASH_FIND_STR(table, name, entry);
        if (entry != NULL) {
            p->value = entry->value;
            p->restore = true;
        }

        entry = alloc_entry();
        entry->key = name;
        HASH_ADD_KEYPTR(hh, table, entry->key, strlen(entry->key), entry);
        return &entry->value;
    }

    void set(ccstr name, T value) { *set(name) = value; }

    T get(ccstr name) {
        Entry* entry = NULL;
        HASH_FIND_STR(table, name, entry);
        if (entry == NULL) {
            T ret;
            ptr0(&ret);
            return ret;
        }
        return entry->value;
    }
};

Ast* unparen(Ast* ast);

struct Decl {
    Ast* decl_ast;
    Ast* where_to_jump;
};

typedef List<Decl> DeclList;
typedef fn<void(DeclList*)> DumpSymbolTableFunc;

struct Parser;

struct Ast_List {
    Parser* parser;
    s32 start;
    s32 len;
    cur2 start_pos;

    void init(Parser* _parser, cur2 _start_pos);
    void init(Parser* _parser);
    void push(Ast* ast);
    Ast* save();
};

struct Parser {
    Scoped_Table<Decl> decl_table;
    Parser_It* it;
    struct {
        Ast** buf;
        s32 cap;
        s32 sp;
    } list_mem;
    i32 expr_lev; // < 0: in control clause, >= 0: in expression
    Token tok;
    Tok_Type last_token_type;
    ccstr filepath; // for debugging purposes

    void init(Parser_It* _it, ccstr _filepath = NULL, bool no_lex = false);
    void cleanup();
    void reinit_without_lex();
    Ast_List new_list();
    void synchronize(Sync_Lookup_Func lookup);
    void _throwError(ccstr s);
    Ast* new_ast(Ast_Type type, cur2 start);
    cur2 _expect(Tok_Type want, ccstr file, u32 line);
    Ast* parse_id_list();
    Ast *parse_spec(Tok_Type spec_type, bool *previous_spec_contains_iota = NULL);
    Ast* parse_decl(Sync_Lookup_Func sync_lookup, bool import_allowed = false);
    void declare_var(ccstr name, Ast* decl, Ast* jump_to);
    Ast* parse_parameters(bool ellipsis_ok);
    Ast* parse_result();
    Ast* parse_signature();
    Ast* parse_id();
    Ast* parse_type(bool is_param = false);
    Ast* parse_simple_stmt();
    Ast* parse_stmt();
    Ast* parse_stmt_list();
    Ast* parse_block();
    Ast* parse_call_args();
    Ast* parse_primary_expr(bool lhs);
    Ast* parse_literal_value();
    bool eat_comma(Tok_Type closing, ccstr context);
    Ast* parse_expr(bool lhs, OpPrec prec = PREC_LOWEST);
    Ast* parse_type_list();
    void resolve(Ast* id);
    Ast* parse_expr_list(bool lhs = false);
    Ast* parse_package_decl();
    Ast* parse_file();
    Ast* fill_end(Ast* ast);
    void lex_with_comments();
    cur2 lex();
    void parser_error(ccstr fmt, ...);
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
};

struct Gomod_Info {
    ccstr module_path;
    ccstr go_version;
    List<Gomod_Directive*> directives;
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

struct Gomod_Parser {
    Parser_It* it;
    Gomod_Token tok;

    void lex();
    void parse(Gomod_Info* info);
};

ccstr _path_join(ccstr a, ...);
#define path_join(...) _path_join(__VA_ARGS__, NULL)

bool dir_exists(ccstr path);
ccstr get_package_name_from_file(ccstr filepath);

enum Import_Location {
    IMPLOC_GOPATH = 0,
    IMPLOC_GOROOT = 1,
    IMPLOC_GOMOD = 2,
    IMPLOC_VENDOR = 3,
};

struct Resolved_Import {
    ccstr path;
    ccstr package_name;
    Import_Location location_type;
};

Resolved_Import* resolve_import(ccstr import_path);

struct Loaded_It {
    Entire_File *ef;
    Parser_It* it;

    void cleanup() {
        if (ef == NULL) return;
        free_entire_file(ef);
        ef = NULL;
    }
};

extern fn<Parser_It* (ccstr filepath)> file_loader;

Ast* parse_file_into_ast(ccstr filepath);

/*
where are possible locations for the cursor to jump from?
    identifier x
        go to decl of x
    selector x.y
        if x is a package, go to decl of y in package x
        if x has type foo, go to field of struct foo
*/

#define PRIMITIVE_TYPE (Ast *)NULL

i32 locate_pos_relative_to_ast(cur2 pos, Ast* ast);

struct Jump_To_Definition_Result {
    ccstr file;
    cur2 pos;
};

struct Infer_Res {
    File_Ast* type;
    File_Ast* base_type;
};

struct Named_Decl_Item {
    Ast* name;
    Ast* spec;
};

struct Named_Decl {
    File_Ast* decl;
    List<Named_Decl_Item>* items;
};

enum {
    LISTDECLS_IMPORTS = (1 << 0),
    LISTDECLS_DECLS = (1 << 1),
};

struct Field {
    File_Ast id;
    File_Ast decl;
};

enum Index_Entry_Type {
    IET_INVALID = 0,
    IET_CONST,
    IET_VAR,
    IET_TYPE,
    IET_FUNC,
};

struct Index_Entry_Hdr {
    Index_Entry_Type type;
    cur2 pos;
};

struct Index_Entry_Result {
    Index_Entry_Hdr hdr;
    ccstr filename;
    ccstr name;

    // this information now in index_reader::meta
    // ccstr package_path;
    // Import_Location loctype;
};

enum Index_File_Type {
    INDEX_FILE_INDEX,
    INDEX_FILE_DATA,
    INDEX_FILE_IMPORTS,
    INDEX_FILE_HASH,
};

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
    bool write8(i64 x);
    bool writestr(ccstr s);
    bool write_file_header(Index_File_Type type);
    void readn(void* buf, s32 n);
    char read1();
    i16 read2();
    i32 read4();
    ccstr readstr();
    bool read_file_header(Index_File_Type wanted_type);
};

struct Index_Writer {
    ccstr index_path;
    Index_Stream findex;
    Index_Stream fdata;

    struct Decl_To_Write {
        ccstr name;
        u32 offset;
    };

    // u32 decl_count;
    List<Decl_To_Write> decls;

    void write_package_hash();
    int init(ccstr import_path);
    void write_headers(ccstr resolved_path, Import_Location loctype);
    void cleanup();
    bool finish_writing();
    bool write_decl(ccstr filename, ccstr name, Ast* spec);
    bool write_hash(u64 hash);
};

struct Index_Reader {
    ccstr index_path;
    ccstr indexfile_path;
    ccstr datafile_path;
    List<i32> *decl_offsets;

    struct {
        ccstr package_path;
        Import_Location loctype;
    } meta;

    // Index_Stream findex;
    Index_Stream fdata;
    // bool ok;

    // honestly having an index_reader class is not factoring out well
    // abstractions suck lol

    bool init(ccstr _index_path);
    List<Index_Entry_Result> *list_decls();
    bool find_decl(ccstr decl_name, Index_Entry_Result *res);
    void cleanup();
};

// Our index maintains and provides THE ETERNALLY CORRECT SOURCE OF TRUTH about
// the following questions (for now):
//
// 1) Our project's dependencies -- WHAT ARE THEY
// 2) Given an import path -- WHERE IS THE PACKAGE
// 3) Given a decl -- WHERE IS IT DECLARED (down to file:pos)
//
// See the top of go.cpp.

struct Eil_Result {
    List<ccstr> *old_imports;
    List<ccstr> *new_imports;
    List<ccstr> *added_imports;
    List<ccstr> *removed_imports;
};

enum Index_Event_Type {
    INDEX_EVENT_FETCH_IMPORTS,
    INDEX_EVENT_REINDEX_PACKAGE,
};

struct Index_Event {
    Index_Event_Type type;
    char import_path[MAX_PATH]; // for reindex_package
};

enum Go_Watch_Type {
    WATCH_WKSP,
    WATCH_GOPATH,
    WATCH_GOROOT,
};

struct Go_Index;

struct Go_Index_Watcher {
    Go_Index *_this;
    Go_Watch_Type type;
    Thread_Handle thread;
    Fs_Watcher watch;
};

#define MAX_INDEX_EVENTS 1024 // don't let file change dos us

struct Go_Index {
    Pool background_mem;
    Pool watcher_mem;
    Pool main_thread_mem;

    List<Index_Event> index_events;
    Lock index_events_lock;
    Lock fs_event_lock;

    // we can get rid of this stupidity when i get async ReadDirectoryChanges working
    Go_Index_Watcher wksp_watcher;
    Go_Index_Watcher gopath_watcher;
    Go_Index_Watcher goroot_watcher;
    Thread_Handle main_loop_thread;

    // this hurts me
    char current_exe_path[MAX_PATH];
    Process buildparser_proc;

    bool init();
    void cleanup();
    void handle_fs_event(Go_Index_Watcher *watcher, Fs_Event *event);

    bool queue_index_event(Index_Event *event);
    bool pop_index_event(Index_Event *event);

    /*
     * re-fetch imports list when:
     *  - file in current workspace changes.
     *      - user saves file from within ide
     *      - file changes from outside.
     *
     * re-crawl a particular package when:
     * (i guess when its hash would change? that would be when?)
     *  - reolved import path changes
     *  - *.go file contents change
     *  - *.go filename changes
     *
     *  weird special case is: when file in current wksp changes, we re-crawl whole wksp? can't be right.
     *
     *  ok so we need to watch for changes on a whole bunch of files
     *
     */

    bool match_import_spec(Ast* import_spec, ccstr want);
    s32 count_decls_in_source(File_Ast* source, int flags);
    List<Named_Decl>* list_decls_in_source(File_Ast* source, int flags, List<Named_Decl>* out = NULL);
    void list_decls_in_source(File_Ast* source, int flags, fn<void(Named_Decl*)> fn);
    File_Ast* find_decl_in_index(ccstr import_path, ccstr desired_decl_name);
    File_Ast* find_decl_in_source(File_Ast* source, ccstr desired_decl_name, bool import_only = false);
    File_Ast* find_decl_in_package(ccstr path, ccstr desired_decl_name, ccstr import_path);
    List<Named_Decl>* list_decls_in_package(ccstr path, ccstr import_path);
    List<ccstr> *list_decl_names_from_index(ccstr import_path);
    List<ccstr> *list_decl_names(ccstr import_path);
    File_Ast* find_decl_of_id(File_Ast* fa, bool import_only = false);
    File_Ast* get_base_type(File_Ast* type);
    // File_Ast* make_file_ast(Ast* ast, ccstr file = NULL);
    File_Ast* make_file_ast(Ast* ast, ccstr file, ccstr import_path);
    File_Ast* get_type_from_decl(File_Ast* decl, ccstr id);
    File_Ast* find_field_or_method_in_type(File_Ast* base_type, File_Ast* interim_type, ccstr name);
    Infer_Res* infer_type(File_Ast* expr, bool resolve = false);
    Ast* locate_id_in_decl(Ast* decl, ccstr id);
    List<Field>* list_fields_in_type(File_Ast* ast);
    int list_fields_in_type(File_Ast* ast, List<Field>* ret);
    int list_methods_in_type(File_Ast* ast, List<Field>* ret);
    int list_methods_in_base_type(File_Ast* ast, List<Field>* ret);
    List<Field>* list_fields_and_methods_in_type(File_Ast* base_type, File_Ast* interim_type);
    ccstr get_id_import_path(File_Ast* ast);
    Jump_To_Definition_Result* jump_to_definition(ccstr filepath, cur2 pos);
    bool autocomplete(ccstr filepath, cur2 pos, bool triggered_by_period, Autocomplete* out);
    Parameter_Hint* parameter_hint(ccstr filepath, cur2 pos, bool triggered_by_paren);

    void read_index();
    bool delete_index();

    bool ensure_entire_index_correct();
    bool ensure_package_correct(ccstr import_path);
    void run_background_thread();
    void handle_error(ccstr err);
    bool ensure_imports_list_correct(Eil_Result *res);
    u64 hash_package(ccstr import_path, Resolved_Import **pres);

    bool run_threads();

    bool is_file_included_in_build(ccstr path);
    List<ccstr>* list_source_files(ccstr dirpath);
    ccstr get_package_name(ccstr path);
    Resolved_Import* resolve_import(ccstr import_path);
};

typedef fn<void(Parser*)> parser_cb;

void with_parser_at_location(ccstr filepath, cur2 location, parser_cb cb);
void with_parser_at_location(ccstr filepath, parser_cb cb);
