#pragma once

#include "common.hpp"
#include "buffer.hpp"
#include "utils.hpp"

#include "uthash.h"

enum ItType {
    IT_INVALID = 0,
    IT_FILE,
    IT_STRING,
    IT_BUFFER,
};

struct Parser_It {
    ItType type;

    union {
        struct {
            FILE* file;
        } file_params;

        struct {
            cur2 pos;
            ccstr string; // TODO: unicode
            s32 len;
        } string_params;

        struct {
            Buffer_It it;
        } buffer_params;
    };

    void init(FILE* _file) {
        ptr0(this);
        type = IT_FILE;

        file_params.file = _file;
    };

    void init(ccstr str, s32 _len) {
        ptr0(this);
        type = IT_STRING;

        string_params.string = str;
        string_params.len = _len;
        string_params.pos = new_cur2(0, 0);
    }

    void init(Buffer* buf) {
        ptr0(this);
        type = IT_BUFFER;

        buffer_params.it.buf = buf;
    }

    u8 peek() {
        switch (type) {
            case IT_FILE:
                {
                    auto ret = fgetc(file_params.file);
                    ungetc(ret, file_params.file);
                    return ret;
                }
            case IT_STRING:
                return string_params.string[string_params.pos.x];
            case IT_BUFFER:
                return (u8)buffer_params.it.peek();
        }
        return 0;
    }

    u8 next() {
        switch (type) {
            case IT_FILE:
                return (u8)fgetc(file_params.file);
            case IT_STRING:
                {
                    auto ret = peek();
                    string_params.pos.x++;
                    return ret;
                }
            case IT_BUFFER:
                return (u8)buffer_params.it.next();
        }
        return 0;
    }

    bool eof() {
        switch (type) {
            case IT_FILE:
                return (bool)feof(file_params.file);
            case IT_STRING:
                return (string_params.pos.x == string_params.len);
            case IT_BUFFER:
                return buffer_params.it.eof();
        }
        return false;
    }

    cur2 get_pos() {
        // TODO: to convert between pos types based on this->type
        switch (type) {
            case IT_STRING:
                return new_cur2(string_params.pos.x, -1);
            case IT_FILE:
                return new_cur2(ftell(file_params.file), -1);
            case IT_BUFFER:
                return buffer_params.it.pos;
        }
        return new_cur2(0, 0);
    }

    void set_pos(cur2 pos) {
        // TODO: to convert between pos types based on this->type
        switch (type) {
            case IT_STRING:
                string_params.pos = pos;
                break;
            case IT_FILE:
                fseek(file_params.file, pos.x, SEEK_SET);
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

enum TokType {
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

ccstr tok_type_str(TokType type);

enum TokImagType {
    IMAG_INT,
    IMAG_FLOAT,
};

union TokenVal {
    u32 int_val;
    float float_val;
    struct {
        TokImagType type;
        union {
            int int_val;
            float float_val;
        };
    } imag_val;
    char char_val; // TODO: unicode
    ccstr string_val;
};

struct Token {
    TokType type;
    ccstr lit;
    cur2 start_before_leading_whitespace;
    cur2 start;
    cur2 end;
    TokenVal val;
};

enum AstType {
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

ccstr ast_type_str(AstType type);

enum AstChanDirection {
    AST_CHAN_RECV,
    AST_CHAN_SEND,
    AST_CHAN_BI,
};

ccstr ast_chan_direction_str(AstChanDirection dir);

struct Ast {
    AstType type;
    cur2 start;
    cur2 end;

    union {
        int extra_data_start;

        struct {
            Ast* signature;
            Ast* body;
        } func_lit;

        struct {
            TokType type;
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
            TokType op;
            Ast* rhs;
        } unary_expr;

        List<Ast*> list;

        struct {
            TokType op;
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
            AstChanDirection direction;
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
            TokType branch_type;
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
            TokType spec_type;
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
            TokType op;
            Ast* rhs;
        } assign_stmt;

        struct {
            TokType op;
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

struct File_Ast {
    Ast* ast;
    ccstr file;
};

struct AC_Result {
    ccstr name;
    // TODO: what else here?
    // File_Ast* decl;
};

enum AutocompleteType {
    AUTOCOMPLETE_NONE = 0,
    AUTOCOMPLETE_STRUCT_FIELDS,
    AUTOCOMPLETE_PACKAGE_EXPORTS,
    AUTOCOMPLETE_IDENTIFIER,
};

struct Autocomplete {
    List<AC_Result>* results;
    AutocompleteType type;
    cur2 keyword_start_position;
};

struct Parameter_Hint {
    Ast* signature;
    Ast* call_args;
};

enum WalkAction {
    WALK_CONTINUE,
    WALK_ABORT,
    WALK_SKIP_CHILDREN,
};

typedef fn<WalkAction(Ast* ast, ccstr name, int depth)> WalkAstFn;

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

OpPrec op_to_prec(TokType type);

typedef fn<bool(TokType)> SyncLookupFunc;

bool lookup_stmt_start(TokType type);
bool lookup_decl_start(TokType type);
bool lookup_expr_end(TokType type);

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

    Arena arena;
    Entry* table;
    List<Overwrite> overwrites;
    List<u32> scopes;

    void init() {
        table = NULL;
        arena.init();
        overwrites.init(LIST_MALLOC, 100);
        scopes.init(LIST_MALLOC, 100);
    }

    void cleanup() {
        Entry* curr;
        Entry* tmp;

        HASH_ITER(hh, table, curr, tmp) {
            HASH_DEL(table, curr);
        }

        arena.cleanup();
        overwrites.cleanup();
        scopes.cleanup();
    }

    void open_scope() { scopes.append(overwrites.len); }

    Entry* alloc_entry() {
        return (Entry*)arena.alloc(sizeof(Entry));
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
    TokType last_token_type;
    ccstr filepath; // for debugging purposes

    void init(Parser_It* _it, ccstr _filepath = NULL);
    void cleanup();
    Ast_List new_list();
    void synchronize(SyncLookupFunc lookup);
    void _throwError(ccstr s);
    Ast* new_ast(AstType type, cur2 start);
    cur2 _expect(TokType want, ccstr file, u32 line);
    Ast* parse_id_list();
    Ast* parse_decl(SyncLookupFunc sync_lookup, bool import_allowed = false);
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
    bool eat_comma(TokType closing, ccstr context);
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

enum Go_Mod_DirectiveType {
    GOMOD_DIRECTIVE_REQUIRE,
    GOMOD_DIRECTIVE_REPLACE,
};

struct Go_Mod_Directive {
    Go_Mod_DirectiveType type;
    ccstr module_path;
    ccstr module_version;

    // for replace
    ccstr replace_path; 
    ccstr replace_version; 
};

struct Go_Mod_Info {
    ccstr module_path;
    ccstr go_version;
    List<Go_Mod_Directive*> directives;
};

enum GoModTokType {
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

ccstr gomod_tok_type_str(GoModTokType t);

struct Go_Mod_Token {
    GoModTokType type;
    ccstr val;
    bool val_truncated;
};

struct Go_Mod_Parser {
    Parser_It* it;
    Go_Mod_Token tok;

    void lex();
    void parse(Go_Mod_Info* info);
};

ccstr path_join(ccstr a, ccstr b);
bool dir_exists(ccstr path);
List<ccstr>* list_source_files(ccstr dirpath);
ccstr get_package_name_from_file(ccstr filepath);

struct Resolved_Import {
    ccstr resolved_path;
    ccstr package_name;
};

Resolved_Import* resolve_import(ccstr import_path);

struct Loaded_It {
    FILE* f;
    Parser_It* it;

    void cleanup() {
        if (f != NULL) {
            fclose(f);
            f = NULL;
        }
    }
};

extern fn<Loaded_It* (ccstr filepath)> file_loader;

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

struct Named_Decl {
    File_Ast* decl;

    struct Name {
        Ast* name;
        Ast* spec;
    };

    List<Name>* names;
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
    IET_VALUE,
    IET_TYPE,
    IET_FUNC,
};

enum Import_Location {
    IMPLOC_GOPATH = 0,
    IMPLOC_GOROOT = 1,
    IMPLOC_GOMOD = 2,
    IMPLOC_VENDOR = 3,
};

struct Index_Entry_Hdr {
    Index_Entry_Type type;
    cur2 pos;
    Import_Location loctype;
    // we need the path as well
};

struct Index_Entry_Result {
    Index_Entry_Hdr hdr;
    ccstr filename;
    ccstr name;
};

enum Index_File_Type {
    INDEX_FILE_INDEX,
    INDEX_FILE_DATA,
    INDEX_FILE_IMPORTS,
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
    Index_Stream findex;
    Index_Stream fdata;
    u32 decl_count_offset;

    struct Decl_To_Write {
        ccstr name;
        u32 offset;
    };

    // u32 decl_count;
    List<Decl_To_Write> decls;

    int init(ccstr import_path);
    void cleanup();
    bool finish_writing();
    bool write_decl(ccstr filename, ccstr name, Ast* spec, Import_Location loctype);
};

struct Index_Reader {
    Index_Stream findex;
    Index_Stream fdata;
    u32 decl_count;
    bool ok;

    bool init(ccstr index_path);
    bool find_decl(ccstr decl_name, Index_Entry_Result *res);
};

struct Golang {
    bool match_import_spec(Ast* import_spec, ccstr want);
    s32 count_decls_in_source(File_Ast* source, int flags);
    List<Named_Decl>* list_decls_in_source(File_Ast* source, int flags, List<Named_Decl>* out = NULL);
    void list_decls_in_source(File_Ast* source, int flags, fn<void(Named_Decl*)> fn);
    File_Ast* find_decl_in_index(ccstr import_path, ccstr desired_decl_name);
    File_Ast* find_decl_in_source(File_Ast* source, ccstr desired_decl_name, bool import_only = false);
    File_Ast* find_decl_in_package(ccstr path, ccstr desired_decl_name, ccstr import_path);
    List<Named_Decl>* list_decls_in_package(ccstr path);
    File_Ast* find_decl_of_id(File_Ast* fa);
    File_Ast* get_base_type(File_Ast* type);
    File_Ast* make_file_ast(Ast* ast, ccstr file = NULL);
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

    void build_index();
    void read_index();
    bool delete_index();
    void crawl_package(ccstr root, ccstr import_path, Import_Location loctype);
};

// Our index maintains and provides THE ETERNALLY CORRECT SOURCE OF TRUTH about
// the following questions (for now):
//
// 1) Our project's dependencies -- WHAT ARE THEY
// 2) Given an import path -- WHERE IS THE PACKAGE
// 3) Given a decl -- WHERE IS IT DECLARED (down to file:pos)
// 
// See the top of go.cpp.
struct Index {
    void update_with_package(ccstr path, ccstr root, ccstr import_path, Import_Location loctype);

    void main_loop();
};
