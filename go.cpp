#include "go.hpp"
#include "utils.hpp"
#include "world.hpp"
#include "mem.hpp"
#include "os.hpp"
#include "set.hpp"
#include "editor.hpp"
#include "meow_hash.hpp"
#include "uthash.h"

#include <type_traits>

// TODO: dynamically determine this
static const char GOROOT[] = "c:\\go";
static const char GOPATH[] = "c:\\users\\brandon\\go";

// -----

s32 num_index_stream_opens = 0;
s32 num_index_stream_closes = 0;

// should we move this logic to os.cpp?
ccstr normalize_resolved_path(ccstr path) {
    auto ret = (cstr)our_strcpy(path);

#if OS_WIN
    auto len = strlen(ret);
    for (u32 i = 0; i < len; i++)
        ret[i] = is_sep(ret[i]) ? PATH_SEP : tolower(ret[i]);
    while (len > 0 && is_sep(ret[len-1])) len--;
    ret[len] = '\0';
#elif OS_MAC
    // TODO
#elif OS_LINUX
    // TODO
#endif

    return (ccstr)ret;
}

File_Result Index_Stream::open(ccstr _path, u32 access, File_Open_Mode open_mode) {
    ptr0(this);

    path = _path;
    offset = 0;
    ok = true;

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

bool Index_Stream::writen(void *buf, int n) {
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

bool Index_Stream::writestr(ccstr s) {
    if (s == NULL) return write2(0);
    auto len = strlen(s);
    if (!write2(len)) return false;
    if (!writen((void*)s, len)) return false;
    return true;
}

void Index_Stream::readn(void *buf, s32 n) {
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
    if (size == 0) return NULL;

    auto s = alloc_array(char, size + 1);
    readn(s, size);
    if (!ok) {
        frame.restore();
        return NULL;
    }

    s[size] = '\0';
    ok = true;
    return s;
}

// -----

ccstr format_pos(cur2 pos) {
    if (pos.y == -1)
        return our_sprintf("%d", pos);
    return our_sprintf("%d:%d", pos.y, pos.x);
}

struct Parser_Input {
    Go_Indexer *indexer;
    Parser_It *it;
    char buf[128];
};

const char* read_from_parser_input(void *p, uint32_t off, TSPoint pos, uint32_t *read) {
    Parser_Input *input = (Parser_Input*)p;
    Parser_It *it = input->it;
    auto buf = input->buf;
    auto bufsize = _countof(input->buf);
    u32 n = 0;

    if (it->type == IT_MMAP)
        it->set_pos(new_cur2((i32)off, (i32)-1));
    else if (it->type == IT_BUFFER)
        it->set_pos(tspoint_to_cur(pos));

    while (!it->eof()) {
        auto uch = it->next();
        if (uch == 0) break;

        auto size = uchar_size(uch);
        if (n + size + 1 > bufsize) break;

        uchar_to_cstr(uch, &buf[n], &size);
        n += size;
    }

    *read = n;
    buf[n] = '\0';
    return buf;
}

void Go_Indexer::background_thread() {
    SCOPED_MEM(&mem);
    use_pool_for_tree_sitter = true;

    Index_Stream s;

#if 0
    crawl_index();
    print("finished crawling index");
    print("writing...");

    if (s.open("db", FILE_MODE_WRITE, FILE_CREATE_NEW) != FILE_RESULT_SUCCESS) return;
    write_object<Go_Index>(&index, &s);
    s.cleanup();
    print("done writing");

    final_mem.cleanup();
    final_mem.init();
#endif

    print("reading...");
    if (s.open("db", FILE_MODE_READ, FILE_OPEN_EXISTING) != FILE_RESULT_SUCCESS) return;
    defer { s.cleanup(); };
    {
        SCOPED_MEM(&final_mem);
        memcpy(&index, read_object<Go_Index>(&s), sizeof(Go_Index));
    }
    print("successfully read index from disk, final_mem.size = %d", final_mem.mem_allocated);

    while (true) continue; // just stay running for now
}

bool Go_Indexer::start_background_thread() {
    auto fn = [](void* param) {
        auto indexer = (Go_Indexer*)param;
        indexer->background_thread();
    };
    return (bgthread = create_thread(fn, this)) != NULL;
}

Parsed_File *Go_Indexer::parse_file(ccstr filepath) {
    Parsed_File *ret = NULL;

    for (auto&& pane : world.wksp.panes) {
        for (auto&& editor : pane.editors) {
            if (streq(editor.filepath, filepath)) {
                auto it = alloc_object(Parser_It);
                it->init(&editor.buf);

                ret = alloc_object(Parsed_File);
                ret->tree_belongs_to_editor = true;
                ret->it = it;
                ret->tree = editor.tree;
                goto done;
            }
        }
    }
done:

    if (ret == NULL) {
        auto ef = read_entire_file(filepath);
        if (ef == NULL) return NULL;

        auto it = alloc_object(Parser_It);
        it->init(ef);

        Parser_Input pinput;
        pinput.indexer = this;
        pinput.it = it;

        TSInput input;
        input.payload = &pinput;
        input.encoding = TSInputEncodingUTF8;
        input.read = read_from_parser_input;

        auto tree = ts_parser_parse(new_ts_parser(), NULL, input);
        if (tree == NULL) return NULL;

        ret = alloc_object(Parsed_File);
        ret->tree_belongs_to_editor = false;
        ret->it = pinput.it;
        ret->tree = tree;
    }

    ret->root = new_ast_node(ts_tree_root_node(ret->tree));
    current_parsed_files.append(ret);
    return ret;
}

void Go_Indexer::free_parsed_file(Parsed_File *file) {
    our_assert(
        current_parsed_files.len > 0 && (*current_parsed_files.last() == file),
        "parsed files freed out of order"
    );
    current_parsed_files.len--;

    if (file->it != NULL) file->it->cleanup();

    // do we even need to free tree, if we're using our custom pool memory?
    /*
    if (file->tree != NULL)
        if (!file->tree_belongs_to_editor)
            ts_tree_delete(file->tree);
    */
}

// returns -1 if pos before ast, 0 if inside, 1 if after
i32 cmp_pos_to_node(cur2 pos, Ast_Node *node) {
    if (pos.y == -1) {
        if (pos.x < node->start_byte) return -1;
        if (pos.x >= node->end_byte) return 1;
        return 0;
    }

    if (pos < node->start) return -1;
    if (pos >= node->end) return 1;
    return 0;
}

void Go_Indexer::walk_ast_node(Ast_Node *node, bool abstract_only, Walk_TS_Callback cb) {
    auto cursor = ts_tree_cursor_new(node->node);
    defer { ts_tree_cursor_delete(&cursor); };

    walk_ts_cursor(&cursor, abstract_only, [&](Ast_Node *node, Ts_Field_Type type, int depth) -> Walk_Action {
        node->indexer = this;
        return cb(node, type, depth);
    });
}

void Go_Indexer::find_nodes_containing_pos(Ast_Node *root, cur2 pos, bool abstract_only, fn<Walk_Action(Ast_Node *it)> callback) {
    walk_ast_node(root, abstract_only, [&](Ast_Node *node, Ts_Field_Type, int) -> Walk_Action {
        int res = cmp_pos_to_node(pos, node);
        if (res < 0) return WALK_ABORT;
        if (res > 0) return WALK_SKIP_CHILDREN;
        return callback(node);
    });
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

// -----

Ast_Node *Ast_Node::dup(TSNode new_node) {
    return indexer->new_ast_node(new_node);
}

ccstr Ast_Node::string() {
    auto last = indexer->current_parsed_files.last();
    our_assert(last != NULL, "no current parsed file");

    auto pf = *last;
    auto it = pf->it;

    // auto start_byte = ts_node_start_byte(node);
    // auto end_byte = ts_node_end_byte(node);

    if (it->type == IT_MMAP)
        it->set_pos(new_cur2((i32)start_byte, (i32)-1));
    else if (it->type == IT_BUFFER)
        it->set_pos(start);

    auto len = end_byte - start_byte;
    auto ret = alloc_array(char, len + 1);
    for (u32 i = 0; i < len; i++)
        ret[i] = it->next();
    ret[len] = '\0';
    return ret;
}

Gotype *Go_Indexer::new_gotype(Gotype_Type type) {
    // maybe optimize size of the object we allocate, like we did with Ast
    auto ret = alloc_object(Gotype);
    ret->type = type;
    return ret;
}

Go_Package *Go_Indexer::find_package_in_index(ccstr import_path, ccstr resolved_path) {
    return index.packages->find([&](Go_Package *it) -> bool {
        return (
            streq(it->import_path, import_path)
            && streq(it->resolved_path, resolved_path)
        );
    });
}

ccstr Go_Indexer::find_import_path_referred_to_by_id(ccstr id, Go_Ctx *ctx, ccstr *resolved_path) {
    auto current_pkg = find_package_in_index(ctx->import_path, ctx->resolved_path);
    if (current_pkg == NULL) return NULL;

    auto imp = current_pkg->individual_imports->find([&](Go_Single_Import *it) -> bool {
        return (
            it->package_name != NULL
            && streq(it->package_name, id)
            && streq(it->file, ctx->filename)
        );
    });
    if (imp == NULL) return NULL;

    auto dep = current_pkg->dependencies->find([&](Go_Dependency *it) -> bool {
        return streq(it->import_path, imp->import_path);
    });
    if (dep == NULL) return NULL;

    *resolved_path = dep->resolved_path;
    return imp->import_path;
}

ccstr parse_go_string(ccstr s) {
    // just strips the quotes for now, handle backslashes and shit when we need

    auto len = strlen(s);
    if ((s[0] == '"' && s[len-1] == '"') || (s[0] == '`' && s[len-1] == '`')) {
        auto ret = (cstr)our_strcpy(s);
        ret[len-1] = '\0';
        return &ret[1];
    }
    return NULL;
}

void Go_Indexer::crawl_index() {
    Pool package_scratch_mem;   // temp pool, torn down after processing each package
    Pool crawl_mem;             // pool holding info needed to orchestrate crawl_index
    Pool intermediate_mem;      // temp mem that holds package info

    package_scratch_mem.init("package_scratch_mem");
    crawl_mem.init("crawl_mem");
    intermediate_mem.init("intermediate_mem");

    defer { package_scratch_mem.cleanup(); };
    defer { crawl_mem.cleanup(); };
    defer { intermediate_mem.cleanup(); };

    SCOPED_MEM(&crawl_mem);

    {
        SCOPED_MEM(&intermediate_mem);
        // index.current_path = world.wksp.path;
        index.current_path = normalize_path_separator((cstr)our_strcpy("C:/Users/Brandon/compose-cli"));
        index.gomod = parse_gomod_file(path_join(index.current_path, "go.mod"));
        index.current_import_path = get_workspace_import_path();
        index.packages = alloc_list<Go_Package>();
    }

    fn<void(ccstr, ccstr)> process_initial_imports;
    fn<void(ccstr, ccstr)> process_import;

    struct Queue_Item {
        ccstr import_path;
        ccstr resolved_path;
    };

    List<Queue_Item> queue;
    queue.init();

    auto append_to_queue = [&](ccstr import_path, ccstr resolved_path) {
        SCOPED_MEM(&crawl_mem);
        auto item = queue.append();
        item->import_path = our_strcpy(import_path);
        item->resolved_path = normalize_resolved_path(resolved_path);
    };

    process_initial_imports = [&](ccstr import_path, ccstr resolved_path) {
        bool is_go_package = false;

        list_directory(resolved_path, [&](Dir_Entry *ent) {
            if (ent->type == DIRENT_FILE) {
                if (!is_go_package)
                    if (str_ends_with(ent->name, ".go"))
                        if (is_file_included_in_build(path_join(resolved_path, ent->name)))
                            is_go_package = true;
                return;
            }

            if (streq(ent->name, "vendor")) return;

            process_initial_imports(
                normalize_path_separator((cstr)path_join(import_path, ent->name), '/'),
                path_join(resolved_path, ent->name)
            );
        });

        if (is_go_package) append_to_queue(import_path, resolved_path);
    };

    process_initial_imports(index.current_import_path, index.current_path);

    auto get_package = [&](ccstr import_path, ccstr resolved_path) -> Go_Package * {
        auto pkg = find_package_in_index(import_path, resolved_path);
        if (pkg == NULL) {
            SCOPED_MEM(&intermediate_mem);

            pkg = index.packages->append();

            pkg->individual_imports = alloc_list<Go_Single_Import>();
            pkg->dependencies = alloc_list<Go_Dependency>();
            pkg->decls = alloc_list<Godecl>();
            pkg->files = alloc_list<Go_Package_File_Info>();

            pkg->import_path = our_strcpy(import_path);
            pkg->resolved_path = normalize_resolved_path(resolved_path);

            pkg->status = GPS_OUTDATED;
        }
        return pkg;
    };

    // get package status, but don't create a new package if one doesn't exist
    // (unlike get_package)
    auto get_package_status = [&](ccstr import_path, ccstr resolved_path) -> Go_Package_Status {
        auto pkg = find_package_in_index(import_path, normalize_resolved_path(resolved_path));
        return pkg == NULL ? GPS_OUTDATED : pkg->status;
    };

    while (queue.len > 0) {
        SCOPED_MEM(&package_scratch_mem);
        defer { package_scratch_mem.reset(); };

        auto item = queue.last();
        auto import_path = item->import_path;
        auto resolved_path = item->resolved_path;
        auto pkg = get_package(import_path, resolved_path);

        if (pkg->status == GPS_READY) {
            queue.len--;
            continue;
        }

        print("processing %s -> %s", import_path, resolved_path);

        pkg->status = GPS_UPDATING;

        defer {
            SCOPED_MEM(&intermediate_mem);
            memcpy(pkg, pkg->copy(), sizeof(Go_Package));
        };

        auto source_files = list_source_files(resolved_path, false);  // for now, don't include tests

        // find "neighbors" a la DFS
        bool added_new_deps = false;
        For (*source_files) {
            auto filename = it;

            auto pf = parse_file(path_join(resolved_path, filename));
            if (pf == NULL) continue;
            defer { free_parsed_file(pf); };

            Go_Ctx ctx;
            ctx.filename = filename;
            ctx.import_path = import_path;
            ctx.resolved_path = resolved_path;

            // ----

            // add import info
            FOR_NODE_CHILDREN (pf->root) {
                auto decl_node = it;
                if (decl_node->type != TS_IMPORT_DECLARATION) continue;

                auto speclist_node = decl_node->child();
                FOR_NODE_CHILDREN (speclist_node) {
                    Ast_Node *name_node = NULL;
                    Ast_Node *path_node = NULL;

                    if (it->type == TS_IMPORT_SPEC) {
                        path_node = it->field(TSF_PATH);
                        name_node = it->field(TSF_NAME);
                    } else if (it->type == TS_INTERPRETED_STRING_LITERAL) {
                        path_node = it;
                        name_node = NULL;
                    } else {
                        continue;
                    }

                    ccstr new_import_path = NULL;
                    Resolved_Import *ri = NULL;

                    new_import_path = parse_go_string(path_node->string());
                    ri = resolve_import_from_filesystem(new_import_path, &ctx);
                    if (ri == NULL) continue;

                    auto status = get_package_status(new_import_path, ri->path);
                    if (status == GPS_UPDATING) {
                        // TODO: notify user that cycle was detected
                    } else if (status == GPS_OUTDATED) {
                        added_new_deps = true;
                        append_to_queue(new_import_path, ri->path);
                    }

                    // save decl
                    auto decl = alloc_object(Godecl);
                    decl->file = ctx.filename;
                    decl->decl_start = decl_node->start;
                    import_spec_to_decl(it, decl);

                    // record dependency
                    auto match_dep = [&](Go_Dependency *it) {
                        return streq(it->import_path, new_import_path) && streq(it->resolved_path, ri->path);
                    };

                    auto dep = pkg->dependencies->find(match_dep);
                    if (dep == NULL) {
                        dep = pkg->dependencies->append();
                        dep->import_path = new_import_path;
                        dep->resolved_path = normalize_resolved_path(ri->path);
                        dep->package_name = our_strcpy(ri->package_name);
                    }

                    // record import
                    auto imp = pkg->individual_imports->append();
                    imp->decl = decl;
                    imp->file = filename;
                    imp->import_path = new_import_path;
                    imp->resolved_path = dep->resolved_path;

                    if (name_node == NULL || name_node->null) {
                        imp->package_name_type = GPN_IMPLICIT;
                        imp->package_name = ri->package_name;
                    } else if (name_node->type == TS_DOT) {
                        imp->package_name_type = GPN_DOT;
                    } else if (name_node->type == TS_BLANK_IDENTIFIER) {
                        imp->package_name_type = GPN_BLANK;
                    } else {
                        imp->package_name_type = GPN_EXPLICIT;
                        imp->package_name = name_node->string();
                    }
                }
            }
        }

        if (added_new_deps) continue;

        // no dependencies left, process the current item now
        // -----

        /*
         * as we create scope_ops, we call node_to_decls, which calls
         * infer_type which requires access to information about all the decls
         * it depends on.
         *
         * so first we should process all the decls, using whatever algorithm
         * we come up with to fix that same problem of needing dependency decls
         * already resolved
         *
         * then we create the scope ops, and i guess *as we're creating it* we
         * need maintain a lookup table of local decls, and somehow provide
         * that information to infer_type et al?
         *
         * fuck me lol
         */

        Go_Ctx ctx = {0};
        ctx.import_path = import_path;
        ctx.resolved_path = resolved_path;

        bool added_new_decl;
        do {
            added_new_decl = false;

            // parse the decls
            For (*source_files) {
                ctx.filename = it;

                auto pf = parse_file(path_join(resolved_path, ctx.filename));
                if (pf == NULL) continue;
                defer { free_parsed_file(pf); };

                List<Goresult> results;
                results.init();

                FOR_NODE_CHILDREN (pf->root) {
                    switch (it->type) {
                    case TS_PACKAGE_CLAUSE:
                        if (pkg->package_name == NULL)
                            pkg->package_name = it->child()->string();
                        break;
                    case TS_VAR_DECLARATION:
                    case TS_CONST_DECLARATION:
                    case TS_FUNCTION_DECLARATION:
                    case TS_METHOD_DECLARATION:
                    case TS_TYPE_DECLARATION:
                    case TS_SHORT_VAR_DECLARATION:
                        results.len = 0;
                        node_to_decls(it, &results, &ctx);
                        for (auto &&result : results) {
                            auto decl = result.decl;
                            auto existing_decl = pkg->decls->find([&](Godecl *it) -> bool {
                                return it->spec_start == decl->spec_start;
                            });
                            if (existing_decl == NULL) {
                                pkg->decls->append(decl);
                                added_new_decl = true;
                            }
                        }
                        break;
                    }
                }
            }
        } while (added_new_decl);

        For (*source_files) {
            ctx.filename = it;

            auto pf = parse_file(path_join(resolved_path, ctx.filename));
            if (pf == NULL) continue;
            defer { free_parsed_file(pf); };

            // ----

            auto finfo = pkg->files->append();
            finfo->filename = our_strcpy(ctx.filename);
            finfo->scope_ops = alloc_list<Go_Scope_Op>();

            List<Goresult> results;
            results.init();

            List<int> open_scopes;
            open_scopes.init();

            SCOPED_FRAME_WITH_MEM(&scoped_table_mem);

            Scoped_Table<Godecl*> table;
            {
                SCOPED_MEM(&scoped_table_mem);
                table.init();
            }
            defer { table.cleanup(); };

            auto old_local_decls = local_decls;
            local_decls = &table;
            defer { local_decls = old_local_decls; };

            auto add_scope_op = [&](Go_Scope_Op_Type type) -> Go_Scope_Op * {
                auto op = finfo->scope_ops->append();
                op->type = type;
                return op;
            };

            walk_ast_node(pf->root, true, [&](Ast_Node* node, Ts_Field_Type, int depth) -> Walk_Action {
                for (; open_scopes.len > 0 && depth <= *open_scopes.last(); open_scopes.len--) {
                    add_scope_op(GSOP_CLOSE_SCOPE)->pos = node->start;
                    table.pop_scope();
                }

                switch (node->type) {
                case TS_IF_STATEMENT:
                case TS_FOR_STATEMENT:
                case TS_TYPE_SWITCH_STATEMENT:
                case TS_EXPRESSION_SWITCH_STATEMENT:
                case TS_BLOCK:
                case TS_METHOD_DECLARATION:
                case TS_FUNCTION_DECLARATION:
                    open_scopes.append(depth);
                    add_scope_op(GSOP_OPEN_SCOPE)->pos = node->start;
                    table.push_scope();
                    break;

                case TS_PARAMETER_LIST:
                case TS_SHORT_VAR_DECLARATION:
                case TS_CONST_DECLARATION:
                case TS_VAR_DECLARATION:
                    {
                        results.len = 0;
                        node_to_decls(node, &results, &ctx);
                        For (results) {
                            auto op = add_scope_op(GSOP_DECL);
                            op->decl = it.decl;
                            op->pos = it.decl->decl_start;
                            table.set(it.decl->name, it.decl);
                        }
                    }
                    break;
                }

                return WALK_CONTINUE;
            });
        }

        pkg->status = GPS_READY;
        queue.len--;
    }

    {
        SCOPED_MEM(&final_mem);
        memcpy(&index, index.copy(), sizeof(Go_Index));
    }
    intermediate_mem.reset();
}

void Go_Indexer::handle_error(ccstr err) {
    // TODO: What's our error handling strategy?
    error("%s", err);
}

u64 Go_Indexer::hash_package(ccstr resolved_package_path) {
    u64 ret = 0;
    ret ^= meow_hash((void*)resolved_package_path, strlen(resolved_package_path));

    {
        SCOPED_FRAME();

        auto files = list_source_files(resolved_package_path, false);
        if (files == NULL) return 0;

        For (*files) {
            auto f = read_entire_file(path_join(resolved_package_path, it));
            if (f == NULL) return 0;
            defer { free_entire_file(f); };

            ret ^= meow_hash(f->data, f->len);
            ret ^= meow_hash((void*)it, strlen(it));
        }
    }

    return ret;
}

// i can't believe this but we *may* need to just do interop with go
bool Go_Indexer::is_file_included_in_build(ccstr path) {
    buildparser_proc.writestr(path);
    buildparser_proc.write1('\n');

    char ch = 0;
    if (!buildparser_proc.read1(&ch))
        panic("buildparser crashed, we're not going to be able to do anything");
    return (ch == 1);
}

List<ccstr>* Go_Indexer::list_source_files(ccstr dirpath, bool include_tests) {
    auto ret = alloc_list<ccstr>();

    auto save_gofiles = [&](Dir_Entry *ent) {
        if (ent->type == DIRENT_DIR) return;
        if (!str_ends_with(ent->name, ".go")) return;
        if (str_ends_with(ent->name, "_test.go") && !include_tests) return;

        {
            SCOPED_FRAME();
            if (!is_file_included_in_build(path_join(dirpath, ent->name))) return;
        }

        ret->append(our_strcpy(ent->name));
    };

    return list_directory(dirpath, save_gofiles) ? ret : NULL;
}

ccstr Go_Indexer::get_package_name_from_file(ccstr filepath) {
    auto pf = parse_file(filepath);
    if (pf == NULL) return NULL;
    defer { free_parsed_file(pf); };

    FOR_NODE_CHILDREN(pf->root) {
        if (it->type == TS_COMMENT) continue;
        if (it->type != TS_PACKAGE_CLAUSE) break;
        return it->child()->string();
    }

    return NULL;
}

ccstr Go_Indexer::get_package_name(ccstr path) {
    if (check_path(path) != CPR_DIRECTORY) return NULL;

    auto files = list_source_files(path, false);
    if (files == NULL) return NULL;

    For (*files) {
        auto filepath = path_join(path, it);
        auto pkgname = get_package_name_from_file(filepath);
        if (pkgname != NULL) return pkgname;
    }

    return NULL;
}

Resolved_Import *Go_Indexer::check_potential_resolved_import(ccstr filepath) {
    auto package_name = get_package_name(filepath);
    if (package_name == NULL) return NULL;

    auto ret = alloc_object(Resolved_Import);
    ret->path = filepath;
    ret->package_name = package_name;
    return ret;
}

Resolved_Import *Go_Indexer::resolve_import_from_gomod(ccstr import_path, Gomod_Info *info, Go_Ctx *ctx) {
    auto import_path_list = make_path(import_path);

    auto try_mapping = [&](ccstr base_path, ccstr resolve_to) -> Resolved_Import * {
        auto base_path_list = make_path(base_path);
        if (!base_path_list->contains(import_path_list)) return NULL;

        ccstr filepath = resolve_to;
        if (import_path_list->parts->len != base_path_list->parts->len)
            filepath = path_join(filepath, get_path_relative_to(import_path, base_path));
        return check_potential_resolved_import(filepath);
    };

    auto get_pkg_mod_path = [&](ccstr import_path, ccstr version) {
        return path_join(GOPATH, our_sprintf("pkg/mod/%s@%s", import_path, version));
    };

#define RETURN_IF(x) { auto ret = (x); if (ret != NULL) return ret; }

    if (info->module_path != NULL)
        RETURN_IF(try_mapping(info->module_path, index.current_path));

    For (*info->directives) {
        switch (it.type) {
        case GOMOD_DIRECTIVE_REQUIRE:
            RETURN_IF(try_mapping(it.module_path, get_pkg_mod_path(it.module_path, it.module_version)));
            break;
        case GOMOD_DIRECTIVE_REPLACE:
            {
                ccstr filepath;
                if (it.replace_version == NULL)
                    filepath = rel_to_abs_path(it.replace_path);
                else
                    filepath = get_pkg_mod_path(it.replace_path, it.replace_version);
                RETURN_IF(try_mapping(it.module_path, filepath));
            }
            break;
        }
    }

#undef RETURN_IF
}

// resolve import by consulting the index
Resolved_Import* Go_Indexer::resolve_import(ccstr import_path, Go_Ctx *ctx) {
    auto pkg = find_package_in_index(ctx->import_path, ctx->resolved_path);
    if (pkg == NULL) return NULL;

    auto dep = pkg->dependencies->find([&](Go_Dependency *it) -> bool {
        return streq(it->import_path, import_path);
    });
    if (dep == NULL) return NULL;

    auto ri = alloc_object(Resolved_Import);
    ri->path = dep->resolved_path;
    ri->package_name = dep->package_name;
    return ri;
}

/*
 * Given a TS_STRUCT_TYPE or TS_INTERFACE_TYPE, returns the interim type
 * (in Ast form) of a given field. Recursively handles embedded
 * fields/specs.
 */
ccstr Go_Indexer::get_workspace_import_path() {
    if (index.gomod->exists) {
        auto module_path = index.gomod->module_path;
        if (module_path != NULL) return module_path;
    }

    // if we're inside gopath
    auto gopath_base = path_join(GOPATH, "src");
    if (str_starts_with(index.current_path, gopath_base)) {
        auto ret = (cstr)get_path_relative_to(index.current_path, gopath_base);
        return normalize_path_separator(ret, '/');
    }

    // should we just panic? this is a pretty bad situation
    // maybe this should be a check on program start?
    return NULL;
}

Go_Package *Go_Indexer::find_package(ccstr import_path, ccstr resolved_path) {
    return index.packages->find([&](Go_Package *it) -> bool {
        return (
            it->status != GPS_OUTDATED
            && streq(it->import_path, import_path)
            && streq(it->resolved_path, resolved_path)
        );
    });
}

Goresult *Go_Indexer::find_decl_of_id(ccstr id_to_find, cur2 id_pos, Go_Ctx *ctx, Go_Single_Import **single_import) {
    Go_Package_File_Info *finfo = NULL;

    // it's assumed that if local_decls is set, it's for the current Go_Ctx passed here
    if (local_decls != NULL) {
        auto decl = local_decls->get(id_to_find);
        if (decl != NULL) return make_goresult(decl, ctx);
    }

    auto pkg = find_package(ctx->import_path, ctx->resolved_path);
    if (pkg != NULL) {
        auto check = [&](Go_Package_File_Info *it) { return streq(it->filename, ctx->filename); };
        finfo = pkg->files->find(check);
        if (finfo != NULL) {
            auto scope_ops = finfo->scope_ops;

            SCOPED_FRAME_WITH_MEM(&scoped_table_mem);

            Scoped_Table<Godecl*> table;
            {
                SCOPED_MEM(&scoped_table_mem);
                table.init();
            }
            defer { table.cleanup(); };

            For (*scope_ops) {
                if (it.pos > id_pos) break;
                switch (it.type) {
                case GSOP_OPEN_SCOPE: table.push_scope(); break;
                case GSOP_CLOSE_SCOPE: table.pop_scope(); break;
                case GSOP_DECL: table.set(it.decl->name, it.decl); break;
                }
            }

            auto decl = table.get(id_to_find);
            if (decl != NULL) return make_goresult(decl, ctx);
        }

        For (*pkg->individual_imports) {
            if (streq(it.file, ctx->filename)) {
                if (it.package_name != NULL && streq(it.package_name, id_to_find)) {
                    if (single_import != NULL)
                        *single_import = &it;
                    return make_goresult(it.decl, ctx);
                }
            }
        }
    }

    return find_decl_in_package(id_to_find, ctx->import_path, ctx->resolved_path);
}

ccstr Go_Indexer::get_filepath_from_ctx(Go_Ctx *ctx) {
    return path_join(ctx->resolved_path, ctx->filename);
}

List<Goresult> *Go_Indexer::get_possible_dot_completions(Ast_Node *operand_node, bool *was_package, Go_Ctx *ctx) {
    switch (operand_node->type) {
    case TS_IDENTIFIER:
    case TS_FIELD_IDENTIFIER:
    case TS_PACKAGE_IDENTIFIER:
    case TS_TYPE_IDENTIFIER:
        do {
            ccstr resolved_path = NULL;
            auto import_path = find_import_path_referred_to_by_id(operand_node->string(), ctx, &resolved_path);
            if (import_path == NULL) break;
            if (resolved_path == NULL) break;

            auto ret = get_package_decls(import_path, resolved_path, true);
            if (ret != NULL)  {
                *was_package = true;
                return ret;
            }
        } while (0);
        break;
    }

    auto res = infer_type(operand_node, ctx);
    if (res == NULL) return NULL;

    auto resolved_res = resolve_type(res->gotype, res->ctx);
    if (resolved_res == NULL) return NULL;

    auto results = alloc_list<Goresult>();
    list_fields_and_methods(res, resolved_res, results);

    *was_package = false;
    return results;
}

Jump_To_Definition_Result* Go_Indexer::jump_to_definition(ccstr filepath, cur2 pos) {
    auto pf = parse_file(filepath);
    if (pf == NULL) return NULL;
    defer { free_parsed_file(pf); };

    auto file = pf->root;

    Go_Ctx ctx = {0};
    ctx.import_path = file_to_import_path(filepath);
    ctx.resolved_path = normalize_resolved_path(our_dirname(filepath));
    ctx.filename = our_basename(filepath);

    Jump_To_Definition_Result result = {0};

    find_nodes_containing_pos(file, pos, false, [&](Ast_Node *node) -> Walk_Action {
        auto contains_pos = [&](Ast_Node *node) -> bool {
            return cmp_pos_to_node(pos, node) == 0;
        };

        switch (node->type) {
        case TS_PACKAGE_CLAUSE:
            {
                auto name_node = node->child();
                if (contains_pos(name_node)) {
                    result.file = filepath;
                    result.pos = name_node->start;
                }
            }
            return WALK_ABORT;

        case TS_IMPORT_SPEC:
            result.file = filepath;
            result.pos = node->start;
            return WALK_ABORT;

        case TS_QUALIFIED_TYPE:
        case TS_SELECTOR_EXPRESSION:
            {
                auto sel_node = node->field(node->type == TS_QUALIFIED_TYPE ? TSF_NAME : TSF_FIELD);
                if (!contains_pos(sel_node)) return WALK_CONTINUE;

                auto operand_node = node->field(node->type == TS_QUALIFIED_TYPE ? TSF_PACKAGE : TSF_OPERAND);

                bool dontcare;
                auto results = get_possible_dot_completions(operand_node, &dontcare, &ctx);
                if (results != NULL) {
                    auto sel_name = sel_node->string();
                    For (*results) {
                        auto decl = it.decl;

                        if (streq(decl->name, sel_name)) {
                            result.pos = decl->name_start;
                            result.file = get_filepath_from_ctx(it.ctx);
                            return WALK_ABORT;
                        }
                    }
                }
            }
            return WALK_ABORT;

        case TS_PACKAGE_IDENTIFIER:
        case TS_TYPE_IDENTIFIER:
        case TS_IDENTIFIER:
        case TS_FIELD_IDENTIFIER:
            {
                auto res = find_decl_of_id(node->string(), node->start, &ctx);
                if (res != NULL) {
                    result.file = get_filepath_from_ctx(res->ctx);
                    if (res->decl->name != NULL)
                        result.pos = res->decl->name_start;
                    else
                        result.pos = res->decl->spec_start;
                }
            }
            return WALK_ABORT;
        }

        return WALK_CONTINUE;
    });

    if (result.file == NULL) return NULL;

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

bool isident(int c) {
    return isalnum(c) || c == '_';
}

bool is_expression_node(Ast_Node *node) {
    switch (node->type) {
    case TS_PARENTHESIZED_EXPRESSION:
    case TS_CALL_EXPRESSION:
    case TS_SELECTOR_EXPRESSION:
    case TS_INDEX_EXPRESSION:
    case TS_SLICE_EXPRESSION:
    case TS_TYPE_ASSERTION_EXPRESSION:
    case TS_TYPE_CONVERSION_EXPRESSION:
    case TS_UNARY_EXPRESSION:
    case TS_BINARY_EXPRESSION:
    case TS_RAW_STRING_LITERAL:
    case TS_INT_LITERAL:
    case TS_FLOAT_LITERAL:
    case TS_IMAGINARY_LITERAL:
    case TS_RUNE_LITERAL:
    case TS_COMPOSITE_LITERAL:
    case TS_FUNC_LITERAL:
    case TS_INTERPRETED_STRING_LITERAL:
    case TS_IDENTIFIER:
    // others that show up in the context of what we consider an expression
    case TS_QUALIFIED_TYPE:
    case TS_TYPE_IDENTIFIER:
    case TS_FIELD_IDENTIFIER:
    case TS_PACKAGE_IDENTIFIER:
        return true;
    }
    return false;
}

bool Go_Indexer::autocomplete(ccstr filepath, cur2 pos, bool triggered_by_period, Autocomplete *out) {
    auto pf = parse_file(filepath);
    if (pf == NULL) return false;
    defer { free_parsed_file(pf); };

    auto intelligently_move_cursor_backwards = [&]() -> cur2 {
        auto it = alloc_object(Parser_It);
        memcpy(it, pf->it, sizeof(Parser_It));
        it->set_pos(pos);

        // if we're on a space, go back until we hit a nonspace.
        //
        // otherwise, check for the following scenario: we're on a non-ident,
        // and previous char is ident. in that case, go back to previous char.

        if (isspace(it->peek())) {
            while (!it->bof() && isspace(it->peek())) it->prev();
        } else if (!isident(it->peek())) {
            it->prev();
            if (!isident(it->peek()) && it->peek() != '.')
                it->next();
        }

        return it->get_pos();
    };

    Go_Ctx ctx = {0};
    ctx.import_path = file_to_import_path(filepath);
    ctx.resolved_path = normalize_resolved_path(our_dirname(filepath));
    ctx.filename = our_basename(filepath);

    List<Goresult> results;
    results.init();
    bool found_something = false;

    enum Current_Situation {
        FOUND_JACK_SHIT,
        FOUND_DOT_COMPLETE,
        FOUND_LONE_IDENTIFIER,
        FOUND_PROBLEM_NEED_TO_EXIT,
    };

    Current_Situation situation = FOUND_JACK_SHIT;
    Ast_Node *expr_to_analyze = NULL;
    cur2 keyword_start = {0};
    ccstr prefix = NULL;

    // i believe right now there are three possibilities:
    //
    // 1) well-formed ts_selector_expression
    // 2) ill-formed ts_selector_expression in the making; we have a ts_anon_dot
    // 3) we have nothing, just do a keyword autocomplete

    find_nodes_containing_pos(pf->root, intelligently_move_cursor_backwards(), false, [&](Ast_Node *node) -> Walk_Action {
        switch (node->type) {
        case TS_QUALIFIED_TYPE:
        case TS_SELECTOR_EXPRESSION:
            {
                auto operand_node = node->field(node->type == TS_QUALIFIED_TYPE ? TSF_PACKAGE : TSF_OPERAND);
                if (operand_node->null) return WALK_ABORT;
                if (cmp_pos_to_node(pos, operand_node) == 0) return WALK_CONTINUE;

                auto sel_node = node->field(node->type == TS_QUALIFIED_TYPE ? TSF_NAME : TSF_FIELD);

                bool dot_found = false;
                for (auto curr = node->child_all(); !curr->null; curr = curr->next_all()) {
                    if (curr->type == TS_ANON_DOT) {
                        dot_found = true;
                        break;
                    }
                }

                if (!dot_found) return WALK_ABORT;

                switch (cmp_pos_to_node(pos, sel_node)) {
                case -1: // pos is before sel
                    keyword_start = pos;
                    prefix = "";
                    break;
                case 0: // pos is inside sel
                    keyword_start = sel_node->start;
                    prefix = our_strcpy(sel_node->string());
                    ((cstr)prefix)[pos.x - sel_node->start.x] = '\0';
                    break;
                case 1: // pos is after sel
                    // if it's directly to the right of sel, like foo.bar|,
                    // then we treat cursor as being "in" sel
                    if (pos == sel_node->end) {
                        keyword_start = sel_node->start;
                        prefix = sel_node->string();
                        break;
                    }
                    return WALK_ABORT;
                }

                expr_to_analyze = operand_node;
                situation = FOUND_DOT_COMPLETE;
            }
            return WALK_ABORT;

        case TS_PACKAGE_IDENTIFIER:
        case TS_TYPE_IDENTIFIER:
        case TS_IDENTIFIER:
        case TS_FIELD_IDENTIFIER:
            expr_to_analyze = node;
            situation = FOUND_LONE_IDENTIFIER;
            keyword_start = node->start;
            prefix = our_strcpy(node->string());
            ((cstr)prefix)[pos.x - node->start.x] = '\0';
            return WALK_ABORT;

        case TS_ANON_DOT:
            {
                auto prev = node->prev();
                if (!prev->null) {
                    expr_to_analyze = prev;
                    situation = FOUND_DOT_COMPLETE;
                    keyword_start = pos;
                    prefix = "";
                } else {
                    auto parent = node->parent();
                    if (!parent->null && parent->type == TS_ERROR) {
                        auto expr = parent->prev();
                        if (!expr->null) {
                            while (!is_expression_node(expr)) {
                                expr = expr->child();
                                if (expr->null) {
                                    situation = FOUND_PROBLEM_NEED_TO_EXIT;
                                    return WALK_ABORT;
                                }

                                Ast_Node *next = NULL;
                                while (!(next = expr->next())->null) expr = next;
                            }

                            expr_to_analyze = expr;
                            situation = FOUND_DOT_COMPLETE;
                            keyword_start = pos;
                            prefix = "";
                        }
                    }
                }
            }
            return WALK_ABORT;
        }
        return WALK_CONTINUE;
    });

    List<AC_Result> *ac_results = NULL;

    switch (situation) {
    case FOUND_PROBLEM_NEED_TO_EXIT:
        return false;
    case FOUND_DOT_COMPLETE:
        {
            bool was_package = false;
            auto results = get_possible_dot_completions(expr_to_analyze, &was_package, &ctx);
            if (results == NULL || results->len == 0) return false;

            ac_results = alloc_list<AC_Result>(results->len);
            For (*results) ac_results->append()->name = it.decl->name;

            out->type = (was_package ? AUTOCOMPLETE_PACKAGE_EXPORTS : AUTOCOMPLETE_FIELDS_AND_METHODS);
        }
        break;
    case FOUND_JACK_SHIT:
        keyword_start = pos;
        prefix = "";
        // fallthrough
    case FOUND_LONE_IDENTIFIER:
        {
            String_Set seen_strings;
            seen_strings.init();

            ac_results = alloc_list<AC_Result>();

            auto pkg = find_package(ctx.import_path, ctx.resolved_path);
            if (pkg != NULL) {
                auto check = [&](Go_Package_File_Info *it) { return streq(it->filename, ctx.filename); };
                auto finfo = pkg->files->find(check);
                if (finfo != NULL) {
                    SCOPED_FRAME_WITH_MEM(&scoped_table_mem);
                    Scoped_Table<bool> table;
                    {
                        SCOPED_MEM(&scoped_table_mem);
                        table.init();
                    }
                    defer { table.cleanup(); };

                    For (*finfo->scope_ops) {
                        if (it.pos > pos) break;
                        switch (it.type) {
                        case GSOP_OPEN_SCOPE: table.push_scope(); break;
                        case GSOP_CLOSE_SCOPE: table.pop_scope(); break;
                        case GSOP_DECL: table.set(it.decl->name, true); break;
                        }
                    }

                    auto entries = table.entries();
                    For (*entries) {
                        ac_results->append()->name = it->name;
                        seen_strings.add(it->name);
                    }
                }

                For (*pkg->individual_imports) {
                    if (streq(it.file, ctx.filename)) {
                        if (!seen_strings.has(it.package_name)) {
                            ac_results->append()->name = it.package_name;
                            seen_strings.add(it.package_name);
                        }
                    }
                }
            }

            auto results = get_package_decls(ctx.import_path, ctx.resolved_path);
            if (results != NULL) {
                For (*results) {
                    auto decl = it.decl;
                    if (decl->type == GODECL_FUNC && decl->gotype->func_recv != NULL)
                        continue;
                    if (!seen_strings.has(decl->name)) {
                        ac_results->append()->name = decl->name;
                        seen_strings.add(decl->name);
                    }
                }
            }

            if (ac_results->len == 0) return false;

            out->type = AUTOCOMPLETE_IDENTIFIER;
        }
        break;
    }

    out->keyword_start_position = keyword_start;
    out->results = ac_results;
    out->prefix = prefix;

    return true;
}

Parameter_Hint *Go_Indexer::parameter_hint(ccstr filepath, cur2 pos, bool triggered_by_paren) {
    auto pf = parse_file(filepath);
    if (pf == NULL) return NULL;
    defer { free_parsed_file(pf); };

    auto go_back_until_non_space = [&]() -> cur2 {
        auto it = alloc_object(Parser_It);
        memcpy(it, pf->it, sizeof(Parser_It));
        it->set_pos(pos);
        while (!it->bof() && isspace(it->peek())) it->prev();
        return it->get_pos();
    };

    Ast_Node *func_expr = NULL;
    cur2 call_args_start;

    find_nodes_containing_pos(pf->root, go_back_until_non_space(), false, [&](Ast_Node *node) -> Walk_Action {
        switch (node->type) {
        case TS_CALL_EXPRESSION:
            {
                if (cmp_pos_to_node(pos, node) != 0) return WALK_ABORT;

                auto func = node->field(TSF_FUNCTION);
                auto args = node->field(TSF_ARGUMENTS);

                if (cmp_pos_to_node(pos, args) < 0) break;
                if (func->null || args->null) break;

                func_expr = func;
                call_args_start = args->start;
                // don't abort, if there's a deeper func_expr we want to use that one
            }
            break;
        case TS_LPAREN:
            {
                auto prev = node->prev_all();
                if (!prev->null)
                    func_expr = prev;
                else {
                    auto parent = node->parent();
                    if (!parent->null && parent->type == TS_ERROR) {
                        auto parent_prev = parent->prev_all();
                        if (!parent_prev->null) {
                            func_expr = parent_prev;
                            call_args_start = node->start;
                        }
                    }
                }
            }
            return WALK_ABORT;
        }
        return WALK_CONTINUE;
    });

    if (func_expr == NULL) return NULL;

    Go_Ctx ctx = {0};
    ctx.import_path = file_to_import_path(filepath);
    ctx.resolved_path = normalize_resolved_path(our_dirname(filepath));
    ctx.filename = our_basename(filepath);

    auto res = infer_type(func_expr, &ctx);
    if (res == NULL) return NULL;
    if (res->gotype->type != GOTYPE_FUNC) return NULL;

    // should we try to "normalize" the types in res?  like say we have
    // packages foo and bar bar calls a func foo.Func foo includes package blah
    // and includes a type blah.Type in foo.Func if we just render "blah.Type"
    // it won't really be clear what blah refers to

    auto hint = alloc_object(Parameter_Hint);
    hint->gotype = res->gotype->copy();
    hint->call_args_start = call_args_start;
    return hint;
}

void Go_Indexer::list_fields_and_methods(Goresult *type_res, Goresult *resolved_type_res, List<Goresult> *ret) {
    // list fields of resolved type
    // ----------------------------

    auto resolved_type = resolved_type_res->gotype;

    switch (resolved_type->type) {
    case GOTYPE_POINTER:
        {
            auto new_resolved_type_res = make_goresult(resolved_type->pointer_base, resolved_type_res->ctx);
            list_fields_and_methods(type_res, new_resolved_type_res, ret);
            return;
        }

    case GOTYPE_STRUCT:
    case GOTYPE_INTERFACE:
        {
            // technically point the same place, but want to be semantically correct
            auto specs = resolved_type->type == GOTYPE_STRUCT ? resolved_type->struct_specs : resolved_type->interface_specs;
            For (*specs) {
                if (it.is_embedded) {
                    auto res = resolve_type(it.embedded_type, resolved_type_res->ctx);
                    if (res == NULL) continue;
                    list_fields_and_methods(make_goresult(it.embedded_type, resolved_type_res->ctx), res, ret);
                } else {
                    ret->append(make_goresult(it.field, resolved_type_res->ctx));
                }
            }
        }
        break;
    }

    // list methods of unresolved type
    // -------------------------------

    auto type = type_res->gotype;
    ccstr type_name = NULL;
    ccstr target_import_path = NULL;
    ccstr target_resolved_path = NULL;

    switch (type->type) {
    case GOTYPE_ID:
        // TODO: if decl of id is not a toplevel, exit (locally defined types won't have methods)
        type_name = type->id_name;
        target_import_path = type_res->ctx->import_path;
        target_resolved_path = type_res->ctx->resolved_path;
        break;
    case GOTYPE_SEL:
        type_name = type->sel_sel;
        target_import_path = find_import_path_referred_to_by_id(type->sel_name, type_res->ctx, &target_resolved_path);
        break;
    default:
        break;
    }

    if (type_name == NULL || target_import_path == NULL) return;

    auto results = get_package_decls(target_import_path, target_resolved_path);
    if (results == NULL) return;

    For (*results) {
        auto decl = it.decl;

        if (decl->type != GODECL_FUNC) continue;

        auto functype = decl->gotype;
        if (functype->func_recv == NULL) continue;

        auto recv = unpointer_type(functype->func_recv, NULL)->gotype;

        if (recv->type != GOTYPE_ID) continue;
        if (!streq(recv->id_name, type_name)) continue;

        auto ctx = alloc_object(Go_Ctx);
        ctx->import_path = target_import_path;
        ctx->filename = decl->file;

        ret->append(make_goresult(decl, ctx));
    }
}

void Go_Indexer::read_index_from_filesystem() {
    // TODO:  do this
}

#define EVENT_DEBOUNCE_DELAY_MS 3000 // wait 3 seconds (arbitrary)

// mutates parts inside path
ccstr remove_ats_from_path(ccstr s) {
    auto path = make_path(s);
    For (*path->parts) {
        auto atpos = strchr((char*)it, '@');
        if (atpos != NULL) *atpos = '\0';
    }
    return path->str();
}

ccstr Go_Indexer::file_to_import_path(ccstr filepath) {
    return directory_to_import_path(our_dirname(filepath));
}

ccstr Go_Indexer::directory_to_import_path(ccstr path_str) {
    // resolution order: vendor, workspace, gopath/pkg/mod, gopath, goroot

    auto path = make_path(path_str);

    enum Path_Type {
        PATH_WHO_CARES,
        PATH_PKGMOD,
        PATH_WKSP,
    };

    // TODO: update to work with nested vendor directories

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
        path_join(index.current_path, "vendor"),
        index.current_path,
        path_join(GOPATH, "pkg/mod"),
        path_join(GOPATH, "src"),
        path_join(GOROOT, "src"),
    };

    Path_Type path_types[] = { PATH_WHO_CARES, PATH_WKSP, PATH_PKGMOD, PATH_WHO_CARES, PATH_WHO_CARES };

    for (u32 i = 0; i < _countof(paths); i++) {
        auto ret = check(paths[i], path_types[i]);
        if (ret != NULL) return normalize_path_separator((cstr)our_strcpy(ret), '/');
    }
    return NULL;
}

void Go_Indexer::handle_fs_event(Go_Index_Watcher *w, Fs_Event *event) {
    // only one thread calls this at a time
    /*
    SCOPED_LOCK(&fs_event_lock);

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
                    get_path_relative_to(path, index.current_path)
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
    */
}

void Go_Indexer::init() {
    ptr0(this);

    mem.init("indexer_mem");
    final_mem.init("final_mem");
    ui_mem.init("ui_mem");
    scoped_table_mem.init("scoped_table_mem");

    SCOPED_MEM(&mem);

    current_parsed_files.init();

    {
        SCOPED_FRAME();
        GetModuleFileNameA(NULL, current_exe_path, _countof(current_exe_path));
        auto path = our_dirname(current_exe_path);
        strcpy_safe(current_exe_path, _countof(current_exe_path), path);
    }

    buildparser_proc.dir = current_exe_path;
    buildparser_proc.use_stdin = true;
    buildparser_proc.run("go run buildparser.go");
}

void Go_Indexer::cleanup() {
    if (bgthread != NULL) {
        kill_thread(bgthread);
        close_thread_handle(bgthread);
        bgthread = NULL;
    }

    buildparser_proc.cleanup();
    mem.cleanup();
    final_mem.cleanup();
    ui_mem.cleanup();
    scoped_table_mem.cleanup();
}

// resolves import by literally fucking around the filesystem trying to figure it out
Resolved_Import* Go_Indexer::resolve_import_from_filesystem(ccstr import_path, Go_Ctx *ctx) {
    // move up directory tree, looking for vendor or go.mod
    auto curr_path = make_path(ctx->resolved_path);
    do {
        auto dir_name = curr_path->str();

        auto vendor_path = path_join(dir_name, "vendor");
        if (check_path(vendor_path) == CPR_DIRECTORY) {
            auto ret = check_potential_resolved_import(path_join(vendor_path, import_path));
            if (ret != NULL) return ret;
        }

        auto gomod_path = path_join(dir_name, "go.mod");
        if (check_path(gomod_path) == CPR_FILE) {
            auto gomod = parse_gomod_file(gomod_path);
            if (gomod->exists) {
                auto ret = resolve_import_from_gomod(import_path, gomod, ctx);
                if (ret != NULL) return ret;
            }
        }
    } while (curr_path->goto_parent());

    auto ret = check_potential_resolved_import(path_join(GOPATH, "src", import_path));
    if (ret == NULL)
        ret = check_potential_resolved_import(path_join(GOROOT, "src", import_path));
    return ret;
}

List<Godecl> *Go_Indexer::parameter_list_to_fields(Ast_Node *params) {
    u32 count = 0;
    FOR_NODE_CHILDREN (params) {
        auto param = it;
        auto type_node = it->field(TSF_TYPE);
        FOR_NODE_CHILDREN (param) {
            if (it->eq(type_node)) break;
            count++;
        }
    }

    auto ret = alloc_list<Godecl>(count);

    FOR_NODE_CHILDREN (params) {
        auto type_node = it->field(TSF_TYPE);
        auto param_node = it;

        bool is_variadic = (param_node->type == TS_VARIADIC_PARAMETER_DECLARATION);

        FOR_NODE_CHILDREN (param_node) {
            if (it->eq(type_node)) break;

            auto field = ret->append();
            field->type = GODECL_FIELD;
            field->decl_start = param_node->start;
            field->spec_start = param_node->start;
            field->name_start = it->start;
            field->name = it->string();
            field->gotype = node_to_gotype(type_node);

            if (is_variadic) {
                auto t = new_gotype(GOTYPE_VARIADIC);
                t->variadic_base = field->gotype;
                field->gotype = t;
            }
        }
    }

    return ret;
}

bool Go_Indexer::node_func_to_gotype_sig(Ast_Node *params, Ast_Node *result, Go_Func_Sig *sig) {
    if (params->type != TS_PARAMETER_LIST) return false;

    sig->params = parameter_list_to_fields(params);

    if (!result->null) {
        if (result->type == TS_PARAMETER_LIST) {
            sig->result = parameter_list_to_fields(result);
        } else {
            sig->result = alloc_list<Godecl>(1);
            auto field = sig->result->append();
            field->type = GODECL_FIELD;
            field->decl_start = result->start;
            field->spec_start = result->start;
            field->name = NULL;
            field->gotype = node_to_gotype(result);
        }
    }

    return true;
}

Gotype *Go_Indexer::node_to_gotype(Ast_Node *node) {
    if (node->null) return NULL;

    Gotype *ret = NULL;

    switch (node->type) {
    // case TS_SIMPLE_TYPE:

    case TS_QUALIFIED_TYPE:
        {
            auto pkg_node = node->field(TSF_PACKAGE);
            if (pkg_node->null) break;
            auto name_node = node->field(TSF_NAME);
            if (name_node->null) break;

            ret = new_gotype(GOTYPE_SEL);
            ret->sel_name = pkg_node->string();
            ret->sel_sel = name_node->string();
        }
        break;

    case TS_TYPE_IDENTIFIER:
        ret = new_gotype(GOTYPE_ID);
        ret->id_name = node->string();
        break;

    case TS_POINTER_TYPE:
        ret = new_gotype(GOTYPE_POINTER);
        ret->pointer_base = node_to_gotype(node->child());
        break;

    case TS_PARENTHESIZED_TYPE:
        ret = node_to_gotype(node->child());
        break;

    case TS_IMPLICIT_LENGTH_ARRAY_TYPE:
        ret = new_gotype(GOTYPE_ARRAY);
        ret->array_base = node_to_gotype(node->field(TSF_ELEMENT));

    case TS_ARRAY_TYPE:
        ret = new_gotype(GOTYPE_ARRAY);
        ret->array_base = node_to_gotype(node->field(TSF_ELEMENT));
        break;

    case TS_SLICE_TYPE:
        ret = new_gotype(GOTYPE_SLICE);
        ret->slice_base = node_to_gotype(node->field(TSF_ELEMENT));
        break;

    case TS_MAP_TYPE:
        ret = new_gotype(GOTYPE_MAP);
        ret->map_key = node_to_gotype(node->field(TSF_KEY));
        ret->map_value = node_to_gotype(node->field(TSF_VALUE));
        break;

    case TS_STRUCT_TYPE:
        {
            auto fieldlist_node = node->child();
            if (fieldlist_node->null) break;
            ret = new_gotype(GOTYPE_STRUCT);

            s32 num_children = 0;

            FOR_NODE_CHILDREN (fieldlist_node) {
                auto field_node = it;
                auto type_node = field_node->field(TSF_TYPE);

                u32 total = 0;
                FOR_NODE_CHILDREN (field_node) {
                    if (it->eq(type_node)) break;
                    total++;
                }
                if (total == 0) total++;

                num_children += total;
            }

            ret->struct_specs = alloc_list<Go_Struct_Spec>(num_children);

            FOR_NODE_CHILDREN (fieldlist_node) {
                auto field_node = it;

                auto tag_node = field_node->field(TSF_TAG);
                auto type_node = field_node->field(TSF_TYPE);
                auto field_type = node_to_gotype(type_node);

                bool names_found = false;
                FOR_NODE_CHILDREN (field_node) {
                    names_found = !it->eq(type_node);
                    break;
                }

                if (names_found) {
                    FOR_NODE_CHILDREN (field_node) {
                        if (it->eq(type_node)) break;

                        auto spec = ret->struct_specs->append();

                        if (!tag_node->null)
                            spec->tag = tag_node->string();

                        spec->is_embedded = false;

                        auto field = alloc_object(Godecl);
                        field->type = GODECL_FIELD;
                        field->gotype = field_type;
                        field->name = it->string();
                        field->name_start = it->start;
                        field->spec_start = field_node->start;
                        field->decl_start = field_node->start;

                        spec->field = field;
                    }
                } else {
                    auto spec = ret->struct_specs->append();
                    if (!tag_node->null)
                        spec->tag = tag_node->string();
                    spec->is_embedded = true;
                    spec->embedded_type = field_type;
                }
            }
        }
        break;

    case TS_CHANNEL_TYPE:
        ret = new_gotype(GOTYPE_CHAN);
        ret->chan_base = node_to_gotype(node->field(TSF_VALUE));
        break;

    case TS_INTERFACE_TYPE:
        {
            auto speclist_node = node->child();
            if (speclist_node->null) break;

            ret = alloc_object(Gotype);
            ret->type = GOTYPE_INTERFACE;
            ret->interface_specs = alloc_list<Go_Struct_Spec>(speclist_node->child_count);

            FOR_NODE_CHILDREN (speclist_node) {
                auto spec = ret->interface_specs->append();
                if (it->type == TS_METHOD_SPEC) {
                    spec->is_embedded = false;

                    auto field = alloc_object(Godecl);
                    field->type = GODECL_FIELD;
                    field->name = it->field(TSF_NAME)->string();
                    field->gotype = new_gotype(GOTYPE_FUNC);
                    node_func_to_gotype_sig(
                        it->field(TSF_PARAMETERS),
                        it->field(TSF_RESULT),
                        &field->gotype->func_sig
                    );

                    spec->field = field;
                } else {
                    spec->is_embedded = true;
                    spec->embedded_type = node_to_gotype(it);
                }
            }
        }
        break;

    case TS_FUNCTION_TYPE:
        ret = new_gotype(GOTYPE_FUNC);
        if (!node_func_to_gotype_sig(node->field(TSF_PARAMETERS), node->field(TSF_RESULT), &ret->func_sig))
            ret = NULL;
        break;
    }

    return ret;
}

Ast_Node *Go_Indexer::new_ast_node(TSNode node) {
    auto ret = alloc_object(Ast_Node);
    ret->init(node);
    ret->indexer = this;
    return ret;
}

void Go_Indexer::import_spec_to_decl(Ast_Node *spec_node, Godecl *decl) {
    decl->type = GODECL_IMPORT;
    decl->spec_start = spec_node->start;

    auto name_node = spec_node->field(TSF_NAME);
    if (!name_node->null) {
        decl->name = name_node->string();
        decl->name_start = name_node->start;
    }

    auto path_node = spec_node->field(TSF_PATH);
    if (!path_node->null)
        decl->import_path = path_node->string();
}

bool Go_Indexer::assignment_to_decls(List<Ast_Node*> *lhs, List<Ast_Node*> *rhs, New_Goresult_Func new_goresult, Go_Ctx *ctx) {
    if (lhs->len == 0 || rhs->len == 0) return false; // what the fuck?

    auto add_new_result = [&](Ast_Node *name_node, Gotype *gotype, Go_Ctx *ctx) {
        if (name_node->type != TS_IDENTIFIER) return;

        auto name = name_node->string();
        if (streq(name, "_")) return;

        auto goresult = new_goresult();
        goresult->ctx = ctx;
        goresult->decl->name = name;
        goresult->decl->name_start = name_node->start;
        goresult->decl->gotype = gotype;
    };

    auto new_primitive_type = [&](ccstr name) -> Gotype * {
        auto ret = new_gotype(GOTYPE_ID);
        ret->id_name = name;
        return ret;
    };

    // scenarios:
    // left = 1, right = 1
    // left = multi, right = multi
    // left = multi, right = 1

    if (lhs->len > 1 && rhs->len == 1) {
        auto res = infer_type(rhs->at(0), ctx);
        if (res == NULL) return false;

        auto gotype = res->gotype;
        auto ln = lhs->len;

        switch (gotype->type) {
        case GOTYPE_MULTI:
            if (ln != gotype->multi_types->len) return false;
            for (int i = 0; i < ln; i++)
                add_new_result(lhs->at(i), gotype->multi_types->at(i), res->ctx);
            break;
        case GOTYPE_ASSERTION:
            if (ln != 1 && ln != 2) return false;
            add_new_result(lhs->at(0), gotype->assertion_base, res->ctx);
            if (ln == 2)
                add_new_result(lhs->at(1), new_primitive_type("bool"), res->ctx);
            break;
        case GOTYPE_RANGE:
            switch (gotype->range_base->type) {
            case GOTYPE_MAP:
                if (ln != 1 && ln != 2) return false;
                add_new_result(lhs->at(0), gotype->range_base->map_key, res->ctx);
                if (ln == 2)
                    add_new_result(lhs->at(0), gotype->range_base->map_value, res->ctx);
                break;
            case GOTYPE_ARRAY:
            case GOTYPE_SLICE:
                if (ln != 2) return false;
                add_new_result(lhs->at(0), new_primitive_type("int"), res->ctx);
                add_new_result(
                    lhs->at(1),
                    gotype->type == GOTYPE_ARRAY ? gotype->array_base : gotype->slice_base,
                    res->ctx
                );
                break;
            case GOTYPE_ID:
                if (!streq(gotype->id_name, "string") || ln != 2) return false;
                add_new_result(lhs->at(0), new_primitive_type("int"), res->ctx);
                add_new_result(lhs->at(1), new_primitive_type("rune"), res->ctx);
                break;
            default:
                return false;
            }
            break;
        default:
            return false;
        }

        return true;
    }

    if (lhs->len == rhs->len) {
        // individual parts of rhs can't  be a multi
        // otherwise, direct assignment

        for (u32 i = 0; i < lhs->len; i++) {
            auto name_node = lhs->at(i);
            if (name_node->type != TS_IDENTIFIER) continue;

            auto value_node = rhs->at(i);
            auto res = infer_type(value_node, ctx);
            if (res == NULL) continue;

            auto gotype = res->gotype;
            if (gotype->type == GOTYPE_MULTI || gotype->type == GOTYPE_RANGE) continue;

            if (gotype->type == GOTYPE_ASSERTION)
                gotype = gotype->assertion_base;

            auto goresult = new_goresult();
            goresult->ctx = res->ctx;

            auto decl = goresult->decl;
            decl->name = name_node->string();
            decl->name_start = name_node->start;
            decl->gotype = res->gotype;
        }

        return true;
    }

    return false;
}

void Go_Indexer::node_to_decls(Ast_Node *node, List<Goresult> *results, Go_Ctx *ctx) {
    auto new_result = [&]() -> Goresult * {
        auto res = results->append();
        res->ctx = ctx;
        res->decl = alloc_object(Godecl);
        res->decl->file = ctx->filename;
        res->decl->decl_start = node->start;
        return res;
    };

    switch (node->type) {
    case TS_FUNCTION_DECLARATION:
    case TS_METHOD_DECLARATION:
        {
            auto name = node->field(TSF_NAME);
            if (name->null) break;

            auto params_node = node->field(TSF_PARAMETERS);
            auto result_node = node->field(TSF_RESULT);

            auto gotype = new_gotype(GOTYPE_FUNC);
            if (!node_func_to_gotype_sig(params_node, result_node, &gotype->func_sig)) break;

            if (node->type == TS_METHOD_DECLARATION) {
                auto recv_node = node->field(TSF_RECEIVER);
                auto recv_type = recv_node->child()->field(TSF_TYPE);
                if (!recv_type->null) {
                    gotype->func_recv = node_to_gotype(recv_type);
                    if (gotype->func_recv == NULL) break;
                }
            }

            auto decl = new_result()->decl;
            decl->type = GODECL_FUNC;
            decl->spec_start = node->start;
            decl->name = name->string();
            decl->gotype = new_gotype(GOTYPE_FUNC);
        }
        break;

    case TS_TYPE_DECLARATION:
        for (auto spec = node->child(); !spec->null; spec = spec->next()) {
            auto name_node = spec->field(TSF_NAME);
            if (name_node->null) continue;

            auto type_node = spec->field(TSF_TYPE);
            if (type_node->null) continue;

            auto gotype = node_to_gotype(type_node);
            if (gotype == NULL) continue;

            auto decl = new_result()->decl;
            decl->type = GODECL_TYPE;
            decl->spec_start = spec->start;
            decl->name = name_node->string();
            decl->name_start = name_node->start;
            decl->gotype = gotype;
        }
        break;

    case TS_PARAMETER_LIST:
    case TS_CONST_DECLARATION:
    case TS_VAR_DECLARATION:
        FOR_NODE_CHILDREN(node) {
            auto spec = it;
            auto type_node = spec->field(TSF_TYPE);
            auto value_node = spec->field(TSF_VALUE);

            if (type_node->null && value_node->null) continue;

            Gotype *type_node_gotype = NULL;
            if (!type_node->null) {
                type_node_gotype = node_to_gotype(type_node);
                if (type_node_gotype == NULL) continue;

                if (node->type == TS_PARAMETER_LIST && spec->type == TS_VARIADIC_PARAMETER_DECLARATION) {
                    auto t = new_gotype(GOTYPE_VARIADIC);
                    t->variadic_base = type_node_gotype;
                    type_node_gotype = t;
                }

                FOR_NODE_CHILDREN (spec) {
                    if (it->eq(type_node) || it->eq(value_node)) break;

                    auto decl = new_result()->decl;

                    switch (node->type) {
                    case TS_PARAMETER_LIST: decl->type = GODECL_FIELD; break;
                    case TS_CONST_DECLARATION: decl->type = GODECL_CONST; break;
                    case TS_VAR_DECLARATION: decl->type = GODECL_VAR; break;
                    }

                    decl->spec_start = spec->start;
                    decl->name = it->string();
                    decl->name_start = it->start;
                    decl->gotype = type_node_gotype;
                }
            } else {
                our_assert(value_node->type == TS_EXPRESSION_LIST, "rhs must be a TS_EXPRESSION_LIST");

                u32 lhs_count = 0;
                FOR_NODE_CHILDREN (spec) {
                    if (it->eq(type_node) || it->eq(value_node)) break;
                    lhs_count++;
                }

                auto lhs = alloc_list<Ast_Node*>(lhs_count);
                auto rhs = alloc_list<Ast_Node*>(value_node->child_count);

                FOR_NODE_CHILDREN (spec) {
                    if (it->eq(type_node) || it->eq(value_node)) break;
                    lhs->append(it);
                }

                FOR_NODE_CHILDREN (value_node) rhs->append(it);

                auto new_goresult = [&]() -> Goresult * {
                    auto res = new_result();
                    auto decl = res->decl;
                    decl->spec_start = spec->start;

                    switch (node->type) {
                    case TS_PARAMETER_LIST: decl->type = GODECL_FIELD; break;
                    case TS_CONST_DECLARATION: decl->type = GODECL_CONST; break;
                    case TS_VAR_DECLARATION: decl->type = GODECL_VAR; break;
                    }

                    return res;
                };

                assignment_to_decls(lhs, rhs, new_goresult, ctx);
            }
        }
        break;

    case TS_SHORT_VAR_DECLARATION:
        {
            auto left = node->field(TSF_LEFT);
            auto right = node->field(TSF_RIGHT);

            if (left->type != TS_EXPRESSION_LIST) break;
            if (right->type != TS_EXPRESSION_LIST) break;

            auto lhs = alloc_list<Ast_Node*>(left->child_count);
            auto rhs = alloc_list<Ast_Node*>(right->child_count);

            FOR_NODE_CHILDREN (left) lhs->append(it); 
            FOR_NODE_CHILDREN (right) rhs->append(it); 

            auto new_goresult = [&]() -> Goresult * {
                auto res = new_result();
                auto decl = res->decl;
                decl->spec_start = node->start;
                decl->type = GODECL_SHORTVAR;
                return res;
            };

            assignment_to_decls(lhs, rhs, new_goresult, ctx);

        }
        break;
    }
}

Goresult *Go_Indexer::unpointer_type(Gotype *type, Go_Ctx *ctx) {
    while (type->type == GOTYPE_POINTER)
        type = type->pointer_base;
    return make_goresult(type, ctx);
}

List<Goresult> *Go_Indexer::get_package_decls(ccstr import_path, ccstr resolved_path, bool public_only) {
    For (*index.packages)
        if (it.status != GPS_OUTDATED)
            if (streq(it.import_path, import_path))
                if (streq(it.resolved_path, resolved_path)) {
                    auto ret = alloc_list<Goresult>(it.decls->len);
                    For (*it.decls) {
                        if (public_only && !(it.name != NULL && isupper(it.name[0])))
                            continue;

                        auto ctx = alloc_object(Go_Ctx);
                        ctx->import_path = import_path;
                        ctx->resolved_path = resolved_path;
                        ctx->filename = it.file;
                        ret->append(make_goresult(&it, ctx));
                    }
                    return ret;
                }

    return NULL;
}

Goresult *Go_Indexer::find_decl_in_package(ccstr id, ccstr import_path, ccstr resolved_path) {
    auto results = get_package_decls(import_path, resolved_path);
    if (results == NULL) return NULL;

    // in the future we can sort this shit
    For (*results)
        if (streq(it.decl->name, id))
            return &it;
    return NULL;
}

Goresult *Go_Indexer::infer_type(Ast_Node *expr, Go_Ctx *ctx) {
    if (expr->null) return NULL;

    switch (expr->type) {
    case TS_UNARY_EXPRESSION:
        {
            auto operator_node = expr->field(TSF_OPERATOR);
            auto operand_node = expr->field(TSF_OPERAND);
            switch (operator_node->type) {
            // case TS_RANGE: // idk even what to do here
            case TS_PLUS: // add
            case TS_DASH: // sub
            case TS_CARET: // xor
                {
                    auto type = new_gotype(GOTYPE_ID);
                    type->id_name = "int";
                    return make_goresult(type, NULL);
                }
                break;
            case TS_BANG: // not
                {
                    auto type = new_gotype(GOTYPE_ID);
                    type->id_name = "bool";
                    return make_goresult(type, NULL);
                }
                break;
            case TS_STAR: // mul
                {
                    auto res = infer_type(operand_node, ctx);
                    if (res == NULL) return NULL;
                    if (res->gotype->type != GOTYPE_POINTER) return NULL;
                    return make_goresult(res->gotype->pointer_base, res->ctx);
                }
                break;
            case TS_AMP: // and
                {
                    auto res = infer_type(operand_node, ctx);
                    if (res == NULL) return NULL;

                    auto type = new_gotype(GOTYPE_POINTER);
                    type->pointer_base = res->gotype;
                    return make_goresult(type, res->ctx);
                }
                break;
            case TS_LT_DASH: // arrow
                {
                    auto res = infer_type(operand_node, ctx);
                    if (res == NULL) return NULL;
                    if (res->gotype->type != GOTYPE_CHAN) return NULL;
                    return make_goresult(res->gotype->chan_base, res->ctx);
                }
                break;
            }
        }
        break;

    case TS_CALL_EXPRESSION:
        {
            auto func_node = expr->field(TSF_FUNCTION);

            auto res = infer_type(func_node, ctx);
            if (res == NULL) break;

            res = resolve_type(res->gotype, res->ctx);
            res = unpointer_type(res->gotype, res->ctx);

            if (res->gotype->type != GOTYPE_FUNC) return NULL;

            auto result = res->gotype->func_sig.result;
            if (result == NULL) return NULL;

            auto ret = new_gotype(GOTYPE_MULTI);
            ret->multi_types = alloc_list<Gotype*>(result->len);
            For (*result) ret->multi_types->append(it.gotype);
            return make_goresult(ret, ctx);
        }
        break;

    case TS_INDEX_EXPRESSION:
        {
            auto operand_node = expr->field(TSF_OPERAND);
            auto res = infer_type(operand_node, ctx);
            if (res == NULL) return NULL;

            res = resolve_type(res->gotype, res->ctx);
            res = unpointer_type(res->gotype, res->ctx);

            auto operand_type = res->gotype;

            switch (operand_type->type) {
            case GOTYPE_ARRAY: return make_goresult(operand_type->array_base, res->ctx);
            case GOTYPE_SLICE: return make_goresult(operand_type->slice_base, res->ctx);
            case GOTYPE_ID:
                if (streq(operand_type->id_name, "string")) {
                    auto ret = new_gotype(GOTYPE_ID);
                    ret->id_name = "rune";
                    return make_goresult(ret, NULL);
                }
                break;
            case GOTYPE_MAP:
                {
                    auto ret = new_gotype(GOTYPE_ASSERTION);
                    ret->assertion_base = operand_type->map_value;
                    return make_goresult(ret, res->ctx);
                }
            }
        }
        break;

    case TS_QUALIFIED_TYPE:
    case TS_SELECTOR_EXPRESSION:
        {
            auto operand_node = expr->field(expr->type == TS_QUALIFIED_TYPE ? TSF_PACKAGE : TSF_OPERAND);
            auto field_node = expr->field(expr->type == TS_QUALIFIED_TYPE ? TSF_NAME : TSF_FIELD);
            auto field_name = field_node->string();

            Goresult *res = NULL;

            switch (operand_node->type) {
            case TS_IDENTIFIER:
            case TS_FIELD_IDENTIFIER:
            case TS_PACKAGE_IDENTIFIER:
            case TS_TYPE_IDENTIFIER:
                {
                    Go_Single_Import *si = NULL;
                    auto decl_res = find_decl_of_id(operand_node->string(), operand_node->start, ctx, &si);
                    if (decl_res != NULL) {
                        if (si != NULL) {
                            auto res = find_decl_in_package(field_name, si->import_path, si->resolved_path);
                            if (res != NULL) {
                                auto ext_decl = res->decl;
                                switch (ext_decl->type) {
                                case GODECL_VAR:
                                case GODECL_CONST:
                                case GODECL_FUNC:
                                    return make_goresult(ext_decl->gotype, res->ctx);
                                }
                            }
                            return NULL;
                        } else {
                            res = make_goresult(decl_res->decl->gotype, decl_res->ctx);
                        }
                    }
                }
            }

            if (res == NULL)
                res = infer_type(operand_node, ctx);

            if (res == NULL) return NULL;

            auto resolved_res = resolve_type(res->gotype, res->ctx);
            resolved_res = unpointer_type(resolved_res->gotype, resolved_res->ctx);

            List<Goresult> results;
            results.init();
            list_fields_and_methods(res, resolved_res, &results);

            For (results) {
                auto decl = it.decl;
                if (streq(decl->name, field_name))
                    return make_goresult(decl->gotype, it.ctx);
            }
        }
        break;

    case TS_SLICE_EXPRESSION:
        return infer_type(expr->field(TSF_OPERAND), ctx);

    case TS_TYPE_CONVERSION_EXPRESSION:
    case TS_COMPOSITE_LITERAL:
        {
            auto type_node = expr->field(TSF_TYPE);
            if (type_node->null) return NULL;
            return make_goresult(node_to_gotype(type_node), ctx);
        }

    case TS_TYPE_ASSERTION_EXPRESSION:
        {
            auto type_node = expr->field(TSF_TYPE);
            if (type_node->null) return NULL;

            auto ret = new_gotype(GOTYPE_ASSERTION);
            ret->assertion_base = node_to_gotype(type_node);
            return make_goresult(ret, ctx);
        }

    // Although its name is TS_PACKAGE_IDENTIFIER, that just means it is part
    // of a selector expression. It may well be just a normal identifier that
    // refers to an object.
    case TS_PACKAGE_IDENTIFIER:
    case TS_TYPE_IDENTIFIER:
    case TS_IDENTIFIER:
    case TS_FIELD_IDENTIFIER:
        {
            auto res = find_decl_of_id(expr->string(), expr->start, ctx);
            if (res == NULL) return NULL;
            if (res->decl->gotype == NULL) return NULL;
            return make_goresult(res->decl->gotype, ctx);
        }
    }

    return NULL;
}

Goresult *make_goresult_from_pointer(void *ptr, Go_Ctx *ctx) {
    our_assert(ptr != NULL, "make_goresult should never contain a NULL value, just return NULL instead!");

    auto ret = alloc_object(Goresult);
    ret->ptr = ptr;
    ret->ctx = ctx;
    return ret;
}

Goresult *make_goresult(Gotype *gotype, Go_Ctx *ctx) { return make_goresult_from_pointer(gotype, ctx); }
Goresult *make_goresult(Godecl *decl, Go_Ctx *ctx) { return make_goresult_from_pointer(decl, ctx); }

Goresult *Go_Indexer::resolve_type(Gotype *type, Go_Ctx *ctx) {
    switch (type->type) {
    case GOTYPE_POINTER:
        {
            auto res = resolve_type(type->pointer_base, ctx);
            if (res == NULL) break;

            auto ret = new_gotype(GOTYPE_POINTER);
            ret->pointer_base = res->gotype;

            return make_goresult(ret, res->ctx);
        }
        break;

    case GOTYPE_ID:
        {
            auto res = find_decl_of_id(type->id_name, type->id_pos, ctx);
            if (res == NULL) break;
            if (res->decl->type != GODECL_TYPE) break;
            return resolve_type(res->decl->gotype, res->ctx);
        }

    case GOTYPE_SEL:
        {
            ccstr resolved_path = NULL;
            auto import_path = find_import_path_referred_to_by_id(type->sel_name, ctx, &resolved_path);
            if (import_path == NULL) break;
            if (resolved_path == NULL) break;

            auto res = find_decl_in_package(type->sel_sel, import_path, resolved_path);
            if (res == NULL) break;
            if (res->decl->type != GODECL_TYPE) break;

            return resolve_type(res->decl->gotype, res->ctx);
        }
    }
    return make_goresult(type, ctx);
}

// -----

void walk_ts_cursor(TSTreeCursor *curr, bool abstract_only, Walk_TS_Callback cb) {
    List<bool> processed;
    processed.init();
    processed.append(false);
    int depth = 0;

    while (true) {
        if (processed[depth]) {
            if (ts_tree_cursor_goto_next_sibling(curr)) {
                processed[depth] = false;
                continue;
            }
            if (ts_tree_cursor_goto_parent(curr)) {
                processed.len--;
                depth--;
                continue;
            }
            break;
        }

        auto node = ts_tree_cursor_current_node(curr);
        if (abstract_only && !ts_node_is_named(node)) {
            processed[depth] = true;
            continue;
        }

        Ast_Node wrapped_node;
        wrapped_node.init(node);

        auto field_type = ts_tree_cursor_current_field_id(curr);
        auto result = cb(&wrapped_node, (Ts_Field_Type)field_type, depth);
        if (result == WALK_ABORT) break;

        processed[depth] = true;

        if (result != WALK_SKIP_CHILDREN)
            if (ts_tree_cursor_goto_first_child(curr))
                processed.append(false), depth++;
    }
}

TSPoint cur_to_tspoint(cur2 c) {
    TSPoint point;
    point.row = c.y;
    point.column = c.x;
    return point;
}

cur2 tspoint_to_cur(TSPoint p) {
    cur2 c;
    c.x = p.column;
    c.y = p.row;
    return c;
}

ccstr ts_field_type_str(Ts_Field_Type type) {
    switch (type) {
    define_str_case(TSF_ALIAS);
    define_str_case(TSF_ALTERNATIVE);
    define_str_case(TSF_ARGUMENTS);
    define_str_case(TSF_BODY);
    define_str_case(TSF_CAPACITY);
    define_str_case(TSF_CHANNEL);
    define_str_case(TSF_COMMUNICATION);
    define_str_case(TSF_CONDITION);
    define_str_case(TSF_CONSEQUENCE);
    define_str_case(TSF_ELEMENT);
    define_str_case(TSF_END);
    define_str_case(TSF_FIELD);
    define_str_case(TSF_FUNCTION);
    define_str_case(TSF_INDEX);
    define_str_case(TSF_INITIALIZER);
    define_str_case(TSF_KEY);
    define_str_case(TSF_LABEL);
    define_str_case(TSF_LEFT);
    define_str_case(TSF_LENGTH);
    define_str_case(TSF_NAME);
    define_str_case(TSF_OPERAND);
    define_str_case(TSF_OPERATOR);
    define_str_case(TSF_PACKAGE);
    define_str_case(TSF_PARAMETERS);
    define_str_case(TSF_PATH);
    define_str_case(TSF_RECEIVER);
    define_str_case(TSF_RESULT);
    define_str_case(TSF_RIGHT);
    define_str_case(TSF_START);
    define_str_case(TSF_TAG);
    define_str_case(TSF_TYPE);
    define_str_case(TSF_UPDATE);
    define_str_case(TSF_VALUE);
    }
    return NULL;
}

ccstr ts_ast_type_str(Ts_Ast_Type type) {
    switch (type) {
    define_str_case(TS_ERROR);
    define_str_case(TS_IDENTIFIER);
    define_str_case(TS_LF);
    define_str_case(TS_SEMI);
    define_str_case(TS_PACKAGE);
    define_str_case(TS_IMPORT);
    define_str_case(TS_ANON_DOT);
    define_str_case(TS_BLANK_IDENTIFIER);
    define_str_case(TS_LPAREN);
    define_str_case(TS_RPAREN);
    define_str_case(TS_CONST);
    define_str_case(TS_COMMA);
    define_str_case(TS_EQ);
    define_str_case(TS_VAR);
    define_str_case(TS_FUNC);
    define_str_case(TS_DOT_DOT_DOT);
    define_str_case(TS_TYPE);
    define_str_case(TS_STAR);
    define_str_case(TS_LBRACK);
    define_str_case(TS_RBRACK);
    define_str_case(TS_STRUCT);
    define_str_case(TS_LBRACE);
    define_str_case(TS_RBRACE);
    define_str_case(TS_INTERFACE);
    define_str_case(TS_MAP);
    define_str_case(TS_CHAN);
    define_str_case(TS_LT_DASH);
    define_str_case(TS_COLON_EQ);
    define_str_case(TS_PLUS_PLUS);
    define_str_case(TS_DASH_DASH);
    define_str_case(TS_STAR_EQ);
    define_str_case(TS_SLASH_EQ);
    define_str_case(TS_PERCENT_EQ);
    define_str_case(TS_LT_LT_EQ);
    define_str_case(TS_GT_GT_EQ);
    define_str_case(TS_AMP_EQ);
    define_str_case(TS_AMP_CARET_EQ);
    define_str_case(TS_PLUS_EQ);
    define_str_case(TS_DASH_EQ);
    define_str_case(TS_PIPE_EQ);
    define_str_case(TS_CARET_EQ);
    define_str_case(TS_COLON);
    define_str_case(TS_FALLTHROUGH);
    define_str_case(TS_BREAK);
    define_str_case(TS_CONTINUE);
    define_str_case(TS_GOTO);
    define_str_case(TS_RETURN);
    define_str_case(TS_GO);
    define_str_case(TS_DEFER);
    define_str_case(TS_IF);
    define_str_case(TS_ELSE);
    define_str_case(TS_FOR);
    define_str_case(TS_RANGE);
    define_str_case(TS_SWITCH);
    define_str_case(TS_CASE);
    define_str_case(TS_DEFAULT);
    define_str_case(TS_SELECT);
    define_str_case(TS_NEW);
    define_str_case(TS_MAKE);
    define_str_case(TS_PLUS);
    define_str_case(TS_DASH);
    define_str_case(TS_BANG);
    define_str_case(TS_CARET);
    define_str_case(TS_AMP);
    define_str_case(TS_SLASH);
    define_str_case(TS_PERCENT);
    define_str_case(TS_LT_LT);
    define_str_case(TS_GT_GT);
    define_str_case(TS_AMP_CARET);
    define_str_case(TS_PIPE);
    define_str_case(TS_EQ_EQ);
    define_str_case(TS_BANG_EQ);
    define_str_case(TS_LT);
    define_str_case(TS_LT_EQ);
    define_str_case(TS_GT);
    define_str_case(TS_GT_EQ);
    define_str_case(TS_AMP_AMP);
    define_str_case(TS_PIPE_PIPE);
    define_str_case(TS_RAW_STRING_LITERAL);
    define_str_case(TS_DQUOTE);
    define_str_case(TS_INTERPRETED_STRING_LITERAL_TOKEN1);
    define_str_case(TS_ESCAPE_SEQUENCE);
    define_str_case(TS_INT_LITERAL);
    define_str_case(TS_FLOAT_LITERAL);
    define_str_case(TS_IMAGINARY_LITERAL);
    define_str_case(TS_RUNE_LITERAL);
    define_str_case(TS_NIL);
    define_str_case(TS_TRUE);
    define_str_case(TS_FALSE);
    define_str_case(TS_COMMENT);
    define_str_case(TS_SOURCE_FILE);
    define_str_case(TS_PACKAGE_CLAUSE);
    define_str_case(TS_IMPORT_DECLARATION);
    define_str_case(TS_IMPORT_SPEC);
    define_str_case(TS_DOT);
    define_str_case(TS_IMPORT_SPEC_LIST);
    define_str_case(TS_DECLARATION);
    define_str_case(TS_CONST_DECLARATION);
    define_str_case(TS_CONST_SPEC);
    define_str_case(TS_VAR_DECLARATION);
    define_str_case(TS_VAR_SPEC);
    define_str_case(TS_FUNCTION_DECLARATION);
    define_str_case(TS_METHOD_DECLARATION);
    define_str_case(TS_PARAMETER_LIST);
    define_str_case(TS_PARAMETER_DECLARATION);
    define_str_case(TS_VARIADIC_PARAMETER_DECLARATION);
    define_str_case(TS_TYPE_ALIAS);
    define_str_case(TS_TYPE_DECLARATION);
    define_str_case(TS_TYPE_SPEC);
    define_str_case(TS_EXPRESSION_LIST);
    define_str_case(TS_PARENTHESIZED_TYPE);
    define_str_case(TS_SIMPLE_TYPE);
    define_str_case(TS_POINTER_TYPE);
    define_str_case(TS_ARRAY_TYPE);
    define_str_case(TS_IMPLICIT_LENGTH_ARRAY_TYPE);
    define_str_case(TS_SLICE_TYPE);
    define_str_case(TS_STRUCT_TYPE);
    define_str_case(TS_FIELD_DECLARATION_LIST);
    define_str_case(TS_FIELD_DECLARATION);
    define_str_case(TS_INTERFACE_TYPE);
    define_str_case(TS_METHOD_SPEC_LIST);
    define_str_case(TS_METHOD_SPEC);
    define_str_case(TS_MAP_TYPE);
    define_str_case(TS_CHANNEL_TYPE);
    define_str_case(TS_FUNCTION_TYPE);
    define_str_case(TS_BLOCK);
    define_str_case(TS_STATEMENT_LIST);
    define_str_case(TS_STATEMENT);
    define_str_case(TS_EMPTY_STATEMENT);
    define_str_case(TS_SIMPLE_STATEMENT);
    define_str_case(TS_SEND_STATEMENT);
    define_str_case(TS_RECEIVE_STATEMENT);
    define_str_case(TS_INC_STATEMENT);
    define_str_case(TS_DEC_STATEMENT);
    define_str_case(TS_ASSIGNMENT_STATEMENT);
    define_str_case(TS_SHORT_VAR_DECLARATION);
    define_str_case(TS_LABELED_STATEMENT);
    define_str_case(TS_EMPTY_LABELED_STATEMENT);
    define_str_case(TS_FALLTHROUGH_STATEMENT);
    define_str_case(TS_BREAK_STATEMENT);
    define_str_case(TS_CONTINUE_STATEMENT);
    define_str_case(TS_GOTO_STATEMENT);
    define_str_case(TS_RETURN_STATEMENT);
    define_str_case(TS_GO_STATEMENT);
    define_str_case(TS_DEFER_STATEMENT);
    define_str_case(TS_IF_STATEMENT);
    define_str_case(TS_FOR_STATEMENT);
    define_str_case(TS_FOR_CLAUSE);
    define_str_case(TS_RANGE_CLAUSE);
    define_str_case(TS_EXPRESSION_SWITCH_STATEMENT);
    define_str_case(TS_EXPRESSION_CASE);
    define_str_case(TS_DEFAULT_CASE);
    define_str_case(TS_TYPE_SWITCH_STATEMENT);
    define_str_case(TS_TYPE_SWITCH_HEADER);
    define_str_case(TS_TYPE_CASE);
    define_str_case(TS_SELECT_STATEMENT);
    define_str_case(TS_COMMUNICATION_CASE);
    define_str_case(TS_EXPRESSION);
    define_str_case(TS_PARENTHESIZED_EXPRESSION);
    define_str_case(TS_CALL_EXPRESSION);
    define_str_case(TS_VARIADIC_ARGUMENT);
    define_str_case(TS_SPECIAL_ARGUMENT_LIST);
    define_str_case(TS_ARGUMENT_LIST);
    define_str_case(TS_SELECTOR_EXPRESSION);
    define_str_case(TS_INDEX_EXPRESSION);
    define_str_case(TS_SLICE_EXPRESSION);
    define_str_case(TS_TYPE_ASSERTION_EXPRESSION);
    define_str_case(TS_TYPE_CONVERSION_EXPRESSION);
    define_str_case(TS_COMPOSITE_LITERAL);
    define_str_case(TS_LITERAL_VALUE);
    define_str_case(TS_KEYED_ELEMENT);
    define_str_case(TS_ELEMENT);
    define_str_case(TS_FUNC_LITERAL);
    define_str_case(TS_UNARY_EXPRESSION);
    define_str_case(TS_BINARY_EXPRESSION);
    define_str_case(TS_QUALIFIED_TYPE);
    define_str_case(TS_INTERPRETED_STRING_LITERAL);
    define_str_case(TS_SOURCE_FILE_REPEAT1);
    define_str_case(TS_IMPORT_SPEC_LIST_REPEAT1);
    define_str_case(TS_CONST_DECLARATION_REPEAT1);
    define_str_case(TS_CONST_SPEC_REPEAT1);
    define_str_case(TS_VAR_DECLARATION_REPEAT1);
    define_str_case(TS_PARAMETER_LIST_REPEAT1);
    define_str_case(TS_TYPE_DECLARATION_REPEAT1);
    define_str_case(TS_FIELD_NAME_LIST_REPEAT1);
    define_str_case(TS_EXPRESSION_LIST_REPEAT1);
    define_str_case(TS_FIELD_DECLARATION_LIST_REPEAT1);
    define_str_case(TS_METHOD_SPEC_LIST_REPEAT1);
    define_str_case(TS_STATEMENT_LIST_REPEAT1);
    define_str_case(TS_EXPRESSION_SWITCH_STATEMENT_REPEAT1);
    define_str_case(TS_TYPE_SWITCH_STATEMENT_REPEAT1);
    define_str_case(TS_TYPE_CASE_REPEAT1);
    define_str_case(TS_SELECT_STATEMENT_REPEAT1);
    define_str_case(TS_ARGUMENT_LIST_REPEAT1);
    define_str_case(TS_LITERAL_VALUE_REPEAT1);
    define_str_case(TS_INTERPRETED_STRING_LITERAL_REPEAT1);
    define_str_case(TS_FIELD_IDENTIFIER);
    define_str_case(TS_LABEL_NAME);
    define_str_case(TS_PACKAGE_IDENTIFIER);
    define_str_case(TS_TYPE_IDENTIFIER);
    }
    return NULL;
}

struct Gomod_Parser {
    Parser_It* it;
    Gomod_Token tok;

    void parse(Gomod_Info *info) {
        ptr0(info);

#define ASSERT(x) if (!(x)) goto done
#define EXPECT(x) ASSERT((lex(), (tok.type == (x))))

        info->directives = alloc_list<Gomod_Directive>();

        auto copy_string = [&](ccstr s) { return our_strcpy(s); };

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
                        info->go_version = copy_string(tok.val);
                    EXPECT(GOMOD_TOK_NEWLINE);
                }
                break;
            case GOMOD_TOK_REQUIRE:
            case GOMOD_TOK_EXCLUDE:
                {
                    lex();

                    auto read_spec = [&]() -> bool {
                        Gomod_Directive directive = {0};

                        ASSERT(tok.type == GOMOD_TOK_STRIDENT);
                        if (keyword == GOMOD_TOK_REQUIRE) {
                            directive.type = GOMOD_DIRECTIVE_REQUIRE;
                                directive.module_path = copy_string(tok.val);
                        }

                        EXPECT(GOMOD_TOK_STRIDENT);

                        if (keyword == GOMOD_TOK_REQUIRE)
                            directive.module_version = copy_string(tok.val);

                        EXPECT(GOMOD_TOK_NEWLINE);

                        if (keyword == GOMOD_TOK_REQUIRE)
                            info->directives->append(&directive);
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
                        Gomod_Directive directive;

                        directive.type = GOMOD_DIRECTIVE_REPLACE;

                        ASSERT(tok.type == GOMOD_TOK_STRIDENT);
                        directive.module_path = copy_string(tok.val);

                        lex();
                        if (tok.type != GOMOD_TOK_ARROW) {
                            ASSERT(tok.type == GOMOD_TOK_STRIDENT);
                            directive.module_version = copy_string(tok.val);
                            EXPECT(GOMOD_TOK_ARROW);
                        }

                        EXPECT(GOMOD_TOK_STRIDENT);
                        directive.replace_path = tok.val;

                        lex();
                        if (tok.type != GOMOD_TOK_NEWLINE) {
                            ASSERT(tok.type == GOMOD_TOK_STRIDENT);
                            directive.replace_version = copy_string(tok.val);
                            EXPECT(GOMOD_TOK_NEWLINE);
                        }

                        info->directives->append(&directive);
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

    void lex() {
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
                while (it->peek() != '\n') it->next();
                lex();
                return;
            }
            break;
        }

        tok.type = GOMOD_TOK_STRIDENT;

        // get ready to read a string or identifier
        List<char> chars;
        chars.init();

        auto should_end = [&](char ch) {
            if (firstchar == '"' || firstchar == '`')
                return ch == firstchar;
            return gomod_isspace(ch) || ch == '\n';
        };

        chars.append(firstchar);

        char ch;
        switch (firstchar) {
        case '"':
            do {
                ch = it->next();
                chars.append(ch);
                if (ch == '\\')
                    chars.append(it->next());
            } while (ch == '"');
            break;
        case '`':
            do {
                ch = it->next();
                chars.append(ch);
            } while (ch != '`');
            break;
        default:
            while ((ch = it->peek()), (!gomod_isspace(ch) && ch != '\n')) {
                it->next();
                chars.append(ch);
            }
            break;
        }

        chars.append('\0');
        tok.val = chars.items;

        ccstr keywords[] = { "module", "go" ,"require", "replace", "exclude" };
        Gomod_Tok_Type types[] = { GOMOD_TOK_MODULE, GOMOD_TOK_GO, GOMOD_TOK_REQUIRE, GOMOD_TOK_REPLACE, GOMOD_TOK_EXCLUDE };

        for (u32 i = 0; i < _countof(keywords); i++) {
            if (streq(tok.val, keywords[i])) {
                tok.val = NULL;
                tok.val_truncated = false;
                tok.type = types[i];
                return;
            }
        }
    }
};

Gomod_Info *parse_gomod_file(ccstr filepath) {
    if (check_path(filepath) != CPR_FILE) {
        auto info = alloc_object(Gomod_Info);
        info->exists = false;
        return info;
    }

    auto ef = read_entire_file(filepath);
    if (ef == NULL) {
        auto info = alloc_object(Gomod_Info);
        info->exists = false;
        return info;
    }
    defer { free_entire_file(ef); };

    Parser_It it;
    it.init(ef);

    auto info = alloc_object(Gomod_Info);

    Gomod_Parser p = {0};
    p.it = &it;
    p.parse(info);

    info->path = our_strcpy(filepath);
    info->exists = true;
    info->hash = meow_hash(ef->data, ef->len);

    return info;
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
            if (is_sep(ch)) ch = PATH_SEP;
            ret[k++] = ch;
        }

        if (i + 1 < num_parts)
            ret[k++] = PATH_SEP;
    }

    ret[k] = '\0';
    return ret;
}

TSParser *new_ts_parser() {
    auto ret = ts_parser_new();
    ts_parser_set_language(ret, tree_sitter_go());
    return ret;
}

// -----
// stupid c++ template shit

template<typename T>
T *clone(T *old) {
    auto ret = alloc_object(T);
    memcpy(ret, old, sizeof(T));
    return ret;
}

template <typename T>
T* copy_object(T *old) {
    return old == NULL ? NULL : old->copy();
}

template <typename T>
List<T> *copy_list(List<T> *arr, fn<T*(T* it)> copy_func) {
    if (arr == NULL) return NULL;

    auto new_arr = alloc_object(List<T>);
    new_arr->init(LIST_POOL, max(arr->len, 1));
    For (*arr) new_arr->append(copy_func(&it));
    return new_arr;
}

template <typename T>
List<T> *copy_list(List<T> *arr) {
    auto copy_func = [&](T *it) -> T* { return copy_object(it); };
    return copy_list<T>(arr, copy_func);
}

// -----
// actual code that tells us how to copy objects

Godecl *Godecl::copy() {
    auto ret = clone(this);

    ret->file = our_strcpy(file);
    ret->name = our_strcpy(name);
    if (type == GODECL_IMPORT)
        ret->import_path = our_strcpy(import_path);
    else
        ret->gotype = copy_object(gotype);
    return ret;
}

Go_Struct_Spec *Go_Struct_Spec::copy() {
    auto ret = clone(this);
    ret->tag = our_strcpy(tag);
    if (is_embedded)
        ret->embedded_type = copy_object(embedded_type);
    else
        ret->field = copy_object(field);
    return ret;
}

Gotype *Gotype::copy() {
    auto ret = clone(this);
    switch (type) {
    case GOTYPE_ID:
        ret->id_name = our_strcpy(id_name);
        break;
    case GOTYPE_SEL:
        ret->sel_name = our_strcpy(sel_name);
        ret->sel_sel = our_strcpy(sel_sel);
        break;
    case GOTYPE_MAP:
        ret->map_key = copy_object(map_key);
        ret->map_value = copy_object(map_value);
        break;
    case GOTYPE_STRUCT:
        ret->struct_specs = copy_list(struct_specs);
        break;
    case GOTYPE_INTERFACE:
        ret->interface_specs = copy_list(interface_specs);
        break;
    case GOTYPE_POINTER: ret->pointer_base = copy_object(pointer_base); break;
    case GOTYPE_SLICE: ret->slice_base = copy_object(slice_base); break;
    case GOTYPE_ARRAY: ret->array_base = copy_object(array_base); break;
    case GOTYPE_CHAN: ret->chan_base = copy_object(chan_base); break;
    case GOTYPE_FUNC:
        ret->func_sig.params = copy_list(func_sig.params);
        ret->func_sig.result = copy_list(func_sig.result);
        ret->func_recv = copy_object(ret->func_recv);
        break;
    case GOTYPE_MULTI:
        {
            Gotype *tmp = NULL;
            auto copy_func = [&](Gotype** it) {
                tmp = copy_object(*it);
                return &tmp;
            };
            ret->multi_types = copy_list<Gotype*>(multi_types, copy_func);
        }
        break;
    case GOTYPE_VARIADIC:
        ret->variadic_base = copy_object(variadic_base);
        break;
    case GOTYPE_ASSERTION:
        ret->assertion_base = copy_object(assertion_base);
        break;
    case GOTYPE_RANGE:
        ret->range_base = copy_object(range_base);
        break;
    }
    return ret;
}

Go_Package *Go_Package::copy() {
    auto ret = clone(this);

    ret->import_path = our_strcpy(import_path);
    ret->resolved_path = our_strcpy(resolved_path);
    ret->package_name = our_strcpy(package_name);

    ret->individual_imports = copy_list(individual_imports);
    ret->dependencies = copy_list(dependencies);
    ret->decls = copy_list(decls);
    ret->files = copy_list(files);

    return ret;
}

Go_Single_Import *Go_Single_Import::copy() {
    auto ret = clone(this);
    ret->file = our_strcpy(file);
    ret->package_name = our_strcpy(package_name);
    ret->import_path = our_strcpy(import_path);
    ret->resolved_path = our_strcpy(resolved_path);
    ret->decl = copy_object(decl);
    return ret;
}

Go_Dependency *Go_Dependency::copy() {
    auto ret = clone(this);
    ret->import_path = our_strcpy(import_path);
    ret->package_name = our_strcpy(package_name);
    ret->resolved_path = our_strcpy(resolved_path);
    return ret;
}

Go_Scope_Op *Go_Scope_Op::copy() {
    auto ret = clone(this);
    if (type == GSOP_DECL)
        ret->decl = copy_object(decl);
    return ret;
}

Go_Package_File_Info *Go_Package_File_Info::copy() {
    auto ret = clone(this);
    ret->filename = our_strcpy(filename);
    ret->scope_ops = copy_list(scope_ops);
    return ret;
}

Go_Index *Go_Index::copy() {
    auto ret = clone(this);
    ret->current_path = our_strcpy(current_path);
    ret->current_import_path = our_strcpy(current_import_path);
    ret->gomod = copy_object(gomod);
    ret->packages = copy_list(packages);
    return ret;
}

Gomod_Info *Gomod_Info::copy() {
    auto ret = clone(this);
    ret->path = our_strcpy(path);
    ret->module_path = our_strcpy(module_path);
    ret->go_version = our_strcpy(go_version);
    ret->directives = copy_list(directives);
    return ret;
}

Gomod_Directive *Gomod_Directive::copy() {
    auto ret = clone(this);
    ret->module_path = our_strcpy(module_path);
    ret->module_version = our_strcpy(module_version);
    ret->replace_path = our_strcpy(replace_path);
    ret->replace_version = our_strcpy(replace_version);
    return ret;
}

// -----
// read functions

#define READ_STR(x) x = s->readstr()
#define READ_OBJ(x) x = read_object<std::remove_pointer<decltype(x)>::type>(s)
#define READ_LIST(x) x = read_list<std::remove_pointer<decltype(x)>::type::type>(s)

// ---

void Godecl::read(Index_Stream *s) {
    READ_STR(file);
    READ_STR(name);

    if (type == GODECL_IMPORT)
        READ_STR(import_path);
    else
        READ_OBJ(gotype);
}

void Go_Struct_Spec::read(Index_Stream *s) {
    READ_STR(tag);
    if (is_embedded)
        READ_OBJ(embedded_type);
    else
        READ_OBJ(field);
}

void Go_Single_Import::read(Index_Stream *s) {
    READ_STR(file);
    READ_STR(package_name);
    READ_STR(import_path);
    READ_STR(resolved_path);
    READ_OBJ(decl);
}

void Gotype::read(Index_Stream *s) {
    switch (type) {
    case GOTYPE_ID:
        READ_STR(id_name);
        break;
    case GOTYPE_SEL:
        READ_STR(sel_name);
        READ_STR(sel_sel);
        break;
    case GOTYPE_MAP:
        READ_OBJ(map_key);
        READ_OBJ(map_value);
        break;
    case GOTYPE_STRUCT:
        READ_LIST(struct_specs);
        break;
    case GOTYPE_INTERFACE:
        READ_LIST(interface_specs);
        break;
    case GOTYPE_POINTER:
        READ_OBJ(pointer_base);
        break;
    case GOTYPE_FUNC:
        READ_LIST(func_sig.params);
        READ_LIST(func_sig.result);
        READ_OBJ(func_recv);
        break;
    case GOTYPE_SLICE:
        READ_OBJ(slice_base);
        break;
    case GOTYPE_ARRAY:
        READ_OBJ(array_base);
        break;
    case GOTYPE_CHAN:
        READ_OBJ(chan_base);
        chan_direction = (Chan_Direction)s->read1();
        break;
    case GOTYPE_MULTI:
        {
            // can't use READ_LIST here because multi_types contains pointers
            // (instead of the objects themselves)
            auto len = s->read4();
            multi_types = alloc_list<Gotype*>(len);
            for (u32 i = 0; i < len; i++) {
                auto gotype = read_object<Gotype>(s);
                multi_types->append(gotype);
            }
        }
        break;
    case GOTYPE_VARIADIC:
        READ_OBJ(variadic_base);
        break;
    case GOTYPE_ASSERTION:
        READ_OBJ(assertion_base);
        break;
    case GOTYPE_RANGE:
        READ_OBJ(range_base);
        range_type = (Range_Type)s->read1();
        break;
    }
}

void Go_Dependency::read(Index_Stream *s) {
    READ_STR(import_path);
    READ_STR(resolved_path);
    READ_STR(package_name);
}

void Go_Scope_Op::read(Index_Stream *s) {
    if (type == GSOP_DECL)
        READ_OBJ(decl);
}

void Go_Package_File_Info::read(Index_Stream *s) {
    READ_STR(filename);
    READ_LIST(scope_ops);
}

void Go_Package::read(Index_Stream *s) {
    READ_STR(import_path);
    READ_STR(resolved_path);
    READ_LIST(individual_imports);
    READ_LIST(dependencies);
    READ_STR(package_name);
    READ_LIST(decls);
    READ_LIST(files);
}

void Go_Index::read(Index_Stream *s) {
    READ_STR(current_path);
    READ_STR(current_import_path);
    READ_OBJ(gomod);
    READ_LIST(packages);
}

void Gomod_Info::read(Index_Stream *s) {
    READ_STR(path);
    READ_STR(module_path);
    READ_STR(go_version);
    READ_LIST(directives);
}

void Gomod_Directive::read(Index_Stream *s) {
    READ_STR(module_path);
    READ_STR(module_version);
    READ_STR(replace_path);
    READ_STR(replace_version);
}

// ---

#define WRITE_STR(x) s->writestr(x)
#define WRITE_OBJ(x) write_object<std::remove_pointer<decltype(x)>::type>(x, s)
#define WRITE_LIST(x) write_list<decltype(x)>(x, s)

void Godecl::write(Index_Stream *s) {
    WRITE_STR(file);
    WRITE_STR(name);

    if (type == GODECL_IMPORT)
        WRITE_STR(import_path);
    else
        WRITE_OBJ(gotype);
}

void Go_Struct_Spec::write(Index_Stream *s) {
    WRITE_STR(tag);
    if (is_embedded)
        WRITE_OBJ(embedded_type);
    else
        WRITE_OBJ(field);
}

void Go_Single_Import::write(Index_Stream *s) {
    WRITE_STR(file);
    WRITE_STR(package_name);
    WRITE_STR(import_path);
    WRITE_STR(resolved_path);
    WRITE_OBJ(decl);
}

void Gotype::write(Index_Stream *s) {
    switch (type) {
    case GOTYPE_ID:
        WRITE_STR(id_name);
        break;
    case GOTYPE_SEL:
        WRITE_STR(sel_name);
        WRITE_STR(sel_sel);
        break;
    case GOTYPE_MAP:
        WRITE_OBJ(map_key);
        WRITE_OBJ(map_value);
        break;
    case GOTYPE_STRUCT:
        WRITE_LIST(struct_specs);
        break;
    case GOTYPE_INTERFACE:
        WRITE_LIST(interface_specs);
        break;
    case GOTYPE_POINTER:
        WRITE_OBJ(pointer_base);
        break;
    case GOTYPE_FUNC:
        WRITE_LIST(func_sig.params);
        WRITE_LIST(func_sig.result);
        WRITE_OBJ(func_recv);
        break;
    case GOTYPE_SLICE:
        WRITE_OBJ(slice_base);
        break;
    case GOTYPE_ARRAY:
        WRITE_OBJ(array_base);
        break;
    case GOTYPE_CHAN:
        WRITE_OBJ(chan_base);
        s->write1(chan_direction);
        break;
    case GOTYPE_MULTI:
        s->write4(multi_types->len);
        For (*multi_types) write_object<Gotype>(it, s);
        break;
    case GOTYPE_VARIADIC:
        WRITE_OBJ(variadic_base);
        break;
    case GOTYPE_ASSERTION:
        WRITE_OBJ(assertion_base);
        break;
    case GOTYPE_RANGE:
        WRITE_OBJ(range_base);
        s->write1(range_type);
        break;
    }
}

void Go_Dependency::write(Index_Stream *s) {
    WRITE_STR(import_path);
    WRITE_STR(resolved_path);
    WRITE_STR(package_name);
}

void Go_Scope_Op::write(Index_Stream *s) {
    if (type == GSOP_DECL)
        WRITE_OBJ(decl);
}

void Go_Package_File_Info::write(Index_Stream *s) {
    WRITE_STR(filename);
    WRITE_LIST(scope_ops);
}

void Go_Package::write(Index_Stream *s) {
    WRITE_STR(import_path);
    WRITE_STR(resolved_path);
    WRITE_LIST(individual_imports);
    WRITE_LIST(dependencies);
    WRITE_STR(package_name);
    WRITE_LIST(decls);
    WRITE_LIST(files);
}

void Go_Index::write(Index_Stream *s) {
    WRITE_STR(current_path);
    WRITE_STR(current_import_path);
    WRITE_OBJ(gomod);
    WRITE_LIST(packages);
}

void Gomod_Info::write(Index_Stream *s) {
    WRITE_STR(path);
    WRITE_STR(module_path);
    WRITE_STR(go_version);
    WRITE_LIST(directives);
}

void Gomod_Directive::write(Index_Stream *s) {
    WRITE_STR(module_path);
    WRITE_STR(module_version);
    WRITE_STR(replace_path);
    WRITE_STR(replace_version);
}
