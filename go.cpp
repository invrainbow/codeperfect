/*
Our overall index-building process should just be like, "ensure the index exists
and is complete and correct." Let's call this "ensure index."

I mean, right now the algorithm is just:
    - look up the package path
    - go through all decls, find the one we want.

So our index should answer three core questions:
    1) our project's dependencies -- WHAT ARE THEY (ensures completeness)
    2) given an import path -- WHERE IS THE PACKAGE (gives us package path)
    3) given a decl -- WHERE IS IT DECLARED (gives us file:pos)

This ensures, for ALL of our dependencies, we can easily get a full filepath +
position. From there we can just parse the decl.

If we can answer any variant of these three questions within 1ms, intellisense
is effectively solved for now.

(For now because after asking these questions, our intellisense still has to a
bit more work.  Eventually, we would hope to just build up a complete database
of our program's types.)

So we want to ensure that as much of the time as possible, our index contains
the complete, correct answer to these questions. This is what our "ensure
index" process takes care of.

So at any given point our index should contain knowledge about these two
questions, and as the "truth" of these questions changes, our index should
update (in an ideal world, immediately).

Several things that happen here:

 - index runs background process checking if it's correct, fixes if not
 - subscribe to various "hooks" that indicate a change of information, index
   goes and retrieves new info
 - IDE notifies index that it's wrong (e.g. gave wrong result)

And if we build the interface correctly, in the future we can seamlessly switch
out the index for something even faster.
*/

#include "go.hpp"

#include "utils.hpp"
#include "world.hpp"
#include "os.hpp"
#include "set.hpp"
#include "editor.hpp"

// due to casey being casey, this can only be included once in the whole project
#include "meow_hash_x64_aesni.h"

// PRELEASE: dynamically determine this
static const char GOROOT[] = "c:\\go";
static const char GOPATH[] = "c:\\users\\brandon\\go";

const u32 INDEX_MAGIC_NUMBER = 0x49fa98;

#define VA_ARGS(...) , ##__VA_ARGS__
#define get_index_path(...) path_join(world.wksp.path, ".ide", "index" VA_ARGS(__VA_ARGS__))

ccstr format_pos(cur2 pos) {
    if (pos.y == -1)
        return our_sprintf("%d", pos);
    return our_sprintf("%d:%d", pos.y, pos.x);
}

File_Ast *File_Ast::dup(Ast *new_ast) {
    auto ret = alloc_object(File_Ast);
    ret->ast = new_ast;
    ret->file = file;
    ret->import_path = import_path;
    return ret;
}

// i can't believe this but we *may* need to just do interop with go

bool Go_Index::is_file_included_in_build(ccstr path) {
    buildparser_proc.writestr(path);
    buildparser_proc.write1('\n');

    char ch = 0;
    if (!buildparser_proc.read1(&ch))
        panic("buildparser crashed, we're not going to be able to do anything");
    return (ch == 1);
}

// FIXME: will fuck up if we list directory first time, dir changes,
// and then we list second time. fix this somehow
List<ccstr>* Go_Index::list_source_files(ccstr dirpath) {
    auto ret = alloc_list<ccstr>();

    auto save_gofiles = [&](Dir_Entry *ent) {
        if (ent->type == DIRENT_DIR) return;
        if (!str_ends_with(ent->name, ".go")) return;

        // TODO: keep this?
        if (str_ends_with(ent->name, "_test.go")) return;

        {
            SCOPED_FRAME();
            if (!is_file_included_in_build(path_join(dirpath, ent->name))) return;
        }

        ret->append(our_strcpy(ent->name));
    };

    return list_directory(dirpath, save_gofiles) ? ret : NULL;
}

List<ccstr>* list_source_files_golist(ccstr dirpath) {
    Process proc;
    proc.init();
    defer { proc.cleanup(); };
    proc.dir = dirpath;
    proc.use_stdin = false;
    if (!proc.run("go list -json | jq \".GoFiles | .[]\"")) return NULL;

    Process_Status status;
    while ((status = proc.status()) == PROCESS_WAITING) continue;

    if (status != PROCESS_DONE) return NULL;

    Text_Renderer r;
    auto ret = alloc_list<ccstr>();
    char ch = 0;
    r.init();

    while (true) {
        if (!proc.read1(&ch)) break;
        if (ch == '"') continue;
        if (ch == '\r') {
            if (!proc.read1(&ch)) break;
            if (ch != '\n') break;
        }
        if (ch == '\n') {
            ret->append(r.finish());
            r.init();
            continue;
        }
        r.writechar(ch);
    }
    if (r.len > 0) ret->append(r.finish());
    return ret;
}

ccstr Go_Index::get_package_name(ccstr path) {
    if (check_path(path) != CPR_DIRECTORY) return NULL;

    auto files = list_source_files(path);
    if (files == NULL) return NULL;

    For (*files) {
        auto filepath = path_join(path, it);
        auto pkgname = get_package_name_from_file(filepath);
        if (pkgname != NULL) return pkgname;
    }

    return NULL;
}

Resolved_Import* Go_Index::resolve_import(ccstr import_path) {
    if (world.wksp.gomod_exists) {
        // In our go.mod, we have a bunch of directives that basically give us
        // the info, "when you come across this import path, go to this file
        // path to find the package." So our strategy is: go through all these
        // "mappings," see if the import path (the "base path") contains as a
        // subfolder the import path we're trying to resolve.
        //
        // If it does, use that mapping to find the path of the directory we
        // think our import path resolves to. Then check if it's a package.  If
        // so, we've hit gold.

        // represents as path as list of its parts, e.g. "a/b/c" becomes ["a", "b", "c"]

        auto import_path_list = make_path(import_path);

        auto try_mapping = [&](ccstr base_path, ccstr resolve_to) -> Resolved_Import * {
            auto base_path_list = make_path(base_path);
            if (!base_path_list->contains(import_path_list)) return NULL;

            ccstr filepath;
            if (import_path_list->parts->len == base_path_list->parts->len)
                filepath = resolve_to;
            else
                filepath = path_join(resolve_to, get_path_relative_to(import_path, base_path));

            auto package_name = get_package_name(filepath);
            if (package_name == NULL) return NULL;

            auto ret = alloc_object(Resolved_Import);
            ret->path = filepath;
            ret->package_name = package_name;
            ret->location_type = IMPLOC_GOMOD;
            return ret;
        };

        auto get_pkg_mod_path = [&](ccstr import_path, ccstr version) {
            return path_join(GOPATH, our_sprintf("pkg/mod/%s@%s", import_path, version));
        };

#define RETURN_IF(x) { auto ret = (x); if (ret != NULL) return ret; }

        auto& info = world.wksp.gomod_info;
        if (info.module_path != NULL)
            RETURN_IF(try_mapping(info.module_path, world.wksp.path));

        For (info.directives) {
            switch (it->type) {
            case GOMOD_DIRECTIVE_REQUIRE:
                RETURN_IF(try_mapping(it->module_path, get_pkg_mod_path(it->module_path, it->module_version)));
                break;
            case GOMOD_DIRECTIVE_REPLACE:
                {
                    ccstr filepath;
                    if (it->replace_version == NULL)
                        filepath = rel_to_abs_path(it->replace_path);
                    else
                        filepath = get_pkg_mod_path(it->replace_path, it->replace_version);
                    RETURN_IF(try_mapping(it->module_path, filepath));
                }
                break;
            }
        }

#undef RETURN_IF

        // no folder path can be resolved from our go.mod. fallthrough and keep
        // checking vendor, goroot, gopath, etc
    }

    auto paths = alloc_list<ccstr>();
    paths->append(path_join(world.wksp.path, "vendor", import_path));
    paths->append(path_join(GOROOT, "src", import_path));
    paths->append(path_join(GOPATH, "src", import_path));

    auto location_types = alloc_list<Import_Location>();
    location_types->append(IMPLOC_VENDOR);
    location_types->append(IMPLOC_GOPATH);
    location_types->append(IMPLOC_GOROOT);

    for (u32 i = 0; i < paths->len; i++) {
        auto package_name = get_package_name(paths->at(i));
        if (package_name != NULL) {
            auto ret = alloc_object(Resolved_Import);
            ret->path = paths->at(i);
            ret->location_type = location_types->at(i);
            ret->package_name = package_name;
            return ret;
        }
    }

    return NULL;
}

ccstr _path_join(ccstr a, ...) {
    auto get_part_length = [&](ccstr s) {
        auto len = strlen(s);
        if (len > 0 && is_sep(s[len-1])) len--;
        return len;
    };

    va_list vl, vlcount;
    s32 total_len = 0;
    s32 num_parts = 0;

    va_start(vl, a);
    va_copy(vlcount, vl);

    for (;; num_parts++) {
        ccstr val = (num_parts == 0 ? a : va_arg(vlcount, ccstr));
        if (val == NULL) break;
        total_len += get_part_length(val);
    }
    total_len += (num_parts-1); // path separators

    auto ret = alloc_array(char, total_len + 1);
    u32 k = 0;

    for (u32 i = 0; i < num_parts; i++) {
        ccstr val = (i == 0 ? a : va_arg(vl, ccstr));
        auto len = get_part_length(val);

        // copy val into ret, normalizing path separators
        for (u32 j = 0; j < len; j++) {
            auto ch = val[j];
            if (is_sep(ch))
                ch = PATH_SEP;
            ret[k++] = ch;
        }

        if (i + 1 < num_parts)
            ret[k++] = PATH_SEP;
    }

    ret[k] = '\0';
    return ret;
}

ccstr get_package_name_from_file(ccstr filepath) {
    Ast *ast = NULL;
    with_parser_at_location(filepath, [&](Parser *p) {
        ast = p->parse_package_decl();
    });
    if (ast == NULL) return NULL;
    if (ast->package_decl.name == NULL) return NULL;
    return ast->package_decl.name->id.lit;
}

Parser_It* default_file_loader(ccstr filepath) {
    // try to find filepath among editors
    for (auto&& pane : world.wksp.panes) {
        For (pane.editors) {
            if (streq(it.filepath, filepath)) {
                auto iter = alloc_object(Parser_It);
                iter->init(&it.buf);
                return iter;
            }
        }
    }

    auto ef = read_entire_file(filepath);
    if (ef == NULL) return NULL;

    auto it = alloc_object(Parser_It);
    it->init(ef);
    return it;
}

fn<Parser_It* (ccstr filepath)> file_loader = default_file_loader;

Ast* parse_file_into_ast(ccstr filepath) {
    Ast *ret = NULL;
    with_parser_at_location(filepath, [&](Parser *p) {
        ret = p->parse_file();
    });
    return ret;
}

void walk_ast(Ast* root, WalkAstFn fn) {
    struct Queue_Item {
        Ast* ast;
        int depth;
        ccstr name;
    };

    // yes, malloc is bad, figure this out later.
    List<Queue_Item> queue;
    queue.init(LIST_MALLOC, 256);
    defer { queue.cleanup(); };

    auto add_to_queue = [&](Ast* ast, ccstr fullname, int depth) {
        if (ast == NULL || ast->type == AST_ILLEGAL) return;

        auto fieldname = strrchr(fullname, '.');
        if (fieldname != NULL)
            fieldname++;

        auto item = queue.append();
        item->ast = ast;
        item->name = fieldname;
        item->depth = depth;
    };

    add_to_queue(root, ".root", 0);

    while (queue.len > 0) {
        auto last = &queue[queue.len - 1];
        auto last_depth = last->depth;
        auto result = fn(last->ast, last->name, last->depth);
        queue.len--;

        if (result == WALK_ABORT) break;
        if (result == WALK_SKIP_CHILDREN) continue;

        SCOPED_FRAME();

        auto children_nodes = alloc_list<Ast*>();
        auto children_strings = alloc_list<ccstr>();

        auto _add_child = [&](Ast *ast, ccstr fullname) {
            children_nodes->append(ast);
            children_strings->append(fullname);
        };

#define add_child(x) _add_child(x, #x)

        auto ast = last->ast;
        switch (ast->type) {
            case AST_ELLIPSIS:
                add_child(ast->ellipsis.type);
                break;
            case AST_LIST:
                {
                    u32 i = 0;
                    For (ast->list) {
                        _add_child(it, our_sprintf(".%d", i++));
                    }
                }
                break;
            case AST_UNARY_EXPR:
                add_child(ast->unary_expr.rhs);
                break;
            case AST_BINARY_EXPR:
                add_child(ast->binary_expr.lhs);
                add_child(ast->binary_expr.rhs);
                break;
            case AST_ARRAY_TYPE:
                add_child(ast->array_type.base_type);
                add_child(ast->array_type.length);
                break;
            case AST_POINTER_TYPE:
                add_child(ast->pointer_type.base_type);
                break;
            case AST_SLICE_TYPE:
                add_child(ast->slice_type.base_type);
                break;
            case AST_MAP_TYPE:
                add_child(ast->map_type.key_type);
                add_child(ast->map_type.value_type);
                break;
            case AST_STRUCT_TYPE:
                add_child(ast->struct_type.fields);
                break;
            case AST_INTERFACE_TYPE:
                add_child(ast->interface_type.specs);
                break;
            case AST_CHAN_TYPE:
                add_child(ast->chan_type.base_type);
                break;
            case AST_FUNC_TYPE:
                add_child(ast->func_type.signature);
                break;
            case AST_PAREN:
                add_child(ast->paren.x);
                break;
            case AST_SELECTOR_EXPR:
                add_child(ast->selector_expr.x);
                add_child(ast->selector_expr.sel);
                break;
            case AST_SLICE_EXPR:
                add_child(ast->slice_expr.x);
                add_child(ast->slice_expr.s1);
                add_child(ast->slice_expr.s2);
                add_child(ast->slice_expr.s3);
                break;
            case AST_INDEX_EXPR:
                add_child(ast->index_expr.x);
                add_child(ast->index_expr.key);
                break;
            case AST_TYPE_ASSERTION_EXPR:
                add_child(ast->type_assertion_expr.x);
                add_child(ast->type_assertion_expr.type);
                break;
            case AST_CALL_EXPR:
                add_child(ast->call_expr.func);
                add_child(ast->call_expr.call_args);
                break;
            case AST_CALL_ARGS:
                add_child(ast->call_args.args);
                break;
            case AST_LITERAL_ELEM:
                add_child(ast->literal_elem.elem);
                add_child(ast->literal_elem.key);
                break;
            case AST_LITERAL_VALUE:
                add_child(ast->literal_value.elems);
                break;
            case AST_FIELD:
                add_child(ast->field.type);
                add_child(ast->field.ids);
                break;
            case AST_PARAMETERS:
                add_child(ast->parameters.fields);
                break;
            case AST_FUNC_LIT:
                add_child(ast->func_lit.signature);
                add_child(ast->func_lit.body);
                break;
            case AST_BLOCK:
                add_child(ast->block.stmts);
                break;
            case AST_SIGNATURE:
                add_child(ast->signature.params);
                add_child(ast->signature.result);
                break;
            case AST_COMPOSITE_LIT:
                add_child(ast->composite_lit.base_type);
                add_child(ast->composite_lit.literal_value);
                break;
            case AST_GO_STMT:
                add_child(ast->go_stmt.x);
                break;
            case AST_DEFER_STMT:
                add_child(ast->defer_stmt.x);
                break;
            case AST_SELECT_STMT:
                add_child(ast->select_stmt.clauses);
                break;
            case AST_SWITCH_STMT:
                add_child(ast->switch_stmt.s1);
                add_child(ast->switch_stmt.s2);
                add_child(ast->switch_stmt.body);
                break;
            case AST_IF_STMT:
                add_child(ast->if_stmt.init);
                add_child(ast->if_stmt.cond);
                add_child(ast->if_stmt.body);
                add_child(ast->if_stmt.else_);
                break;
            case AST_FOR_STMT:
                add_child(ast->for_stmt.s1);
                add_child(ast->for_stmt.s2);
                add_child(ast->for_stmt.s3);
                add_child(ast->for_stmt.body);
                break;
            case AST_RETURN_STMT:
                add_child(ast->return_stmt.exprs);
                break;
            case AST_BRANCH_STMT:
                add_child(ast->branch_stmt.label);
                break;
            case AST_DECL:
                add_child(ast->decl.specs);
                break;
            case AST_ASSIGN_STMT:
                add_child(ast->assign_stmt.lhs);
                add_child(ast->assign_stmt.rhs);
                break;
            case AST_VALUE_SPEC:
                add_child(ast->value_spec.ids);
                add_child(ast->value_spec.type);
                add_child(ast->value_spec.vals);
                break;
            case AST_IMPORT_SPEC:
                add_child(ast->import_spec.path);
                add_child(ast->import_spec.package_name);
                break;
            case AST_TYPE_SPEC:
                add_child(ast->type_spec.id);
                add_child(ast->type_spec.type);
                break;
            case AST_INC_DEC_STMT:
                add_child(ast->inc_dec_stmt.x);
                break;
            case AST_SEND_STMT:
                add_child(ast->send_stmt.value);
                add_child(ast->send_stmt.chan);
                break;
            case AST_LABELED_STMT:
                add_child(ast->labeled_stmt.label);
                add_child(ast->labeled_stmt.stmt);
                break;
            case AST_EXPR_STMT:
                add_child(ast->expr_stmt.x);
                break;
            case AST_PACKAGE_DECL:
                add_child(ast->package_decl.name);
                break;
            case AST_SOURCE:
                add_child(ast->source.package_decl);
                add_child(ast->source.imports);
                add_child(ast->source.decls);
                break;
            case AST_STRUCT_FIELD:
                add_child(ast->struct_field.ids);
                add_child(ast->struct_field.type);
                add_child(ast->struct_field.tag);
                break;
            case AST_INTERFACE_SPEC:
                add_child(ast->interface_spec.name);
                add_child(ast->interface_spec.signature);
                add_child(ast->interface_spec.type);
                break;
            case AST_CASE_CLAUSE:
                add_child(ast->case_clause.stmts);
                add_child(ast->case_clause.vals);
                break;
            case AST_COMM_CLAUSE:
                add_child(ast->comm_clause.stmts);
                add_child(ast->comm_clause.comm);
                break;
            case AST_FUNC_DECL:
                add_child(ast->func_decl.name);
                add_child(ast->func_decl.body);
                add_child(ast->func_decl.signature);
                add_child(ast->func_decl.recv);
                break;
        }

        for (i32 i = children_nodes->len - 1; i >= 0; i--)
            add_to_queue(children_nodes->at(i), children_strings->at(i), last_depth + 1);

#undef add_child
    }
}

void find_nodes_containing_pos(Ast *root, cur2 pos, fn<Walk_Action(Ast *it)> callback) {
    walk_ast(root, [&](Ast* ast, ccstr name, int depth) -> Walk_Action {
        int res = locate_pos_relative_to_ast(pos, ast);
        if (res < 0) return WALK_ABORT;
        if (res > 0) return WALK_SKIP_CHILDREN;
        return callback(ast);
    });
}

void Ast_List::init(Parser* _parser, cur2 _start_pos) {
    parser = _parser;
    start = parser->list_mem.sp;
    len = 0;
    start_pos = _start_pos;
}

void Ast_List::init(Parser* _parser) { return init(_parser, _parser->tok.start); }

void Ast_List::push(Ast* ast) {
    auto& mem = parser->list_mem;

    if (mem.sp + 1 >= mem.cap) {
        mem.cap *= 2;
        mem.buf = (Ast**)realloc(mem.buf, sizeof(Ast*) * mem.cap);
    }
    mem.buf[mem.sp++] = ast;
    len++;
}

int get_ast_header_size() {
    Ast ast;
    return ((u64)(void*)&ast.extra_data_start - (u64)(void*)&ast);
}

int AST_SIZES[_AST_TYPES_COUNT_];

void init_ast_sizes() {
    mem0(AST_SIZES, sizeof(AST_SIZES));

    AST_SIZES[AST_FUNC_LIT] = sizeof(Ast::func_lit);
    AST_SIZES[AST_DECL] = sizeof(Ast::decl);
    AST_SIZES[AST_SIGNATURE] = sizeof(Ast::signature);
    AST_SIZES[AST_BASIC_LIT] = sizeof(Ast::basic_lit);
    AST_SIZES[AST_UNARY_EXPR] = sizeof(Ast::unary_expr);
    AST_SIZES[AST_LIST] = sizeof(Ast::list);
    AST_SIZES[AST_BINARY_EXPR] = sizeof(Ast::binary_expr);
    AST_SIZES[AST_ID] = sizeof(Ast::id);
    AST_SIZES[AST_SLICE_TYPE] = sizeof(Ast::slice_type);
    AST_SIZES[AST_MAP_TYPE] = sizeof(Ast::map_type);
    AST_SIZES[AST_CHAN_TYPE] = sizeof(Ast::chan_type);
    AST_SIZES[AST_POINTER_TYPE] = sizeof(Ast::pointer_type);
    AST_SIZES[AST_ARRAY_TYPE] = sizeof(Ast::array_type);
    AST_SIZES[AST_PAREN] = sizeof(Ast::paren);
    AST_SIZES[AST_SELECTOR_EXPR] = sizeof(Ast::selector_expr);
    AST_SIZES[AST_SLICE_EXPR] = sizeof(Ast::slice_expr);
    AST_SIZES[AST_INDEX_EXPR] = sizeof(Ast::index_expr);
    AST_SIZES[AST_TYPE_ASSERTION_EXPR] = sizeof(Ast::type_assertion_expr);
    AST_SIZES[AST_CALL_EXPR] = sizeof(Ast::call_expr);
    AST_SIZES[AST_CALL_ARGS] = sizeof(Ast::call_args);
    AST_SIZES[AST_LITERAL_ELEM] = sizeof(Ast::literal_elem);
    AST_SIZES[AST_LITERAL_VALUE] = sizeof(Ast::literal_value);
    AST_SIZES[AST_ELLIPSIS] = sizeof(Ast::ellipsis);
    AST_SIZES[AST_FIELD] = sizeof(Ast::field);
    AST_SIZES[AST_PARAMETERS] = sizeof(Ast::parameters);
    AST_SIZES[AST_BLOCK] = sizeof(Ast::block);
    AST_SIZES[AST_COMPOSITE_LIT] = sizeof(Ast::composite_lit);
    AST_SIZES[AST_GO_STMT] = sizeof(Ast::go_stmt);
    AST_SIZES[AST_DEFER_STMT] = sizeof(Ast::defer_stmt);
    AST_SIZES[AST_IF_STMT] = sizeof(Ast::if_stmt);
    AST_SIZES[AST_RETURN_STMT] = sizeof(Ast::return_stmt);
    AST_SIZES[AST_BRANCH_STMT] = sizeof(Ast::branch_stmt);
    AST_SIZES[AST_STRUCT_TYPE] = sizeof(Ast::struct_type);
    AST_SIZES[AST_INTERFACE_TYPE] = sizeof(Ast::interface_type);
    AST_SIZES[AST_FUNC_TYPE] = sizeof(Ast::func_type);
    AST_SIZES[AST_VALUE_SPEC] = sizeof(Ast::value_spec);
    AST_SIZES[AST_IMPORT_SPEC] = sizeof(Ast::import_spec);
    AST_SIZES[AST_TYPE_SPEC] = sizeof(Ast::type_spec);
    AST_SIZES[AST_ASSIGN_STMT] = sizeof(Ast::assign_stmt);
    AST_SIZES[AST_INC_DEC_STMT] = sizeof(Ast::inc_dec_stmt);
    AST_SIZES[AST_SEND_STMT] = sizeof(Ast::send_stmt);
    AST_SIZES[AST_LABELED_STMT] = sizeof(Ast::labeled_stmt);
    AST_SIZES[AST_EXPR_STMT] = sizeof(Ast::expr_stmt);
    AST_SIZES[AST_PACKAGE_DECL] = sizeof(Ast::package_decl);
    AST_SIZES[AST_SOURCE] = sizeof(Ast::source);
    AST_SIZES[AST_STRUCT_FIELD] = sizeof(Ast::struct_field);
    AST_SIZES[AST_INTERFACE_SPEC] = sizeof(Ast::interface_spec);
    AST_SIZES[AST_SWITCH_STMT] = sizeof(Ast::switch_stmt);
    AST_SIZES[AST_SELECT_STMT] = sizeof(Ast::select_stmt);
    AST_SIZES[AST_FOR_STMT] = sizeof(Ast::for_stmt);
    AST_SIZES[AST_CASE_CLAUSE] = sizeof(Ast::case_clause);
    AST_SIZES[AST_COMM_CLAUSE] = sizeof(Ast::comm_clause);
    AST_SIZES[AST_FUNC_DECL] = sizeof(Ast::func_decl);
}

const int AST_HEADER_SIZE = get_ast_header_size();

Ast* new_ast(Ast_Type type, cur2 start) {
    static bool ast_sizes_initialized = false;

    if (!ast_sizes_initialized)  {
        init_ast_sizes();
        ast_sizes_initialized = true;
    }

    auto ret = (Ast*)alloc_memory(AST_HEADER_SIZE + AST_SIZES[type]);
    // auto ret = (Ast*)alloc_memory(sizeof(Ast));
    ret->type = type;
    ret->start = start;
    return ret;
}

Ast* Ast_List::save() {
    auto& mem = parser->list_mem;

    auto items = alloc_array(Ast*, len);
    memcpy(items, mem.buf + start, sizeof(Ast*) * len);
    mem.sp = start;

    auto ast = new_ast(AST_LIST, start_pos);
    ast->list.init(LIST_FIXED, len, items);
    ast->list.len = len;

    return parser->fill_end(ast);
}

Ast* unparen(Ast* ast) {
    while (ast != NULL && ast->type == AST_PAREN)
        ast = ast->paren.x;
    return ast;
}

Ast* unpointer(Ast* ast) {
    while (ast != NULL && ast->type == AST_POINTER_TYPE)
        ast = ast->pointer_type.base_type;
    return ast;
}

#define expect(x) _expect(x, __FILE__, __LINE__)

void Parser::parser_error(ccstr fmt, ...) {
    va_list args;
    va_start(args, fmt);

    print("%s", filepath);
    print("pos = %s", format_pos(tok.start));

    /*
    // for debugging
    if (!streq(filepath, "./test.go"))
        copy_file(filepath, "./test.go", true);
    */

    vprintf(fmt, args);
    printf("\n");

    va_end(args);
}

void Parser::lex_with_comments() {
    tok.start_before_leading_whitespace = it->get_pos();

    auto should_insert_semi = [&]() -> bool {
        switch (last_token_type) {
            case TOK_ID:
            case TOK_INT:
            case TOK_FLOAT:
            case TOK_IMAG:
            case TOK_RUNE:
            case TOK_STRING:
            case TOK_BREAK:
            case TOK_CONTINUE:
            case TOK_FALLTHROUGH:
            case TOK_RETURN:
            case TOK_INC:
            case TOK_DEC:
            case TOK_RPAREN:
            case TOK_RBRACE:
            case TOK_RBRACK:
                // semicolon should be auto inserted after these tokens
                return true;

            case TOK_PERIOD:
                // PREVIOUS NOTE: these tokens don't trigger semicolon insertion per language spec, but
                // it makes things easier for us
                // return true;

                // NEW NOTE: we can't do this, because it causes failure when parsing:
                //
                //     func foo() {
                //         x = x.
                //             call()
                //     }
                return false;
        }
        return false;
    };

    auto insert_semi = [&]() -> bool {
        if (should_insert_semi()) {
            tok.start = it->get_pos();
            tok.type = TOK_SEMICOLON;
            tok.lit = ";"; // NOTE: do we do this?
            return true;
        }
        return false;
    };

    // eat whitespace, inserting semicolon if we hit a newline
    for (; isspace(it->peek()) && !it->eof(); it->next())
        if (it->peek() == '\n')
            if (insert_semi())
                return;

    // At this point, preprocessing is done --    we've eaten whitespace and
    // checked for semicolon insertion. We're now ready to read the token itself.

    cstr buffer = NULL;
    s32 buffer_len = 0;
    s32 buffer_pos = 0;
    bool initial_run = true;

    auto next = [&]() -> u8 {
        auto ret = it->next();
        if (initial_run)
            buffer_len++;
        else if (buffer != NULL && buffer_pos < buffer_len)
            buffer[buffer_pos++] = ret;
        return ret;
    };

    auto read_rune = [&]() -> bool {
        auto ch = next();

        if (ch == '\n') {
            parser_error("Invalid newline found inside rune or string literal.");
            return false;
        }

        if (ch == '\\') {
            u32 read_hex = 0;
            bool ok = true;

            ch = next();
            switch (ch) {
                case 'x': read_hex = 2; break;
                case 'u': read_hex = 4; break;
                case 'U': read_hex = 8; break;
            }

            if (read_hex > 0) {
                auto esc_ch = ch;
                for (u32 i = 0; i < read_hex; i++) {
                    auto ch = next();
                    if (!ishex(ch)) {
                        if (ok) {
                            parser_error("Expected hex character to follow '\\%c' in rune literal.", esc_ch);
                            ok = false;
                        }
                    }
                }
            } else if (isoct(ch)) {
                if (!isoct(next())) ok = false;
                if (!isoct(next())) ok = false;
                if (!ok)
                    parser_error("Expected octal digit to follow '\\' in rune literal.");
            }

            return ok;
        }
        return true;
    };

    auto get_type = [&]() -> Tok_Type {
        if (it->eof()) return TOK_EOF;

        auto ch = next();

        // check if it's a symbolic token
        switch (ch) {
            case '+':
                switch (it->peek()) {
                    case '+': return next(), TOK_INC;
                    case '=': return next(), TOK_ADD_ASSIGN;
                }
                return TOK_ADD;
            case '-':
                switch (it->peek()) {
                    case '-': return next(), TOK_DEC;
                    case '=': return next(), TOK_SUB_ASSIGN;
                }
                return TOK_SUB;
            case '&':
                switch (it->peek()) {
                    case '&': return next(), TOK_LAND;
                    case '=': return next(), TOK_AND_ASSIGN;
                    case '^':
                        next();
                        if (it->peek() == '=')
                            return next(), TOK_AND_NOT_ASSIGN;
                        return TOK_AND_NOT;
                }
                return TOK_AND;
            case '*':
                if (it->peek() == '=')
                    return next(), TOK_MUL_ASSIGN;
                return TOK_MUL;
            case '%':
                if (it->peek() == '=')
                    return next(), TOK_REM_ASSIGN;
                return TOK_REM;
            case '|':
                switch (it->peek()) {
                    case '|': return next(), TOK_LOR;
                    case '=': return next(), TOK_OR_ASSIGN;
                }
                return TOK_OR;
            case '<':
                switch (it->peek()) {
                    case '<':
                        next();
                        if (it->peek() == '=')
                            return next(), TOK_SHL_ASSIGN;
                        return TOK_SHL;
                    case '-':
                        next();
                        return TOK_ARROW;
                    case '=':
                        next();
                        return TOK_LEQ;
                }
                return TOK_LSS;
            case '>':
                switch (it->peek()) {
                    case '>':
                        next();
                        if (it->peek() == '=')
                            return next(), TOK_SHR_ASSIGN;
                        return TOK_SHR;
                    case '=':
                        return next(), TOK_GEQ;
                }
                return TOK_GTR;
            case '=':
                if (it->peek() == '=')
                    return next(), TOK_EQL;
                return TOK_ASSIGN;
            case '^':
                if (it->peek() == '=')
                    return next(), TOK_XOR_ASSIGN;
                return TOK_XOR;
            case '(': return TOK_LPAREN;
            case '[': return TOK_LBRACK;
            case '{': return TOK_LBRACE;
            case ')': return TOK_RPAREN;
            case ']': return TOK_RBRACK;
            case '}': return TOK_RBRACE;
            case ';': return TOK_SEMICOLON;
            case ',': return TOK_COMMA;
            case ':':
                if (it->peek() == '=')
                    return next(), TOK_DEFINE;
                return TOK_COLON;
            case '!':
                return it->peek() == '=' ? (next(), TOK_NEQ) : TOK_NOT;
            case '.':
                if (it->peek() == '.') {
                    next();
                    if (it->peek() != '.') {
                        parser_error("Invalid start of ellipsis");
                        return TOK_ILLEGAL;
                    }
                    it->next();
                    return TOK_ELLIPSIS;
                }
                return TOK_PERIOD;
            case '/':
                switch (it->peek()) {
                    case '/':
                        while (it->peek() != '\n')
                            next();
                        return TOK_COMMENT;
                    case '*':
                        {
                            bool last_was_star = false;
                            while (true) {
                                auto ch = next();
                                if (ch == '/' && last_was_star) break;
                                last_was_star = (ch == '*');
                            }
                        }
                        return TOK_COMMENT;
                    case '=':
                        return next(), TOK_QUO_ASSIGN;
                }
                return TOK_QUO;
            case '`':
                while (it->peek() != '`')
                    next();
                next();
                return TOK_STRING;
            case '"':
                {
                    bool illegal = false;
                    while (it->peek() != '"') {
                        if (it->peek() == '\n')
                            return TOK_ILLEGAL;
                        if (!read_rune())
                            illegal = true;
                    }
                    next(); // eat the '"'
                    return illegal ? TOK_ILLEGAL : TOK_STRING;
                }
            case '\'':
                {
                    bool illegal = false;
                    if (!read_rune())
                        illegal = true;
                    if (next() != '\'') {
                        while (next() != '\'')
                            continue;
                        illegal = true;
                        parser_error("Expected only one character in rune literal.");
                    }
                    return illegal ? TOK_ILLEGAL : TOK_RUNE;
                }
        }

        if (isdigit(ch)) {
            bool is_imag = false;
            bool is_float = false;

            // TODO: actually properly lex the number lit
            auto is_number_lit = [&](u8 c, u8 prev) -> bool {
                if (ishex(c)) return true;
                switch (c) {
                    case '_':
                    case 'b':
                    case 'B':
                    case 'x':
                    case 'X':
                    case 'e':
                    case 'E':
                    case 'i':
                        is_imag = true;
                        return true;
                    case '.':
                        is_float = true;
                        return true;
                    case '+':
                    case '-':
                        switch (prev) {
                            case 'e':
                            case 'E':
                            case 'p':
                            case 'P':
                                return true;
                        }
                }
                return false;
            };

            u8 ch, last = 0;
            while (is_number_lit((ch = it->peek()), last)) {
                next();
                last = ch;
            }

            if (is_imag) return TOK_IMAG;
            if (is_float) return TOK_FLOAT;
            return TOK_INT;
        }

        if (!isid(ch)) {
            parser_error("Illegal character 0x%X found.", ch);
            return TOK_ILLEGAL;
        }

        while (isid(it->peek()))
            next();

        if (!initial_run) {
            struct Keyword {
                Tok_Type type;
                ccstr string;
                s32 len;
            };

            Keyword keywords[] = {
                    {TOK_BREAK, "break", 5},
                    {TOK_CASE, "case", 4},
                    {TOK_CHAN, "chan", 4},
                    {TOK_CONST, "const", 5},
                    {TOK_CONTINUE, "continue", 8},
                    {TOK_DEFAULT, "default", 7},
                    {TOK_DEFER, "defer", 5},
                    {TOK_ELSE, "else", 4},
                    {TOK_FALLTHROUGH, "fallthrough", 11},
                    {TOK_FOR, "for", 3},
                    {TOK_FUNC, "func", 4},
                    {TOK_GO, "go", 2},
                    {TOK_GOTO, "goto", 4},
                    {TOK_IF, "if", 2},
                    {TOK_IMPORT, "import", 6},
                    {TOK_INTERFACE, "interface", 9},
                    {TOK_MAP, "map", 3},
                    {TOK_PACKAGE, "package", 7},
                    {TOK_RANGE, "range", 5},
                    {TOK_RETURN, "return", 6},
                    {TOK_SELECT, "select", 6},
                    {TOK_STRUCT, "struct", 6},
                    {TOK_SWITCH, "switch", 6},
                    {TOK_TYPE, "type", 4},
                    {TOK_VAR, "var", 3},
            };

            For (keywords)
                if (it.len == buffer_len && strneq(it.string, buffer, it.len))
                    return it.type;
        }

        return TOK_ID;
    };

    // TODO: Ok, I think the plan is to call get_type() twice, once to count how
    // many chars, and once to save it

    tok.start = it->get_pos();

    // call get_type(), initial run
    initial_run = true;
    auto type = get_type();
    if (type != TOK_EOF && type != TOK_ILLEGAL) {
        switch (type) {
            case TOK_STRING:
            case TOK_ID:
            case TOK_INT:
            case TOK_FLOAT:
            case TOK_IMAG:
            case TOK_RUNE:
            case TOK_COMMENT:
                buffer = alloc_array(char, buffer_len + 1);
                break;
        }
        it->set_pos(tok.start);
        initial_run = false;
        type = get_type();
        if (buffer != NULL)
            buffer[buffer_len] = '\0';
    } else if (type == TOK_EOF && insert_semi()) {
        return;
    }

    tok.type = type;
    tok.lit = buffer;

    // NOTE: parse values of literals. I don't think this is actually
    // necessary...
    if (tok.type == TOK_STRING) {
        // len includes the '\0'
        auto value_len = buffer_len - 2;
        auto value = alloc_array(char, value_len + 1);
        strncpy(value, buffer + 1, value_len);
        value[value_len] = '\0';
        tok.val.string_val = value;
    }
}

void Parser::init(Parser_It* _it, ccstr _filepath, bool no_lex) {
    ptr0(this);

    // init fields
    it = _it;
    filepath = _filepath;

    // init list mem
    list_mem.cap = 128;
    list_mem.buf = (Ast**)our_malloc(sizeof(Ast*) * list_mem.cap);
    list_mem.sp = 0;

    // init decl_table hash table
    decl_table.init();

    // read first token
    if (!no_lex) lex();
}

void Parser::cleanup() {
    our_free(list_mem.buf);
    decl_table.cleanup();
}

void Parser::reinit_without_lex() {
    auto old_it = it;
    auto old_filepath = filepath;
    cleanup();
    old_it->set_pos(new_cur2(0, 0));
    init(old_it, old_filepath, true);
}

Ast_List Parser::new_list() {
    Ast_List list;
    list.init(this);
    return list;
}

void Parser::synchronize(Sync_Lookup_Func lookup) {
    while (!lookup(tok.type) && tok.type != TOK_EOF)
        lex();
}

void Parser::_throwError(ccstr s) {
    parser_error("%s", s);

    // what should we do with parser errors though?
    // throw std::runtime_error(s);
}

cur2 Parser::_expect(Tok_Type want, ccstr file, u32 line) {
    bool match = false;
    auto start = tok.start;
    auto got = tok.type;

    if (want == TOK_SEMICOLON) {
        switch (got) {
            case TOK_SEMICOLON:
            case TOK_RPAREN:
            case TOK_RBRACE:
                match = true;
                break;
        }
        if (got == TOK_SEMICOLON)
            lex();
    } else {
        match = (got == want);
        lex();
    }

    if (!match) {
        parser_error("%s:%d: Expected %s at %s, got %s instead", file, line, tok_type_str(want), format_pos(start), tok_type_str(got));
        if (want == TOK_SEMICOLON)
            synchronize(lookup_stmt_start);
        else
            lex(); // make progress
    }

    return match ? start : INVALID_POS;
}

Ast* Parser::parse_id_list() {
    auto list = new_list();
    list.push(parse_id());
    while (tok.type == TOK_COMMA) {
        lex();
        list.push(parse_id());
    }
    return list.save();
}

Ast *Parser::parse_spec(Tok_Type spec_type, bool *previous_spec_contains_iota) {
    switch (spec_type) {
    case TOK_VAR:
    case TOK_CONST:
        {
            auto start = tok.start;

            auto ids = parse_id_list();
            auto type = parse_type();

            Ast* vals = NULL;
            if (tok.type == TOK_ASSIGN) {
                lex();
                vals = parse_expr_list(false);
            }

            auto contains_iota = [&](Ast* rhs) {
                if (rhs == NULL) return false;
                if (rhs->type != AST_LIST) return false;

                bool ret = false;
                walk_ast(rhs, [&](Ast* ast, ccstr, int) -> Walk_Action {
                    if (ast->type == AST_ID && streq(ast->id.lit, "iota")) {
                        ret = true;
                        return WALK_ABORT;
                    }
                    return WALK_CONTINUE;
                });
                return ret;
            };

            if (contains_iota(vals))
                if (previous_spec_contains_iota != NULL)
                    *previous_spec_contains_iota = true;

            if (type == NULL && vals == NULL)
                if (previous_spec_contains_iota != NULL)
                    if (!*previous_spec_contains_iota)
                        parser_error("%s spec needs type or values.", tok_type_str(spec_type));

            auto ast = new_ast(AST_VALUE_SPEC, start);
            ast->value_spec.ids = ids;
            ast->value_spec.type = type;
            ast->value_spec.vals = vals;
            ast->value_spec.spec_type = spec_type;

            For (ids->list) { declare_var(it->id.lit, ast, it); }

            return fill_end(ast);
        }
    case TOK_TYPE:
        {
            auto ast = new_ast(AST_TYPE_SPEC, tok.start);

            ast->type_spec.id = parse_id();

            bool alias = false;
            if (tok.type == TOK_ASSIGN) {
                alias = true;
                lex();
            }

            ast->type_spec.is_alias = alias;
            ast->type_spec.type = parse_type();

            if (ast->type_spec.type == NULL)
                parser_error("Type definition must be a valid type.");

            auto id = ast->type_spec.id;
            declare_var(id->id.lit, ast, id);
            return fill_end(ast);
        }
    case TOK_IMPORT:
        {
            auto ast = new_ast(AST_IMPORT_SPEC, tok.start);

            Ast* name = NULL;
            switch (tok.type) {
                case TOK_PERIOD:
                    name = new_ast(AST_ID, tok.start);
                    name->id.lit = tok.lit;
                    lex();
                    name = fill_end(name);
                    break;
                case TOK_ID:
                    name = parse_id();
                    break;
            }
            ast->import_spec.package_name = name;

            if (tok.type != TOK_STRING) {
                parser_error("Expected string for package path in import.");
                return NULL;
            }

            auto path = new_ast(AST_BASIC_LIT, tok.start);
            path->basic_lit.val = tok.val;
            path->basic_lit.lit = tok.lit;
            lex();
            ast->import_spec.path = fill_end(path);

            return fill_end(ast);
        }
    }
    return NULL; // we should never get here
}

Ast* Parser::parse_decl(Sync_Lookup_Func sync_lookup, bool import_allowed) {
    bool ok = false;

    auto start = tok.start;
    auto spec_type = tok.type;

    switch (spec_type) {
        case TOK_FUNC:
            {
                auto ast = new_ast(AST_FUNC_DECL, lex());

                Ast* recv = NULL;
                if (tok.type == TOK_LPAREN)
                    recv = parse_parameters(false);

                ast->func_decl.recv = recv;

                auto name = parse_id();
                ast->func_decl.name = name;

                ast->func_decl.signature = parse_signature();

                if (tok.type == TOK_LBRACE)
                    ast->func_decl.body = parse_block();

                declare_var(name->id.lit, ast, name);

                expect(TOK_SEMICOLON);
                return fill_end(ast);
            }
        case TOK_CONST:
        case TOK_VAR:
        case TOK_TYPE:
            ok = true;
            break;
        case TOK_IMPORT:
            if (import_allowed)
                ok = true;
            else
                parser_error("Unexpected `import` found.");
            break;
        default:
            parser_error("Expected to find a declaration.");
            break;
    }

    if (!ok) {
        synchronize(sync_lookup);
        return NULL; // NOTE: BAD_STMT or something instead?
    }

    bool previous_spec_contains_iota = false;

    lex(); // eat the decl keyword
    auto specs = new_list();

    auto parse = [&]() {
        auto spec = parse_spec(spec_type, &previous_spec_contains_iota);
        if (spec == NULL)
            synchronize(sync_lookup);
        else
            specs.push(spec);
    };

    // try {
    if (tok.type == TOK_LPAREN) {
        for (lex(); tok.type != TOK_RPAREN && tok.type != TOK_EOF; expect(TOK_SEMICOLON))
            parse();
        expect(TOK_RPAREN);
    } else {
        parse();
    }
    expect(TOK_SEMICOLON);
    // } catch (Parse_Error &e) {
    //     synchronize(sync_lookup);
    // }

    auto ast = new_ast(AST_DECL, start);
    ast->decl.type = spec_type;
    ast->decl.specs = specs.save();
    return fill_end(ast);
}

void Parser::declare_var(ccstr name, Ast* decl, Ast* jump_to) {
    auto p = decl_table.set(name);
    p->decl_ast = decl;
    p->where_to_jump = jump_to;
}

Ast* Parser::parse_parameters(bool ellipsis_ok) {
    auto ret = new_ast(AST_PARAMETERS, expect(TOK_LPAREN));

    auto fields = new_list();

    // parse initial list of types (might also be a list of ids)
    auto list = new_list();
    if (tok.type != TOK_RPAREN) {
        while (true) {
            auto type = parse_type(ellipsis_ok);
            if (type != NULL) list.push(type);
            if (tok.type != TOK_COMMA) break;
            lex();
            if (tok.type == TOK_RPAREN) break;
        }
    }
    auto ids = list.save();

    // analyze our list of types
    // case 1: func (a, b, c int, d float)
    // case 2: func (typeA, typeB, typeC)
    // just try to parse the next type

    auto type = parse_type(ellipsis_ok);

    auto push_field = [&](Ast* type, Ast* ids) {
        auto field = new_ast(AST_FIELD, ids == NULL ? type->start : ids->start);
        field->field.type = type;
        field->field.ids = ids;
        fields.push(field);

        if (ids != NULL)
            For (ids->list) declare_var(it->id.lit, field, it);
    };

    // case 1
    if (type != NULL) {
        push_field(type, ids);
        while (eat_comma(TOK_RPAREN, "parameter list")) {
            if (tok.type == TOK_RPAREN) break;
            auto ids = parse_id_list();
            push_field(parse_type(true), ids);
        }
    } else {
        For (ids->list) push_field(it, NULL);
    }

    expect(TOK_RPAREN);

    ret->parameters.fields = fields.save();
    return fill_end(ret);
}

Ast* Parser::parse_result() {
    if (tok.type == TOK_LPAREN)
        return parse_parameters(false);

    auto type = parse_type();
    if (type == NULL) return NULL;

    auto field = new_ast(AST_FIELD, type->start);
    field->field.type = type;
    field->field.ids = NULL;

    auto fields = new_list();
    fields.push(field);

    auto ret = new_ast(AST_PARAMETERS, field->start);
    ret->parameters.fields = fields.save();
    return ret;
}

Ast* Parser::parse_signature() {
    auto ast = new_ast(AST_SIGNATURE, tok.start);
    ast->signature.params = parse_parameters(true);
    ast->signature.result = parse_result();
    return fill_end(ast);
}

Ast* Parser::parse_id() {
    auto lit = tok.lit;
    bool bad = false;

    if (tok.type != TOK_ID) {
        lit = "_";
        bad = true;
    }

    auto ast = new_ast(AST_ID, expect(TOK_ID));
    ast->id.lit = lit;
    ast->id.bad = bad;
    return fill_end(ast);
}

Ast* Parser::parse_type(bool is_param) {
    Ast* ast;

    switch (tok.type) {
        case TOK_ELLIPSIS:
            if (!is_param) break;
            ast = new_ast(AST_ELLIPSIS, lex());
            ast->ellipsis.type = parse_type();
            return fill_end(ast);

        case TOK_LPAREN:
            ast = new_ast(AST_PAREN, lex());
            ast->paren.x = parse_type();
            expect(TOK_RPAREN);
            return fill_end(ast);

        case TOK_ID:
            {
                auto ret = parse_id();
                if (tok.type == TOK_PERIOD) {
                    auto period_pos = tok.start;
                    lex();

                    auto ast = new_ast(AST_SELECTOR_EXPR, ret->start);
                    ast->selector_expr.x = ret;
                    ast->selector_expr.period_pos = period_pos;
                    ast->selector_expr.sel = parse_id();
                    ret = fill_end(ast);
                }
                return ret;
            }
        case TOK_LBRACK:
            {
                ast = new_ast(AST_ILLEGAL, lex());

                Ast* length = NULL, * type = NULL;

                if (tok.type != TOK_RBRACK) {
                    if (tok.type == TOK_ELLIPSIS) {
                        length = new_ast(AST_ELLIPSIS, lex());
                    } else {
                        length = parse_expr(false);
                    }
                }
                expect(TOK_RBRACK);
                type = parse_type();

                if (length == NULL) {
                    ast->type = AST_SLICE_TYPE;
                    ast->slice_type.base_type = type;
                } else {
                    ast->type = AST_ARRAY_TYPE;
                    ast->array_type.length = length;
                    ast->array_type.base_type = type;
                }
                return fill_end(ast);
            }

        case TOK_MUL:
            ast = new_ast(AST_POINTER_TYPE, lex());
            ast->pointer_type.base_type = parse_type();
            return fill_end(ast);

        case TOK_MAP:
            ast = new_ast(AST_MAP_TYPE, lex());

            expect(TOK_LBRACK);
            ast->map_type.key_type = parse_type();
            expect(TOK_RBRACK);
            ast->map_type.value_type = parse_type();

            return fill_end(ast);

        case TOK_CHAN:
            {
                ast = new_ast(AST_CHAN_TYPE, lex());

                auto direction = AST_CHAN_BI;
                if (tok.type == TOK_ARROW) {
                    lex();
                    direction = AST_CHAN_SEND;
                }

                ast->chan_type.direction = direction;
                ast->chan_type.base_type = parse_type();
                return fill_end(ast);
            }
        case TOK_ARROW:
            ast = new_ast(AST_CHAN_TYPE, lex());
            expect(TOK_CHAN);
            ast->chan_type.direction = AST_CHAN_RECV;
            ast->chan_type.base_type = parse_type();
            return fill_end(ast);
        case TOK_STRUCT:
            {
                ast = new_ast(AST_STRUCT_TYPE, lex());
                expect(TOK_LBRACE);

                auto fields = new_list();
                while (tok.type != TOK_RBRACE && tok.type != TOK_EOF) {
                    auto field = new_ast(AST_STRUCT_FIELD, tok.start);

                    Ast* type = NULL;
                    Ast* ids = NULL;
                    if (tok.type == TOK_MUL) {
                        type = parse_type();
                    } else {
                        ids = parse_id_list();
                        type = parse_type();
                        if (type == NULL) {
                            if (ids->list.len > 1)
                                parser_error("Cannot declare more than one type in struct field.");
                            type = ids->list.items[0];
                            ids = NULL; // TODO: a way to free ASTs (use pool mem)

                            if (tok.type == TOK_PERIOD) {
                                auto period_pos = tok.start;
                                lex();

                                auto newtype = new_ast(AST_SELECTOR_EXPR, type->start);
                                newtype->selector_expr.period_pos = period_pos;
                                newtype->selector_expr.x = type;
                                newtype->selector_expr.sel = parse_id();
                                type = fill_end(newtype);
                            }
                        }
                    }

                    field->struct_field.ids = ids;
                    field->struct_field.type = type;

                    if (tok.type == TOK_STRING) {
                        auto tag = new_ast(AST_BASIC_LIT, tok.start);
                        tag->basic_lit.val = tok.val;
                        tag->basic_lit.lit = tok.lit;
                        lex();
                        field->struct_field.tag = fill_end(tag);
                    }

                    fields.push(fill_end(field));
                    expect(TOK_SEMICOLON);
                }
                expect(TOK_RBRACE);

                ast->struct_type.fields = fields.save();
                return fill_end(ast);
            }
        case TOK_INTERFACE:
            {
                ast = new_ast(AST_INTERFACE_TYPE, lex());
                expect(TOK_LBRACE);

                auto specs = new_list();
                while (tok.type != TOK_RBRACE && tok.type != TOK_EOF) {
                    auto spec = new_ast(AST_INTERFACE_SPEC, tok.start);

                    Ast* name = NULL;
                    Ast* signature = NULL;
                    Ast* type = NULL;

                    name = parse_id();
                    if (tok.type == TOK_LPAREN) {
                        signature = parse_signature();
                    } else {
                        if (tok.type == TOK_PERIOD) {
                            auto period_pos = tok.start;
                            lex();
                            auto sel = parse_id();

                            auto newname = new_ast(AST_SELECTOR_EXPR, name->start);
                            newname->selector_expr.sel = sel;
                            newname->selector_expr.x = name;
                            newname->selector_expr.period_pos = period_pos;
                            name = fill_end(newname);
                        }
                        // NOTE: be more strict about requiring this to be
                        // TypeName?
                        type = name;
                        name = NULL;
                    }

                    spec->interface_spec.name = name;
                    spec->interface_spec.type = type;
                    spec->interface_spec.signature = signature;
                    specs.push(spec);

                    expect(TOK_SEMICOLON);
                }
                expect(TOK_RBRACE);

                ast->interface_type.specs = specs.save();
                return fill_end(ast);
            }
        case TOK_FUNC:
            ast = new_ast(AST_FUNC_TYPE, lex());
            ast->func_type.signature = parse_signature();
            return fill_end(ast);
    }
    return NULL;
}

Ast* Parser::parse_simple_stmt() {
    auto lhs = parse_expr_list();

    switch (tok.type) {
        case TOK_DEFINE:
        case TOK_ASSIGN:
        case TOK_ADD_ASSIGN:
        case TOK_SUB_ASSIGN:
        case TOK_MUL_ASSIGN:
        case TOK_QUO_ASSIGN:
        case TOK_REM_ASSIGN:
        case TOK_AND_ASSIGN:
        case TOK_OR_ASSIGN:
        case TOK_XOR_ASSIGN:
        case TOK_SHL_ASSIGN:
        case TOK_SHR_ASSIGN:
        case TOK_AND_NOT_ASSIGN:
            {
                auto assign_op = tok.type;

                auto ret = new_ast(AST_ASSIGN_STMT, lhs->start);
                ret->assign_stmt.op = assign_op;
                ret->assign_stmt.lhs = lhs;

                lex();

                Ast* rhs = NULL;
                bool is_range = false;

                if (tok.type == TOK_RANGE && (assign_op == TOK_DEFINE || assign_op == TOK_ASSIGN)) {
                    auto expr = new_ast(AST_UNARY_EXPR, lex());
                    expr->unary_expr.op = TOK_RANGE;
                    expr->unary_expr.rhs = parse_expr(false);
                    fill_end(expr);

                    auto list = new_list();
                    list.push(expr);
                    rhs = list.save();
                } else {
                    rhs = parse_expr_list(false);
                }

                if (assign_op == TOK_DEFINE)
                    For (lhs->list) declare_var(it->id.lit, ret, it);

                ret->assign_stmt.rhs = rhs;
                return fill_end(ret);
            }
    }

    if (lhs->list.len > 1) {
        parser_error("Expected only 1 expression.");
    }

    auto expr = lhs->list.items[0];
    if (expr == NULL) return NULL; // when does this happen?

    switch (tok.type) {
        case TOK_COLON:
            {
                lex();

                // NOTE: remember that labels can also be jumped to with go to
                // definition
                auto ast = new_ast(AST_LABELED_STMT, expr->start);
                ast->labeled_stmt.label = expr;
                ast->labeled_stmt.stmt = parse_stmt();
                return fill_end(ast);
            }
        case TOK_ARROW:
            {
                lex();

                auto ast = new_ast(AST_SEND_STMT, expr->start);
                ast->send_stmt.chan = expr;
                ast->send_stmt.value = parse_expr(false);
                return fill_end(ast);
            }
        case TOK_INC:
        case TOK_DEC:
            {
                auto op = tok.type;
                lex();

                auto ast = new_ast(AST_INC_DEC_STMT, expr->start);
                ast->inc_dec_stmt.x = expr;
                ast->inc_dec_stmt.op = op;
                return fill_end(ast);
            }
    }

    auto ret = new_ast(AST_EXPR_STMT, expr->start);
    ret->expr_stmt.x = expr;
    return fill_end(ret);
}

Ast* Parser::parse_stmt() {
    switch (tok.type) {
        case TOK_ID:
        case TOK_INT:
        case TOK_FLOAT:
        case TOK_IMAG:
        case TOK_RUNE:
        case TOK_STRING:
        case TOK_FUNC:
        case TOK_LPAREN:
        case TOK_LBRACK:
        case TOK_STRUCT:
        case TOK_MAP:
        case TOK_CHAN:
        case TOK_INTERFACE:
        case TOK_ADD:
        case TOK_SUB:
        case TOK_MUL:
        case TOK_AND:
        case TOK_XOR:
        case TOK_ARROW:
        case TOK_NOT:
            {
                auto stmt = parse_simple_stmt();

                // labeled statements will have already parsed the following
                // statement entirely, don't expect a semicolon.
                if (stmt->type != AST_LABELED_STMT)
                    expect(TOK_SEMICOLON);

                return stmt;
            }
        case TOK_GO:
            {
                auto ast = new_ast(AST_GO_STMT, lex());
                ast->go_stmt.x = parse_expr(false);
                expect(TOK_SEMICOLON);
                return fill_end(ast);
            }
        case TOK_DEFER:
            {
                auto ast = new_ast(AST_DEFER_STMT, lex());
                ast->defer_stmt.x = parse_expr(false);
                expect(TOK_SEMICOLON);
                return fill_end(ast);
            }
        case TOK_RETURN:
            {
                auto ast = new_ast(AST_RETURN_STMT, lex());
                if (tok.type != TOK_SEMICOLON && tok.type != TOK_RBRACE)
                    ast->return_stmt.exprs = parse_expr_list(false);
                expect(TOK_SEMICOLON);
                return fill_end(ast);
            }
        case TOK_SEMICOLON:
            return fill_end(new_ast(AST_EMPTY, lex()));
        case TOK_CONST:
        case TOK_VAR:
        case TOK_TYPE:
            return parse_decl(lookup_stmt_start, false);
        case TOK_BREAK:
        case TOK_FALLTHROUGH:
        case TOK_CONTINUE:
        case TOK_GOTO:
            {
                auto type = tok.type;

                auto ast = new_ast(AST_BRANCH_STMT, lex());
                ast->branch_stmt.branch_type = type;
                if (type != TOK_FALLTHROUGH && tok.type == TOK_ID)
                    ast->branch_stmt.label = parse_id();

                expect(TOK_SEMICOLON);
                return fill_end(ast);
            }
        case TOK_LBRACE:
            {
                auto ret = parse_block();
                expect(TOK_SEMICOLON);
                return ret;
            }
        case TOK_SWITCH:
            {
                auto ast = new_ast(AST_SWITCH_STMT, lex());

                Ast* s1 = NULL, * s2 = NULL;

                if (tok.type != TOK_LBRACE) {
                    auto old = expr_lev;
                    expr_lev = -1;
                    defer { expr_lev = old; };

                    if (tok.type != TOK_SEMICOLON) {
                        s2 = parse_simple_stmt();
                    }
                    if (tok.type == TOK_SEMICOLON) {
                        lex();
                        s1 = s2;
                        s2 = NULL;

                        if (tok.type != TOK_LBRACE) {
                            s2 = parse_simple_stmt();
                        }
                    }
                }

                auto is_type_switch_guard = [&](Ast* ast) -> bool {
                    if (ast == NULL) return false;
                    auto is_type_switch_assert = [&](Ast* ast) -> bool {
                        return (ast->type == AST_TYPE_ASSERTION_EXPR &&
                            ast->type_assertion_expr.type == NULL);
                    };
                    switch (ast->type) {
                        case AST_EXPR_STMT:
                            return is_type_switch_assert(ast->expr_stmt.x);
                        case AST_ASSIGN_STMT:
                            {
                                auto& lhs = ast->assign_stmt.lhs->list;
                                auto& rhs = ast->assign_stmt.rhs->list;
                                if (lhs.len == 1 && rhs.len == 1 &&
                                    is_type_switch_assert(rhs.items[0])) {
                                    switch (ast->assign_stmt.op) {
                                        case TOK_ASSIGN:
                                        case TOK_DEFINE:
                                            return true;
                                    }
                                }
                            }
                    }
                    return false;
                };

                auto is_type_switch = is_type_switch_guard(s2);
                expect(TOK_LBRACE);

                auto clauses = new_list();
                while (tok.type == TOK_CASE || tok.type == TOK_DEFAULT) {
                    auto type = tok.type;
                    auto clause = new_ast(AST_CASE_CLAUSE, lex());

                    if (type == TOK_CASE) {
                        if (is_type_switch)
                            clause->case_clause.vals = parse_type_list();
                        else
                            clause->case_clause.vals = parse_expr_list(false);
                    }

                    expect(TOK_COLON);

                    clause->case_clause.stmts = parse_stmt_list();
                    clauses.push(fill_end(clause));
                }

                expect(TOK_RBRACE);
                expect(TOK_SEMICOLON);

                ast->switch_stmt.s1 = s1;
                ast->switch_stmt.s2 = s2;
                ast->switch_stmt.body = clauses.save();
                ast->switch_stmt.is_type_switch = is_type_switch;

                return fill_end(ast);
            }
        case TOK_IF:
            {
                fn<Ast* ()> parse_if_stmt;
                parse_if_stmt = [&]() -> Ast* {
                    auto ast = new_ast(AST_IF_STMT, lex());
                    Ast* init = NULL, * cond = NULL;

                    auto parse_header = [&]() {
                        if (tok.type == TOK_LBRACE)
                            parser_error("Missing condition in if statement.");

                        auto old = expr_lev;
                        expr_lev = -1;
                        defer { expr_lev = old; };

                        if (tok.type != TOK_SEMICOLON)
                            init = parse_simple_stmt();

                        if (tok.type == TOK_SEMICOLON) {
                            lex();
                            cond = parse_expr(false);
                        } else {
                            if (init->type != AST_EXPR_STMT)
                                parser_error("Expected expression as condition in if header.");
                            cond = init->expr_stmt.x;
                            init = NULL;
                        }
                    };

                    parse_header();
                    ast->if_stmt.init = init;
                    ast->if_stmt.cond = cond;
                    ast->if_stmt.body = parse_block();

                    Ast* else_ = NULL;
                    if (tok.type == TOK_ELSE) {
                        lex();
                        switch (tok.type) {
                            case TOK_IF:
                                else_ = parse_if_stmt();
                                break;
                            case TOK_LBRACE:
                                else_ = parse_block();
                                expect(TOK_SEMICOLON);
                                break;
                            default:
                                parser_error("Expected if or block after else.");
                                break;
                        }
                    } else {
                        expect(TOK_SEMICOLON);
                    }

                    ast->if_stmt.else_ = else_;
                    return fill_end(ast);
                };
                return parse_if_stmt();
            }
        case TOK_SELECT:
            {
                auto ast = new_ast(AST_SELECT_STMT, lex());

                expect(TOK_LBRACE);

                decl_table.open_scope();

                auto clauses = new_list();
                while (tok.type == TOK_CASE || tok.type == TOK_DEFAULT) {
                    decl_table.open_scope();

                    auto clause = new_ast(AST_COMM_CLAUSE, tok.start);

                    Ast* comm = NULL;

                    if (tok.type == TOK_CASE) {
                        lex();
                        auto list = parse_expr_list();
                        auto& lhs = list->list;

                        if (tok.type == TOK_ARROW) {
                            auto chan = lhs.items[0];
                            comm = new_ast(AST_SEND_STMT, chan->start);
                            comm->send_stmt.chan = chan;
                            comm->send_stmt.value = parse_expr(false);
                            comm = fill_end(comm);
                        } else {
                            auto type = tok.type;
                            if (type == TOK_ASSIGN || type == TOK_DEFINE) {
                                lex();
                                auto start = tok.start;

                                auto rhs = new_list();
                                rhs.push(parse_expr(false));

                                comm = new_ast(AST_ASSIGN_STMT, list->start);
                                comm->assign_stmt.lhs = list;
                                comm->assign_stmt.op = type;
                                comm->assign_stmt.rhs = rhs.save();

                                if (type == TOK_DEFINE)
                                    For (list->list) declare_var(it->id.lit, comm, it);

                                comm = fill_end(comm);
                            } else {
                                auto x = lhs.items[0];
                                comm = new_ast(AST_EXPR_STMT, x->start);
                                comm->expr_stmt.x = x;
                                comm = fill_end(comm);
                            }
                        }
                    } else {
                        lex(); // skip the TOK_DEFAULT
                    }

                    expect(TOK_COLON);

                    clause->comm_clause.comm = comm;
                    clause->comm_clause.stmts = parse_stmt_list();

                    decl_table.close_scope();

                    clauses.push(fill_end(clause));
                }

                decl_table.close_scope();

                expect(TOK_RBRACE);
                expect(TOK_SEMICOLON);

                ast->select_stmt.clauses = clauses.save();
                return fill_end(ast);
            }
        case TOK_FOR:
            {
                auto ast = new_ast(AST_FOR_STMT, lex());

                Ast* s1 = NULL;
                Ast* s2 = NULL;
                Ast* s3 = NULL;
                bool is_range = false;

                if (tok.type != TOK_LBRACE) {
                    auto old = expr_lev;
                    expr_lev = -1;
                    defer { expr_lev = old; };

                    if (tok.type != TOK_SEMICOLON) {
                        // "for range x { ... }"
                        if (tok.type == TOK_RANGE) {
                            auto expr = new_ast(AST_UNARY_EXPR, lex());
                            expr->unary_expr.op = TOK_RANGE;
                            expr->unary_expr.rhs = parse_expr(false);
                            expr = fill_end(expr);

                            auto rhs = new_list();
                            rhs.push(expr);

                            s2 = new_ast(AST_ASSIGN_STMT, expr->start);
                            s2->assign_stmt.rhs = rhs.save();
                            s2->assign_stmt.lhs = NULL;
                            s2->assign_stmt.op = TOK_ILLEGAL;
                            s2 = fill_end(s2);

                            is_range = true;
                        } else {
                            s2 = parse_simple_stmt();
                            if (s2->type == AST_ASSIGN_STMT) {
                                switch (s2->assign_stmt.op) {
                                    case TOK_DEFINE:
                                    case TOK_ASSIGN:
                                        {
                                            auto rhs = s2->assign_stmt.rhs;

                                            if (rhs->type == AST_UNARY_EXPR)
                                                if (rhs->unary_expr.op == TOK_RANGE)
                                                    is_range = true;

                                            if (s2->assign_stmt.op == TOK_DEFINE)
                                                For (s2->assign_stmt.lhs->list)
                                                declare_var(it->id.lit, s2, it);

                                            break;
                                        }
                                }
                            }
                        }
                    }

                    if (!is_range && tok.type == TOK_SEMICOLON) {
                        lex();
                        s1 = s2;
                        s2 = NULL;
                        if (tok.type != TOK_SEMICOLON)
                            s2 = parse_simple_stmt();
                        expect(TOK_SEMICOLON);
                        if (tok.type != TOK_LBRACE)
                            s3 = parse_simple_stmt();
                    }
                }

                ast->for_stmt.s1 = s1;
                ast->for_stmt.s2 = s2;
                ast->for_stmt.s3 = s3;
                ast->for_stmt.body = parse_block();

                expect(TOK_SEMICOLON);
                return fill_end(ast);
            }
    }
    return NULL;
}

Ast* Parser::parse_stmt_list() {
    auto is_terminal = [&](Tok_Type type) -> bool {
        switch (type) {
            case TOK_CASE:
            case TOK_DEFAULT:
            case TOK_RBRACE:
            case TOK_EOF:
                return true;
        }
        return false;
    };

    auto stmts = new_list();
    while (!is_terminal(tok.type)) {
        auto stmt = parse_stmt();

        // Here, is_terminal only checks for four different token types. If we have
        // a syntax error, and the next token isn't terminal but doesn't result in
        // a valid statement, we'd just loop forever. To fix that we break out if
        // we couldn't read a statement.
        //
        // TODO: Should we synx or anything like tht?
        if (stmt == NULL) break;

        stmts.push(stmt);
    }
    return stmts.save();
}

Ast* Parser::parse_block() {
    auto ast = new_ast(AST_BLOCK, expect(TOK_LBRACE));

    decl_table.open_scope();
    ast->block.stmts = parse_stmt_list();
    decl_table.close_scope();

    expect(TOK_RBRACE);
    return fill_end(ast);
}

Ast* Parser::parse_call_args() {
    auto ast = new_ast(AST_CALL_ARGS, expect(TOK_LPAREN));

    expr_lev++;
    bool ellip = false;

    auto args = new_list();
    while (tok.type != TOK_RPAREN && tok.type != TOK_EOF && !ellip) {
        auto expr = parse_expr(false);
        args.push(expr);
        if (tok.type == TOK_ELLIPSIS) {
            ellip = true;
            lex();
        }
        if (!eat_comma(TOK_RPAREN, "function call")) break;
    }
    expect(TOK_RPAREN);
    expr_lev--;

    ast->call_args.args = args.save();
    ast->call_args.ellip = ellip;
    return fill_end(ast);
}

Ast* Parser::parse_primary_expr(bool lhs) {
    Ast* ret = NULL;
    auto start = tok.start;

    // parse operand
    switch (tok.type) {
        case TOK_ADD:
        case TOK_SUB:
        case TOK_NOT:
        case TOK_XOR:
        case TOK_MUL:
        case TOK_AND:
        case TOK_ARROW:
            // TODO: oh fuck there's some weird shit around parsing
            // arrows...
            ret = new_ast(AST_UNARY_EXPR, start);
            ret->unary_expr.op = tok.type;
            lex();
            ret->unary_expr.rhs = parse_primary_expr(false);
            ret = fill_end(ret);
            break;
        case TOK_LPAREN:
            ret = new_ast(AST_PAREN, lex());
            expr_lev++;
            ret->paren.x = parse_expr(false);
            expr_lev--;
            expect(TOK_RPAREN);
            ret = fill_end(ret);
            break;
        case TOK_INT:
        case TOK_FLOAT:
        case TOK_IMAG:
        case TOK_RUNE:
        case TOK_STRING:
            ret = new_ast(AST_BASIC_LIT, tok.start);
            ret->basic_lit.val = tok.val;
            ret->basic_lit.lit = tok.lit;
            lex();
            ret = fill_end(ret);
            break;
        case TOK_ID:
            ret = parse_id();
            if (!lhs)
                resolve(ret);
            break;
        case TOK_FUNC:
            ret = new_ast(AST_FUNC_LIT, lex());
            ret->func_lit.signature = parse_signature();
            expr_lev++;
            ret->func_lit.body = parse_block();
            expr_lev--;
            ret = fill_end(ret);
            break;
        default:
            ret = parse_type();
            break;
    }

    if (ret == NULL) {
        return NULL;
    }

    auto make_ast = [&](Ast_Type type) -> Ast* {
        return new_ast(type, ret->start);
    };

    while (true) {
        switch (tok.type) {
            case TOK_PERIOD:
                {
                    auto period_pos = tok.start;

                    lex();
                    if (lhs) resolve(ret);

                    switch (tok.type) {
                        case TOK_ID:
                            {
                                auto ast = make_ast(AST_SELECTOR_EXPR);
                                ast->selector_expr.x = ret;
                                ast->selector_expr.period_pos = period_pos;
                                ast->selector_expr.sel = parse_id();
                                ret = fill_end(ast);
                                break;
                            }
                        case TOK_LPAREN:
                            {
                                lex();
                                auto ast = make_ast(AST_TYPE_ASSERTION_EXPR);
                                ast->type_assertion_expr.x = ret;
                                if (tok.type == TOK_TYPE) {
                                    lex();
                                    ast->type_assertion_expr.type = NULL;
                                } else {
                                    ast->type_assertion_expr.type = parse_type();
                                }
                                expect(TOK_RPAREN);
                                ret = fill_end(ast);
                                break;
                            }
                        default:
                            parser_error("expected selector or type assertion");

                            auto ast = make_ast(AST_SELECTOR_EXPR);

                            auto sel = new_ast(AST_ID, tok.start_before_leading_whitespace);
                            sel->id.lit = "_";
                            sel = fill_end(sel);

                            lex();

                            ast->selector_expr.x = ret;
                            ast->selector_expr.period_pos = period_pos;
                            ast->selector_expr.sel = sel;
                            ret = fill_end(ast);
                            break;
                    }
                }
                break;
            case TOK_LBRACK:
                {
                    lex();

                    if (lhs)
                        resolve(ret);

                    expr_lev++;

                    Ast* keys[3] = { NULL, NULL, NULL };
                    s32 colons = 0;

                    if (tok.type != TOK_COLON)
                        keys[0] = parse_expr(false);

                    for (u32 i = 1; tok.type == TOK_COLON && i < 3; i++) {
                        colons++;
                        lex();
                        if (tok.type != TOK_COLON && tok.type != TOK_RBRACK &&
                            tok.type != TOK_EOF)
                            keys[i] = parse_expr(false);
                    }

                    expr_lev--;

                    expect(TOK_RBRACK);

                    if (colons == 0) {
                        auto ast = make_ast(AST_INDEX_EXPR);
                        ast->index_expr.x = ret;
                        ast->index_expr.key = keys[0];
                        ret = fill_end(ast);
                        break;
                    }

                    if (colons == 2)
                        if (keys[1] == NULL || keys[2] == NULL)
                            parser_error("Second and third indexes are required in 3-index slice.");

                    auto ast = new_ast(AST_SLICE_EXPR, ret->start);
                    ast->slice_expr.x = ret;
                    ast->slice_expr.s1 = keys[0];
                    ast->slice_expr.s2 = keys[1];
                    ast->slice_expr.s3 = keys[2];

                    ret = fill_end(ast);
                    break;
                }
            case TOK_LPAREN:
                {
                    if (lhs)
                        resolve(ret);

                    auto ast = make_ast(AST_CALL_EXPR);
                    ast->call_expr.func = ret;
                    ast->call_expr.call_args = parse_call_args();
                    ret = fill_end(ast);
                    break;
                }
            case TOK_LBRACE:
                {
                    // check that expression is a literal type
                    // TOK_LBRACE is only allowed if part of composite literal
                    // and if the type is an id, must be part of expression
                    switch (ret->type) {
                        case AST_ID:
                            if (expr_lev < 0) goto done;
                            break;
                        case AST_ARRAY_TYPE:
                        case AST_SLICE_TYPE:
                        case AST_STRUCT_TYPE:
                        case AST_MAP_TYPE:
                            break;
                        case AST_SELECTOR_EXPR:
                            if (ret->selector_expr.x->type != AST_ID) goto done;
                            if (expr_lev < 0) goto done;
                            break;
                        default:
                            goto done;
                    }

                    if (lhs) resolve(ret);

                    auto literal_value = parse_literal_value();

                    auto ast = make_ast(AST_COMPOSITE_LIT);
                    ast->composite_lit.base_type = ret;
                    ast->composite_lit.literal_value = literal_value;
                    ret = fill_end(ast);

                    break;
                }
            default:
                goto done;
        }
    }
done:

    return ret;
}

Ast* Parser::parse_literal_value() {
    auto ast = new_ast(AST_LITERAL_VALUE, expect(TOK_LBRACE));

    auto parse_literal_elem = [&]() -> Ast* {
        return (tok.type == TOK_LBRACE ? parse_literal_value()
            : parse_expr(false));
    };

    auto elems = new_list();
    while (tok.type != TOK_RBRACE && tok.type != TOK_EOF) {
        auto item = new_ast(AST_LITERAL_ELEM, tok.start);

        Ast* key = parse_literal_elem();
        Ast* elem = NULL;
        if (tok.type == TOK_COLON) {
            lex();
            elem = parse_literal_elem();
        } else {
            elem = key;
            key = NULL;
        }

        item->literal_elem.key = key;
        item->literal_elem.elem = elem;
        elems.push(fill_end(item));

        if (!eat_comma(TOK_RBRACE, "composite literal"))
            break;
    }

    expect(TOK_RBRACE);
    ast->literal_value.elems = elems.save();
    return fill_end(ast);
}

bool Parser::eat_comma(Tok_Type closing, ccstr context) {
    if (tok.type == TOK_COMMA) {
        lex();
        return true;
    }
    if (tok.type != closing)
        parser_error("Expected %s in %s.", tok_type_str(closing), context);
    return false;
}

Ast* Parser::parse_expr(bool lhs, OpPrec prec) {
    auto x = parse_primary_expr(lhs);

    while (true) {
        auto oprec = op_to_prec(tok.type);
        auto op = tok.type;

        if (prec >= oprec) return x;

        if (lhs) {
            resolve(x);
            lhs = false;
        }

        lex();

        auto bin = new_ast(AST_BINARY_EXPR, x->start);
        bin->binary_expr.lhs = x;
        bin->binary_expr.op = tok.type;
        bin->binary_expr.rhs = parse_expr(false, oprec);
        x = fill_end(bin);
    }
}

Ast* Parser::parse_type_list() {
    auto types = new_list();
    types.push(parse_type());
    while (tok.type == TOK_COMMA) {
        lex();
        types.push(parse_expr(false));
    }
    return types.save();
}

void Parser::resolve(Ast* id) {
    if (id->type != AST_ID) return;
    if (streq(id->id.lit, "_")) return;

    auto decl = decl_table.get(id->id.lit);
    id->id.decl = decl.decl_ast;
    id->id.decl_id = decl.where_to_jump;
}

Ast* Parser::parse_expr_list(bool lhs) {
    auto exprs = new_list();

    exprs.push(parse_expr(lhs));
    while (tok.type == TOK_COMMA) {
        lex();
        exprs.push(parse_expr(lhs));
    }

    auto ret = exprs.save();

    if (lhs) {
        switch (tok.type) {
            case TOK_DEFINE:
            case TOK_COLON:
                break;
            default:
                For (ret->list) resolve(it);
                break;
        }
    }

    return ret;
}

Ast* Parser::parse_package_decl() {
    auto pkg = new_ast(AST_PACKAGE_DECL, expect(TOK_PACKAGE));
    pkg->package_decl.name = parse_id();
    // NOTE: check for invalid "_"
    expect(TOK_SEMICOLON);
    return fill_end(pkg);
}

Ast* Parser::parse_file() {
    auto ast = new_ast(AST_SOURCE, tok.start);
    ast->source.package_decl = parse_package_decl();

    auto imports = new_list();
    while (tok.type == TOK_IMPORT) {
        auto decl = parse_decl(lookup_decl_start, true);
        if (decl != NULL)
            imports.push(decl);
    }
    ast->source.imports = imports.save();

    decl_table.open_scope();

    auto decls = new_list();
    while (tok.type != TOK_EOF) {
        auto decl = parse_decl(lookup_decl_start, false);
        if (decl != NULL)
            decls.push(decl);
    }
    ast->source.decls = decls.save();

    decl_table.close_scope();
    return fill_end(ast);
}

Ast* Parser::fill_end(Ast* ast) {
    ast->end = tok.start_before_leading_whitespace;
    return ast;
}

cur2 Parser::lex() {
    auto ret = tok.start;
    do {
        lex_with_comments();
        if (tok.type != TOK_COMMENT)
            last_token_type = tok.type;
    } while (tok.type == TOK_COMMENT);
    return ret;
}

#undef expect
#undef parser_error

File_Ast* Go_Index::make_file_ast(Ast* ast, ccstr file, ccstr import_path) {
    File_Ast* ret = alloc_object(File_Ast);
    ret->ast = ast;
    ret->file = file;
    ret->import_path = import_path;
    return ret;
}

bool Go_Index::match_import_spec(Ast* import_spec, ccstr want) {
    auto name = import_spec->import_spec.package_name;
    if (name != NULL)
        return streq(name->id.lit, want);

    auto path_str = import_spec->import_spec.path->basic_lit.val.string_val;
    auto imp = resolve_import(path_str);
    if (imp == NULL)
        return false;

    return streq(imp->package_name, want);
}

void Go_Index::list_decls_in_source(File_Ast* source, int flags, fn<void(Named_Decl*)> fn) {
    auto& s = source->ast->source;

    auto process_decl = [&](Ast* decl, Named_Decl* ret) {
        ret->decl = source->dup(decl);

        switch (decl->type) {
            case AST_FUNC_DECL:
                {
                    ret->items = alloc_list<Named_Decl_Item>(1);

                    auto item = ret->items->append();
                    item->name = decl->func_decl.name;
                    item->spec = decl;
                    break;
                }
            case AST_DECL:
                switch (decl->decl.type) {
                    case TOK_TYPE:
                        {
                            ret->items = alloc_list<Named_Decl_Item>(decl->decl.specs->list.len);
                            For (decl->decl.specs->list) {
                                auto item = ret->items->append();
                                item->name = it->type_spec.id;
                                item->spec = it;
                            }
                            break;
                        }
                    case TOK_VAR:
                    case TOK_CONST:
                        {
                            u32 len = 0;
                            For (decl->decl.specs->list) { len += it->value_spec.ids->list.len; }
                            ret->items = alloc_list<Named_Decl_Item>(len);
                            For (decl->decl.specs->list) {
                                auto spec = it;
                                For (it->value_spec.ids->list) {
                                    auto item = ret->items->append();
                                    item->spec = spec;
                                    item->name = it;
                                }
                            }
                            break;
                        }
                    case TOK_IMPORT:
                        ret->items = alloc_list<Named_Decl_Item>(decl->decl.specs->list.len);
                        For (decl->decl.specs->list) {
                            auto path = it->import_spec.path;
                            auto name = it->import_spec.package_name;
                            if (name != NULL) {
                                auto item = ret->items->append();
                                item->name = name;
                                item->spec = it;
                                continue;
                            }

                            auto path_str = path->basic_lit.val.string_val;
                            auto resolved_import = resolve_import(path_str);
                            if (resolved_import == NULL) continue;

                            auto ast = alloc_object(Ast);
                            ast->type = AST_ID;
                            ast->id.lit = resolved_import->package_name;

                            auto item = ret->items->append();
                            item->name = ast;
                            item->spec = it;
                        }
                        break;
                }
                break;
        }
    };

    if (flags & LISTDECLS_IMPORTS) {
        For (s.imports->list) {
            Named_Decl decl;
            process_decl(it, &decl);
            fn(&decl);
        }
    }

    if (flags & LISTDECLS_DECLS) {
        For (s.decls->list) {
            Named_Decl decl;
            process_decl(it, &decl);
            fn(&decl);
        }
    }
}

List<Named_Decl>* Go_Index::list_decls_in_source(File_Ast* source, int flags, List<Named_Decl>* out) {
    if (out == NULL)
        out = alloc_list<Named_Decl>();
    list_decls_in_source(source, flags, [&](Named_Decl* it) { out->append(it); });
    return out;
}

File_Ast* Go_Index::find_decl_in_source(File_Ast* source, ccstr desired_decl_name, bool import_only) {
    int flags = LISTDECLS_IMPORTS;
    if (!import_only)
        flags |= LISTDECLS_DECLS;

    auto make_result = [&](Ast* decl) -> File_Ast* {
        auto ret = alloc_object(File_Ast);
        ret->ast = decl;
        ret->file = source->file;
        return ret;
    };

    auto decls = list_decls_in_source(source, flags);
    for (auto&& decl : *decls)
        For (*decl.items)
            if (streq(it.name->id.lit, desired_decl_name))
                return make_result(it.spec);

    return NULL;
}

Ast *Index_Reader::parse_decl_from_entry(Index_Entry *entry) {
    Ast *ast = NULL;
    auto filepath = path_join(meta.package_path, entry->filename);
    with_parser_at_location(filepath, entry->hdr.pos, [&](Parser *p) {
        switch (entry->hdr.type) {
        case IET_FUNC: ast = p->parse_decl(lookup_decl_start); break;
        case IET_CONST: ast = p->parse_spec(TOK_CONST); break;
        case IET_VAR: ast = p->parse_spec(TOK_VAR); break;
        case IET_TYPE: ast = p->parse_spec(TOK_TYPE); break;
        }
    });
    return ast;
}

File_Ast* Go_Index::find_decl_in_package(ccstr desired_decl_name, ccstr import_path) {
    auto index_path = get_index_path(import_path);

    Index_Reader reader;
    if (!reader.init(index_path)) return NULL;
    defer { reader.cleanup(); };

    Index_Entry res;
    if (!reader.find_decl(desired_decl_name, &res)) return NULL;

    auto ast = reader.parse_decl_from_entry(&res);
    if (ast == NULL) return  NULL;

    auto filepath = path_join(reader.meta.package_path, res.filename);
    return make_file_ast(ast, filepath, import_path);
}

List<Named_Decl>* Go_Index::list_decls_in_package(ccstr path, ccstr import_path) {
    auto files = list_source_files(path);
    if (files == NULL) return NULL;

    auto ret = alloc_list<Named_Decl>();
    For (*files) {
        auto filename = path_join(path, it);
        auto ast = parse_file_into_ast(filename);
        if (ast != NULL)
            list_decls_in_source(make_file_ast(ast, filename, import_path), LISTDECLS_DECLS, ret);
    }

    return ret;
}

List<ccstr> *Go_Index::list_decl_names(ccstr import_path) {
    Index_Reader reader;
    if (!reader.init(get_index_path(import_path))) return NULL;
    defer { reader.cleanup(); };

    auto decls = reader.list_decls();
    if (decls == NULL) return NULL;

    auto ret = alloc_list<ccstr>(decls->len);
    For (*decls) ret->append(our_strcpy(it.name));
    return ret;
}

File_Ast* Go_Index::find_decl_of_id(File_Ast* fa) {
    auto ret = find_decl_of_id(fa, true);
    if (ret == NULL)
        ret = find_decl_of_id(fa, false);
    return ret;
}

File_Ast* Go_Index::find_decl_of_id(File_Ast* fa, bool import_only) {
    auto id = fa->ast;
    if (id->id.decl != NULL)
        return fa->dup(id->id.decl);

    auto lit = id->id.lit;
    if (import_only)
        return find_decl_in_source(fa->dup(parse_file_into_ast(fa->file)), lit, true);
    return find_decl_in_package(lit, fa->import_path);
}

File_Ast* Go_Index::get_base_type(File_Ast* type) {
    while (true) {
        auto ast = unparen(type->ast);
        switch (ast->type) {
        case AST_POINTER_TYPE:
            {
                auto base_type = get_base_type(type->dup(ast->pointer_type.base_type));
                auto ast2 = new_ast(AST_POINTER_TYPE, new_cur2(0, 0));
                ast2->pointer_type.base_type = base_type->ast;
                return base_type->dup(ast2);
            }
            break;
        case AST_ID:
            {
                auto decl = find_decl_of_id(type->dup(ast));
                if (decl == NULL)
                    goto done;

                auto new_type = get_type_from_decl(decl, ast->id.lit);
                if (new_type == NULL)
                    goto done;

                type = new_type;
                break;
            }
        case AST_SELECTOR_EXPR:
            {
                auto& sel = ast->selector_expr;
                if (sel.x->type != AST_ID)
                    goto done;

                goto done;  // TODO: fix this shit below

                /*
                // FIXME: it just makes no sense what i'm doing here
                // i think i was originally intending to use sel.x->id.lit to LOOK UP the package path, and append that to GOPATH
                // but now we should just call resolve_import
                //
                // Also, find_decl_in_package should be passed an import path;
                // I just don't know what the fuck this code even does right now
                auto pkg_path = path_join(GOPATH, sel.x->id.lit);
                auto decl = find_decl_in_package(pkg_path, sel.sel->id.lit, NULL);

                auto new_type = get_type_from_decl(decl, sel.sel->id.lit);
                if (new_type == NULL)
                    goto done;

                type = new_type;
                break;
                */
            }
        default:
            goto done;
        }
    }
done:
    return type;
}

File_Ast* Go_Index::get_type_from_decl(File_Ast* decl, ccstr id) {
    auto ast = decl->ast;
    switch (ast->type) {
    case AST_FIELD:
        For (ast->field.ids->list)
            if (it->type == AST_ID && streq(it->id.lit, id))
                return decl->dup(ast->field.type);
        break;
    case AST_ASSIGN_STMT:
        {
            // a, b, c = foo()                 // lhs.len = 1
            // a, b, c = foo(), bar(), baz()   // lhs.len > 1 and rhs.len == 1
            // a = foo()                       // lhs.len > 1 and rhs.len == lhs.len

            auto op = ast->assign_stmt.op;
            if (op != TOK_DEFINE && op != TOK_ASSIGN) return NULL;

            auto lhs = ast->assign_stmt.lhs;
            auto rhs = ast->assign_stmt.rhs;

            if (lhs->list.len == 1) {
                if (!streq(lhs->list[0]->id.lit, id)) return NULL;
                if (rhs->list.len != 1) return NULL;

                auto expr = rhs->list[0];

                auto r = infer_type(decl->dup(expr));
                if (r == NULL) return NULL;

                auto type = r->type->ast;

                if (type->type == AST_PARAMETERS) {
                    if (type->parameters.fields->list.len > 1) return NULL;

                    auto field = type->parameters.fields->list[0];
                    auto ids = field->field.ids;

                    if (ids != NULL && ids->list.len > 1) return NULL;

                    r->type->ast = field->field.type;
                }

                return r->type;
            }

            // at this point, lhslen > 1

            i32 idx = -1;
            {
                i32 i = 0;
                For (ast->assign_stmt.lhs->list) {
                    if (it->type == AST_ID && streq(it->id.lit, id)) {
                        idx = i;
                        break;
                    }
                    i++;
                }
            }

            if (idx == -1) return NULL;

            // ok, so we've identified the `idx`th item in lhs as the matching identifier.
            // now we have to figure out what element in rhs it corresponds to.

            // figure out how many values are in rhs.
            auto rhslen = rhs->list.len;
            if (rhslen == 1) {
                auto first = rhs->list[0];
                if (first->type == AST_PARAMETERS) {
                    s32 count = 0;
                    For (first->parameters.fields->list)
                        count += it->field.ids->list.len;
                    rhslen = count;
                }
            }

            // if rhslen == 1, expect it to be a parameter list
            if (rhslen == 1) {
                auto r = infer_type(decl->dup(rhs->list[0]));
                if (r == NULL) return NULL;

                auto type = r->type->ast;
                if (type->type != AST_PARAMETERS) return NULL;

                // get the type of the `idx`th parameter.
                {
                    s32 offset = 0;
                    For (type->parameters.fields->list) {
                        s32 len = 1;
                        if (it->field.ids != NULL)
                            len = it->field.ids->list.len;
                        if (offset + len >= idx)
                            return r->type->dup(it->field.type);
                        offset += len;
                    }
                }

                // somehow didn't find anything? (if rhslen <= idx)
                return NULL;
            }

            if (rhslen == lhs->list.len) {
                auto r = infer_type(decl->dup(rhs->list[idx]));
                return r == NULL ? NULL : r->type;
            }

            // rhslen != lhslen
            return NULL;
        }
    case AST_VALUE_SPEC:
        {
            if (ast->value_spec.type != NULL)
                return decl->dup(ast->value_spec.type);

            auto& spec_ids = ast->value_spec.ids->list;

            i32 idx = -1;
            for (u32 i = 0; i < spec_ids.len; i++) {
                if (streq(spec_ids[i]->id.lit, id)) {
                    idx = i;
                    break;
                }
            }
            if (idx == -1) return NULL;

            auto& spec_vals = ast->value_spec.vals->list;

            if (spec_ids.len == spec_vals.len) {
                auto r = infer_type(decl->dup(spec_vals[idx]));
                return r == NULL ? NULL : r->type;
            } else {
                do {
                    if (spec_vals.len != 1) break;

                    auto val = spec_vals[0];
                    if (val == NULL) break;
                    if (val->type != AST_CALL_EXPR &&
                        val->type != AST_TYPE_ASSERTION_EXPR)
                        break;

                    auto r = infer_type(decl->dup(val));
                    if (r == NULL) break;

                    Ast* type = r->type->ast;
                    if (type->type != AST_PARAMETERS) break;

                    auto& fields = type->parameters.fields->list;
                    if (idx >= fields.len) break;

                    auto field = fields[idx];
                    return r->type->dup(field->field.type);
                } while (0);
            }
            break;
        }
    case AST_TYPE_SPEC:
        return decl->dup(ast->type_spec.type);
    case AST_FUNC_DECL:
    case AST_IMPORT_SPEC:
        return decl;
    }
    return NULL;
}

/*
 * Given an AST_STRUCT_TYPE or AST_INTERFACE_TYPE, returns the interim type
 * (in Ast form) of a given field. Recursively handles embedded
 * fields/specs.
 */
File_Ast* Go_Index::find_field_or_method_in_type(File_Ast* base_type, File_Ast* interim_type, ccstr name) {
    File_Ast result;
    bool found = false;

    {
        SCOPED_FRAME();
        auto fields = list_fields_and_methods_in_type(base_type, interim_type);
        For (*fields) {
            if (streq(it.id.ast->id.lit, name)) {
                found = true;
                memcpy(&result, &it.decl, sizeof(it.decl));
                break;
            }
        }
    }

    if (found) {
        auto ret = alloc_object(File_Ast);
        memcpy(ret, &result, sizeof(result));
        return ret;
    }

    return NULL;
}

/*
 * This infers the type of expr and returns the decl of the immediate type
 * and base type. For example:
 *
 *     type a struct { foo int }
 *     type b a
 *     type c b
 *     var x c
 *
 * infer_type("x") will return c as the immediate type and struct { foo int
 * } as the base type.
 */
Infer_Res* Go_Index::infer_type(File_Ast* expr, bool resolve) {
    File_Ast *interim_type = NULL;

    auto ast = unparen(expr->ast);
    switch (ast->type) {
    case AST_UNARY_EXPR:
        {
            auto r = infer_type(expr->dup(ast->unary_expr.rhs));
            if (r == NULL) goto done;

            switch (ast->unary_expr.op) {
            // case TOK_RANGE: // idk even what to do here
            case TOK_ADD:
            case TOK_SUB:
            case TOK_NOT:
            case TOK_XOR:
                {
                    auto id = new_ast(AST_ID, new_cur2(0, 0));
                    id->id.lit = "int";
                    interim_type = make_file_ast(id, NULL, NULL);
                }
                break;
            case TOK_MUL:
                {
                    if (r->base_type == NULL) break;
                    auto type = r->base_type->ast;
                    if (type->type != AST_POINTER_TYPE) break;
                    interim_type = r->base_type->dup(type->pointer_type.base_type);
                }
                break;
            case TOK_AND:
                {
                    // don't need base type here
                    auto type = r->type->ast;
                    auto new_type = new_ast(AST_POINTER_TYPE, new_cur2(0, 0));
                    new_type->pointer_type.base_type = type;
                    interim_type = r->type->dup(new_type);
                }
                break;
            case TOK_ARROW:
                {
                    if (r->base_type == NULL) break;
                    auto type = r->base_type->ast;
                    if (type->type != AST_CHAN_TYPE) break;
                    interim_type = r->base_type->dup(type->chan_type.base_type);
                }
                break;
            }
        }
        break;
    case AST_CALL_EXPR:
        {
            auto r = infer_type(expr->dup(ast->call_expr.func), true);
            if (r == NULL) goto done;
            if (r->base_type == NULL) goto done;

            auto type = r->base_type->ast;
            Ast* sig = NULL;

            if (type->type == AST_FUNC_DECL)
                sig = type->func_decl.signature;
            else if (type->type == AST_FUNC_LIT)
                sig = type->func_lit.signature;
            else
                goto done;

            interim_type = r->base_type->dup(sig->signature.result);
        }
        break;
    case AST_INDEX_EXPR:
        do {
            auto r = infer_type(expr->dup(ast->call_expr.func), true);
            if (r == NULL) break;

            auto type = r->base_type->ast;
            while (type->type == AST_POINTER_TYPE)
                type = type->pointer_type.base_type;
            if (type == NULL) break;

            switch (type->type) {
            case AST_ARRAY_TYPE:
                interim_type = r->base_type->dup(type->array_type.base_type);
                break;
            case AST_MAP_TYPE:
                interim_type = r->base_type->dup(type->map_type.value_type);
                break;
            case AST_SLICE_TYPE:
                interim_type = r->base_type->dup(type->slice_type.base_type);
                break;
            case AST_ID:
                if (streq(ast->id.lit, "string"))
                    interim_type = make_file_ast(PRIMITIVE_TYPE, NULL, NULL);
                break;
            }
        } while (0);
        break;
    case AST_SELECTOR_EXPR:
        do {
            auto& selexpr = ast->selector_expr;

            if (selexpr.x->type == AST_ID) {
                do {
                    if (selexpr.x->id.decl != NULL) break;

                    auto file = parse_file_into_ast(expr->file);
                    if (file == NULL) break;

                    auto decl = find_decl_in_source(expr->dup(file), selexpr.x->id.lit);
                    if (decl == NULL) break;
                    if (decl->ast->type != AST_IMPORT_SPEC) break;

                    auto import_path = decl->ast->import_spec.path->basic_lit.val.string_val;
                    auto sel = selexpr.sel->id.lit;

                    decl = find_decl_in_package(sel, import_path);
                    if (decl == NULL) break;

                    interim_type = get_type_from_decl(decl, sel);
                    goto done;
                } while (0);
            }

            auto r = infer_type(expr->dup(selexpr.x), true);
            if (r == NULL) break;

            auto field = find_field_or_method_in_type(r->base_type, r->type, selexpr.sel->id.lit);
            if (field == NULL) break;

            switch (field->ast->type) {
            case AST_FUNC_DECL:
                interim_type = field;
                break;
            case AST_STRUCT_FIELD:
                interim_type = field->dup(field->ast->struct_field.type);
                break;
            }
        } while (0);
        break;
    case AST_SLICE_EXPR:
        {
            auto r = infer_type(expr->dup(ast->slice_expr.x));
            if (r == NULL) goto done;
            interim_type = r->type;
        }
        break;
    case AST_TYPE_ASSERTION_EXPR:
        interim_type = expr->dup(ast->type_assertion_expr.type);
        break;
    case AST_ID:
        {
            auto decl = find_decl_of_id(expr->dup(ast));
            if (decl == NULL) goto done;
            interim_type = get_type_from_decl(decl, ast->id.lit);
        }
        break;
    case AST_COMPOSITE_LIT:
        interim_type = expr->dup(ast->composite_lit.base_type);
        break;
        break;
    }
done:

    if (interim_type == NULL) return NULL;

    File_Ast* base_type = NULL;
    if (resolve) {
        base_type = get_base_type(interim_type);
        if (base_type == NULL) return NULL;
    }

    auto result = alloc_object(Infer_Res);
    result->type = interim_type;
    result->base_type = base_type;

    return result;
}

ccstr get_workspace_import_path() {
    if (world.wksp.gomod_exists) {
        auto module_path = world.wksp.gomod_info.module_path;;
        if (module_path != NULL) return module_path;
    }

    // if we're inside gopath
    auto gopath_base = path_join(GOPATH, "src");
    if (str_starts_with(world.wksp.path, gopath_base))
        return get_path_relative_to(world.wksp.path, gopath_base);

    return NULL; // should we just panic? this is a pretty bad situation
}

// This function assumes that decl contains id.
Ast* Go_Index::locate_id_in_decl(Ast* decl, ccstr id) {
    switch (decl->type) {
    case AST_IMPORT_SPEC:
        {
            auto name = decl->import_spec.package_name;
            if (name != NULL)
                return name;
            return decl;
        }
    case AST_VALUE_SPEC:
        For (decl->value_spec.ids->list) {
            if (streq(it->id.lit, id))
                return it;
        }
        break;
    case AST_TYPE_SPEC:
        return decl->type_spec.id;
    case AST_FUNC_DECL:
        return decl->func_decl.name;
    }
    return NULL;
}

Jump_To_Definition_Result* Go_Index::jump_to_definition(ccstr filepath, cur2 pos) {
    Ast* file = NULL;

    {
        auto iter = file_loader(filepath);
        defer { iter->cleanup(); };
        if (iter == NULL) return NULL;

        if (iter->type == IT_BUFFER)
            pos = iter->buffer_params.it.buf->offset_to_cur(pos.x);

        Parser p;
        p.init(iter);
        p.filepath = filepath;
        defer { p.cleanup(); };
        file = p.parse_file();
    }

    if (file == NULL) return NULL;

    Jump_To_Definition_Result result;
    result.file = NULL;

    find_nodes_containing_pos(file, pos, [&](Ast *ast) -> Walk_Action {
        auto contains_pos = [&](Ast* ast) -> bool {
            return locate_pos_relative_to_ast(pos, ast) == 0;
        };

        switch (ast->type) {
        case AST_PACKAGE_DECL:
            if (contains_pos(ast->package_decl.name)) {
                result.file = filepath;
                result.pos = ast->package_decl.name->start;
            }
            return WALK_ABORT;
        case AST_IMPORT_SPEC:
            result.file = filepath;
            result.pos = ast->start;
            return WALK_ABORT;
        case AST_SELECTOR_EXPR:
            {
                auto sel = ast->selector_expr.sel;
                if (!contains_pos(sel))
                    return WALK_CONTINUE;

                auto x = ast->selector_expr.x;
                if (x->type == AST_ID) {
                    auto try_to_find_package = [&]() -> bool {
                        auto spec = find_decl_of_id(make_file_ast(x, filepath, file_to_import_path(filepath)));
                        if (spec == NULL) return false;
                        if (spec->ast->type != AST_IMPORT_SPEC) return false;

                        auto import_path = spec->ast->import_spec.path->basic_lit.val.string_val;
                        auto decl = find_decl_in_package(sel->id.lit, import_path);

                        if (decl == NULL) return false;

                        auto id = locate_id_in_decl(decl->ast, sel->id.lit);
                        if (id == NULL) return false;

                        result.file = decl->file;
                        result.pos = id->start;
                        return true;
                    };

                    if (try_to_find_package())
                        return WALK_ABORT;
                }

                do {
                    auto r = infer_type(make_file_ast(ast->selector_expr.x, filepath, file_to_import_path(filepath)), true);
                    if (r == NULL) break;

                    auto field = find_field_or_method_in_type(r->base_type, r->type, sel->id.lit);
                    if (field == NULL) break;

                    result.file = field->file;
                    result.pos = field->ast->start;
                } while (0);

                return WALK_ABORT;
            }
        case AST_ID:
            do {
                auto decl = find_decl_of_id(make_file_ast(ast, filepath, file_to_import_path(filepath)));
                if (decl == NULL) break;

                auto id = locate_id_in_decl(decl->ast, ast->id.lit);
                if (id == NULL) break;

                result.file = decl->file;
                result.pos = id->start;
            } while (0);
            return WALK_ABORT;
        }
        return WALK_CONTINUE;
    });

    if (result.file == NULL)
        return NULL;

    if (result.pos.y == -1) {
        auto ef = read_entire_file(result.file);
        if (ef == NULL) return NULL;
        defer { free_entire_file(ef); };

        cur2 newpos = {0};
        for (u32 i = 0; i < ef->len && i < result.pos.x; i++) {
            if (ef->data[i] == '\r') continue;
            if (ef->data[i] == '\n') {
                newpos.y++;
                newpos.x = 0;
                continue;
            }
            newpos.x++;
        }
        result.pos = newpos;
    }

    auto ret = alloc_object(Jump_To_Definition_Result);
    *ret = result;
    return ret;
}

bool Go_Index::autocomplete(ccstr filepath, cur2 pos, bool triggered_by_period, Autocomplete *out) {
    auto file = parse_file_into_ast(filepath);
    if (file == NULL) return NULL;

    Autocomplete ret = { 0 };

    auto generate_results_from_current_package = [&]() -> List<AC_Result>* {
        auto curr_path = our_dirname(filepath);
        auto wksp_path = world.wksp.path;

        if (!make_path(wksp_path)->contains(make_path(curr_path)))
            return NULL;

        auto import_path = path_join(
            get_workspace_import_path(),
            get_path_relative_to(curr_path, wksp_path)
        );

        auto names = list_decl_names(import_path);
        if (names == NULL) return NULL;

        {
            SCOPED_MEM(&world.autocomplete_mem);
            auto ret = alloc_list<AC_Result>();
            For (*names) {
                auto r = ret->append();
                r->name = our_strcpy(it);
            }
            return ret;
        }
    };

    find_nodes_containing_pos(file, pos, [&](Ast *ast) -> Walk_Action {
        switch (ast->type) {
        case AST_SELECTOR_EXPR:
            {
                auto sel = ast->selector_expr.sel;
                if (pos < ast->selector_expr.period_pos || pos > sel->end)
                    return WALK_CONTINUE;

                auto x = ast->selector_expr.x;
                if (x->type == AST_ID) {
                    bool found_package = false;

                    do {
                        auto spec = find_decl_of_id(make_file_ast(x, filepath, file_to_import_path(filepath)), true);
                        if (spec == NULL) break;

                        auto spec_ast = spec->ast;
                        if (spec_ast->type != AST_IMPORT_SPEC) break;

                        auto import_path = spec_ast->import_spec.path->basic_lit.val.string_val;
                        auto names = list_decl_names(import_path);
                        if (names == NULL) break;

                        // at this point, we have confirmation that x refers to a package.
                        // doesn't matter if we can't find any decls, we should terminate here
                        // with what we find, and not keep searching as though x might be something else

                        {
                            SCOPED_MEM(&world.autocomplete_mem);
                            ret.results = alloc_list<AC_Result>();
                            For (*names) {
                                auto result = ret.results->append();
                                result->name = our_strcpy(it);
                            }
                        }

                        ret.type = AUTOCOMPLETE_PACKAGE_EXPORTS;
                        ret.keyword_start_position = ast->selector_expr.period_pos;
                        return WALK_ABORT;
                    } while (0);
                }

                auto r = infer_type(make_file_ast(x, filepath, file_to_import_path(filepath)), true);
                if (r == NULL) return WALK_ABORT;

                auto fields = list_fields_and_methods_in_type(r->base_type, r->type);

                {
                    SCOPED_MEM(&world.autocomplete_mem);
                    ret.results = alloc_list<AC_Result>(fields->len);
                    For (*fields) {
                        auto result = ret.results->append();
                        result->name = our_strcpy(it.id.ast->id.lit);
                    }
                }

                ret.type = AUTOCOMPLETE_STRUCT_FIELDS;
                ret.keyword_start_position = ast->selector_expr.period_pos;
            }
            return WALK_ABORT;

        case AST_ID:
            out->type = AUTOCOMPLETE_IDENTIFIER;
            out->keyword_start_position = ast->start;
            out->results = generate_results_from_current_package();
            break;
        }

        return WALK_CONTINUE;
    });

    if (ret.results == NULL) {
        if (triggered_by_period) return false;

        ret.type = AUTOCOMPLETE_IDENTIFIER;
        ret.keyword_start_position = pos;
        ret.results = generate_results_from_current_package();
    }

    memcpy(out, &ret, sizeof(ret));
    return true;
}

Parameter_Hint* Go_Index::parameter_hint(ccstr filepath, cur2 pos, bool triggered_by_paren) {
    auto file = parse_file_into_ast(filepath);
    if (file == NULL) return NULL;

    Parameter_Hint* ret = NULL;

    auto walk_callback = [&](Ast* ast) -> Walk_Action {
        if (ast->type != AST_CALL_EXPR)
            return WALK_CONTINUE;

        auto args = ast->call_expr.call_args;
        if (locate_pos_relative_to_ast(pos, args) != 0)
            return WALK_CONTINUE;

        auto type = infer_type(make_file_ast(ast->call_expr.func, filepath, get_workspace_import_path()), true);
        if (type != NULL) {
            auto get_func_signature = [&](Ast* func) -> Ast* {
                switch (func->type) {
                case AST_FUNC_DECL: return func->func_decl.signature;
                case AST_FUNC_TYPE: return func->func_type.signature;
                }
                return NULL;
            };

            auto sig = get_func_signature(type->base_type->ast);
            if (sig != NULL) {
                ret = alloc_object(Parameter_Hint);
                ret->signature = sig;
                ret->call_args = args;
            }
        }

        return WALK_ABORT;
    };

    find_nodes_containing_pos(file, pos, walk_callback);
    return ret;
}

List<Field>* Go_Index::list_fields_in_type(File_Ast* ast) {
    s32 len = list_fields_in_type(ast, NULL);
    auto ret = alloc_list<Field>(len);
    list_fields_in_type(ast, ret);
    return ret;
}

int Go_Index::list_methods_in_base_type(File_Ast* ast, List<Field>* ret) {
    auto type = ast->ast;
    if (type == NULL) return 0;
    int count = 0;

    switch (type->type) {
    case AST_POINTER_TYPE:
        {
            auto pointee = type->pointer_type.base_type;
            auto base_type = get_base_type(ast->dup(pointee));
            count += list_methods_in_base_type(base_type, ret);
        }
        break;
    case AST_STRUCT_TYPE:
        For (type->struct_type.fields->list) {
            auto& f = it->struct_field;
            if (f.ids == NULL) {
                auto embedded_type = unpointer(f.type);
                if (embedded_type->type == AST_ID || embedded_type->type == AST_SELECTOR_EXPR)
                    count += list_methods_in_type(ast->dup(embedded_type), ret);
            }
        }
        break;
    }
    return count;
}

int Go_Index::list_methods_in_type(File_Ast* type, List<Field>* ret) {
    auto ast = unpointer(type->ast);
    ccstr type_name = NULL;
    ccstr package_to_search = NULL;

    switch (ast->type) {
    case AST_ID:
        type_name = ast->id.lit;
        package_to_search = type->import_path;
        break;
    case AST_SELECTOR_EXPR:
        {
            // foo.Bar
            // assert foo is package
            // probably assert first letter of Bar is capitalized
            // assert bar is name of a type

            auto sel = ast->selector_expr.sel;
            if (sel == NULL) break;
            if (sel->type != AST_ID) break;

            type_name = sel->id.lit;
            if (!isupper(type_name[0])) break;

            auto x = ast->selector_expr.x;
            if (x == NULL) break;
            if (x->type != AST_ID) break;

            auto id_decl = find_decl_of_id(type->dup(x));
            if (id_decl == NULL) break;
            if (id_decl->ast->type != AST_IMPORT_SPEC) break;

            package_to_search = id_decl->ast->import_spec.path->basic_lit.val.string_val;
        }
        break;
    }

    if (type_name == NULL || package_to_search == NULL) return 0;

    /*
    Ok, so our algorithm *really* needs to be:

        if the index is available
            prefix = our_sprintf("%s.", type_name)
            for each item in index that starts with prefix
                use item to generate field
                append field to ret
        else
            go and list decls from the package
            if decl is func and receiver is our type_name
                use decl to generate field
                append field to ret

    ...fuck this shit lol
    honestly, should we just have intellisense not work if the index isn't working lmao
    */

    int count = 0;
    auto prefix = our_sprintf("%s.", type_name);

    Index_Reader reader;
    if (!reader.init(get_index_path(package_to_search))) return 0;
    defer { reader.cleanup(); };

    auto decls = reader.list_decls();
    if (decls == NULL) return NULL;

    For (*decls) {
        if (it.hdr.type != IET_FUNC) continue;
        if (!str_starts_with(it.name, prefix)) continue;

        auto ast = reader.parse_decl_from_entry(&it);
        if (ast == NULL) continue;
        if (ast->type != AST_FUNC_DECL) continue;

        auto filepath = path_join(reader.meta.package_path, it.filename);

        auto r = ret->append();

        r->decl.ast = ast;
        r->decl.file = filepath;
        r->decl.import_path = package_to_search;

        r->id.ast = ast->func_decl.name;
        r->id.file = filepath;
        r->id.import_path = package_to_search;

        count++;
    }

    return count;
}

List<Field>* Go_Index::list_fields_and_methods_in_type(File_Ast* base_type, File_Ast* interim_type) {
    bool is_named_type = false;
    switch (unpointer(interim_type->ast)->type) {
    case AST_ID:
    case AST_SELECTOR_EXPR:
        is_named_type = true;
        break;
    }

    auto ret = alloc_list<Field>();
    list_fields_in_type(base_type, ret);
    if (is_named_type) {
        list_methods_in_type(interim_type, ret);
        list_methods_in_base_type(base_type, ret);
    }

    return ret;
}

int Go_Index::list_fields_in_type(File_Ast* ast, List<Field>* ret) {
    auto type = ast->ast;
    if (type == NULL) return 0;
    int count = 0;

    switch (type->type) {
    case AST_POINTER_TYPE:
        {
            auto pointee = type->pointer_type.base_type;
            auto base_type = get_base_type(ast->dup(pointee));
            count += list_fields_in_type(base_type, ret);
        }
        break;
    case AST_STRUCT_TYPE:
        For (type->struct_type.fields->list) {
            auto& f = it->struct_field;
            if (f.ids == NULL) {
                auto base_type = get_base_type(ast->dup(f.type));
                count += list_fields_in_type(base_type, ret);
                continue;
            }
            for (auto&& id : f.ids->list) {
                if (ret != NULL) {
                    auto item = ret->append();
                    item->id.ast = id;
                    item->id.file = ast->file;
                    item->decl.ast = it;
                    item->decl.file = ast->file;
                }
                count++;
            }
        }
        break;
    case AST_INTERFACE_TYPE:
        For (type->interface_type.specs->list) {
            auto& s = it->interface_spec;

            if (s.type != NULL) {
                auto base_type = get_base_type(ast->dup(s.type));
                if (base_type != NULL)
                    count += list_fields_in_type(base_type, ret);
                continue;
            }

            if (ret != NULL) {
                auto item = ret->append();
                item->id.ast = s.name;
                item->id.file = ast->file;
                item->decl.ast = it;
                item->decl.file = ast->file;
            }
            count++;
        }
        break;
    }

    return count;
}

// returns -1 if pos is before ast
//                    0 if pos is inside ast
//                    1 if pos is after ast
i32 locate_pos_relative_to_ast(cur2 pos, Ast* ast) {
    if (pos < ast->start) return -1;
    if (pos > ast->end) return 1;
    return 0;
};

bool is_prime(u32 x) {
    if (x < 2) return false;
    if (x < 4) return true;
    if (x % 2 == 0) return false;

    for (int i = 3; i <= floor(sqrt((double)x)); i += 2)
        if (x % i == 0)
            return false;
    return true;
}

u32 next_prime(u32 x) {
    while (!is_prime(x))
        x++;
    return x;
}

ccstr tok_type_str(Tok_Type type) {
    switch (type) {
        define_str_case(TOK_ILLEGAL);
        define_str_case(TOK_EOF);
        define_str_case(TOK_COMMENT);
        define_str_case(TOK_ID);
        define_str_case(TOK_INT);
        define_str_case(TOK_FLOAT);
        define_str_case(TOK_IMAG);
        define_str_case(TOK_RUNE);
        define_str_case(TOK_STRING);
        define_str_case(TOK_ADD);
        define_str_case(TOK_SUB);
        define_str_case(TOK_MUL);
        define_str_case(TOK_QUO);
        define_str_case(TOK_REM);
        define_str_case(TOK_AND);
        define_str_case(TOK_OR);
        define_str_case(TOK_XOR);
        define_str_case(TOK_SHL);
        define_str_case(TOK_SHR);
        define_str_case(TOK_AND_NOT);
        define_str_case(TOK_ADD_ASSIGN);
        define_str_case(TOK_SUB_ASSIGN);
        define_str_case(TOK_MUL_ASSIGN);
        define_str_case(TOK_QUO_ASSIGN);
        define_str_case(TOK_REM_ASSIGN);
        define_str_case(TOK_AND_ASSIGN);
        define_str_case(TOK_OR_ASSIGN);
        define_str_case(TOK_XOR_ASSIGN);
        define_str_case(TOK_SHL_ASSIGN);
        define_str_case(TOK_SHR_ASSIGN);
        define_str_case(TOK_AND_NOT_ASSIGN);
        define_str_case(TOK_LAND);
        define_str_case(TOK_LOR);
        define_str_case(TOK_ARROW);
        define_str_case(TOK_INC);
        define_str_case(TOK_DEC);
        define_str_case(TOK_EQL);
        define_str_case(TOK_LSS);
        define_str_case(TOK_GTR);
        define_str_case(TOK_ASSIGN);
        define_str_case(TOK_NOT);
        define_str_case(TOK_NEQ);
        define_str_case(TOK_LEQ);
        define_str_case(TOK_GEQ);
        define_str_case(TOK_DEFINE);
        define_str_case(TOK_ELLIPSIS);
        define_str_case(TOK_LPAREN);
        define_str_case(TOK_LBRACK);
        define_str_case(TOK_LBRACE);
        define_str_case(TOK_COMMA);
        define_str_case(TOK_PERIOD);
        define_str_case(TOK_RPAREN);
        define_str_case(TOK_RBRACK);
        define_str_case(TOK_RBRACE);
        define_str_case(TOK_SEMICOLON);
        define_str_case(TOK_COLON);
        define_str_case(TOK_BREAK);
        define_str_case(TOK_CASE);
        define_str_case(TOK_CHAN);
        define_str_case(TOK_CONST);
        define_str_case(TOK_CONTINUE);
        define_str_case(TOK_DEFAULT);
        define_str_case(TOK_DEFER);
        define_str_case(TOK_ELSE);
        define_str_case(TOK_FALLTHROUGH);
        define_str_case(TOK_FOR);
        define_str_case(TOK_FUNC);
        define_str_case(TOK_GO);
        define_str_case(TOK_GOTO);
        define_str_case(TOK_IF);
        define_str_case(TOK_IMPORT);
        define_str_case(TOK_INTERFACE);
        define_str_case(TOK_MAP);
        define_str_case(TOK_PACKAGE);
        define_str_case(TOK_RANGE);
        define_str_case(TOK_RETURN);
        define_str_case(TOK_SELECT);
        define_str_case(TOK_STRUCT);
        define_str_case(TOK_SWITCH);
        define_str_case(TOK_TYPE);
        define_str_case(TOK_VAR);
    }
    return NULL;
}

ccstr ast_type_str(Ast_Type type) {
    switch (type) {
        define_str_case(AST_ILLEGAL);
        define_str_case(AST_BASIC_LIT);
        define_str_case(AST_ID);
        define_str_case(AST_UNARY_EXPR);
        define_str_case(AST_BINARY_EXPR);
        define_str_case(AST_ARRAY_TYPE);
        define_str_case(AST_POINTER_TYPE);
        define_str_case(AST_SLICE_TYPE);
        define_str_case(AST_MAP_TYPE);
        define_str_case(AST_STRUCT_TYPE);
        define_str_case(AST_INTERFACE_TYPE);
        define_str_case(AST_CHAN_TYPE);
        define_str_case(AST_FUNC_TYPE);
        define_str_case(AST_PAREN);
        define_str_case(AST_SELECTOR_EXPR);
        define_str_case(AST_SLICE_EXPR);
        define_str_case(AST_INDEX_EXPR);
        define_str_case(AST_TYPE_ASSERTION_EXPR);
        define_str_case(AST_CALL_EXPR);
        define_str_case(AST_CALL_ARGS);
        define_str_case(AST_LITERAL_ELEM);
        define_str_case(AST_LITERAL_VALUE);
        define_str_case(AST_LIST);
        define_str_case(AST_ELLIPSIS);
        define_str_case(AST_FIELD);
        define_str_case(AST_PARAMETERS);
        define_str_case(AST_FUNC_LIT);
        define_str_case(AST_BLOCK);
        define_str_case(AST_SIGNATURE);
        define_str_case(AST_COMPOSITE_LIT);
        define_str_case(AST_GO_STMT);
        define_str_case(AST_DEFER_STMT);
        define_str_case(AST_SELECT_STMT);
        define_str_case(AST_SWITCH_STMT);
        define_str_case(AST_IF_STMT);
        define_str_case(AST_FOR_STMT);
        define_str_case(AST_RETURN_STMT);
        define_str_case(AST_BRANCH_STMT);
        define_str_case(AST_EMPTY_STMT);
        define_str_case(AST_DECL);
        define_str_case(AST_ASSIGN_STMT);
        define_str_case(AST_VALUE_SPEC);
        define_str_case(AST_IMPORT_SPEC);
        define_str_case(AST_TYPE_SPEC);
        define_str_case(AST_INC_DEC_STMT);
        define_str_case(AST_SEND_STMT);
        define_str_case(AST_LABELED_STMT);
        define_str_case(AST_EXPR_STMT);
        define_str_case(AST_PACKAGE_DECL);
        define_str_case(AST_SOURCE);
        define_str_case(AST_STRUCT_FIELD);
        define_str_case(AST_INTERFACE_SPEC);
        define_str_case(AST_CASE_CLAUSE);
        define_str_case(AST_COMM_CLAUSE);
        define_str_case(AST_FUNC_DECL);
        define_str_case(AST_EMPTY);
    }
    return NULL;
};

ccstr ast_chan_direction_str(Ast_Chan_Direction dir) {
    switch (dir) {
        define_str_case(AST_CHAN_RECV);
        define_str_case(AST_CHAN_SEND);
        define_str_case(AST_CHAN_BI);
    }
    return NULL;
}

bool isid(int c) { return (isalnum(c) || c == '_'); }

bool ishex(int c) {
    if ('0' <= c && c <= '9') return true;
    if ('a' <= c && c <= 'f') return true;
    if ('A' <= c && c <= 'F') return true;
    return false;
}

bool isoct(int c) { return ('0' <= c && c <= '7'); }

bool lookup_stmt_start(Tok_Type type) {
    switch (type) {
    case TOK_BREAK:
    case TOK_CONST:
    case TOK_CONTINUE:
    case TOK_DEFER:
    case TOK_FALLTHROUGH:
    case TOK_FOR:
    case TOK_GO:
    case TOK_GOTO:
    case TOK_IF:
    case TOK_RETURN:
    case TOK_SELECT:
    case TOK_SWITCH:
    case TOK_TYPE:
    case TOK_VAR:
        return true;
    }
    return false;
}

bool lookup_decl_start(Tok_Type type) {
    switch (type) {
    case TOK_CONST:
    case TOK_TYPE:
    case TOK_VAR:
        return true;
    }
    return false;
}

bool lookup_expr_end(Tok_Type type) {
    switch (type) {
    case TOK_COMMA:
    case TOK_COLON:
    case TOK_SEMICOLON:
    case TOK_RPAREN:
    case TOK_RBRACK:
    case TOK_RBRACE:
        return true;
    }
    return false;
}

OpPrec op_to_prec(Tok_Type type) {
    switch (type) {
    case TOK_MUL:
    case TOK_QUO:
    case TOK_REM:
    case TOK_SHL:
    case TOK_SHR:
    case TOK_AND:
    case TOK_AND_NOT:
        return PREC_MUL;
    case TOK_ADD:
    case TOK_SUB:
    case TOK_OR:
    case TOK_XOR:
        return PREC_ADD;
    case TOK_EQL:
    case TOK_NEQ:
    case TOK_LSS:
    case TOK_LEQ:
    case TOK_GTR:
    case TOK_GEQ:
        return PREC_COMPARE;
    case TOK_LAND:
        return PREC_LAND;
    case TOK_LOR:
        return PREC_LOR;
    }
    return PREC_LOWEST;
}

ccstr gomod_tok_type_str(Gomod_Tok_Type t) {
    switch (t) {
        define_str_case(GOMOD_TOK_ILLEGAL);
        define_str_case(GOMOD_TOK_EOF);
        define_str_case(GOMOD_TOK_COMMENT);
        define_str_case(GOMOD_TOK_LPAREN);
        define_str_case(GOMOD_TOK_RPAREN);
        define_str_case(GOMOD_TOK_ARROW);
        define_str_case(GOMOD_TOK_NEWLINE);
        define_str_case(GOMOD_TOK_MODULE);
        define_str_case(GOMOD_TOK_GO);
        define_str_case(GOMOD_TOK_REQUIRE);
        define_str_case(GOMOD_TOK_REPLACE);
        define_str_case(GOMOD_TOK_EXCLUDE);
        define_str_case(GOMOD_TOK_STRIDENT);
    }
    return NULL;
};

void Gomod_Parser::parse(Gomod_Info* info) {
#define ASSERT(x) if (!(x)) goto done
#define EXPECT(x) ASSERT((lex(), (tok.type == (x))))

    while (true) {
        lex();
        while (tok.type == GOMOD_TOK_NEWLINE)
            lex();
        if (tok.type == GOMOD_TOK_EOF || tok.type == GOMOD_TOK_ILLEGAL)
            break;

        auto keyword = tok.type;
        switch (keyword) {
        case GOMOD_TOK_MODULE:
            {
                lex();
                if (tok.type == GOMOD_TOK_LPAREN) {
                    EXPECT(GOMOD_TOK_LPAREN);
                    EXPECT(GOMOD_TOK_NEWLINE);
                    EXPECT(GOMOD_TOK_STRIDENT);
                    if (!tok.val_truncated)
                        info->module_path = our_strcpy(tok.val);
                    EXPECT(GOMOD_TOK_NEWLINE);
                    EXPECT(GOMOD_TOK_RPAREN);
                } else if (tok.type == GOMOD_TOK_STRIDENT) {
                    if (!tok.val_truncated)
                        info->module_path = our_strcpy(tok.val);
                } else {
                    goto done;
                }
                EXPECT(GOMOD_TOK_NEWLINE);
            }
            break;
        case GOMOD_TOK_GO:
            {
                EXPECT(GOMOD_TOK_STRIDENT);
                if (!tok.val_truncated)
                    info->go_version = our_strcpy(tok.val);
                EXPECT(GOMOD_TOK_NEWLINE);
            }
            break;
        case GOMOD_TOK_REQUIRE:
        case GOMOD_TOK_EXCLUDE:
            {
                lex();

                auto read_spec = [&]() -> bool {
                    Gomod_Directive* directive = NULL;

                    ASSERT(tok.type == GOMOD_TOK_STRIDENT);
                    if (keyword == GOMOD_TOK_REQUIRE) {
                        directive = alloc_object(Gomod_Directive);
                        directive->type = GOMOD_DIRECTIVE_REQUIRE;
                        directive->module_path = tok.val;
                    }

                    EXPECT(GOMOD_TOK_STRIDENT);

                    if (keyword == GOMOD_TOK_REQUIRE) {
                        directive->module_version = tok.val;
                        info->directives.append(directive);
                    }

                    EXPECT(GOMOD_TOK_NEWLINE);
                    return true;

                done:
                    return false;
                };

                if (tok.type == GOMOD_TOK_LPAREN) {
                    EXPECT(GOMOD_TOK_NEWLINE);
                    while (lex(), tok.type != GOMOD_TOK_RPAREN) {
                        ASSERT(read_spec());
                    }
                    EXPECT(GOMOD_TOK_NEWLINE);
                } else {
                    ASSERT(read_spec());
                }
            }
            break;
        case GOMOD_TOK_REPLACE:
            {
                lex();

                auto read_spec = [&]() -> bool {
                    auto directive = alloc_object(Gomod_Directive);
                    directive->type = GOMOD_DIRECTIVE_REPLACE;

                    ASSERT(tok.type == GOMOD_TOK_STRIDENT);
                    directive->module_path = tok.val;

                    lex();
                    if (tok.type != GOMOD_TOK_ARROW) {
                        ASSERT(tok.type == GOMOD_TOK_STRIDENT);
                        directive->module_version = tok.val;
                        EXPECT(GOMOD_TOK_ARROW);
                    }

                    EXPECT(GOMOD_TOK_STRIDENT);
                    directive->replace_path = tok.val;

                    lex();
                    if (tok.type != GOMOD_TOK_NEWLINE) {
                        ASSERT(tok.type == GOMOD_TOK_STRIDENT);
                        directive->replace_version = tok.val;
                        EXPECT(GOMOD_TOK_NEWLINE);
                    }
                    return true;

                done:
                    return false;
                };

                if (tok.type == GOMOD_TOK_LPAREN) {
                    EXPECT(GOMOD_TOK_NEWLINE);
                    while (lex(), (tok.type != GOMOD_TOK_RPAREN)) {
                        ASSERT(read_spec());
                    }
                } else if (tok.type == GOMOD_TOK_STRIDENT) {
                    ASSERT(read_spec());
                }
            }
            break;
        default:
            goto done;
        }
    }

done:
    return;
}

#undef EXPECT
#undef ASSERT

bool gomod_isspace(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\r';
}

void Gomod_Parser::lex() {
    tok.type = GOMOD_TOK_ILLEGAL;
    tok.val_truncated = false;
    tok.val = NULL;

    while (gomod_isspace(it->peek()))
        it->next();

    if (it->eof()) {
        tok.type = GOMOD_TOK_EOF;
        return;
    }

    char firstchar = it->next();
    switch (firstchar) {
    case '(': tok.type = GOMOD_TOK_LPAREN; return;
    case ')': tok.type = GOMOD_TOK_RPAREN; return;
    case '\n':
        while (it->peek() == '\n') it->next();
        tok.type = GOMOD_TOK_NEWLINE;
        return;

    case '=':
        if (it->peek() == '>') {
            it->next();
            tok.type = GOMOD_TOK_ARROW;
            return;
        }
        break;

    case '/':
        if (it->peek() == '/') {
            it->next();
            while (it->peek() != '\n')
                it->next();
            lex();
            return;
        }
        break;
    }

    tok.type = GOMOD_TOK_STRIDENT;

    // get ready to read a string or identifier
    String_Allocator salloc;
    tok.val = salloc.start(MAX_PATH);

    auto should_end = [&](char ch) {
        if (firstchar == '"' || firstchar == '`')
            return ch == firstchar;
        return gomod_isspace(ch) || ch == '\n';
    };

    auto try_push_char = [&](char ch) {
        if (salloc.push(ch)) return true;

        // unable to push? truncate val and ingest rest of string
        tok.val_truncated = true;
        salloc.truncate();
        while (!should_end(it->peek())) it->next();
        return false;
    };

    try_push_char(firstchar);
    char ch;

    switch (firstchar) {
    case '"':
        do {
            ch = it->next();
            if (!try_push_char(ch)) break;
            if (ch == '\\')
                if (!try_push_char(it->next())) break;
        } while (ch == '"');
        break;
    case '`':
        do {
            ch = it->next();
            if (!try_push_char(ch)) break;
        } while (ch != '`');
        break;
    default:
        while ((ch = it->peek()), (!gomod_isspace(ch) && ch != '\n')) {
            it->next();
            if (!try_push_char(ch)) break;
        }
        break;
    }

    salloc.done();

    ccstr keywords[] = { "module", "go" ,"require", "replace", "exclude" };
    Gomod_Tok_Type types[] = { GOMOD_TOK_MODULE, GOMOD_TOK_GO, GOMOD_TOK_REQUIRE, GOMOD_TOK_REPLACE, GOMOD_TOK_EXCLUDE };

    for (u32 i = 0; i < _countof(keywords); i++) {
        if (streq(tok.val, keywords[i])) {
            salloc.revert();
            tok.val = NULL;
            tok.val_truncated = false;
            tok.type = types[i];
            return;
        }
    }
}

ccstr index_entry_type_str(Index_Entry_Type type) {
    switch (type) {
        define_str_case(IET_INVALID);
        define_str_case(IET_CONST);
        define_str_case(IET_VAR);
        define_str_case(IET_TYPE);
        define_str_case(IET_FUNC);
    }
    return NULL;
}

ccstr get_receiver_typename_from_func_decl(Ast *func_decl) {
    auto recv = func_decl->func_decl.recv;

    if (recv == NULL) return NULL;
    if (recv->type != AST_PARAMETERS) return NULL;

    auto fields = recv->parameters.fields;
    if (fields == NULL) return NULL;
    if (fields->list.len != 1) return NULL;

    auto recvtype = fields->list[0]->field.type;
    if (recvtype == NULL) return NULL;
    recvtype = unpointer(recvtype);

    if (recvtype->type != AST_ID) return NULL;
    return recvtype->id.lit;
}

Index_Stream *open_and_validate_imports_db(ccstr path) {
    if (check_path(path) != CPR_FILE) return NULL;

    Frame frame;

    auto f = alloc_object(Index_Stream);
    if (f->open(path, FILE_MODE_READ, FILE_OPEN_EXISTING) != FILE_RESULT_SUCCESS) {
        frame.restore();
        return NULL;
    }

    if (!f->read_file_header(INDEX_FILE_IMPORTS)) {
        frame.restore();
        f->cleanup();
        return NULL;
    }

    return f;
}

bool are_sets_equal(String_Set *a, String_Set *b) {
    auto a_items = a->items();
    For (*a_items)
        if (!b->has(it))
            return false;

    auto b_items = b->items();
    For (*b_items)
        if (!a->has(it))
            return false;

    return true;
}

void get_package_imports(ccstr path, List<ccstr> *out, String_Set *seen) {
    list_directory(path, [&](Dir_Entry* ent) {
        auto new_path = path_join(path, ent->name);

        if (ent->type == DIRENT_DIR) {
            if (!streq(ent->name, "vendor"))
                get_package_imports(new_path, out, seen);
            return;
        }

        if (!str_ends_with(ent->name, ".go")) return;
        if (str_ends_with(ent->name, "_test.go")) return;

        with_parser_at_location(new_path, [&](Parser *p) {
            p->parse_package_decl();
            while (p->tok.type == TOK_IMPORT) {
                auto decl = p->parse_decl(lookup_decl_start, true);
                if (decl == NULL) continue;

                For (decl->decl.specs->list) {
                    auto import_path = it->import_spec.path->basic_lit.val.string_val;
                    if (seen->has(import_path)) continue;

                    import_path = our_strcpy(import_path);
                    out->append(import_path);
                    seen->add(import_path);
                }
            }
        });
    });
}

List<ccstr> *get_package_imports(ccstr path) {
    auto ret = alloc_list<ccstr>();
    String_Set set;
    set.init();
    defer { set.cleanup() ;};

    get_package_imports(path, ret, &set);
    return ret;
}

/*
INDEX FOLDER:
    - imports
    - folders/mapped/one/to/one/with/libraries
        - index
        - data
        - hash
*/

List<ccstr> *read_imports_from_index(ccstr imports_path) {
    auto f = open_and_validate_imports_db(imports_path);
    if (f == NULL) return NULL;
    defer { f->cleanup(); };

    auto num_imports = f->read4();
    auto ret = alloc_list<ccstr>();
    for (u32 i = 0; i < num_imports; i++)
        ret->append(f->readstr());
    return ret;
}

bool Go_Index::ensure_imports_list_correct(Eil_Result *res) {
    Frame frame;

    auto _handle_error = [&](ccstr message) {
        handle_error(message);
        frame.restore();
        return false;
    };

    auto imports_path = get_index_path("imports");
    auto old_imports = read_imports_from_index(imports_path); // can be NULL, just means first time running
    auto new_imports = get_package_imports(world.wksp.path);

    if (new_imports == NULL)
        return _handle_error("Unable to get workspace imports.");

    {
        // write out new imports
        Index_Stream fw;
        if (fw.open(imports_path, FILE_MODE_WRITE, FILE_CREATE_NEW) != FILE_RESULT_SUCCESS)
            return _handle_error("Unable to open imports for writing.");
        defer { fw.cleanup(); };

        fw.write_file_header(INDEX_FILE_IMPORTS);
        fw.write4(new_imports->len);
        For (*new_imports) fw.writestr(it);
    }

    // compare old imports to new

    String_Set current_imports_set;
    current_imports_set.init();
    if (old_imports != NULL)
        For (*old_imports) current_imports_set.add(it);

    String_Set new_imports_set;
    new_imports_set.init();
    For (*new_imports) new_imports_set.add(it);

    auto removed_imports = alloc_list<ccstr>();
    if (old_imports != NULL)
        For (*old_imports)
            if (!new_imports_set.has(it))
                removed_imports->append(it);

    auto added_imports = alloc_list<ccstr>();
    For (*new_imports)
        if (!current_imports_set.has(it))
            added_imports->append(it);

    res->old_imports = old_imports;
    res->new_imports = new_imports;
    res->added_imports = added_imports;
    res->removed_imports = removed_imports;
    return true;
}

void Go_Index::handle_error(ccstr err) {
    // TODO: What's our error handling strategy?
    error("%s", err);
}

u64 Go_Index::hash_package(ccstr import_path, Resolved_Import **pres) {
    u64 ret = 0;
    auto add_hash = [&](void *p, s32 n) {
        ret ^= MeowU64From(MeowHash(MeowDefaultSeed, n, p), 0);
    };

    auto res = resolve_import(import_path);
    if (res == NULL) return 0;
    add_hash((void*)res->path, strlen(res->path));

    bool error = false;
    list_directory(res->path, [&](Dir_Entry* ent) {
        if (ent->type == DIRENT_DIR) return;
        if (!str_ends_with(ent->name, ".go")) return;
        if (str_ends_with(ent->name, "_test.go")) return;

        auto f = read_entire_file(path_join(res->path, ent->name));
        if (f == NULL) {
            error = true;
            return;
        }
        defer { free_entire_file(f); };

        add_hash(f->data, f->len);
        add_hash(ent->name, strlen(ent->name));
    });

    if (error) return 0;

    *pres = res;
    return ret;
}

u64 read_index_hash(ccstr import_path) {
    auto hash_file = get_index_path(import_path, "hash");
    Index_Stream f;

    if (f.open(hash_file, FILE_MODE_READ, FILE_OPEN_EXISTING) != FILE_RESULT_SUCCESS)
        return 0;
    defer { f.cleanup(); };

    if (!f.read_file_header(INDEX_FILE_HASH)) return 0;

    u64 ret = 0;
    f.readn(&ret, sizeof(ret));
    return f.ok ? ret : 0;
}

bool Go_Index::ensure_package_correct(ccstr import_path) {
    Resolved_Import *ri = NULL;
    auto hash = hash_package(import_path, &ri);
    if (hash == 0)
        return handle_error(our_sprintf("Unable to hash package %s", import_path)), false;

    print("Checking import %s, hash = %llx", import_path, hash);

    auto current_hash = read_index_hash(import_path);
    if (current_hash == hash)
        return print("Hash hasn't changed, no need to update."), true;

    print("Hash is different, updating index...");

    // update index
    {
        auto path = ri->path;
        print("crawling %s", path);

        // TODO: maybe we need to use a separate allocator for holding the names in Index_Writer.decls!
        Index_Writer w;
        if (w.init(import_path) != FILE_RESULT_SUCCESS) return false;
        defer { w.cleanup(); };

        w.write_hash(hash);
        w.write_headers(path, ri->location_type);

        list_directory(path, [&](Dir_Entry* ent) {
            if (ent->type == DIRENT_DIR) return;

            // for now, only grab non-test .go files
            if (!str_ends_with(ent->name, ".go")) return;
            if (str_ends_with(ent->name, "_test.go")) return;

            auto filepath = path_join(path, ent->name);
            auto ast = parse_file_into_ast(filepath);
            if (ast == NULL) return; // what to do here? this is an error

            list_decls_in_source(make_file_ast(ast, filepath, import_path), LISTDECLS_DECLS, [&](Named_Decl* decl) {
                for (auto&& it : *decl->items) {
                    auto name = it.name->id.lit;
                    if (it.spec->type == AST_FUNC_DECL) {
                        auto receiver_name = get_receiver_typename_from_func_decl(it.spec);
                        if (receiver_name != NULL)
                            name = our_sprintf("%s.%s", receiver_name, name);
                    }
                    w.write_decl(ent->name, name, it.spec);
                }
            });
        });

        w.finish_writing();
    }
}

bool Go_Index::ensure_entire_index_correct() {
    ensure_directory_exists(get_index_path());

    Eil_Result res;
    if (!ensure_imports_list_correct(&res)) return false;
    For (*res.new_imports) ensure_package_correct(it);
}

#define EVENT_DEBOUNCE_DELAY_MS 3000 // wait 3 seconds (arbitrary)

void Go_Index::run_background_thread() {
    SCOPED_MEM(&background_mem);

    {
        // when we start up, first ensure the entire index is correct
        SCOPED_FRAME();
        ensure_entire_index_correct();
    }

    auto pop_index_event = [&](Index_Event *out) -> bool {
        SCOPED_LOCK(&index_events_lock);
        if (index_events.len == 0) return false;

        auto current_time = current_time_in_nanoseconds();
        for (u32 i = 0; i < index_events.len; i++) {
            auto event = &index_events[i];
            if ((current_time - event->time) / 1000000.f > EVENT_DEBOUNCE_DELAY_MS) {
                memcpy(out, event, sizeof(Index_Event));
                index_events.remove(i);
                return true;
            }
        }
        return false;
    };

    for (Index_Event event = {0}; true; sleep_milliseconds(100)) {
        while (pop_index_event(&event)) {
            switch (event.type) {
            case INDEX_EVENT_FETCH_IMPORTS:
                {
                    Eil_Result res;
                    if (!ensure_imports_list_correct(&res)) break;
                    For (*res.added_imports) ensure_package_correct(it);
                    background_mem.reset();
                    break;
                }
                break;
            case INDEX_EVENT_REINDEX_PACKAGE:
                ensure_package_correct(event.import_path);
                background_mem.reset();
                break;
            }
        }
    }
}

bool Go_Index::delete_index() {
    SCOPED_FRAME();
    return delete_rm_rf(get_index_path());
}

ccstr remove_ats_from_path(ccstr s) {
    auto path = make_path(s);

    // Fuck this shit, we're just going to mutate path directly.  Yes,
    // `parts` is a list of const char*s,  but we only do that because
    // C is stupid and if you pass strings around as char* you end up
    // having to cast them to const everywhere.
    //
    // WE DO NOT EVER use const to indicate that memory is read-only.
    // In our programs, we do not have the concept of read-only memory.
    // Why would I ever want to BAN MYSELF from writing to MY OWN
    // MEMORY?

    For (*path->parts) {
        // in every part, remove "@whatever"
        auto atpos = strchr((char*)it, '@');
        if (atpos != NULL) *atpos = '\0';
    }

    return path->str();
}

ccstr Go_Index::file_to_import_path(ccstr filepath) {
    return directory_to_import_path(our_dirname(filepath));
}

ccstr Go_Index::directory_to_import_path(ccstr path_str) {
    // resolution order: vendor, workspace, gopath/pkg/mod, gopath, goroot

    // TODO: SCOPED_FRAME()?

    auto path = make_path(path_str);

    enum Path_Type {
        PATH_WHO_CARES,
        PATH_PKGMOD,
        PATH_WKSP,
    };

    auto check = [&](ccstr base_path_str, Path_Type path_type) -> ccstr {
        auto base_path = make_path(base_path_str);
        if (!base_path->contains(path)) return NULL;

        auto ret = get_path_relative_to(path_str, base_path_str);
        if (path_type == PATH_WKSP)
            return path_join(get_workspace_import_path(), ret);
        if (path_type == PATH_PKGMOD)
            return remove_ats_from_path(ret);
        return ret;
    };

    ccstr paths[] = {
        path_join(world.wksp.path, "vendor"),
        world.wksp.path,
        path_join(GOPATH, "pkg/mod"),
        path_join(GOPATH, "src"),
        path_join(GOROOT, "src"),
    };

    Path_Type path_types[] = { PATH_WHO_CARES, PATH_WKSP, PATH_PKGMOD, PATH_WHO_CARES, PATH_WHO_CARES };

    for (u32 i = 0; i < _countof(paths); i++) {
        auto ret = check(paths[i], path_types[i]);
        if (ret != NULL) return ret;
    }
    return NULL;
}

void Go_Index::handle_fs_event(Go_Index_Watcher *w, Fs_Event *event) {
    // only one thread calls this at a time
    SCOPED_LOCK(&fs_event_lock);

    SCOPED_MEM(&watcher_mem);
    defer { watcher_mem.reset(); };

    auto watch_path = w->watch.path;

    auto _directory_to_import_path = [&](ccstr path) -> ccstr {
        switch (w->type) {
        case WATCH_WKSP:
            {
                auto vendor_path = path_join(watch_path, "vendor");
                if (path_contains_in_subtree(vendor_path, path))
                    return get_path_relative_to(path, vendor_path);
                return path_join(
                    get_workspace_import_path(),
                    get_path_relative_to(path, world.wksp.path)
                );
            }
            break;
        case WATCH_PKGMOD:
            return remove_ats_from_path(get_path_relative_to(path, watch_path));
        case WATCH_GOPATH:
        case WATCH_GOROOT:
            return get_path_relative_to(path, watch_path);
        }
        return NULL;
    };

    auto queue_event = [&](Index_Event_Type type, ccstr import_path) {
        SCOPED_LOCK(&index_events_lock);

        if (index_events.len > MAX_INDEX_EVENTS) return;

        auto pred = [&](Index_Event *event) -> bool {
            if (event->type != type) return false;

            if (type == INDEX_EVENT_REINDEX_PACKAGE)
                if (!streqi(event->import_path, import_path))
                    return false;

            return true;
        };

        auto idx = index_events.find(pred);
        if (idx != -1) {
            index_events[idx].time = current_time_in_nanoseconds();
            return;
        }

        auto ev = index_events.append();
        ev->time = current_time_in_nanoseconds();
        ev->type = type;

        if (import_path != NULL)
            strcpy_safe(ev->import_path, _countof(ev->import_path), import_path);
    };

    auto queue_for_rescan = [&](ccstr directory) {
        auto import_path = _directory_to_import_path(directory);
        if (import_path == NULL) return;
        queue_event(INDEX_EVENT_REINDEX_PACKAGE, import_path);
    };

    auto path = path_join(watch_path, event->filepath);

    if (w->type == WATCH_WKSP)
        if (str_starts_with(path, path_join(watch_path, ".ide")))
            return;

    auto is_git_folder = [&](ccstr path, bool isdir) -> bool {
        SCOPED_FRAME();
        auto pathlist = make_path(path);
        return pathlist->parts->find([&](ccstr *it) { return streqi(*it, ".git"); }) != -1;
    };

    auto res = check_path(path);
    switch (res) {
    case CPR_DIRECTORY:
        if (is_git_folder(path, true)) break;

        switch (event->type) {
        case FSEVENT_CHANGE:
            break; // what does change even mean here?
        case FSEVENT_DELETE:
        case FSEVENT_CREATE:
            queue_for_rescan(path);
            break;
        case FSEVENT_RENAME:
            {
                auto old_path = path_join(watch_path, event->old_filepath);
                if (is_git_folder(old_path, true)) break;
                queue_for_rescan(path);
                queue_for_rescan(old_path);
            }
            break;
        }
    case CPR_FILE:
        if (is_git_folder(path, false)) break;
        if (!str_ends_with(path, ".go")) break;
        if (str_ends_with(path, "_test.go")) break;

        if (w->type == WATCH_WKSP)
            if (!str_starts_with(path, path_join(watch_path, "vendor")))
                queue_event(INDEX_EVENT_FETCH_IMPORTS, NULL);

        // some .go file was added/deleted/changed/renamed
        // queue its directory for a re-scan
        queue_for_rescan(our_dirname(path));
    }
}

bool Go_Index::init() {
    ptr0(this);

    general_mem.init();
    background_mem.init();
    watcher_mem.init();
    main_thread_mem.init();

    SCOPED_MEM(&general_mem);

    index_events.init(LIST_POOL, 32);
    index_events_lock.init();
    fs_event_lock.init();

    if (!wksp_watcher.watch.init(world.wksp.path)) return false;
    if (!gopath_watcher.watch.init(path_join(GOPATH, "src"))) return false;
    if (!pkgmod_watcher.watch.init(path_join(GOPATH, "pkg/mod"))) return false;
    if (!goroot_watcher.watch.init(path_join(GOROOT, "src"))) return false;

    buildparser_proc.init();
    buildparser_proc.use_stdin = true;

    {
        SCOPED_FRAME();
        GetModuleFileNameA(NULL, current_exe_path, _countof(current_exe_path));
        auto path = our_dirname(current_exe_path);
        strcpy_safe(current_exe_path, _countof(current_exe_path), path);
    }

    buildparser_proc.dir = current_exe_path;
    buildparser_proc.run("go run buildparser.go");
}

void run_watcher_stub(void *param) {
    auto w = (Go_Index_Watcher*)param;
    Fs_Event event = {0};

    while (true) {
        if (w->watch.next_event(&event))
            w->_this->handle_fs_event(w, &event);
        else
            error("unable to get next event"); // should we sleep a bit?
    }
}

void run_background_thread_stub(void *p) {
    ((Go_Index*)p)->run_background_thread();
};

bool Go_Index::run_threads() {
    auto _this = this;

    auto init_watcher = [&](Go_Index_Watcher *w, Go_Watch_Type type) -> bool {
        w->type = type;
        w->_this = _this;
        w->thread = create_thread(run_watcher_stub, w);
        return w->thread != NULL;
    };

    if (!init_watcher(&wksp_watcher, WATCH_WKSP)) return false;
    if (!init_watcher(&gopath_watcher, WATCH_GOPATH)) return false;
    if (!init_watcher(&pkgmod_watcher, WATCH_PKGMOD)) return false;
    if (!init_watcher(&goroot_watcher, WATCH_GOROOT)) return false;

    bg_thread = create_thread(run_background_thread_stub, this);
    if (bg_thread == NULL) return false;

    return true;
}

void Go_Index::cleanup() {
    auto kill_and_close = [&](Thread_Handle *phandle) {
        if (*phandle != NULL) {
            kill_thread(*phandle);
            close_thread_handle(*phandle);
            *phandle = NULL;
        }
    };

    kill_and_close(&bg_thread);
    kill_and_close(&wksp_watcher.thread);
    kill_and_close(&gopath_watcher.thread);
    kill_and_close(&pkgmod_watcher.thread);
    kill_and_close(&goroot_watcher.thread);

    wksp_watcher.watch.cleanup();
    gopath_watcher.watch.cleanup();
    pkgmod_watcher.watch.cleanup();
    goroot_watcher.watch.cleanup();

    index_events_lock.cleanup();
    fs_event_lock.cleanup();

    background_mem.cleanup();
    watcher_mem.cleanup();
    main_thread_mem.cleanup();
    general_mem.cleanup();
}

// -----

s32 num_index_stream_opens = 0;
s32 num_index_stream_closes = 0;

File_Result Index_Stream::open(ccstr _path, u32 access, File_Open_Mode open_mode) {
    ptr0(this);

    path = _path;
    offset = 0;
    ok = true;

    // TODO: CREATE_NEW? what's the policy around if index already exists?
    auto ret = f.init(path, access, open_mode);
    if (ret == FILE_RESULT_SUCCESS)
        num_index_stream_opens++;
    return ret;
}

void Index_Stream::cleanup() {
    num_index_stream_closes++;
    f.cleanup();
}

bool Index_Stream::seek(u32 _offset) {
    offset = f.seek(_offset);
    return true; // ???
}

bool Index_Stream::writen(void* buf, int n) {
    s32 written = 0;
    if (f.write((char*)buf, n, &written)) {
        offset += written;
        return true;
    }
    return false;
}

bool Index_Stream::write1(i8 x) { return writen(&x, 1); }
bool Index_Stream::write2(i16 x) { return writen(&x, 2); }
bool Index_Stream::write4(i32 x) { return writen(&x, 4); }
bool Index_Stream::write8(i64 x) { return writen(&x, 8); }

bool Index_Stream::writestr(ccstr s) {
    auto len = strlen(s);
    if (!write2(len)) return false;
    if (!writen((void*)s, len)) return false;
    return true;
}

bool Index_Stream::write_file_header(Index_File_Type type) {
    if (!write4(INDEX_MAGIC_NUMBER)) return false;
    if (!write1(type)) return false;

    return true;
}

void Index_Stream::readn(void* buf, s32 n) {
    ok = f.read((char*)buf, n);
    if (ok) offset += n;
}

char Index_Stream::read1() {
    char ch = 0;
    readn(&ch, 1);
    return ch;
}

i16 Index_Stream::read2() {
    char buf[2];
    readn(buf, 2);
    return ok ? *(i16*)buf : 0;
}

i32 Index_Stream::read4() {
    char buf[4];
    readn(buf, 4);
    return ok ? *(i32*)buf : 0;
}

ccstr Index_Stream::readstr() {
    Frame frame;

    auto size = (u32)read2();
    if (!ok) return NULL;

    auto s = alloc_array(char, size + 1);
    for (u32 i = 0; i < size; i++) {
        s[i] = read1();
        if (!ok) {
            frame.restore();
            return NULL;
        }
    }

    ok = true;
    return s;
}

bool Index_Stream::read_file_header(Index_File_Type wanted_type) {
    if (read4() == INDEX_MAGIC_NUMBER)
        if (read1() == wanted_type)
            return true;
    return false;
}

int Index_Writer::init(ccstr import_path) {
    ptr0(this);

    index_path = get_index_path(import_path);

    if (!ensure_directory_exists(index_path))
        return FILE_RESULT_FAILURE;

    auto res = findex.open(path_join(index_path, "index"), FILE_MODE_WRITE, FILE_CREATE_NEW);
    if (res != FILE_RESULT_SUCCESS) return res;

    res = fdata.open(path_join(index_path, "data"), FILE_MODE_WRITE, FILE_CREATE_NEW);
    if (res != FILE_RESULT_SUCCESS) return res;

    // initialize decls
    decls.init(LIST_MALLOC, 128);

    return FILE_RESULT_SUCCESS;
}

void Index_Writer::write_headers(ccstr resolved_path, Import_Location loctype) {
    findex.write_file_header(INDEX_FILE_INDEX);
    findex.writestr(resolved_path);
    findex.write1(loctype);

    fdata.write_file_header(INDEX_FILE_DATA);
}

void Index_Writer::cleanup() {
    findex.cleanup();
    fdata.cleanup();
    decls.cleanup();
}

#define IW_ASSERT(x) if (!(x)) return false

bool Index_Writer::write_hash(u64 hash) {
    Index_Stream f;
    if (f.open(path_join(index_path, "hash"), FILE_MODE_WRITE, FILE_CREATE_NEW) != FILE_RESULT_SUCCESS)
        return false;
    defer { f.cleanup(); };

    if (!f.write_file_header(INDEX_FILE_HASH)) return false;
    if (!f.writen(&hash, sizeof(hash))) return false;

    return true;
}

bool Index_Writer::finish_writing() {
    IW_ASSERT(findex.write4(decls.len));

    decls.sort([](Decl_To_Write *a, Decl_To_Write *b) -> int {
        return strcmp(a->name, b->name);
    });

    For (decls) {
        IW_ASSERT(findex.write4(it.offset));
    }
}

bool Index_Writer::write_decl(ccstr filename, ccstr name, Ast* spec) {
    auto get_entry_type = [&]() {
        switch (spec->type) {
        case AST_TYPE_SPEC: return IET_TYPE;
        case AST_FUNC_DECL: return IET_FUNC;
        case AST_VALUE_SPEC:
            switch (spec->value_spec.spec_type) {
            case TOK_CONST: return IET_CONST;
            case TOK_VAR: return IET_VAR;
            }
            break;
        }
        return IET_INVALID;
    };

    auto type = get_entry_type();
    if (type == IET_INVALID) return false;

    auto has_dot = [&](ccstr s) {
        for (u32 i = 0; s[i] != '\0'; i++)
            if (s[i] == '.')
                return true;
        return false;
    };

    if (true || has_dot(name)) {
        print(
            "        write: %s, %s, %s, %s",
            format_pos(spec->start),
            index_entry_type_str(type),
            name,
            our_basename(filename)
        );
    }

    auto decl_to_write = decls.append();
    decl_to_write->offset = fdata.offset;
    decl_to_write->name = our_strcpy(name);

    Index_Entry_Hdr hdr = {
        .type = type,
        .pos = spec->start,
    };

    IW_ASSERT(fdata.writen(&hdr, sizeof(hdr)));
    IW_ASSERT(fdata.writestr(our_basename(filename)));
    IW_ASSERT(fdata.writestr(name));

    return true;
}

#undef IW_ASSERT

bool Index_Reader::init(ccstr _index_path) {
    index_path = _index_path;
    indexfile_path = path_join(index_path, "index");
    datafile_path = path_join(index_path, "data");

    Index_Stream findex;
    if (findex.open(indexfile_path, FILE_MODE_READ, FILE_OPEN_EXISTING) != FILE_RESULT_SUCCESS) {
        print("couldn't open index");
        return false;
    }
    defer { findex.cleanup(); };

    if (fdata.open(datafile_path, FILE_MODE_READ, FILE_OPEN_EXISTING) != FILE_RESULT_SUCCESS) {
        print("couldn't open data");
        return false;
    }

    assert(findex.read_file_header(INDEX_FILE_INDEX));
    assert(fdata.read_file_header(INDEX_FILE_DATA));

    meta.package_path = findex.readstr();
    meta.loctype = (Import_Location)findex.read1();

    auto decl_count = findex.read4();
    decl_offsets = alloc_list<i32>(decl_count);
    for (u32 i = 0; i < decl_count; i++)
        decl_offsets->append(findex.read4());
}

void Index_Reader::cleanup() {
    fdata.cleanup();
}

List<Index_Entry> *Index_Reader::list_decls() {
    auto ret = alloc_list<Index_Entry>();
    For (*decl_offsets) {
        auto out = ret->append();

        fdata.seek(it);
        fdata.readn(&out->hdr, sizeof(out->hdr));
        out->filename = fdata.readstr();     // TODO: do we need to copy this?
        out->name = fdata.readstr();
    }
    return ret;
}

bool Index_Reader::find_decl(ccstr decl_name, Index_Entry *out) {
    auto get_name = [&](i32 offset) -> ccstr {
        if (offset == -1) return decl_name;
        fdata.seek(offset);

        Index_Entry_Hdr hdr;
        fdata.readn(&hdr, sizeof(hdr));
        fdata.readstr(); // filename
        return fdata.readstr(); // decl name
    };

    auto cmpfunc = [&](i32 *a, i32 *b) {
        return strcmp(get_name(*a), get_name(*b));
    };

    i32 key = -1;
    auto poffset = decl_offsets->bfind(&key, cmpfunc);
    if (poffset == NULL) return false;

    fdata.seek(*poffset);
    fdata.readn(&out->hdr, sizeof(out->hdr));
    out->filename = fdata.readstr();     // TODO: do we need to copy this?
    out->name = fdata.readstr();
    return true;
}

void with_parser_at_location(ccstr filepath, cur2 location, parser_cb cb) {
    auto iter = file_loader(filepath);
    if (iter == NULL) return;
    defer { iter->cleanup(); };
    iter->set_pos(location);

    Parser p;
    p.init(iter, filepath);
    defer { p.cleanup(); };

    cb(&p);
}

void with_parser_at_location(ccstr filepath, parser_cb cb) {
    with_parser_at_location(filepath, new_cur2(0, 0), cb);
}
