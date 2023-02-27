#include "go.hpp"
#include "utils.hpp"
#include "world.hpp"
#include "mem.hpp"
#include "os.hpp"
#include "set.hpp"
#include "editor.hpp"
#include "hash64.hpp"
#include <stdlib.h>
#include "defer.hpp"
#include "unicode.hpp"
#include "enums.hpp"

#if OS_WINBLOWS
#include <windows.h>
#elif OS_MAC || OS_LINUX
#include <dlfcn.h>
#endif

#define GO_DEBUG 0

#if GO_DEBUG
#define go_print(fmt, ...) print("[go] " fmt, ##__VA_ARGS__)
#else
#define go_print(fmt, ...)
#endif

const char BUILTIN_FAKE_FILENAME[] = "this is a fake file";

void index_print(ccstr fmt, ...) {
    va_list args;
    va_start(args, fmt);

    auto msg = cp_vsprintf(fmt, args);

    {
        auto &wnd = world.wnd_index_log;

        int index = -1;
        if (wnd.len < INDEX_LOG_CAP) {
            index = wnd.len++;
        } else {
            index = wnd.start;
            wnd.start = (wnd.start + 1) % INDEX_LOG_CAP;
        }

        char *dest = wnd.buf[index];
        if (strlen(msg) > INDEX_LOG_MAXLEN - 1)
            msg = cp_sprintf("%.*s...", INDEX_LOG_MAXLEN - 1 - 3, msg);
        cp_strcpy(dest, INDEX_LOG_MAXLEN, msg);

        wnd.cmd_scroll_to_end = true;
    }

    go_print("%s", msg);
}

s32 num_index_stream_opens = 0;
s32 num_index_stream_closes = 0;

bool Index_Stream::open(ccstr _path, bool write) {
    ptr0(this);

    path = _path;
    offset = 0;
    ok = true;

    File_Mapping_Opts opts; ptr0(&opts);
    opts.write = write;
    if (write) opts.initial_size = 1024;

    fm = map_file_into_memory(path, &opts);
    return fm != NULL;
}

void Index_Stream::cleanup() {
    num_index_stream_closes++;
    fm->cleanup();
}

bool Index_Stream::writen(void *buf, int n) {
    if (offset + n > fm->len)
        if (!fm->resize(fm->len * 2))
            return false;

    memcpy(&fm->data[offset], buf, n);
    offset += n;
    return true;
}

bool Index_Stream::write1(i8 x) { return writen(&x, 1); }
bool Index_Stream::write2(i16 x) { return writen(&x, 2); }
bool Index_Stream::write4(i32 x) { return writen(&x, 4); }

bool Index_Stream::writestr(ccstr s) {
    if (!s) return write2(0);
    auto len = strlen(s);
    if (!write2(len)) return false;
    if (!writen((void*)s, len)) return false;
    return true;
}

void Index_Stream::finish_writing() {
    fm->finish_writing(offset);
}

void Index_Stream::readn(void *buf, s32 n) {
    if (offset + n > fm->len) {
        ok = false;
        return;
    }

    memcpy(buf, &fm->data[offset], n);
    offset += n;
    ok = true;
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
    if (!size) return new_array(char, 1); // empty string

    auto s = new_array(char, size + 1);
    readn(s, size);
    if (!ok) {
        frame.restore();
        return NULL;
    }

    s[size] = '\0';
    return s;
}

Go_Index *Index_Stream::read_index() {
    auto magic_number = read4();
    if (!ok) {
        go_print("unable to read magic bytes");
        return NULL;
    }

    if (magic_number != GO_INDEX_MAGIC_NUMBER) {
        go_print("magic bytes incorrect");
        ok = false;
        return NULL;
    }

    auto ver = read4();
    if (!ok) {
        go_print("unable to read index version");
        return NULL;
    }

    if (ver != GO_INDEX_VERSION) {
        go_print("index version mismatch, got %d, expected %d", ver, GO_INDEX_VERSION);
        ok = false;
        return NULL;
    }

    return read_object<Go_Index>(this);
}

void Index_Stream::write_index(Go_Index *index) {
    write4(GO_INDEX_MAGIC_NUMBER);
    write4(GO_INDEX_VERSION);
    write_object<Go_Index>(index, this);

    finish_writing();
}

void Type_Renderer::write_type(Gotype *t, Type_Renderer_Handler custom_handler, bool omit_func_keyword) {
    if (!t) return;
    if (custom_handler(this, t)) return;

#define recur(t) write_type(t, custom_handler)

    switch (t->type) {
    case GOTYPE_CONSTRAINT:
        Fori (t->constraint_terms) {
            if (i) write(" | ");
            recur(it);
        }
        break;

    case GOTYPE_CONSTRAINT_UNDERLYING:
        write("~");
        recur(t->base);
        break;

    case GOTYPE_GENERIC:
        recur(t->base);
        write("[");
        Fori (t->generic_args) {
            if (i) write(", ");
            recur(it);
        }
        write("]");
        break;

    case GOTYPE_BUILTIN:
        switch (t->builtin_type) {
        case GO_BUILTIN_COMPLEXTYPE: write("ComplexType"); break;
        case GO_BUILTIN_FLOATTYPE: write("FloatType"); break;
        case GO_BUILTIN_INTEGERTYPE: write("IntegerType"); break;
        case GO_BUILTIN_TYPE: write("Type"); break;
        case GO_BUILTIN_TYPE1: write("Type1"); break;
        case GO_BUILTIN_BOOL: write("bool"); break;
        case GO_BUILTIN_ANY: write("any"); break;
        case GO_BUILTIN_BYTE: write("byte"); break;
        case GO_BUILTIN_COMPLEX128: write("complex128"); break;
        case GO_BUILTIN_COMPLEX64: write("complex64"); break;
        case GO_BUILTIN_ERROR: write("error"); break;
        case GO_BUILTIN_FLOAT32: write("float32"); break;
        case GO_BUILTIN_FLOAT64: write("float64"); break;
        case GO_BUILTIN_INT: write("int"); break;
        case GO_BUILTIN_INT16: write("int16"); break;
        case GO_BUILTIN_INT32: write("int32"); break;
        case GO_BUILTIN_INT64: write("int64"); break;
        case GO_BUILTIN_INT8: write("int8"); break;
        case GO_BUILTIN_RUNE: write("rune"); break;
        case GO_BUILTIN_STRING: write("string"); break;
        case GO_BUILTIN_UINT: write("uint"); break;
        case GO_BUILTIN_UINT16: write("uint16"); break;
        case GO_BUILTIN_UINT32: write("uint32"); break;
        case GO_BUILTIN_UINT64: write("uint64"); break;
        case GO_BUILTIN_UINT8: write("uint8"); break;
        case GO_BUILTIN_UINTPTR: write("uintptr"); break;
        }
        break;
    case GOTYPE_ID:
        write("%s", t->id_name);
        break;
    case GOTYPE_SEL:
        write("%s.%s", t->sel_name, t->sel_sel);
        break;
    case GOTYPE_MAP:
        write("map[");
        recur(t->map_key);
        write("]");
        recur(t->map_value);
        break;
    case GOTYPE_STRUCT:
        if (!full) {
            write("struct");
            break;
        }

        if (isempty(t->struct_specs)) {
            write("struct {}");
            break;
        }

        write("struct {\n");
        For (t->struct_specs) {
            write("%s ", it.field->name);
            recur(it.field->gotype);
            write("\n");
        }
        write("}");
        break;
    case GOTYPE_INTERFACE:
        if (!full) {
            write("interface");
            break;
        }

        if (isempty(t->interface_specs)) {
            write("interface {}");
            break;
        }

        write("interface {\n");
        For (t->interface_specs) {
            // TODO: branch on it.field->field_is_embedded
            if (it.field->field_is_embedded) continue;

            write("%s ", it.field->name);
            write_type(it.field->gotype, custom_handler, true);
            write("\n");
        }
        write("}");
        break;
    case GOTYPE_POINTER:
        write("*");
        recur(t->pointer_base);
        break;
    case GOTYPE_FUNC: {
        if (!omit_func_keyword)
            write("func");

        auto write_params = [&](List<Godecl> *params, bool is_result) {
            write("(");

            u32 i = 0;
            For (params) {
                if (!is_goident_empty(it.name))
                    write("%s ", it.name);
                recur(it.gotype);
                if (i < params->len - 1)
                    write(", ");
                i++;
            }

            write(")");
        };

        auto &sig = t->func_sig;
        write_params(sig.params, false);

        auto result = sig.result;
        if (result && result->len > 0) {
            write(" ");
            if (result->len == 1 && is_goident_empty(result->at(0).name))
                recur(result->at(0).gotype);
            else
                write_params(result, true);
        }
        break;
    }
    case GOTYPE_SLICE:
        if (t->slice_is_variadic)
            write("...");
        else
            write("[]");
        recur(t->slice_base);
        break;
    case GOTYPE_ARRAY:
        write("[]");
        recur(t->array_base);
        break;
    case GOTYPE_CHAN:
        if (t->chan_direction == CHAN_RECV)
            write("<-");
        write("chan ");
        recur(t->chan_base);
        if (t->chan_direction == CHAN_SEND)
            write("<-");
        break;
    case GOTYPE_MULTI:
        write("<multi type>"); // would this ever happen
        break;
    }

#undef recur
}

void Module_Resolver::init(ccstr root_filepath, ccstr _gomodcache) {
    ptr0(this);

    mem.init();
    {
        SCOPED_MEM(&mem);
        gomodcache = cp_strdup(_gomodcache);
        root_import_to_resolved = new_object(Trie_Node);
        root_resolved_to_import = new_object(Trie_Node);
        workspace.modules = new_list(Go_Work_Module);
        workspace.flag = GWS_NO_GOWORK;
    }

    auto get_module_import_path = [&](ccstr filepath) -> ccstr {
        auto pf = parse_file(path_join(filepath, "go.mod"), LANG_GOMOD, false);
        if (!pf) return NULL;

        FOR_NODE_CHILDREN (pf->root) {
            if (it->type() != TSGM_MODULE_DIRECTIVE) continue;

            auto child = it->child();
            if (!child) continue;
            if (child->type() != TSGM_MODULE_PATH) continue;

            return child->string();
        }
        return NULL;
    };

    // Contains a mapping of import_path -> resolved_path for all modules in
    // this workspace. If this isn't a workspace, then it's a single pair with
    // the current module.
    Table<ccstr> module_lookup; module_lookup.init();

    // Module paths that we grabbed from go.work/go.mod. Ultimately, we want
    // to depend on the output of `go list` because that is "what go actually
    // sees", but we fall back on this list if for some reason `go list -m
    // all` fails.
    auto inferred_modules = new_list(Go_Work_Module);

    auto process_module_filepath = [&](ccstr filepath) -> bool {
        auto import_path = get_module_import_path(filepath);
        if (!import_path) return false;

        Go_Work_Module mod;
        mod.import_path = import_path;
        mod.resolved_path = filepath;
        inferred_modules->append(&mod);

        module_lookup.set(import_path, filepath);
        return true;
    };

    do {
        auto gowork_path = GHGetGoWork(world.current_path);
        if (!gowork_path) break;
        defer { GHFree(gowork_path); };

        auto pf = parse_file(gowork_path, LANG_GOWORK, false);
        if (!pf) break;

        if (are_filepaths_same_file(world.current_path, cp_dirname(gowork_path)))
            workspace.flag = GWS_GOWORK_AT_ROOT;
        else
            workspace.flag = GWS_GOWORK_SOMEWHERE_ELSE;

        auto work_path = cp_dirname(gowork_path);

        auto root = pf->root;
        FOR_NODE_CHILDREN (root) {
            // TODO: handle replace?
            if (it->type() != TSGW_USE_DIRECTIVE) continue;

            auto child = it;
            FOR_NODE_CHILDREN (child) {
                auto fpnode = it->child();
                if (fpnode->type() != TSGW_FILE_PATH) continue;

                auto relpath = fpnode->string();
                if (!relpath) continue;

                auto abspath = rel_to_abs_path(relpath, work_path);
                if (!abspath) continue;

                process_module_filepath(abspath);
            }
        }
    } while (0);

    if (workspace.flag == GWS_NO_GOWORK)
        if (!process_module_filepath(root_filepath))
            cp_panic("CodePerfect was unable to read your project folder as a Go module or workspace.\n\nPlease make sure this folder contains a valid go.mod or go.work file. If you're sure it does, you can run `go list -m all` in a terminal in this folder and inspect the output.");

    auto prepare_golist_proc = [&]() -> Process* {
        auto proc = new_object(Process);

        proc->init();
        proc->dir = root_filepath;
        proc->skip_shell = true;
        if (!proc->run(cp_sprintf("%s list -m all", world.go_binary_path))) return NULL;

        auto start = current_time_milli();

        while (proc->status() != PROCESS_DONE) {
            if (current_time_milli() - start > 10000)
                return NULL;
            sleep_milliseconds(100);
        }

        // should we surface these errors? even if only for debugging?
        if (proc->status() == PROCESS_ERROR)
            return NULL;

        // wait another second for pipe to be readable
        while (!proc->can_read()) {
            if (current_time_milli() - start > 1000)
                return NULL;
            sleep_milliseconds(100);
        }

        return proc;
    };

    auto proc = prepare_golist_proc();
    if (!proc || !proc->can_read())
        cp_panic("Unable to run `go list -m all` in folder.");

    bool found_module = false;
    bool done = false;

    do {
        auto line = new_list(char);
        while (true) {
            char ch = 0;
            if (!proc->read1(&ch) || ch == '\n') {
                if (ch == 0) done = true;
                break;
            }
            line->append(ch);
        }
        line->append('\0');
        // print("%s", line->items);

        ccstr import_path = NULL;
        ccstr resolved_path = NULL;

        auto parts = split_string(line->items, ' ');
        if (parts->len == 1) {
            import_path = cp_strdup(parts->at(0));
            resolved_path = module_lookup.get(import_path);
            if (!resolved_path) continue;

            {
                SCOPED_MEM(&mem);
                Go_Work_Module mod;
                mod.import_path = import_path;
                mod.resolved_path = resolved_path;
                workspace.modules->append(mod.copy());
            }
        } else if (parts->len == 2) {
            import_path = parts->at(0);
            auto version = parts->at(1);
            auto subpath = normalize_path_in_module_cache(cp_sprintf("%s@%s", import_path, version));
            resolved_path = path_join(gomodcache, subpath);
        } else if (parts->len == 4) {
            import_path = parts->at(0);
            auto new_filepath = parts->at(3);
            resolved_path = rel_to_abs_path(new_filepath);
        } else if (parts->len == 5) {
            import_path = parts->at(0);
            auto new_import_path = parts->at(3);
            auto version = parts->at(4);
            auto subpath = normalize_path_in_module_cache(cp_sprintf("%s@%s", new_import_path, version));
            resolved_path = path_join(gomodcache, subpath);
        } else {
            continue;
        }

        // go_print("%s -> %s", import_path, resolved_path);
        add_path(import_path, resolved_path);
    } while (!done);

    if (!workspace.modules->len)
        For (inferred_modules)
            workspace.modules->append(it.copy());

    world.which_workspace_mem ^= 1;
    auto mem = world.which_workspace_mem ? &world.workspace_mem_1 : &world.workspace_mem_2;
    mem->reset();
    {
        SCOPED_MEM(mem);
        auto newobj = workspace.copy();
        world.workspace = newobj;
    }
}

// -----

bool is_name_special_function(ccstr name) {
    if (strlen(name) >= 5)
        if (str_starts_with(name, "Test"))
            if (isupper(name[4]))
                return true;

    if (strlen(name) >= 8)
        if (str_starts_with(name, "Example"))
            if (isupper(name[7]))
                return true;

    if (strlen(name) >= 10)
        if (str_starts_with(name, "Benchmark"))
            if (isupper(name[9]))
                return true;

    return false;
}

bool is_name_private(ccstr name) {
    if (!isupper(name[0]))
        return true;
    if (is_name_special_function(name))
        return true;
    return false;
}

struct Parser_Input {
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
        it->set_pos(new_cur2(off, -1));
    else if (it->type == IT_BUFFER)
        it->set_pos(tspoint_to_cur(pos));

    while (!it->eof()) {
        auto uch = it->next();
        if (!uch) break;

        if (it->type == IT_MMAP) {
            if (n + 2 > bufsize) break;
            buf[n++] = (char)uch;
        } else if (it->type == IT_BUFFER) {
            auto size = uchar_size(uch);
            if (n + size + 1 > bufsize) break;
            n += uchar_to_cstr(uch, &buf[n]);
        }
    }

    *read = n;
    buf[n] = '\0';
    return buf;
}

bool isident(int c) {
    return c == '_' || uni_isalpha(c) || uni_isdigit(c);
}

Goresult *Goresult::wrap(Godecl *new_decl) { return make_goresult(new_decl, ctx); }
Goresult *Goresult::wrap(Gotype *new_gotype) { return make_goresult(new_gotype, ctx); }

// @Write
Go_File *get_ready_file_in_package(Go_Package *pkg, ccstr filename) {
    auto file = pkg->files->find([&](auto it) { return streq(filename, it->filename); });

    if (!file)
        file = pkg->files->append();
    else
        file->pool.cleanup();
    file->pool.init("file pool", 512); // tweak this

    {
        SCOPED_MEM(&file->pool);
        file->filename = cp_strdup(filename);
        file->scope_ops = new_list(Go_Scope_Op);
        file->decls = new_list(Godecl);
        file->imports = new_list(Go_Import);
        file->references = new_list(Go_Reference);
    }

    return file;
}

/*
granularize our background thread loop
either:
    - look at tree-sitter and come up with our own incremental implementation for go_file
    - or, just resign ourselves to creating a new go_file every autocomplete, param hint, jump to def, etc.
        - honestly, wouldn't this slow things down?
*/

void Go_Indexer::reload_single_file(ccstr filepath) {
    auto import_path = filepath_to_import_path(cp_dirname(filepath));
    auto pkg = find_package_in_index(import_path);
    if (!pkg) return;

    auto file = get_ready_file_in_package(pkg, cp_basename(filepath));
    if (!file) return;

    auto pf = parse_file(filepath, LANG_GO, false);
    if (!pf) return;
    defer { free_parsed_file(pf); };

    ccstr package_name = NULL;
    process_tree_into_gofile(file, pf->root, filepath, &package_name);
    if (!str_ends_with(filepath, "_test.go"))
        replace_package_name(pkg, package_name);
}

void Go_Indexer::reload_editor(void *editor) {
    auto it = (Editor*)editor;

    SCOPED_FRAME();

    Timer t;
    t.init(cp_sprintf("reload %s", cp_basename(it->filepath)), false);

    auto filename = cp_basename(it->filepath);

    auto import_path = filepath_to_import_path(cp_dirname(it->filepath));
    auto pkg = find_package_in_index(import_path);
    if (!pkg) return;

    t.log("get package");

    auto file = get_ready_file_in_package(pkg, filename);

    t.log("get file");

    auto iter = new_object(Parser_It);
    iter->init(it->buf);
    auto root_node = new_ast_node(ts_tree_root_node(it->buf->tree), iter);

    ccstr package_name = NULL;
    process_tree_into_gofile(file, root_node, it->filepath, &package_name);
    if (!str_ends_with(it->filepath, "_test.go"))
        replace_package_name(pkg, package_name);

    t.log("process tree");

    it->buf->tree_dirty = false;
}

// @Write
// Should only be called from main thread.
void Go_Indexer::reload_all_editors(bool force) {
    For (&world.panes) {
        For (&it.editors) {
            if (!it.buf->tree_dirty && !force) continue;
            reload_editor(&it);
        }
    }
}


/*
The procedure is:

    - try to read from disk
    - add entire workspace to queue
    - look through all packages, add any uncrawled imports to queue
    - start event loop
        - if there's anything in the queue, process it
        - if there have been any file changes, process them
        - either of this may add more items to the queue
        - if we've allocated > x bytes of memory from final_mem, copy index over to new pool
*/

// @Write
void Go_Indexer::replace_package_name(Go_Package *pkg, ccstr package_name) {
    if (!package_name) return;

    if (pkg->package_name)
        if (streq(pkg->package_name, package_name))
            return;

    {
        SCOPED_MEM(&final_mem);
        pkg->package_name = cp_strdup(package_name);
    }
}

void Go_Indexer::init_builtins(Go_Package *pkg) {
    pkg->package_name = "@builtin";

    ccstr fake_filename = cp_strdup(BUILTIN_FAKE_FILENAME);

    auto f = get_ready_file_in_package(pkg, fake_filename);
    f->hash = CUSTOM_HASH_BUILTINS;

    auto add_builtin = [&](Godecl_Type decl_type, Gotype_Builtin_Type type, ccstr name) -> Gotype * {
        SCOPED_MEM(&final_mem);

        auto gotype = new_gotype(GOTYPE_BUILTIN);
        gotype->builtin_type = type;

        auto decl = f->decls->append();
        decl->is_toplevel = true;
        decl->type = decl_type;
        decl->name = cp_strdup(name);
        decl->gotype = gotype;

        return gotype;
    };

    add_builtin(GODECL_TYPE, GO_BUILTIN_COMPLEXTYPE, "ComplexType");
    add_builtin(GODECL_TYPE, GO_BUILTIN_FLOATTYPE, "FloatType");
    add_builtin(GODECL_TYPE, GO_BUILTIN_INTEGERTYPE, "IntegerType");
    add_builtin(GODECL_TYPE, GO_BUILTIN_TYPE, "Type");
    add_builtin(GODECL_TYPE, GO_BUILTIN_TYPE1, "Type1");
    add_builtin(GODECL_TYPE, GO_BUILTIN_BOOL, "bool");
    add_builtin(GODECL_TYPE, GO_BUILTIN_BYTE, "byte");
    add_builtin(GODECL_TYPE, GO_BUILTIN_COMPLEX128, "complex128");
    add_builtin(GODECL_TYPE, GO_BUILTIN_COMPLEX64, "complex64");
    add_builtin(GODECL_TYPE, GO_BUILTIN_ERROR, "error");
    add_builtin(GODECL_TYPE, GO_BUILTIN_FLOAT32, "float32");
    add_builtin(GODECL_TYPE, GO_BUILTIN_FLOAT64, "float64");
    add_builtin(GODECL_TYPE, GO_BUILTIN_INT, "int");
    add_builtin(GODECL_TYPE, GO_BUILTIN_INT16, "int16");
    add_builtin(GODECL_TYPE, GO_BUILTIN_INT32, "int32");
    add_builtin(GODECL_TYPE, GO_BUILTIN_INT64, "int64");
    add_builtin(GODECL_TYPE, GO_BUILTIN_INT8, "int8");
    add_builtin(GODECL_TYPE, GO_BUILTIN_RUNE, "rune");
    add_builtin(GODECL_TYPE, GO_BUILTIN_STRING, "string");
    add_builtin(GODECL_TYPE, GO_BUILTIN_UINT, "uint");
    add_builtin(GODECL_TYPE, GO_BUILTIN_UINT16, "uint16");
    add_builtin(GODECL_TYPE, GO_BUILTIN_UINT32, "uint32");
    add_builtin(GODECL_TYPE, GO_BUILTIN_UINT64, "uint64");
    add_builtin(GODECL_TYPE, GO_BUILTIN_UINT8, "uint8");
    add_builtin(GODECL_TYPE, GO_BUILTIN_UINTPTR, "uintptr");

    auto add_builtin_value = [&](Gotype_Builtin_Type type, ccstr name) {
        SCOPED_MEM(&final_mem);

        auto decl = f->decls->append();
        decl->type = GODECL_VAR; // TODO: special godecl_type for builtin values?
        decl->is_toplevel = true;
        decl->name = cp_strdup(name);
        decl->gotype = new_gotype(GOTYPE_BUILTIN);
        decl->gotype->builtin_type = type;
    };

    add_builtin_value(GO_BUILTIN_BOOL, "true");
    add_builtin_value(GO_BUILTIN_BOOL, "false");
    add_builtin_value(GO_BUILTIN_TYPE, "nil"); // nil type?

    {
        Gotype *func_type = NULL;

        auto start = [&](Gotype *recv = NULL) {
            func_type = new_gotype(GOTYPE_FUNC);
            func_type->func_recv = recv;
            func_type->func_sig.params = new_list(Godecl);
            func_type->func_sig.result = new_list(Godecl);
        };

        auto add_param = [&](ccstr name, Gotype *gotype) {
            auto decl = func_type->func_sig.params->append();
            decl->type = GODECL_PARAM;
            decl->name = name;
            decl->gotype = gotype;
        };

        auto add_result = [&](Gotype *gotype) {
            auto decl = func_type->func_sig.result->append();
            decl->type = GODECL_PARAM;
            decl->name = NULL;
            decl->gotype = gotype;
        };

        auto save = [&](Gotype_Builtin_Type type, ccstr name) {
            auto gotype = add_builtin(GODECL_FUNC, type, name);

            SCOPED_MEM(&final_mem);
            gotype->builtin_underlying_base = func_type->copy();
        };

        auto builtin = [&](ccstr name) -> Gotype * {
            auto decl = f->decls->find([&](auto it) { return streq(it->name, name); });
            if (!decl) return NULL;
            return decl->gotype;
        };

        auto slice = [&](Gotype *base) -> Gotype * {
            auto ret = new_gotype(GOTYPE_SLICE);
            ret->slice_base = base;
            return ret;
        };

        auto variadic = [&](Gotype *base) -> Gotype * {
            auto ret = new_gotype(GOTYPE_SLICE);
            ret->slice_base = base;
            ret->slice_is_variadic = true;
            return ret;
        };

        auto chan = [&](Gotype *base, Chan_Direction direction) -> Gotype * {
            auto ret = new_gotype(GOTYPE_SLICE);
            ret->chan_base = base;
            ret->chan_direction = direction;
            return ret;
        };

        auto map = [&](Gotype *key, Gotype *value) -> Gotype * {
            auto ret = new_gotype(GOTYPE_MAP);
            ret->map_key = key;
            ret->map_value = value;
            return ret;
        };

        auto pointer = [&](Gotype *base) -> Gotype * {
            auto ret = new_gotype(GOTYPE_POINTER);
            ret->pointer_base = base;
            return ret;
        };

        auto empty_interface = [&]() -> Gotype * {
            auto ret = new_gotype(GOTYPE_INTERFACE);
            ret->interface_specs = new_list(Go_Interface_Spec, 0);
            return ret;
        };

        // "any"
        {
            SCOPED_MEM(&final_mem);

            auto gotype = add_builtin(GODECL_TYPE, GO_BUILTIN_ANY, "any");
            gotype->base = empty_interface();
        }

        // error interface
        {
            SCOPED_MEM(&final_mem);

            start();
            add_result(builtin("string"));

            auto field = new_object(Godecl);
            field->type = GODECL_PARAM;
            field->name = "Error";
            field->gotype = func_type->copy();

            auto error_interface = new_gotype(GOTYPE_INTERFACE);
            error_interface->interface_specs = new_list(Go_Interface_Spec, 1);

            auto spec = error_interface->interface_specs->append();
            spec->field = field;

            auto error_type = builtin("error");
            error_type->builtin_underlying_base = error_interface;
        }

        // func append(slice []Type, elems ...Type) []Type
        start();
        add_param("slice", slice(builtin("Type")));
        add_param("elems", variadic(builtin("Type")));
        add_result(slice(builtin("Type")));
        save(GO_BUILTIN_APPEND, "append");

        // func cap(v Type) int
        start();
        add_param("v", builtin("Type"));
        add_result(builtin("int"));
        save(GO_BUILTIN_CAP, "cap");

        // func close(c chan<- Type)
        start();
        add_param("c", chan(builtin("Type"), CHAN_SEND));
        save(GO_BUILTIN_CLOSE, "close");

        // func complex(r, i FloatType) ComplexType
        start();
        add_param("r", builtin("FloatType"));
        add_param("i", builtin("FloatType"));
        add_result(builtin("ComplexType"));
        save(GO_BUILTIN_CAP, "complex");

        // func copy(dst, src []Type) int
        start();
        add_param("dst", slice(builtin("Type")));
        add_param("src", slice(builtin("Type")));
        add_result(builtin("int"));
        save(GO_BUILTIN_COPY, "copy");

        // func delete(m map[Type]Type1, key Type)
        start();
        add_param("m", map(builtin("Type"), builtin("Type1")));
        add_param("key", builtin("Type"));
        save(GO_BUILTIN_DELETE, "delete");

        // func imag(c ComplexType) FloatType
        start();
        add_param("c", builtin("ComplexType"));
        add_result(builtin("FloatType"));
        save(GO_BUILTIN_IMAG, "imag");

        // func len(v Type) int
        start();
        add_param("v", builtin("Type"));
        add_result(builtin("int"));
        save(GO_BUILTIN_LEN, "len");

        // func make(t Type, size ...IntegerType) Type
        start();
        add_param("t", builtin("Type"));
        add_param("size", variadic(builtin("IntegerType")));
        add_result(builtin("Type"));
        save(GO_BUILTIN_MAKE, "make");

        // func new(Type) *Type
        start();
        add_param(NULL, builtin("Type"));
        add_result(pointer(builtin("Type")));
        save(GO_BUILTIN_NEW, "new");

        // func panic(v interface{})
        start();
        add_param("v", empty_interface());
        save(GO_BUILTIN_PANIC, "panic");

        // func print(args ...Type)
        start();
        add_param("args", variadic(builtin("Type")));
        save(GO_BUILTIN_PRINT, "print");

        // func println(args ...Type)
        start();
        add_param("args", variadic(builtin("Type")));
        save(GO_BUILTIN_PRINTLN, "println");

        // func real(c ComplexType) FloatType
        start();
        add_param("c", builtin("ComplexType"));
        add_result(builtin("FloatType"));
        save(GO_BUILTIN_REAL, "real");

        // func recover() interface{}
        start();
        add_result(empty_interface());
        save(GO_BUILTIN_RECOVER, "recover");
    }
}

// @Write
void Go_Indexer::background_thread() {
    // initialize stuff
    // ===

    Pool thread_mem;
    thread_mem.init("thread_mem");
    defer { thread_mem.cleanup(); };

    SCOPED_MEM(&mem);
    use_pool_for_tree_sitter = true;

    List<ccstr> package_queue;
    String_Set already_enqueued_packages;

    index_print("Looking up dependency tree.");

    {
        SCOPED_MEM(&thread_mem);
        module_resolver.init(world.current_path, gomodcache);
        package_lookup.init();
        package_queue.init();
        already_enqueued_packages.init();
    }

    // utility functions
    // ===

    auto enqueue_package = [&](ccstr import_path) {
        // TODO: periodically clean up thread_mem
        SCOPED_MEM(&thread_mem);

        if (already_enqueued_packages.has(import_path))
            return;

        package_queue.append(cp_strdup(import_path));
        already_enqueued_packages.add(import_path);
    };

    auto mark_package_for_reprocessing = [&](ccstr import_path) {
        auto pkg = find_package_in_index(import_path);
        if (pkg)
            pkg->status = GPS_OUTDATED;
        enqueue_package(import_path);
    };

    auto enqueue_imports_from_file = [&](Go_File *file) {
        if (!file->imports) return;
        For (file->imports) {
            if (!it.import_path) continue;
            if (already_enqueued_packages.has(it.import_path)) continue;

            auto pkg = find_package_in_index(it.import_path);
            if (pkg && pkg->status == GPS_READY) continue;

            enqueue_package(it.import_path);
        }
    };

    auto fill_package_hash = [&](Go_Package *pkg) -> bool {
        auto path = get_package_path(pkg->import_path);
        if (!path) return false;

        auto hash = hash_package(path);
        if (!hash) return false;

        pkg->hash = hash;
        return true;
    };

    auto is_go_package = [&](ccstr path) -> bool {
        bool ret = false;
        list_directory(path, [&](auto it) {
            if (it->type == DIRENT_FILE)
                if (str_ends_with(it->name, ".go"))
                    if (is_file_included_in_build(path_join(path, it->name))) {
                        ret = true;
                        return false;
                    }
            return true;
        });
        return ret;
    };

    auto rebuild_package_lookup = [&]() {
        package_lookup.clear();

        if (!index.packages) return;

        Fori (index.packages) {
            bool found = false;
            package_lookup.get(it.import_path, &found);
            if (found)
                print("duplicate entry detected");
            package_lookup.set(it.import_path, i);
        }
    };

    auto remove_package = [&](Go_Package *pkg) {
        start_writing();
        defer { stop_writing(); };

        pkg->cleanup();
        index.packages->remove(pkg);
        rebuild_package_lookup();
    };

    // random shit

    // try to read in index from disk
    // ===

    do {
        index_print("Reading existing database...");

        auto index_file = path_join(world.current_path, ".cpdb");

        Index_Stream s;
        if (!s.open(index_file)) {
            index_print("No database found (or couldn't open).");
            break;
        }

        defer { s.cleanup(); };

        {
            SCOPED_MEM(&final_mem);

            auto obj = s.read_index();
            if (!s.ok) {
                index_print("Unable to read database file.");
                delete_file(index_file);
                break;
            }

            memcpy(&index, obj, sizeof(Go_Index));
        }

#ifdef DEBUG_BUILD
        index_print("Successfully read database from disk, final_mem.size = %d", final_mem.mem_allocated);
#else
        index_print("Successfully read database from disk.");
#endif
    } while (0);

    // initialize index
    // ===

    auto init_index = [&](bool force_reset_index) {
        bool reset_index = force_reset_index;

        auto workspace = module_resolver.workspace.copy();

        auto workspace_changed = [&]() -> bool {
            if (!index.workspace) return true;

            if (index.workspace->flag != workspace->flag) return true;
            if (index.workspace->modules->len != workspace->modules->len) return true;

            auto hash_kinda = [&](auto &mod) {
                return cp_sprintf("%s // %s", mod.import_path, mod.resolved_path);
            };

            String_Set set; set.init();
            For (workspace->modules) set.add(hash_kinda(it));

            For (index.workspace->modules) {
                auto h = hash_kinda(it);
                if (!set.has(h)) return true;
                set.remove(h);
            }
            return (set.len > 0);
        };

        if (workspace_changed())
            reset_index = true;

        index.workspace = workspace;
        if (!index.packages || reset_index)
            index.packages = new_list(Go_Package);
    };

    init_index(false);
    rebuild_package_lookup();

    auto rescan_gomod = [&](bool force_reset_index) {
        module_resolver.cleanup();
        module_resolver.init(world.current_path, gomodcache);
        init_index(force_reset_index);
    };

    auto rescan_everything = [&]() {
        // make sure workspace is in index or queue
        // ===
        {
            SCOPED_FRAME();

            auto import_paths_queue = new_list(ccstr);
            auto resolved_paths_queue = new_list(ccstr);

            For (index.workspace->modules) {
                import_paths_queue->append(it.import_path);
                resolved_paths_queue->append(it.resolved_path);
            }

            while (import_paths_queue->len > 0) {
                auto import_path = *import_paths_queue->last();
                auto resolved_path = *resolved_paths_queue->last();

                import_paths_queue->len--;
                resolved_paths_queue->len--;

                bool already_in_index = (find_up_to_date_package(import_path));
                bool is_go_package = false;

                list_directory(resolved_path, [&](Dir_Entry *ent) {
                    do {
                        if (ent->type == DIRENT_FILE) {
                            if (!already_in_index && !is_go_package)
                                if (str_ends_with(ent->name, ".go") || streq(ent->name, "go.mod"))
                                    if (is_file_included_in_build(path_join(resolved_path, ent->name)))
                                        is_go_package = true;
                            break;
                        }

                        if (streq(ent->name, "vendor")) break;
                        if (streq(ent->name, ".git")) break;

                        import_paths_queue->append(normalize_path_sep(path_join(import_path, ent->name), '/'));
                        resolved_paths_queue->append(path_join(resolved_path, ent->name));
                    } while (0);

                    return true;
                });

                if (!already_in_index && is_go_package)
                    enqueue_package(import_path);
            }
        }

        // queue up builtins
        // ===

        if (!find_up_to_date_package("@builtin"))
            enqueue_package("@builtin");

        // if we have any ready packages, see if they have any imports that were missed
        // ===

        {
            SCOPED_FRAME();

            For (index.packages) {
                auto &pkg = it;

                if (pkg.status != GPS_READY) {
                    enqueue_package(pkg.import_path);
                    continue;
                }

                if (pkg.files)
                    For (pkg.files)
                        enqueue_imports_from_file(&it);
            }
        }

        // mark all packages for outdated hash check
        // ===
        For (index.packages) it.checked_for_outdated_hash = false;
    };

    rescan_everything(); // kick off rescan

    auto handle_fsevent = [&](ccstr filepath) {
        filepath = path_join(world.current_path, filepath);

        auto import_path = filepath_to_import_path(filepath);
        if (!import_path) return;

        switch (check_path(filepath)) {
        case CPR_DIRECTORY:
            if (is_go_package(filepath)) {
                start_writing(true);
                mark_package_for_reprocessing(import_path);
            }
            break;
        case CPR_FILE: {
            if (is_go_package(cp_dirname(filepath)) && str_ends_with(filepath, ".go")) {
                start_writing(true);
                mark_package_for_reprocessing(cp_dirname(import_path));
            }

            auto workspace_changed = [&]() {
                if (are_filepaths_equal(filepath, path_join(world.current_path, "go.work")))
                    return true;
                For (index.workspace->modules)
                    if (streq(filepath, path_join(it.resolved_path, "go.mod")))
                        return true;
                return false;
            };

            if (status == IND_READY && workspace_changed()) {
                start_writing();
                rescan_gomod(false);
                rescan_everything();
            }
            break;
        }
        case CPR_NONEXISTENT: {
            auto pkg = find_package_in_index(import_path);
            if (pkg) {
                remove_package(pkg);
                break;
            }

            pkg = find_package_in_index(cp_dirname(import_path));
            if (pkg) {
                start_writing(true);
                mark_package_for_reprocessing(pkg->import_path);
            }
            break;
        }
        }
    };

    // main loop
    // ===

    int last_final_mem_allocated = final_mem.mem_allocated;
    u64 last_write_time = 0;
    int packages_processed_since_last_write = 0;
    ccstr last_package_processed = NULL;
    u64 last_hash_check = MAX_U64;

    index_print("Entering main loop...");

    bool try_write_after_checking_hashes = false;
    for (;; sleep_milliseconds(100)) {
        // SCOPED_FRAME(); // does this work? lol

        bool try_write_this_time = false;
        bool cleanup_unused_memory_this_time = false;

        // process messages from main thread
        // ---

        auto process_message = [&](auto msg) {
            switch (msg->type) {
            case GOMSG_RESCAN_INDEX:
                if (status != IND_READY) break;
                start_writing();
                rescan_gomod(false);
                rescan_everything();
                break;

            case GOMSG_OBLITERATE_AND_RECREATE_INDEX:
                if (status != IND_READY) {
                    if (status == IND_WRITING) {
                        while (reacquires > 0)
                            stop_writing();
                    } else {
                        break;
                    }
                }

                cp_assert(status == IND_READY);

                start_writing();
                delete_file(path_join(world.current_path, ".cpdb"));
                index.cleanup();
                final_mem.reset();
                package_lookup.clear();
                rescan_gomod(true);
                rescan_everything();
                break;

            case GOMSG_CLEANUP_UNUSED_MEMORY:
                cleanup_unused_memory_this_time = true;
                break;

            case GOMSG_FSEVENT:
                handle_fsevent(msg->fsevent_filepath);
                break;
            }
        };

        {
            auto msgs = message_queue.start();
            For (msgs) process_message(&it);
            message_queue.end();
        }

        // process items in queue
        // ---

        bool queue_had_stuff = (package_queue.len > 0);

        for (int items_processed = 0; items_processed < 10 && package_queue.len > 0; items_processed++) {
            Pool scratch_mem;
            scratch_mem.init("scratch_mem");
            defer { scratch_mem.cleanup(); };
            SCOPED_MEM(&scratch_mem);

            // pop package from end of queue
            ccstr import_path = NULL;
            {
                import_path = *package_queue.last();
                package_queue.len--;
                already_enqueued_packages.remove(import_path);
            }

            auto pkg = find_package_in_index(import_path);
            if (pkg && pkg->status == GPS_READY) // already been processed
                continue;

            // we defer this, because in case we don't find any files,
            // we don't actually want to create the package
            auto create_package_if_null = [&]() {
                if (pkg) return;

                SCOPED_MEM(&final_mem);
                auto idx = index.packages->len;
                pkg = index.packages->append();
                pkg->status = GPS_OUTDATED;
                pkg->files = new_list(Go_File);
                pkg->import_path = cp_strdup(import_path);
                package_lookup.set(pkg->import_path, idx);
            };

            if (streq(import_path, "@builtin")) {
                create_package_if_null();
                pkg->status = GPS_UPDATING; // i don't think we actually need this anymore...

                init_builtins(pkg);
                fill_package_hash(pkg);

                pkg->status = GPS_READY;
                pkg->checked_for_outdated_hash = true;
                continue;
            }

            auto resolved_path = get_package_path(import_path);
            if (!resolved_path) {
                // This means this package is one of our dependencies, but it
                // has not been added to go.mod yet, so we can't resolve it.
                continue;
            }

            Timer t; t.init();

            auto source_files = list_source_files(resolved_path, true);
            if (!source_files) continue;
            if (!source_files->len) continue;

            create_package_if_null();

            pkg->status = GPS_UPDATING;
            pkg->cleanup();

            ccstr package_name = NULL;
            ccstr test_package_name = NULL;

            For (source_files) {
                auto filename = it;

                // TODO: refactor, pretty sure we're doing same thing elsewhere

                SCOPED_FRAME();

                auto filepath = path_join(resolved_path, filename);

                auto pf = parse_file(filepath, LANG_GO);
                if (!pf) continue;
                defer { free_parsed_file(pf); };

                auto file = get_ready_file_in_package(pkg, filename);

                ccstr pkgname = NULL;
                process_tree_into_gofile(file, pf->root, filepath, &pkgname);

                if (pkgname) {
                    SCOPED_MEM(&final_mem);
                    if (str_ends_with(filename, "_test.go")) {
                        if (!test_package_name)
                            test_package_name = cp_strdup(pkgname);
                    } else {
                        if (!package_name)
                            package_name = cp_strdup(pkgname);
                    }
                }

                enqueue_imports_from_file(file);
            }

            if (!package_name) package_name = test_package_name;
            replace_package_name(pkg, package_name);

            fill_package_hash(pkg);

            pkg->status = GPS_READY;
            pkg->checked_for_outdated_hash = true;

            if (!last_package_processed || !streq(import_path, last_package_processed)) {
                SCOPED_MEM(&mem);
                last_package_processed = cp_strdup(import_path);
                packages_processed_since_last_write++;
            }

            index_print("Processed %s in %dms.", import_path, t.read_total() / 1000000);
        }

        if (!package_queue.len) {
            int i = 0;
            int num_checked = 0;

            if (queue_had_stuff) {
                index_print("Scanning packages for changes...");
                try_write_after_checking_hashes = true;
            }

            auto to_remove = new_list(int);

            for (; i < index.packages->len && num_checked < 50; i++) {
                auto &it = index.packages->at(i);

                if (it.status != GPS_READY) continue;
                if (it.checked_for_outdated_hash) continue;

                num_checked++;
                it.checked_for_outdated_hash = true;

                // if this is a duplicate, remove
                bool found = false;
                auto true_copy = package_lookup.get(it.import_path, &found);
                if (!found) {
#ifdef DEBUG_BUILD
                    cp_panic("we have a package that wasn't found in package_lookup");
#endif
                } else {
                    if (true_copy != i) {
#ifdef DEBUG_BUILD
                        cp_panic("why do we have a copy?");
#endif
                        to_remove->append(i);
                        continue;
                    }
                }

                auto package_path = get_package_path(it.import_path);
                if (!package_path) continue;

                auto hash = hash_package(package_path);
                if (!hash) {
                    // path no longer exists, so remove it
                    to_remove->append(i);
                    continue;
                }
                if (it.hash == hash) continue;

                // hash changed, mark outdated & queue for re-processing
                mark_package_for_reprocessing(it.import_path);
            }
            bool done = (i == index.packages->len && !package_queue.len);

            int offset = 0;
            For (to_remove) {
                remove_package(&index.packages->at(it - offset));
                offset++;
            }

            if (done) {
                if (try_write_after_checking_hashes) {
                    index_print("Finished scanning packages.");
                    try_write_this_time = true;
                    try_write_after_checking_hashes = false;
                } else if (status == IND_WRITING) { // is this even necessary?
                    stop_writing();
                }
            }
        }

        // clean up memory if it's getting out of control
        // ---

        // clean up every 50 megabytes
        if (cleanup_unused_memory_this_time || final_mem.mem_allocated - last_final_mem_allocated > 1024 * 1024 * 50) {
            index_print("Cleaning up unused memory...");

            Pool new_pool;
            new_pool.init();

            Go_Index *new_index = NULL;

            Timer t;
            t.init();

            {
                SCOPED_MEM(&new_pool);
                new_index = index.copy();
            }

            index.cleanup();

            t.log("copy");

            {
                start_writing();
                defer { stop_writing(); };

                memcpy(&index, new_index, sizeof(index));
                final_mem.cleanup();
                memcpy(&final_mem, &new_pool, sizeof(Pool));
            }

            t.log("write");

            last_final_mem_allocated = final_mem.mem_allocated;
        }

        // write index to disk
        // ---

        auto time = current_time_nano();

        do {
            if (package_queue.len > 0) break;
            if (!try_write_this_time) break;

            defer {
                if (status == IND_WRITING)
                    stop_writing();
            };

            auto should_write = [&]() {
                if (packages_processed_since_last_write > 5)
                    return true;
                if (packages_processed_since_last_write > 0 && !last_write_time)
                    return true;
                if (last_write_time) {
                    auto ten_minutes_in_ns = (u64)10 * 60 * 1000 * 1000 * 1000;
                    if (time - last_write_time >= ten_minutes_in_ns)
                        return true;
                }
                return false;
            };

            if (!should_write()) break;

            index_print("Writing index to disk...");

            // Set last_write_time, even if the write operation itself later fails.
            // This way we're not stuck trying over and over to write.
            last_write_time = time;
            packages_processed_since_last_write = 0;
            last_package_processed = NULL;

            Timer t;
            t.init();

            {
                Index_Stream s;
                if (!s.open(path_join(world.current_path, ".cpdb.tmp"), true)) {
                    index_print("Unable to open database file for writing.");
                    break;
                }
                defer { s.cleanup(); };

                s.write_index(&index);
                s.finish_writing();
            }

            if (!move_file_atomically(path_join(world.current_path, ".cpdb.tmp"), path_join(world.current_path, ".cpdb"))) {
                index_print("Unable to commit new database file, error: %s", get_last_error());
                break;
            }

            index_print("Finished writing (took %d ms).", t.read_time() / 1000000);
        } while (0);
    }
}

bool Go_Indexer::start_background_thread() {
    SCOPED_MEM(&mem);
    auto fn = [](void* param) {
        auto indexer = (Go_Indexer*)param;
        indexer->background_thread();
    };
    return (bgthread = create_thread(fn, this));
}

TSLanguage *get_ts_language(Parse_Lang lang) {
    switch (lang) {
    case LANG_GO: return tree_sitter_go();
    case LANG_GOMOD: return tree_sitter_gomod();
    case LANG_GOWORK: return tree_sitter_gowork();
    }
    return NULL;
}

TSParser *new_ts_parser(Parse_Lang lang) {
    auto tslang = get_ts_language(lang);
    if (!tslang) return NULL;

    auto parser = ts_parser_new();
    ts_parser_set_language(parser, tslang);
    return parser;
}

Parsed_File *parse_file(ccstr filepath, Parse_Lang lang, bool use_latest) {
    Parsed_File *ret = NULL;

    if (use_latest) {
        auto editor = find_editor_by_filepath(filepath);
        if (!editor) return NULL;
        if (!editor->buf->tree) return NULL;

        auto it = new_object(Parser_It);
        it->init(editor->buf);

        ret = new_object(Parsed_File);
        ret->tree_belongs_to_editor = true;
        ret->editor_parser = editor->buf->parser;
        ret->it = it;
        ret->tree = ts_tree_copy(editor->buf->tree);
    } else {
        auto fm = map_file_into_memory(filepath);
        if (!fm) return NULL;

        auto it = new_object(Parser_It);
        it->init(fm);

        Parser_Input pinput;
        pinput.it = it;

        TSInput input;
        input.payload = &pinput;
        input.encoding = TSInputEncodingUTF8;
        input.read = read_from_parser_input;

        auto parser = new_ts_parser(lang);
        if (!parser) return NULL;

        auto tree = ts_parser_parse(parser, NULL, input);
        if (!tree) return NULL;

        ret = new_object(Parsed_File);
        ret->tree_belongs_to_editor = false;
        ret->editor_parser = NULL;
        ret->it = pinput.it;
        ret->tree = tree;
    }

    ret->root = new_ast_node(ts_tree_root_node(ret->tree), ret->it);
    return ret;
}

Ast_Node *new_ast_node(TSNode node, Parser_It *it) {
    if (ts_node_is_null(node)) return NULL;

    auto ret = new_object(Ast_Node);
    ret->init(node, it);
    return ret;
}

void Go_Indexer::free_parsed_file(Parsed_File *file) {
    if (file->it) file->it->cleanup();

    // do we even need to/can we even free tree, if we're using our custom pool memory?
    /*
    if (file->tree)
        if (!file->tree_belongs_to_editor)
            ts_tree_delete(file->tree);
    */
}

// returns -1 if pos before ast, 0 if inside, 1 if after
i32 cmp_pos_to_node(cur2 pos, Ast_Node *node, bool end_inclusive = false) {
    if (pos.y == -1) {
        if (pos.x < node->start_byte()) return -1;

        if (end_inclusive) {
            if (pos.x > node->end_byte()) return 1;
        } else {
            if (pos.x >= node->end_byte()) return 1;
        }
        return 0;
    }

    if (pos < node->start()) return -1;
    if (end_inclusive) {
        if (pos > node->end()) return 1;
    } else {
        if (pos >= node->end()) return 1;
    }
    return 0;
}

// @Functional
void walk_ast_node(Ast_Node *node, bool abstract_only, Walk_TS_Callback cb) {
    auto cursor = ts_tree_cursor_new(node->node);
    defer { ts_tree_cursor_delete(&cursor); };

    walk_ts_cursor(&cursor, abstract_only, [&](Ast_Node *it, Ts_Field_Type type, int depth) -> Walk_Action {
        it->it = node->it;
        return cb(it, type, depth);
    });
}

// @Functional
void find_nodes_containing_pos(Ast_Node *root, cur2 pos, bool abstract_only, fn<Walk_Action(Ast_Node *it)> callback, bool end_inclusive) {
    walk_ast_node(root, abstract_only, [&](Ast_Node *node, Ts_Field_Type, int) -> Walk_Action {
        int res = cmp_pos_to_node(pos, node, end_inclusive);
        if (res < 0) return WALK_ABORT;
        if (res) return WALK_SKIP_CHILDREN;

        return callback(node);
    });
}

// -----

Ast_Node *Ast_Node::dup(TSNode new_node) {
    return new_ast_node(new_node, it);
}

ccstr Ast_Node::string() {
    auto len = end_byte() - start_byte();

    if (it->type == IT_MMAP) {
        it->set_pos(new_cur2(start_byte(), -1));

        auto ret = new_array(char, len + 1);
        for (u32 i = 0; i < len; i++)
            ret[i] = it->next();
        ret[len] = '\0';
        return ret;
    }

    if (it->type == IT_BUFFER) {
        auto pos = start();
        auto buf = it->get_buf();
        pos.x = buf->idx_byte_to_cp(pos.y, pos.x);
        it->set_pos(pos);

        auto ret = new_list(char);
        while (ret->len < len) {
            char utf8[4];
            int n = uchar_to_cstr(it->next(), utf8);
            if (ret->len + n > len) break;

            for (int i = 0; i < n; i++)
                ret->append(utf8[i]);
        }
        ret->append('\0');
        return ret->items;
    }

    return NULL;
}

Gotype *new_gotype(Gotype_Type type) {
    // maybe optimize size of the object we allocate, like we did with Ast
    auto ret = new_object(Gotype);
    ret->type = type;
    return ret;
}

Gotype *new_primitive_type(ccstr name) {
    auto ret = new_gotype(GOTYPE_ID);
    ret->id_name = name;
    return ret;
}

Goresult* new_primitive_type_goresult(ccstr name) {
    auto ctx = new_object(Go_Ctx);
    ctx->import_path = "@builtin";
    ctx->filename = cp_strdup(BUILTIN_FAKE_FILENAME);

    return make_goresult(new_primitive_type(name), ctx);
}

bool isastnull(Ast_Node* x) {
    return !x; // || isastnull(x);
}

Go_Package *Go_Indexer::find_package_in_index(ccstr import_path) {
    if (!import_path) return NULL;

    bool found = false;
    auto ret = package_lookup.get(import_path, &found);
    if (!found) return NULL;

    if (ret < 0 || ret >= index.packages->len) {
        index_print("package_lookup points to an invalid package for %s", import_path);
        return NULL;
    }

    return index.packages->items + ret;
}

Go_Package *Go_Indexer::find_up_to_date_package(ccstr import_path) {
    auto pkg = find_package_in_index(import_path);
    return (pkg && pkg->status != GPS_OUTDATED) ? pkg : NULL;
}

ccstr Go_Indexer::get_import_package_name(Go_Import *it) {
    if (it->package_name_type == GPN_DOT)
        return NULL;

    if (it->package_name_type == GPN_EXPLICIT)
        if (it->package_name)
            return it->package_name;

    auto pkg = find_up_to_date_package(it->import_path);
    if (pkg) return pkg->package_name;

    return NULL;
}

ccstr Go_Indexer::find_import_path_referred_to_by_id(ccstr id, Go_Ctx *ctx) {
    auto pkg = find_up_to_date_package(ctx->import_path);
    if (!pkg) return NULL;

    For (pkg->files) {
        if (streq(it.filename, ctx->filename)) {
            For (it.imports) {
                auto package_name = get_import_package_name(&it);
                if (package_name && streq(package_name, id))
                    return it.import_path;
            }
            break;
        }
    }

    return NULL;
}

ccstr parse_go_string(ccstr s) {
    // just strips the quotes for now, handle backslashes and shit when we need

    auto len = strlen(s);
    if ((s[0] == '"' && s[len-1] == '"') || (s[0] == '`' && s[len-1] == '`')) {
        auto ret = (cstr)cp_strdup(s);
        ret[len-1] = '\0';
        return &ret[1];
    }
    return NULL;
}

void Go_Indexer::iterate_over_scope_ops(Ast_Node *root, fn<bool(Go_Scope_Op*)> cb, ccstr filename) {
    struct Open_Scope {
        int depth;
        cur2 close_pos;
    };

    List<Open_Scope> open_scopes;
    open_scopes.init();

    auto scope_ops_decls = new_list(Godecl);

    walk_ast_node(root, true, [&](Ast_Node* node, Ts_Field_Type field, int depth) -> Walk_Action {
        for (; open_scopes.len > 0; open_scopes.len--) {
            auto it = open_scopes.last();
            if (depth > it->depth) break;

            Go_Scope_Op op;
            op.type = GSOP_CLOSE_SCOPE;
            op.pos = it->close_pos; // node->start();
            if (!cb(&op)) return WALK_ABORT;
        }

        auto node_type = node->type();
        switch (node_type) {
        case TS_IF_STATEMENT:
        case TS_FOR_STATEMENT:
        case TS_EXPRESSION_SWITCH_STATEMENT:
        case TS_BLOCK:
        case TS_FUNC_LITERAL:
        case TS_TYPE_SWITCH_STATEMENT: {
            Open_Scope os;
            os.depth = depth;
            os.close_pos = node->end();
            open_scopes.append(&os);

            Go_Scope_Op op;
            op.type = GSOP_OPEN_SCOPE;
            op.pos = node->start();
            if (!cb(&op)) return WALK_ABORT;
            break;
        }

        case TS_TYPE_CASE:
        case TS_DEFAULT_CASE: {
            auto parent = node->parent();
            if (isastnull(parent)) break;
            if (parent->type() != TS_TYPE_SWITCH_STATEMENT) break;

            auto alias = parent->field(TSF_ALIAS);
            if (isastnull(alias)) break;
            if (alias->type() != TS_EXPRESSION_LIST) break;

            FOR_NODE_CHILDREN (alias) {
                if (it->type() != TS_IDENTIFIER) break;

                auto decl = new_object(Godecl);
                decl->name = cp_strdup(it->string());
                decl->type = GODECL_TYPECASE;
                decl->decl_start = node->start();
                decl->decl_end = node->start();
                decl->spec_start = node->start();
                decl->name_start = it->start();
                decl->name_end = it->end();

                Gotype *gotype = NULL;
                if (node_type == TS_TYPE_CASE && node->child_count() > 1) {
                    gotype = node_to_gotype(node->child());
                } else {
                    gotype = new_gotype(GOTYPE_INTERFACE);
                    gotype->interface_specs = new_list(Go_Interface_Spec, 0);
                }

                decl->gotype = gotype;

                Go_Scope_Op op;
                op.type = GSOP_DECL;
                op.decl = decl;
                op.decl_scope_depth = open_scopes.len;
                op.pos = decl->decl_start;
                if (!cb(&op)) return WALK_ABORT;
            }
            break;
        }

        case TS_METHOD_DECLARATION:
        case TS_FUNCTION_DECLARATION:
        case TS_TYPE_DECLARATION:
        case TS_PARAMETER_LIST:
        case TS_SHORT_VAR_DECLARATION:
        case TS_CONST_DECLARATION:
        case TS_VAR_DECLARATION:
        case TS_RANGE_CLAUSE:
        case TS_RECEIVE_STATEMENT:
        case TS_LABELED_STATEMENT:
        case TS_TYPE_PARAMETER_DECLARATION: {
            if (node_type == TS_METHOD_DECLARATION || node_type == TS_FUNCTION_DECLARATION) {
                Open_Scope os;
                os.depth = depth;
                os.close_pos = node->end();
                open_scopes.append(&os);

                Go_Scope_Op op;
                op.type = GSOP_OPEN_SCOPE;
                op.pos = node->start();
                if (!cb(&op)) return WALK_ABORT;
            }

            if (node_type == TS_RECEIVE_STATEMENT) {
                bool ok = false;
                do {
                    auto parent = node->parent();
                    if (isastnull(parent)) break;
                    if (parent->type() != TS_COMMUNICATION_CASE) break;
                    ok = true;
                } while (0);

                if (!ok) return WALK_ABORT;
            }

            scope_ops_decls->len = 0;
            node_to_decls(node, scope_ops_decls, filename);

            for (u32 i = 0; i < scope_ops_decls->len; i++) {
                Go_Scope_Op op;
                op.type = GSOP_DECL;
                op.decl = &scope_ops_decls->items[i];
                op.decl_scope_depth = open_scopes.len;
                op.pos = scope_ops_decls->items[i].decl_start;
                if (!cb(&op)) return WALK_ABORT;
            }
            break;
        }

        case TS_METHOD_SPEC:
        case TS_FIELD_DECLARATION: {
            auto process_field = [&](Godecl *field) -> bool {
                if (!field->name) return true;

                // add an open scope just for the field
                {
                    Open_Scope os;
                    os.depth = depth;
                    os.close_pos = field->name_end;
                    open_scopes.append(&os);

                    Go_Scope_Op op;
                    op.type = GSOP_OPEN_SCOPE;
                    op.pos = field->name_start;
                    if (!cb(&op)) return false;
                }

                // add the field as a decl
                {
                    Go_Scope_Op op;
                    op.type = GSOP_DECL;
                    op.decl = field;
                    op.decl_scope_depth = open_scopes.len;
                    op.pos = field->name_start;
                    if (!cb(&op)) return false;
                }

                // close the scope
                {
                    auto it = open_scopes.last();
                    Go_Scope_Op op;
                    op.type = GSOP_CLOSE_SCOPE;
                    op.pos = it->close_pos;
                    if (!cb(&op)) return false;
                    open_scopes.len--;
                }

                return true;
            };

            if (node_type == TS_METHOD_SPEC) {
                auto field = read_method_spec_into_field(node, 0);
                if (!field) break;
                if (!process_field(field)) return WALK_ABORT;
            } else {
                auto specs = read_struct_field_into_specs(node, 0, false);
                if (!specs) break;
                For (specs)
                    if (!process_field(it.field))
                        return WALK_ABORT;
            }
            break;
        }
        }

        return WALK_CONTINUE;
    });

    For (&open_scopes) {
        Go_Scope_Op op;
        op.type = GSOP_CLOSE_SCOPE;
        op.pos = root->end();
        cb(&op);
    }
}

Go_Reference *Go_Indexer::node_to_reference(Ast_Node *it) {
    Go_Reference ref; ptr0(&ref);
    bool ok = false;

    switch (it->type()) {
    case TS_IDENTIFIER:
    case TS_FIELD_IDENTIFIER:
    case TS_PACKAGE_IDENTIFIER:
    case TS_TYPE_IDENTIFIER:
        ref.is_sel = false;
        ref.start = it->start();
        ref.end = it->end();
        ref.name = it->string();
        ok = true;
        break;

    case TS_QUALIFIED_TYPE:
    case TS_SELECTOR_EXPRESSION: {
        Ast_Node *x = NULL, *sel = NULL;

        if (it->type() == TS_QUALIFIED_TYPE) {
            x = it->field(TSF_PACKAGE);
            sel = it->field(TSF_NAME); // TODO: this is wrong, look at astviewer
        } else {
            x = it->field(TSF_OPERAND);
            sel = it->field(TSF_FIELD);
        }

        auto xtype = expr_to_gotype(x);
        if (!xtype) break;

        ref.is_sel = true;
        ref.x = expr_to_gotype(x);
        ref.x_start = x->start();
        ref.x_end = x->end();
        ref.sel = sel->string();
        ref.sel_start = sel->start();
        ref.sel_end = sel->end();
        ok = true;
        break;
    }
    }

    if (!ok) return NULL;

    auto ret = new_object(Go_Reference);
    memcpy(ret, &ref, sizeof(Go_Reference));
    return ret;
}

/*
void Go_Indexer::get_preceding_comment(Ast_Node* it) {
    // TODO
}
*/

/*
 - adds decls to package->decls
 - adds scope ops to the right entry in package->files
 - adds import paths to individual_imports
*/

// @Write
void Go_Indexer::process_tree_into_gofile(
    Go_File *file,
    Ast_Node *root,
    ccstr filepath,
    ccstr *package_name,
    bool time
) {
    Timer t;
    t.init("process_tree_into_gofile");

    auto filename = cp_basename(filepath);

    if (time) t.log("get filename");

    // add decls
    // ---------

    file->decls->len = 0;

    FOR_NODE_CHILDREN (root) {
        switch (it->type()) {
        case TS_PACKAGE_CLAUSE:
            if (package_name)
                if (!*package_name)
                    *package_name = it->child()->string();
            break;
        case TS_VAR_DECLARATION:
        case TS_CONST_DECLARATION:
        case TS_FUNCTION_DECLARATION:
        case TS_METHOD_DECLARATION:
        case TS_TYPE_DECLARATION:
        // We dont handle this here right? This is a child of
        // TS_TYPE_DECLARATION.
        // case TS_TYPE_ALIAS:
        case TS_SHORT_VAR_DECLARATION:
            node_to_decls(it, file->decls, filename, &file->pool);
            break;
        }
    }

    if (time) t.log("get decls");

    // add scope_ops
    // -------------

    file->scope_ops->len = 0;

    iterate_over_scope_ops(root, [&](Go_Scope_Op *it) -> bool {
        auto op = file->scope_ops->append();
        {
            SCOPED_MEM(&file->pool);
            memcpy(op, it->copy(), sizeof(Go_Scope_Op));
        }
        return true;
    }, filename);

    if (time) t.log("get scope ops");

    // add references
    // --------------

    file->references->len = 0;
    if (path_has_descendant(world.current_path, filepath)) {
        int scope_ops_idx = 0;

        auto is_selector_sel = [&](Ast_Node *it) {
            auto parent = it->parent();
            if (isastnull(parent)) return false;

            Ast_Node *field = NULL;
            switch (parent->type()) {
            case TS_QUALIFIED_TYPE: field = parent->field(TSF_NAME); break;
            case TS_SELECTOR_EXPRESSION: field = parent->field(TSF_FIELD); break;
            default: return false;
            }

            return !isastnull(field) && field->eq(it);
        };

        walk_ast_node(root, true, [&](auto it, auto, auto) {
            Go_Reference *ref = NULL;

            switch (it->type()) {
            case TS_IDENTIFIER:
            case TS_FIELD_IDENTIFIER:
            case TS_PACKAGE_IDENTIFIER:
            case TS_TYPE_IDENTIFIER:
                if (is_selector_sel(it)) break;
                // fallthrough
            case TS_QUALIFIED_TYPE:
            case TS_SELECTOR_EXPRESSION:
                ref = node_to_reference(it);
                break;
            }

            if (ref) {
                SCOPED_MEM(&file->pool);
                file->references->append(ref->copy());
            }

            return WALK_CONTINUE;
        });

        file->references->sort([&](auto pa, auto pb) {
            auto a = pa->true_start();
            auto b = pb->true_start();
            return a.cmp(b);
        });
    }

    // add import info
    // ---------------

    file->imports->len = 0;

    bool imports_seen = false;

    FOR_NODE_CHILDREN (root) {
        auto decl_node = it;

        if (decl_node->type() != TS_IMPORT_DECLARATION) {
            if (imports_seen)
                break;
            else
                continue;
        }

        imports_seen = true;

        import_decl_to_goimports(decl_node, file->imports);

        {
            SCOPED_MEM(&file->pool);
            Fori (file->imports) memcpy(&it, it.copy(), sizeof(Go_Import));
        }
    }

    if (time) t.log("get import info");

    file->hash = hash_file(filepath);
}

void Go_Indexer::import_decl_to_goimports(Ast_Node *decl_node, List<Go_Import> *out) {
    auto process_spec = [&](Ast_Node *it) {
        Ast_Node *name_node = NULL;
        Ast_Node *path_node = NULL;

        if (it->type() == TS_IMPORT_SPEC) {
            path_node = it->field(TSF_PATH);
            name_node = it->field(TSF_NAME);
        } else if (it->type() == TS_INTERPRETED_STRING_LITERAL) {
            path_node = it;
            name_node = NULL;
        } else return;

        auto new_import_path = parse_go_string(path_node->string());
        if (!new_import_path) return;

        // decl
        auto decl = new_object(Godecl);
        decl->decl_start = decl_node->start();
        decl->decl_end = decl_node->end();
        import_spec_to_decl(it, decl);

        // import
        auto imp = out->append();
        imp->decl = decl;
        imp->import_path = new_import_path;
        if (isastnull(name_node))
            imp->package_name_type = GPN_IMPLICIT;
        else if (name_node->type() == TS_DOT)
            imp->package_name_type = GPN_DOT;
        else if (name_node->type() == TS_BLANK_IDENTIFIER)
            imp->package_name_type = GPN_BLANK;
        else if (name_node->type() == TS_PACKAGE_IDENTIFIER && streq(name_node->string(), "_"))
            imp->package_name_type = GPN_BLANK;
        else
            imp->package_name_type = GPN_EXPLICIT;

        if (imp->package_name_type == GPN_EXPLICIT)
            imp->package_name = name_node->string();
    };

    auto child = decl_node->child();
    switch (child->type()) {
    case TS_IMPORT_SPEC_LIST:
        FOR_NODE_CHILDREN (child)
            process_spec(it);
        break;
    case TS_IMPORT_SPEC:
    case TS_INTERPRETED_STRING_LITERAL:
        process_spec(child);
        break;
    }
}

void Go_Indexer::handle_error(ccstr err) {
    // TODO: What's our error handling strategy?
    error("%s", err);
}

u64 Go_Indexer::hash_file(ccstr filepath) {
    auto fm = map_file_into_memory(filepath);
    if (!fm) return 0;
    defer { fm->cleanup(); };

    u64 ret = 0;
    auto name = cp_basename(filepath);
    ret ^= hash64(fm->data, fm->len);
    ret ^= hash64((void*)name, strlen(name));
    return ret;
}

u64 Go_Indexer::hash_package(ccstr resolved_package_path) {
    if (!resolved_package_path) return 0;

    if (streq(resolved_package_path, "@builtin"))
        return CUSTOM_HASH_BUILTINS;

    u64 ret = 0;
    ret ^= hash64((void*)resolved_package_path, strlen(resolved_package_path));

    {
        SCOPED_FRAME();

        auto files = list_source_files(resolved_package_path, true);
        if (!files) return 0;
        For (files)
            ret ^= hash_file(path_join(resolved_package_path, it));
    }

    return ret;
}

bool is_file_included_in_build(ccstr path) {
    return GHBuildEnvIsFileIncluded((char*)path);
}

List<ccstr>* Go_Indexer::list_source_files(ccstr dirpath, bool include_tests) {
    auto ret = new_list(ccstr);

    auto save_gofiles = [&](Dir_Entry *ent) -> bool {
        do {
            if (ent->type == DIRENT_DIR) break;
            if (!str_ends_with(ent->name, ".go")) break;
            if (str_ends_with(ent->name, "_test.go") && !include_tests) break;

            {
                SCOPED_FRAME();
                if (!is_file_included_in_build(path_join(dirpath, ent->name))) break;
            }

            ret->append(cp_strdup(ent->name));
        } while (0);

        return true;
    };

    return list_directory(dirpath, save_gofiles) ? ret : NULL;
}

ccstr Go_Indexer::get_package_path(ccstr import_path) {
    auto ret = module_resolver.resolve_import(import_path);
    if (ret) return ret;

    auto path = path_join(goroot, import_path);
    if (check_path(path) != CPR_DIRECTORY) return NULL;

    bool is_package = false;

    list_directory(path, [&](Dir_Entry *ent) -> bool {
        if (ent->type == DIRENT_FILE)
            if (str_ends_with(ent->name, ".go")) {
                is_package = true;
                return false;
            }
        return true;
    });

    return is_package ? path : NULL;
}

Goresult *Go_Indexer::find_decl_of_id(ccstr id_to_find, cur2 id_pos, Go_Ctx *ctx, Go_Import **single_import) {
    if (!ctx) return NULL;

    auto find_in_current_file = [&]() -> Goresult* {
        auto pkg = find_up_to_date_package(ctx->import_path);
        if (!pkg) return NULL;

        auto check = [&](Go_File *it) { return streq(it->filename, ctx->filename); };
        auto file = pkg->files->find(check);
        if (!file) return NULL;

        auto scope_ops = file->scope_ops;

        SCOPED_FRAME_WITH_MEM(&scoped_table_mem);

        Scoped_Table<Godecl*> table;
        {
            SCOPED_MEM(&scoped_table_mem);
            table.init();
        }
        defer { table.cleanup(); };

        For (scope_ops) {
            if (it.pos > id_pos) break;

            bool get_out = false;
            switch (it.type) {
            case GSOP_OPEN_SCOPE:
                table.push_scope();
                break;
            case GSOP_CLOSE_SCOPE:
                table.pop_scope();
                break;
            case GSOP_DECL:
                if (!streq(it.decl->name, id_to_find)) break;
                if (it.decl->decl_start <= id_pos && id_pos < it.decl->decl_end)
                    if (it.decl_scope_depth == table.frames.len)
                        if (!(it.decl->name_start <= id_pos && id_pos < it.decl->name_end))
                            break;
                table.set(it.decl->name, it.decl);

                // ENG-133: handle the specific case where pos is on the name of a func decl
                // because if we don't it might conflict with the generic type params
                if (it.decl->type == GODECL_FUNC)
                    if (it.decl->name_start <= id_pos && id_pos < it.decl->name_end)
                        get_out = true;

                break;
            }

            if (get_out) break;
        }

        auto decl = table.get(id_to_find);
        if (decl) {
            if (decl->type != GODECL_METHOD_RECEIVER_TYPE_PARAM)
                return make_goresult(decl, ctx);

            auto res = resolve_type_to_decl(decl->base, ctx);
            if (!res) return NULL;

            if (res->decl->type != GODECL_TYPE) return NULL;

            auto type_params = res->decl->type_params;
            if (!type_params) return NULL;
            if (decl->type_param_index >= type_params->len) return NULL;

            return res->wrap(type_params->at(decl->type_param_index).copy());
        }

        For (file->imports) {
            if (it.package_name_type == GPN_DOT) {
                auto ret = find_decl_in_package(id_to_find, it.import_path);
                if (ret) return ret;
                continue;
            }

            auto package_name = get_import_package_name(&it);
            if (!package_name) continue;
            if (!streq(package_name, id_to_find)) continue;

            if (single_import) *single_import = &it;
            return make_goresult(it.decl, ctx);
        }

        return NULL;
    };

    auto ret = find_in_current_file();
    if (ret) return ret;

    ret = find_decl_in_package(id_to_find, ctx->import_path);
    if (ret) return ret;

    ret = find_decl_in_package(id_to_find, "@builtin");
    if (ret) return ret;

    return NULL;
}

ccstr Go_Indexer::get_package_referred_to_by_ast(Ast_Node *node, Go_Ctx *ctx) {
    switch (node->type()) {
    case TS_IDENTIFIER:
    case TS_FIELD_IDENTIFIER:
    case TS_PACKAGE_IDENTIFIER:
    case TS_TYPE_IDENTIFIER:
        auto import_path = find_import_path_referred_to_by_id(node->string(), ctx);
        if (import_path) return import_path;
        break;
    }
    return NULL;
}

bool Go_Indexer::is_gotype_error(Goresult *res) {
    auto gotype = res->gotype;
    if (gotype->type != GOTYPE_ID) return false;
    if (!streq(gotype->id_name, "error")) return false;

    auto declres = find_decl_of_id(gotype->id_name, gotype->id_pos, res->ctx);
    if (!declres) return false;

    auto decl = declres->decl;
    if (decl->type != GODECL_TYPE) return false;
    if (decl->gotype->type != GOTYPE_BUILTIN) return false;

    return true;
}

List<Postfix_Completion_Type> *Go_Indexer::get_postfix_completions(Ast_Node *operand_node, Go_Ctx *ctx) {
    auto import_path = get_package_referred_to_by_ast(operand_node, ctx);
    if (import_path) {
        // TODO: do something special with packages
        return NULL;
    }

    auto ret = new_list(Postfix_Completion_Type);

    auto try_based_on_gotype = [&]() -> bool {
        auto gotype = expr_to_gotype(operand_node);
        if (!gotype) return false;

        auto res = evaluate_type(gotype, ctx);
        if (!res) return false;

        // If res->gotype is an ID called "error" and resolves to the error in
        // @builtin, then we want to add PFC_CHECK.
        if (is_gotype_error(res))
            ret->append(PFC_CHECK);

        auto rres = resolve_type(res);
        if (!rres) return false;

        auto basetype = rres->gotype;

        if (basetype->type == GOTYPE_MULTI)
            if (basetype->multi_types->len == 1)
                basetype = basetype->multi_types->at(0);

        switch (basetype->type) {
        case GOTYPE_SLICE:
        case GOTYPE_ARRAY:
        case GOTYPE_MAP:
            switch (operand_node->type()) {
            case TS_PACKAGE_IDENTIFIER:
            case TS_TYPE_IDENTIFIER:
            case TS_IDENTIFIER:
            case TS_FIELD_IDENTIFIER:
            case TS_QUALIFIED_TYPE:
            case TS_SELECTOR_EXPRESSION:
                ret->append(PFC_ASSIGNAPPEND);
                break;
            }
            ret->append(PFC_APPEND);
            ret->append(PFC_LEN);
            ret->append(PFC_CAP);
            ret->append(PFC_FOR);
            ret->append(PFC_FORKEY);
            ret->append(PFC_FORVALUE);
            ret->append(PFC_EMPTY);
            ret->append(PFC_NOTEMPTY);
            ret->append(PFC_IFNIL);
            ret->append(PFC_IFNOTNIL);
            ret->append(PFC_IFEMPTY);
            ret->append(PFC_IFNOTEMPTY);
            ret->append(PFC_NIL);
            ret->append(PFC_NOTNIL);
            return true;

        case GOTYPE_STRUCT:
        case GOTYPE_INTERFACE:
            ret->append(PFC_IFNIL);
            ret->append(PFC_IFNOTNIL);
            ret->append(PFC_NIL);
            ret->append(PFC_NOTNIL);
            return true;

        case GOTYPE_MULTI:
            if (operand_node->type() == TS_CALL_EXPRESSION) {
                For (basetype->multi_types) {
                    if (it->type == GOTYPE_ID && streq(it->id_name, "error")) {
                        ret->append(PFC_CHECK);
                        return true;
                    }
                }
            }
            break; // don't return true
        }

        return false;
    };

    auto try_based_on_identifier = [&]() {
        switch (operand_node->type()) {
        case TS_PACKAGE_IDENTIFIER:
        case TS_TYPE_IDENTIFIER:
        case TS_IDENTIFIER:
        case TS_FIELD_IDENTIFIER:
            break;
        default:
            return false;
        }

        auto id = operand_node->string();

        // add everything (until we find a reason not to)
        ret->append(PFC_ASSIGNAPPEND);
        ret->append(PFC_APPEND);
        ret->append(PFC_LEN);
        ret->append(PFC_CAP);
        ret->append(PFC_FOR);
        ret->append(PFC_FORKEY);
        ret->append(PFC_FORVALUE);
        ret->append(PFC_NIL);
        ret->append(PFC_NOTNIL);
        ret->append(PFC_NOT);
        ret->append(PFC_EMPTY);
        ret->append(PFC_NOTEMPTY);
        ret->append(PFC_IFEMPTY);
        ret->append(PFC_IFNOTEMPTY);
        ret->append(PFC_IF);
        ret->append(PFC_IFNOT);
        ret->append(PFC_IFNIL);
        ret->append(PFC_IFNOTNIL);
        ret->append(PFC_CHECK);
        ret->append(PFC_DEFSTRUCT);
        ret->append(PFC_DEFINTERFACE);
        ret->append(PFC_SWITCH);
        return true;
    };

    if (try_based_on_gotype()) return ret;
    if (try_based_on_identifier()) return ret;

    ret->append(PFC_LEN);
    ret->append(PFC_CAP);
    ret->append(PFC_NIL);
    ret->append(PFC_NOTNIL);
    ret->append(PFC_NOT);
    ret->append(PFC_IF);
    ret->append(PFC_IFNOT);
    ret->append(PFC_IFNIL);
    ret->append(PFC_IFNOTNIL);
    ret->append(PFC_SWITCH);
    return ret;
}

List<Goresult> *Go_Indexer::list_lazy_type_dotprops(Gotype *type, Go_Ctx *ctx) {
    auto res = evaluate_type(type, ctx);
    if (!res) return NULL;

    auto rres = resolve_type(res);
    if (!rres) return NULL;

    rres = subst_generic_type(rres);
    if (!rres) return NULL;

    rres = remove_override_ctx(rres->gotype, rres->ctx);

    auto tmp = new_list(Goresult);
    list_dotprops(res, rres, tmp);

    auto results = new_list(Goresult);
    For (tmp) {
        if (!streq(it.ctx->import_path, ctx->import_path))
            if (!isupper(it.decl->name[0]))
                continue;
        results->append(&it);
    }
    return results;
}

List<Goresult> *Go_Indexer::get_node_dotprops(Ast_Node *operand_node, bool *was_package, Go_Ctx *ctx) {
    // try as type
    do {
        auto gotype = expr_to_gotype(operand_node);
        if (!gotype) break;

        auto ret = list_lazy_type_dotprops(gotype, ctx);
        if (!ret) break;

        *was_package = false;
        return ret;
    } while (0);

    // try as import
    do {
        auto import_path = get_package_referred_to_by_ast(operand_node, ctx);
        if (!import_path) break;

        auto ret = list_package_decls(import_path, LISTDECLS_PUBLIC_ONLY | LISTDECLS_EXCLUDE_METHODS);
        if (!ret) break;

        *was_package = true;
        return ret;
    } while (0);

    // shit out of luck
    return NULL;
}

bool Go_Indexer::are_ctxs_equal(Go_Ctx *a, Go_Ctx *b) {
    if (!a || !b) return false;
    if (!streq(a->import_path, b->import_path)) return false;
    if (!streq(a->filename, b->filename)) return false;

    return true;
}

bool Go_Indexer::are_decls_equal(Goresult *adecl, Goresult *bdecl) {
    if (!adecl) return false;
    if (!bdecl) return false;
    if (!are_ctxs_equal(adecl->ctx, bdecl->ctx)) return false;

    return adecl->decl->name_start == bdecl->decl->name_start;
}

bool Go_Indexer::are_gotypes_equal(Goresult *ra, Goresult *rb) {
    auto resolve_aliased_type = [&](Goresult *res) {
        return res; // TODO
    };

    ra = resolve_aliased_type(ra);
    rb = resolve_aliased_type(rb);

    auto a = ra->gotype;
    auto b = rb->gotype;

    // a and b must be resolved; no lazy types
    if (a->type > _GOTYPE_LAZY_MARKER_) return false;
    if (b->type > _GOTYPE_LAZY_MARKER_) return false;

    if (a->type != b->type) {
        auto a_is_ref = is_type_ident(a);
        auto b_is_ref = is_type_ident(b);

        if (a_is_ref && b_is_ref) {
            auto resa = resolve_type_to_decl(a, ra->ctx);
            auto resb = resolve_type_to_decl(b, rb->ctx);
            return are_decls_equal(resa, resb);
        }

        Goresult *ref = NULL, *other = NULL;
        if (a_is_ref) {
            ref = ra;
            other = rb;
        } else {
            ref = rb;
            other = ra;
        }

        auto rres = resolve_type(ref);
        if (!rres) return false;

        return rres->gotype->type == other->gotype->type
            && are_gotypes_equal(rres, other);
    }

    // a->type == b->type

    switch (a->type) {
    case GOTYPE_INTERFACE: {
        auto validate_methods = [&](List<Goresult> *meths) {
            if (!meths) return false;

            SCOPED_FRAME();
            String_Set seen; seen.init();
            For (meths) {
                auto name = it.decl->name;
                if (seen.has(name))
                    return false;
                seen.add(name);
            }
            return true;
        };

        auto ameths = list_interface_methods(ra);
        if (!validate_methods(ameths)) return false;

        auto bmeths = list_interface_methods(rb);
        if (!validate_methods(bmeths)) return false;

        if (ameths->len != bmeths->len) return false;

        for (auto &&bmeth : *bmeths) {
            bool found = false;
            for (auto &&ameth : *ameths) {
                auto adecl = ameth.decl;
                auto bdecl = ameth.decl;

                if (!streq(adecl->name, bdecl->name)) continue;

                auto atype = ameth.wrap(adecl->gotype);
                auto btype = bmeth.wrap(bdecl->gotype);
                if (!are_gotypes_equal(atype, btype)) continue;

                found = true;
                break;
            }
            if (!found) return false;
        }
        return true;
    }

    case GOTYPE_ID: {
        auto adecl = find_decl_of_id(a->id_name, a->id_pos, ra->ctx);
        auto bdecl = find_decl_of_id(b->id_name, b->id_pos, rb->ctx);
        return are_decls_equal(adecl, bdecl);
    }

    case GOTYPE_SEL: {
        auto aimp = find_import_path_referred_to_by_id(a->sel_name, ra->ctx);
        auto bimp = find_import_path_referred_to_by_id(b->sel_name, rb->ctx);
        if (!aimp || !bimp) return false;
        if (!streq(aimp, bimp)) return false;
        if (!streq(a->sel_sel, b->sel_sel)) return false;
        return true;
    }

    case GOTYPE_STRUCT: {
        auto sa = a->struct_specs;
        auto sb = a->struct_specs;
        if (sa->len != sb->len) return false;

        Fori (sa) {
            auto &ita = it;
            auto &itb = sb->at(i);

            if (ita.tag || itb.tag) {
                if (!ita.tag || !itb.tag) return false;
                if (!streq(ita.tag, itb.tag)) return false;
            }

            if (ita.field->field_is_embedded != itb.field->field_is_embedded) return false;
            if (!streq(ita.field->name, itb.field->name)) return false;

            if (!are_gotypes_equal(ra->wrap(ita.field->gotype), rb->wrap(itb.field->gotype)))
                return false;
        }

        return true;
    }

    case GOTYPE_MAP: {
        auto akey = ra->wrap(a->map_key);
        auto aval = ra->wrap(a->map_value);
        auto bkey = rb->wrap(b->map_key);
        auto bval = rb->wrap(b->map_value);
        return are_gotypes_equal(akey, bkey) && are_gotypes_equal(aval, bval);
    }

    case GOTYPE_FUNC: {
        auto &fa = a->func_sig;
        auto &fb = b->func_sig;

        auto mismatch = [&](List<Godecl> *aarr, List<Godecl> *barr) -> bool {
            if (isempty(fa.params) != isempty(fb.params))
                return true;
            if (fa.params)
                if (fa.params->len != fb.params->len)
                    return true;
            return false;
        };

        if (mismatch(fa.params, fb.params)) return false;

        if (!isempty(fa.params)) {
            Fori (fa.params) {
                auto ga = ra->wrap(it.gotype);
                auto gb = rb->wrap(fb.params->at(i).gotype);
                if (!are_gotypes_equal(ga, gb)) return false;
            }
        }

        if (mismatch(fa.result, fb.result)) return false;

        if (!isempty(fa.result)) {
            Fori (fa.result) {
                auto ga = ra->wrap(it.gotype);
                auto gb = rb->wrap(fb.result->at(i).gotype);
                if (!are_gotypes_equal(ga, gb)) return false;
            }
        }

        return true;
    }

    case GOTYPE_POINTER:
    case GOTYPE_ASSERTION:
    case GOTYPE_RECEIVE:
    case GOTYPE_SLICE:
    case GOTYPE_ARRAY:
    case GOTYPE_CHAN:
    case GOTYPE_RANGE: {
        auto abase = ra->wrap(a->base);
        auto bbase = rb->wrap(b->base);
        return are_gotypes_equal(abase, bbase);
    }

    case GOTYPE_MULTI:
        return false; // this isn't a real go type, it's for our purposes

    case GOTYPE_BUILTIN:
        return a->builtin_type == b->builtin_type;
    }
    return false;
}

List<Find_Decl> *Go_Indexer::find_interfaces(Goresult *target, bool search_everywhere) {
    reload_all_editors();

    if (!target->decl) return NULL;
    if (target->decl->type != GODECL_TYPE) return NULL;
    if (target->decl->gotype->type == GOTYPE_INTERFACE) return NULL;

    auto methods = new_list(Goresult);
    if (!list_type_methods(target->decl->name, target->ctx->import_path, methods))
        return NULL;

    For (methods) {
        auto filepath = ctx_to_filepath(it.ctx);
        auto decl = it.decl;

        print("%s %s %s", filepath, decl->decl_start.str(), decl->name);
    }

    auto ret = new_list(Find_Decl);

    For (index.packages) {
        if (it.status != GPS_READY) continue;

        auto import_path = it.import_path;
        auto package_name = it.package_name;

        if (!search_everywhere)
            if (!index_has_module_containing(import_path))
                continue;

        For (it.files) {
            auto ctx = new_object(Go_Ctx);
            ctx->import_path = import_path;
            ctx->filename = it.filename;

            auto filepath = ctx_to_filepath(ctx);

            For (it.decls) {
                if (it.type != GODECL_TYPE) continue;

                auto gotype = it.gotype;
                if (gotype->type != GOTYPE_INTERFACE) continue;

                // TODO: validate methods

                auto match = [&]() {
                    auto imethods = list_interface_methods(make_goresult(gotype, ctx));
                    if (!imethods) return false;

                    // test that all(<methods contains i> for i in imethods)
                    For (imethods) {
                        auto imeth_name = it.decl->name;
                        if (!imeth_name) return false;

                        auto imeth = make_goresult(it.decl->gotype, ctx);
                        bool found = false;

                        For (methods) {
                            if (!it.decl->name) continue;
                            if (!streq(imeth_name, it.decl->name)) continue;

                            auto gotype_res = target->wrap(it.decl->gotype);
                            if (!are_gotypes_equal(imeth, gotype_res)) continue;

                            found = true;
                            break;
                        }

                        if (!found) return false;
                    }

                    return true;
                };

                if (!match()) continue;

                Find_Decl result; ptr0(&result);
                result.filepath = filepath;
                result.decl = make_goresult(&it, ctx);
                result.package_name = package_name;
                ret->append(&result);
            }
        }
    }

    return ret;
}

List<Find_Decl> *Go_Indexer::find_implementations(Goresult *target, bool search_everywhere) {
    reload_all_editors();

    if (!target->decl) return NULL;
    if (target->decl->type != GODECL_TYPE) return NULL;
    if (target->decl->gotype->type != GOTYPE_INTERFACE) return NULL;

    auto ctx = target->ctx;

    // ctx for these is target->ctx
    auto methods = new_list(Goresult);
    if (!list_interface_methods(target->wrap(target->decl->gotype), methods))
        return NULL;
    // For (target->decl->gotype->interface_specs) methods->append(it.field);

    struct Type_Info {
        bool *methods_matched;
        Goresult *decl;
        ccstr package_name;
    };

    Table<Type_Info*> huge_table;
    huge_table.init();

    auto get_type_info = [&](ccstr key) -> Type_Info * {
        bool found = false;
        auto ret = huge_table.get(key, &found);
        if (!found) {
            ret = new_object(Type_Info);
            ret->methods_matched = new_array(bool, methods->len);
            huge_table.set(key, ret);
        }
        return ret;
    };

    For (index.packages) {
        if (it.status != GPS_READY) continue;

        auto import_path = it.import_path;
        auto package_name = it.package_name;

        if (!search_everywhere)
            if (!index_has_module_containing(import_path))
                continue;

        For (it.files) {
            auto ctx = new_object(Go_Ctx);
            ctx->import_path = import_path;
            ctx->filename = it.filename;

            For (it.decls) {
                if (it.type != GODECL_FUNC && it.type != GODECL_TYPE) continue;

                if (it.type == GODECL_TYPE) {
                    auto type_name = cp_sprintf("%s:%s", import_path, it.name);
                    auto type_info = get_type_info(type_name);
                    type_info->decl = make_goresult(&it, ctx);
                    type_info->package_name = package_name;
                    continue;
                }

                auto gotype = it.gotype;
                if (gotype->type != GOTYPE_FUNC) continue;
                if (!gotype->func_recv) continue;

                auto recv = gotype->func_recv;
                if (recv->type == GOTYPE_POINTER)
                    recv = recv->pointer_base;

                if (recv->type != GOTYPE_ID) continue;

                auto type_name = cp_sprintf("%s:%s", import_path, recv->id_name);
                auto method_name = it.name;

                Fori (methods) {
                    if (!streq(it.decl->name, method_name)) continue;
                    if (!are_gotypes_equal(it.wrap(it.decl->gotype), make_goresult(gotype, ctx)))
                        continue; // break here

                    auto type_info = get_type_info(type_name);
                    type_info->methods_matched[i] = true;

                    bool found = false;
                    auto info = huge_table.get(type_name, &found);
                    if (!found) {
                        info = new_object(Type_Info);
                        huge_table.set(type_name, info);
                    }

                    info->methods_matched[i] = true;
                    break;
                }
            }
        }
    }

    auto ret = new_list(Find_Decl);

    auto entries = huge_table.entries();

    Fori (entries) {
        auto info = it->value;

        auto match = [&]() {
            for (int i = 0; i < methods->len; i++)
                if (!info->methods_matched[i])
                    return false;
            return true;
        };

        if (!match()) continue;

        auto parts = split_string(it->name, ':');
        if (parts->len != 2) continue;

        auto import_path = parts->at(0);
        auto type_name = parts->at(1);

        Find_Decl result; ptr0(&result);
        result.filepath = ctx_to_filepath(info->decl->ctx);
        result.decl = info->decl;
        result.package_name = info->package_name;
        ret->append(&result);
    }

    return ret;
}

ccstr Go_Indexer::get_godecl_recvname(Godecl *it) {
    if (it->type != GODECL_FUNC) return NULL;
    if (!it->gotype) return NULL;
    if (it->gotype->type != GOTYPE_FUNC) return NULL;

    auto recv = it->gotype->func_recv;
    if (!recv) return NULL;

    recv = unpointer_type(recv);
    if (!recv) return NULL;
    if (recv->type != GOTYPE_ID) return NULL;

    return recv->id_name;
}

Jump_To_Definition_Result* Go_Indexer::jump_to_symbol(ccstr symbol) {
    reload_all_editors();

    char* pkgname = NULL;
    ccstr rest = NULL;

    {
        auto dot = strchr(symbol, '.');
        rest = dot+1;

        auto len = dot - symbol;
        pkgname = new_array(char, len+1);
        strncpy(pkgname, symbol, len);
        pkgname[len] = '\0';
    }

    For (index.packages) {
        if (it.status != GPS_READY) continue;

        if (!index.workspace->find_module_containing(it.import_path)) continue;

        if (!streq(it.package_name, pkgname)) continue;

        auto import_path = it.import_path;
        For (it.files) {
            auto filename = it.filename;
            For (it.decls) {
                auto key = it.name;
                auto recvname = get_godecl_recvname(&it);
                if (recvname)
                    key = cp_sprintf("%s.%s", recvname, it.name);

                if (!streq(key, rest)) continue;

                Go_Ctx ctx; ptr0(&ctx);
                ctx.filename = filename;
                ctx.import_path = import_path;
                auto filepath = ctx_to_filepath(&ctx);

                auto ret = new_object(Jump_To_Definition_Result);
                // TODO: fill out decl if we need it, i guess
                ret->file = filepath;
                ret->pos = it.name_start;
                return ret;
            }
        }
    }

    return NULL;
}

List<Find_References_File> *Go_Indexer::find_references(ccstr filepath, cur2 pos, bool include_self) {
    auto result = jump_to_definition(filepath, pos);
    if (!result) return NULL;
    return find_references(result->decl, include_self);
}

// TODO: maybe we should have the caller call reload_all_editors(), wrapped
// in a function like init_indexer_session() or something
List<Call_Hier_Node>* Go_Indexer::generate_caller_hierarchy(Goresult *declres) {
    reload_all_editors();

    auto ret = new_list(Call_Hier_Node);
    actually_generate_caller_hierarchy(declres, ret);
    return ret;
}

Godecl *Go_Indexer::find_toplevel_containing(Go_File *file, cur2 start, cur2 end) {
    int lo = 0, hi = file->decls->len;
    while (lo <= hi) {
        auto mid = (lo+hi)/2;
        auto it = file->decls->items + mid;

        if (start < it->decl_start)
            hi = mid-1;
        else if (end > it->decl_end)
            lo = mid+1;
        else
            return it;
    }
    return NULL;
}

void Go_Indexer::actually_generate_caller_hierarchy(Goresult *declres, List<Call_Hier_Node> *out) {
    auto ref_files = actually_find_references(declres, false);
    if (!ref_files) return;
    // if (!ref_files->len) return;

    For (ref_files) {
        auto filepath = it.filepath;
        auto ctx = filepath_to_ctx(filepath);
        Go_Package *pkg = NULL;
        auto file = find_gofile_from_ctx(ctx, &pkg);

        if (!file || !pkg) continue;

        For (it.results) {
            auto &ref = it.reference;

            // get the bounds of the reference
            cur2 start, end;
            if (ref->is_sel) {
                start = ref->x_start;
                end = ref->sel_end;
            } else {
                start = ref->start;
                end = ref->end;
            }

            auto enclosing_decl = find_toplevel_containing(file, start, end);
            if (!enclosing_decl)
                continue;

            auto declres = make_goresult(enclosing_decl, ctx);

            auto fd = new_object(Find_Decl);
            fd->filepath = filepath;
            fd->decl = declres;
            fd->package_name = pkg->package_name;

            Call_Hier_Node node;
            node.decl = fd;
            node.children = new_list(Call_Hier_Node);
            node.ref = it.reference;
            out->append(&node);

            if (enclosing_decl->gotype)
                if (enclosing_decl->gotype->type == GOTYPE_FUNC)
                    actually_generate_caller_hierarchy(declres, node.children);
        }
    }
}

List<Call_Hier_Node>* Go_Indexer::generate_callee_hierarchy(Goresult *declres) {
    reload_all_editors();

    auto ret = new_list(Call_Hier_Node);
    actually_generate_callee_hierarchy(declres, ret);
    return ret;
}

void Go_Indexer::actually_generate_callee_hierarchy(Goresult *declres, List<Call_Hier_Node> *out, List<Seen_Callee_Entry> *seen) {
    Go_Package *pkg = NULL;
    auto file = find_gofile_from_ctx(declres->ctx, &pkg);
    if (!file) return;

    // auto ctx = declres->ctx;
    auto decl = declres->decl;

    int start_index = -1;
    {
        auto refs = file->references;
        int lo = 0, hi = refs->len-1;
        while (lo < hi) {
            auto mid = (lo+hi)/2;
            auto &it = file->references->at(mid);
            if (it.true_start() < decl->decl_start)
                lo = mid+1;
            else
                hi = mid-1;
        }
        start_index = lo;
    }

    if (!seen)
        seen = new_list(Seen_Callee_Entry);

    auto find_seen = [&](Goresult *res) -> Call_Hier_Node* {
        For (seen) {
            auto a = it.declres;
            auto b = res;

            if (!streq(a->ctx->import_path, b->ctx->import_path)) continue;
            if (!streq(a->ctx->filename, b->ctx->filename)) continue;
            if (a->decl->name_start != b->decl->name_start) continue;
            if (!streq(a->decl->name, b->decl->name)) continue;

            return &it.node;
        }
        return NULL;
    };

    for (int i = start_index; i < file->references->len; i++) {
        auto &it = file->references->at(i);
        if (it.true_start() >= decl->decl_end)
            break;

        if (!it.is_sel && it.start == decl->name_start) continue;

        auto res = get_reference_decl(&it, declres->ctx);
        if (!res) continue;
        if (!index_has_module_containing(res->ctx->import_path)) continue;

        auto decl = res->decl;
        auto gotype = decl->gotype;
        if (!gotype) continue;
        if (gotype->type != GOTYPE_FUNC) continue;

        auto existing = find_seen(res);
        if (existing) {
            out->append(existing);
            continue;
        }

        auto pkg = find_up_to_date_package(res->ctx->import_path);
        if (!pkg) continue;

        auto fd = new_object(Find_Decl);
        fd->filepath = ctx_to_filepath(res->ctx);
        fd->decl = res;
        fd->package_name = pkg->package_name;

        Call_Hier_Node node; ptr0(&node);
        node.decl = fd;
        node.children = new_list(Call_Hier_Node);
        node.ref = &it;
        out->append(&node);

        Seen_Callee_Entry se;
        se.declres = res;
        memcpy(&se.node, &node, sizeof(Call_Hier_Node));
        seen->append(&se);

        actually_generate_callee_hierarchy(res, node.children, seen);
    }
}

List<Find_References_File> *Go_Indexer::find_references(Goresult *declres, bool include_self) {
    reload_all_editors();
    return actually_find_references(declres, include_self);
}

Goresult *Go_Indexer::get_reference_decl(Go_Reference *ref, Go_Ctx *ctx) {
    if (!ref->is_sel)
        return find_decl_of_id(ref->name, ref->start, ctx);

    auto results = list_lazy_type_dotprops(ref->x, ctx);
    if (!results) {
        auto import_path = find_import_path_referred_to_by_id(ref->x->lazy_id_name, ctx);
        if (!import_path) return NULL;

        results = list_package_decls(import_path, LISTDECLS_PUBLIC_ONLY | LISTDECLS_EXCLUDE_METHODS);
        if (!results) return NULL;
    }

    For (results)
        if (streq(it.decl->name, ref->sel))
            return &it;
    return NULL;
}

List<Find_References_File> *Go_Indexer::actually_find_references(Goresult *declres, bool include_self) {
    auto decl = declres->decl;
    if (!decl) return NULL;
    if (decl->type == GODECL_IMPORT) return NULL;

    auto decl_name = decl->name;
    if (!decl_name) return NULL;

    auto ctx = declres->ctx;
    if (!ctx) return NULL;

    bool is_private = islower(decl_name[0]);

    enum Case_Type {
        CASE_NORMAL,
        CASE_FIELD,
        CASE_METHOD,
    };

    Case_Type case_type;
    switch (decl->type) {
    case GODECL_VAR:
    case GODECL_CONST:
    case GODECL_TYPE:
    case GODECL_PARAM:
    case GODECL_SHORTVAR:
    case GODECL_TYPECASE:
        case_type = CASE_NORMAL;
        break;

    case GODECL_FUNC:
        if (!decl->gotype->func_recv)
            case_type = CASE_NORMAL;
        else
            case_type = CASE_METHOD;
        break;

    case GODECL_FIELD:
        case_type = CASE_FIELD;
        break;
    }

    auto ret = new_list(Find_References_File);

    auto is_match = [&](Goresult *r) {
        if (!streq(r->ctx->import_path, declres->ctx->import_path)) return false;
        if (!streq(r->ctx->filename, declres->ctx->filename)) return false;

        auto a = r->decl;
        auto b = decl;

        if (!a || !b) return false;
        if (a->name_start != b->name_start) return false;
        if (!streq(a->name, b->name)) return false;

        return true;
    };

    auto process = [&](Go_Package *pkg, Go_File *file) {
        Go_Ctx ctx2;
        ctx2.import_path = pkg->import_path;
        ctx2.filename = file->filename;

        bool same_file_as_decl = false;
        if (streq(ctx2.import_path, ctx->import_path) && streq(ctx2.filename, ctx->filename))
            same_file_as_decl = true;

        auto results = new_list(Find_References_Result);

        bool same_package = streq(pkg->import_path, ctx->import_path);

        ccstr package_name_we_want = NULL;
        bool package_is_dot = false;

        if (!same_package && case_type == CASE_NORMAL) {
            For (file->imports) {
                if (it.package_name_type == GPN_BLANK) continue;

                if (streq(it.import_path, ctx->import_path)) {
                    if (it.package_name_type == GPN_DOT)
                        package_is_dot = true;
                    else
                        package_name_we_want = get_import_package_name(&it);
                    break;
                }
            }

            if (!package_is_dot && !package_name_we_want) return;
        }

        auto process_ref = [&](Go_Reference *it) {
            if (!streq(it->is_sel ? it->sel : it->name, decl_name))
                return;

            auto check_is_self = [&]() {
                if (!same_file_as_decl) return false;
                if (it->is_sel) return false;
                if (it->start != decl->name_start) return false;
                if (it->end != decl->name_end) return false;

                return true;
            };

            bool is_self = check_is_self();
            if (!include_self && is_self)
                return;

            /*
            factors:
             - normal? method? field?
             - is toplevel? private?

            if not toplevel:            parse the current file
            else if private:            parse the current package
            else: (public toplevel)     parse everything

            parsing a file:
                normal:
                    - if current package, look for foo
                    - if other package, look for foo.bar where foo is current import
                field:
                    - look for foo.bar where bar == decl->name and type(foo) == our type
                    - look for foo where foo is a key in a struct literal
                    - means we need a way to backref type?
                method:
                    - look for foo.bar where bar == decl->name and type(foo) == decl->func_recv

            this starts to take longer, but fundamentally jetbrains and vscode
            have to do this too (or give incorrect results), and we are
            guaranteed to be faster than them (being non-pessimized)
            */

            switch (case_type) {
            case CASE_NORMAL:
                if (!same_package) {
                    if (it->is_sel) {
                        if (package_is_dot) return;
                        if (it->x->type != GOTYPE_LAZY_ID) return;
                        if (!streq(it->x->lazy_id_name, package_name_we_want)) return;
                    } else {
                        if (!package_is_dot) return;
                    }
                } else {
                    if (it->is_sel) return;
                }
                break;
            case CASE_METHOD:
                if (!it->is_sel && !is_self) return;
                break;
            }

            int tidx = 0;
            auto toplevels = file->decls;

            auto decl = get_reference_decl(it, &ctx2);
            if (decl && is_match(decl)) {
                Find_References_Result result; ptr0(&result);
                result.reference = it;

                auto pos = it->is_sel ? it->x_start : it->start;
                while (toplevels->at(tidx).spec_start < pos && tidx < toplevels->len)
                    tidx++;

                if (tidx) {
                    auto &tl = toplevels->at(tidx - 1);
                    if (tl.spec_start < pos && pos <= tl.decl_end) {
                        ccstr name = NULL;
                        switch (tl.type) {
                        case GODECL_VAR:
                        case GODECL_CONST:
                        case GODECL_TYPE:
                        case GODECL_FUNC: {
                            name = cp_strdup(tl.name);
                            auto recvname = get_godecl_recvname(&tl);
                            if (recvname)
                                name = cp_sprintf("%s.%s", recvname, name);
                        }
                        }
                        result.toplevel_name = name;
                    }
                }

                results->append(&result);
            }
        };

        For (file->references) process_ref(&it);

        if (results->len > 0) {
            Find_References_File out;
            out.filepath = cp_strdup(path_join(get_package_path(pkg->import_path), file->filename));
            out.results = results;
            ret->append(&out);
        }
    };

    if (!decl->is_toplevel) {
        Go_Package *pkg = NULL;
        auto gofile = find_gofile_from_ctx(ctx, &pkg);
        if (!gofile) return NULL;
        process(pkg, gofile);
    } else if (islower(decl_name[0])) {
        auto pkg = find_package_in_index(ctx->import_path);
        if (!pkg) return NULL;
        For (pkg->files) process(pkg, &it);
    } else {
        For (index.packages) {
            if (it.status != GPS_READY) continue;
            if (!index_has_module_containing(it.import_path))
                continue;
            auto &pkg = it;
            For (it.files) process(&pkg, &it);
        }
    }

    return ret;
}

Go_Work_Module *Go_Workspace::find_module_containing(ccstr import_path) {
    For (modules)
        if (path_has_descendant(it.import_path, import_path))
            return &it;
    return NULL;
}

Go_Work_Module *Go_Workspace::find_module_containing_resolved(ccstr resolved_path) {
    For (modules)
        if (path_has_descendant(it.resolved_path, resolved_path))
            return &it;
    return NULL;
}

bool Go_Indexer::index_has_module_containing(ccstr path) {
    if (!index.workspace) return false;
    return (index.workspace->find_module_containing(path) != NULL);
}

List<Go_Import> *Go_Indexer::optimize_imports(ccstr filepath) {
    Timer t; t.init("list_missing_imports");

    auto editor = find_editor_by_filepath(filepath);
    if (!editor) return NULL;
    if (!editor->buf) return NULL;

    if (editor->buf->tree_dirty)
        reload_editor(editor);

    t.log("reload");

    auto pf = parse_file(filepath, LANG_GO, true);
    if (!pf) return NULL;
    defer { free_parsed_file(pf); };

    t.log("parse_file");

    auto ctx = filepath_to_ctx(filepath);
    if (!ctx) return NULL;

    /*
    String_Set package_refs; package_refs.init();
    String_Set full_refs; full_refs.init();

    walk_ast_node(pf->root, true, [&](auto it, auto, auto) {
        Ast_Node *x = NULL, *sel = NULL;

        switch (it->type()) {
        case TS_QUALIFIED_TYPE:
            x = it->field(TSF_PACKAGE);
            sel = it->field(TSF_NAME); // TODO: this is wrong, look at astviewer
            break;

        case TS_SELECTOR_EXPRESSION:
            x = it->field(TSF_OPERAND);
            sel = it->field(TSF_FIELD);

            switch (x->type()) {
            case TS_IDENTIFIER:
            case TS_FIELD_IDENTIFIER:
            case TS_PACKAGE_IDENTIFIER:
            case TS_TYPE_IDENTIFIER:
                break;
            default:
                return WALK_CONTINUE;
            }
            break;

        default:
            return WALK_CONTINUE;
        }

        if (isastnull(x) || isastnull(sel)) return WALK_CONTINUE;

        auto node = x->dup();

        auto r = refs.append();
        r->node = node;
        r->package = x->string();
        r->sel = sel->string();

        return WALK_SKIP_CHILDREN;
    });
    */

    t.log("list all references");

    struct Ref_Package {
        ccstr name;
        List<ccstr> sels;
    };

    auto pkgs = new_list(Ref_Package);
    String_Set referenced_package_names; referenced_package_names.init();

    // put this earlier maybe?
    auto gofile = find_gofile_from_ctx(ctx);
    if (!gofile) return NULL;

    For (gofile->references) {
        auto &ref = it;

        if (!ref.is_sel) continue;
        if (ref.x->type != GOTYPE_LAZY_ID) continue;

        auto pkgname = ref.x->lazy_id_name;

        auto has_nonimport_decl = [&]() -> bool {
            auto res = find_decl_of_id(pkgname, ref.x_start, ctx);
            if (!res) return false;
            if (res->decl->type == GODECL_IMPORT) return false;
            return true;
        };

        if (has_nonimport_decl()) continue;

        auto pkg = pkgs->find([&](auto it) { return streq(it->name, pkgname); });
        if (!pkg) {
            pkg = pkgs->append();
            pkg->name = pkgname;
            pkg->sels.init();

            referenced_package_names.add(pkgname);
        }

        if (!pkg->sels.find([&](auto it) { return streq(*it, ref.sel); }))
            pkg->sels.append(ref.sel);
    }

    t.log("sort into packages");

    String_Set imported_package_names; imported_package_names.init();
    auto ret = new_list(Go_Import);
    auto dot_imports = new_list(Go_Import);

    if (gofile->imports) {
        For (gofile->imports) {
            auto package_name = get_import_package_name(&it);

            if (it.package_name_type == GPN_DOT) {
                dot_imports->append(&it);
                continue;
            }

            // leave _ imports alone
            if (it.package_name_type != GPN_BLANK)
                if (!package_name || !referenced_package_names.has(package_name))
                    continue;

            ret->append(&it);

            if (package_name)
                imported_package_names.add(package_name);
        }
    }

    t.log("get existing imports");


    {
        Table<ccstr> dot_names; dot_names.init();

        For (dot_imports) {
            auto import_path = it.import_path;
            auto decls = list_package_decls(import_path, LISTDECLS_PUBLIC_ONLY | LISTDECLS_EXCLUDE_METHODS);
            For (decls) dot_names.set(it.decl->name, import_path);
        }

        String_Set included_imports; included_imports.init();

        // go thru references
        For (gofile->references) {
            if (it.is_sel) continue;

            // is the ref associated with an import path from our crawling of dot imports?
            auto import_path = dot_names.get(it.name);
            if (import_path) {
                // does the ref actually point to that import path?
                auto res = find_decl_of_id(it.name, it.start, ctx);
                if (streq(import_path, res->ctx->import_path)) {
                    // if so, check that import path off
                    included_imports.add(import_path);
                }
            }
        }

        For (dot_imports)
            if (included_imports.has(it.import_path))
                ret->append(&it);
    }

    t.log("handle dot imports");

    For (pkgs) {
        if (imported_package_names.has(it.name)) continue;

        print("unaccounted package: %s", it.name);

        auto import_path = find_best_import(it.name, &it.sels);
        if (!import_path) continue; // couldn't find anything

        auto imp = ret->append();
        imp->package_name_type = GPN_IMPLICIT;
        imp->import_path = import_path;
    }

    t.log("list unaccounted-for imports");

    t.total();

    return ret;
}

ccstr Go_Indexer::find_best_import(ccstr package_name, List<ccstr> *identifiers) {
    List<Go_Package*> candidates; candidates.init();
    List<int> indexes; indexes.init();

    For (index.packages) {
        if (it.status != GPS_READY) continue;
        if (!it.package_name) continue;
        if (!streq(it.package_name, package_name)) continue;

        if (!index_has_module_containing(it.import_path))
            if (is_import_path_internal(it.import_path))
                continue;

        candidates.append(&it);
        indexes.append(indexes.len);
    }

    if (!candidates.len) return NULL;

    String_Set all_identifiers; all_identifiers.init();
    For (identifiers) all_identifiers.add(it);

    struct Score {
        bool in_goroot;
        bool in_workspace;
        int matching_idents;
    };

    List<Score> scores; scores.init();

    for (int i = 0; i < candidates.len; i++) {
        auto it = candidates[i];

        auto &score = scores[i];
        ptr0(&score);

        if (it->files) {
            For (it->files) {
                if (!it.decls) continue;
                For (it.decls) {
                    if (it.type == GODECL_FUNC)
                        if (!it.gotype->func_recv)
                            continue;
                    if (all_identifiers.has(it.name))
                        score.matching_idents++;
                }
            }
        }

        if (index_has_module_containing(it->import_path))
            score.in_workspace = true;

        auto package_path = get_package_path(it->import_path);
        if (package_path)
            if (path_has_descendant(package_path, goroot))
                score.in_goroot = true;
    };

    auto compare_scores = [&](Score *a, Score *b) {
        if (a->matching_idents != b->matching_idents)
            return a->matching_idents - b->matching_idents;

        if (a->in_workspace != b->in_workspace)
            return a->in_workspace ? 1 : -1;

        if (a->in_goroot != b->in_goroot)
            return a->in_goroot ? 1 : -1;

        return 0;
    };

    indexes.sort([&](auto a, auto b) {
        return -compare_scores(&scores[*a], &scores[*b]);
    });

    return candidates[indexes[0]]->import_path;
}

Goresult *Go_Indexer::find_enclosing_toplevel(ccstr filepath, cur2 pos) {
    auto ctx = filepath_to_ctx(filepath);
    auto gofile = find_gofile_from_ctx(ctx);
    if (!gofile) return NULL;

    auto decl = find_toplevel_containing(gofile, pos, pos);
    if (!decl) return NULL;

    return make_goresult(decl, ctx);
}

Generate_Struct_Tags_Result* Go_Indexer::generate_struct_tags(ccstr filepath, cur2 pos, Generate_Struct_Tags_Op op, ccstr lang, Case_Style case_style) {
    reload_all_editors();

    auto pf = parse_file(filepath, LANG_GO, true);
    if (!pf) return NULL;
    defer { free_parsed_file(pf); };

    auto file = pf->root;

    auto ctx = filepath_to_ctx(filepath);
    if (!ctx) return NULL;

    Ast_Node *target_struct = NULL;

    // find the innermost struct
    find_nodes_containing_pos(file, pos, true, [&](auto node) {
        if (node->type() == TS_STRUCT_TYPE)
            target_struct = node->dup();
        return WALK_CONTINUE;
    });

    auto ret = new_object(Generate_Struct_Tags_Result);
    ret->insert_starts = new_list(cur2);
    ret->insert_ends = new_list(cur2);
    ret->insert_texts = new_list(ccstr);

    auto make_tag_name = [&](ccstr name) -> ccstr {
        auto len = strlen(name);
        cp_assert(len);

        auto parts = new_list(ccstr);

        String_Cursor sc;
        sc.init(name);

        while (!sc.end()) {
            auto curr = new_list(char);

            char ch = sc.next();
            curr->append(ch);

            if (!sc.end()) {
                // if it's upper and next is upper, read all the uppers
                if (isupper(ch) && isupper(sc.get())) {
                    auto chars = sc.slurp([](char ch) { return isupper(ch); });
                    if (chars->len) {
                        // unless we're at the end, don't eat the last upper, because it's part of next word
                        // for instance, in GetOSName, we would read "OSN" but we only want "OS"
                        if (!sc.end()) {
                            sc.back();
                            chars->len--;
                        }

                        curr->concat(chars);
                    }
                } else {
                    curr->concat(sc.slurp([](char ch) { return !isupper(ch); }));
                }
            }

            curr->append('\0');
            parts->append(curr->items);
        }

        auto ret = new_list(char);

        Fori (parts) {
            switch (case_style) {
            case CASE_SNAKE:
                if (i) ret->append('_');
                for (auto p = it; *p; p++)
                    ret->append(tolower(*p));
                break;
            case CASE_PASCAL:
                for (auto p = it; *p; p++) {
                    if (p == it)
                        ret->append(toupper(*p));
                    else
                        ret->append(tolower(*p));
                }
                break;
            case CASE_CAMEL:
                for (auto p = it; *p; p++) {
                    if (p == it && i)
                        ret->append(toupper(*p));
                    else
                        ret->append(tolower(*p));
                }
                break;
            }
        }

        ret->append('\0');
        return ret->items;
    };

    if (!target_struct) return NULL;

    fn<void(Ast_Node*)> process_struct = [&](auto node) {
        auto listnode = node->child();
        if (isastnull(listnode)) return;

        FOR_NODE_CHILDREN (listnode) {
            if (it->type() != TS_FIELD_DECLARATION) return;

            if (op == GSTOP_ADD_ONE || op == GSTOP_REMOVE_ONE) {
                if (cmp_pos_to_node(pos, it) != 0)
                    continue;
            } else {
                auto typenode = it->field(TSF_TYPE);
                if (!isastnull(typenode))
                    if (typenode->type() == TS_STRUCT_TYPE)
                        process_struct(typenode);
            }

            if (op == GSTOP_ADD_ONE || op == GSTOP_ADD_ALL) {
                auto name = it->field(TSF_NAME);
                if (isastnull(name)) continue;  // don't fuck with embedded type fields

                auto namestr = name->string();
                if (!namestr) continue;
                if (!namestr[0]) continue;
                if (!isupper(namestr[0])) continue;

                auto tagname = make_tag_name(namestr);

                auto tag = it->field(TSF_TAG);
                if (!isastnull(tag)) {
                    auto tagstr = tag->string();
                    if (!tagstr) continue;

                    tagstr = parse_go_string(tagstr);
                    if (!tagstr) continue;

                    GoUint8 ok = false;
                    if (GHHasTag((char*)tagstr, (char*)lang, &ok)) continue;
                    if (!ok) continue;

                    tagstr = GHAddTag((char*)tagstr, (char*)lang, (char*)tagname, &ok);
                    if (!ok) continue;
                    defer { GHFree((void*)tagstr); };

                    auto newtagstr = cp_sprintf("`%s`", tagstr);
                    ret->insert_starts->append(tag->start());
                    ret->insert_ends->append(tag->end());
                    ret->insert_texts->append(newtagstr);
                } else {
                    auto newtagstr = cp_sprintf(" `%s:\"%s\"`", lang, tagname);
                    ret->insert_starts->append(it->end());
                    ret->insert_ends->append(it->end());
                    ret->insert_texts->append(newtagstr);
                }
            } else if (op == GSTOP_REMOVE_ONE || op == GSTOP_REMOVE_ALL) {
                auto tag = it->field(TSF_TAG);
                if (isastnull(tag)) continue;

                auto type = it->field(TSF_TYPE);
                if (isastnull(type)) continue;

                ret->insert_starts->append(type->end());
                ret->insert_ends->append(tag->end());
                ret->insert_texts->append("");
            }
        }
    };

    process_struct(target_struct);

    ret->highlight_start = target_struct->start();
    ret->highlight_end = target_struct->end();
    return ret;
}

Generate_Func_Sig_Result* Go_Indexer::generate_function_signature(ccstr filepath, cur2 pos) {
    reload_all_editors();

    auto pf = parse_file(filepath, LANG_GO, true);
    if (!pf) return NULL;
    defer { free_parsed_file(pf); };

    auto file = pf->root;

    auto ctx = filepath_to_ctx(filepath);
    if (!ctx) return NULL;

    Ast_Node *callnode = NULL;

    find_nodes_containing_pos(file, pos, true, [&](auto node) -> Walk_Action {
        switch (node->type()) {
        case TS_QUALIFIED_TYPE:
        case TS_SELECTOR_EXPRESSION: {
            Ast_Node *sel = NULL;
            if (node->type() == TS_QUALIFIED_TYPE)
                sel = node->field(TSF_NAME);
            else
                sel = node->field(TSF_FIELD);
            // maybe warn user to put cursor directly over name of function
            if (cmp_pos_to_node(pos, sel) != 0)
                return WALK_CONTINUE;
            break;
        }
        case TS_PACKAGE_IDENTIFIER:
        case TS_TYPE_IDENTIFIER:
        case TS_IDENTIFIER:
        case TS_LABEL_NAME:
        case TS_FIELD_IDENTIFIER:
            break;
        default:
            return WALK_CONTINUE;
        }

        auto parent = node->parent();
        if (!isastnull(parent) && parent->type() == TS_CALL_EXPRESSION)
            callnode = parent;

        return WALK_ABORT;
    });

    if (!callnode) return NULL;

    auto expr_to_evaled_type = [&](Ast_Node *expr) -> Goresult* {
        auto gotype = expr_to_gotype(expr);
        if (!gotype) return NULL;
        return evaluate_type(gotype, ctx);
    };

    auto get_expr_type_decl = [&](Ast_Node *expr) -> Goresult* {
        auto res = expr_to_evaled_type(expr);
        if (!res) return NULL;

        res = unpointer_type(res);
        return resolve_type_to_decl(res->gotype, res->ctx);
    };

    auto func = callnode->field(TSF_FUNCTION);
    if (isastnull(func)) return NULL;

    auto funcref = node_to_reference(func);
    if (!funcref) return NULL;

    auto ret = new_object(Generate_Func_Sig_Result);

    auto existing_decl = get_reference_decl(funcref, ctx);
    if (existing_decl) {
        ret->existing_decl_filepath = ctx_to_filepath(existing_decl->ctx);
        ret->existing_decl_pos = existing_decl->decl->name_start;
        return ret;
    }

    Type_Renderer rend; rend.init();
    rend.write("func ");
    bool add_newlines_after = false;

    if (funcref->is_sel) {
        auto res = evaluate_type(funcref->x, ctx);
        if (!res) {
            // try as import
            if (funcref->x->type != GOTYPE_LAZY_ID) return NULL;

            auto import_path = find_import_path_referred_to_by_id(funcref->x->lazy_id_name, ctx);
            if (import_path == NULL) return NULL;
            if (!index_has_module_containing(import_path)) return NULL;

            auto pkg = find_up_to_date_package(import_path);
            if (!pkg) return NULL;
            if (isempty(pkg->files)) return NULL;

            auto package_path = get_package_path(import_path);
            if (!package_path) return NULL;

            add_newlines_after = false;
            ret->insert_filepath = path_join(package_path, pkg->files->at(0).filename);

            auto fm = map_file_into_memory(ret->insert_filepath);
            if (!fm) return NULL;
            defer { fm->cleanup(); };

            cur2 pos = new_cur2(0, 0);
            for (int i = 0; i < fm->len; i++) {
                if (fm->data[i] == '\n') {
                    pos.y++;
                    pos.x = 0;
                } else {
                    pos.x++;
                }
            }
            ret->insert_pos = pos;
        } else {
            Goresult *declres = NULL;

            switch (res->gotype->type) {
            case GOTYPE_ID:
            case GOTYPE_SEL:
                declres = resolve_type_to_decl(res->gotype, res->ctx);
                break;
            }

            if (!declres) return NULL;
            if (!index_has_module_containing(declres->ctx->import_path)) return NULL;

            auto filepath = ctx_to_filepath(declres->ctx);
            if (!filepath) return NULL;

            auto decl = declres->decl;

            ret->insert_filepath = filepath;
            ret->insert_pos = decl->decl_end;

            auto type_name = decl->name;
            if (!type_name) {
                // TODO: start calling tell_user_error from inside here?
                return NULL;
            }

            ccstr type_var = NULL;
            {
                auto s = new_list(char);
                for (int i = 0, len = strlen(type_name); i < len && s->len < 3; i++)
                    if (isupper(type_name[i]))
                        s->append(tolower(type_name[i]));
                s->append('\0');
                type_var = s->items;
            }

            rend.write("(%s %s) ", type_var, type_name);
            add_newlines_after = false;
        }
    } else {
        auto curr = callnode;
        while (true) {
            auto parent = curr->parent();
            if (parent->type() == TS_SOURCE_FILE)
                break;
            curr = parent;
        }

        add_newlines_after = true;
        ret->insert_pos = curr->start();
        ret->insert_filepath = filepath;
    }

    rend.write("%s(", funcref->is_sel ? funcref->sel : funcref->name);

    auto argsnode = callnode->field(TSF_ARGUMENTS);
    if (isastnull(argsnode)) return NULL;

    ret->imports_needed = new_list(ccstr);
    ret->imports_needed_names = new_list(ccstr);

    Go_Package *pkg = NULL;
    auto insert_file = find_gofile_from_ctx(filepath_to_ctx(ret->insert_filepath), &pkg);
    if (!insert_file) return NULL;
    if (!pkg) return NULL;

    auto current_import_path = pkg->import_path;

    Table<ccstr> existing_imports; existing_imports.init();
    Table<ccstr> existing_imports_r; existing_imports_r.init();
    For (insert_file->imports) {
        auto package_name = get_import_package_name(&it);
        if (!package_name) continue;
        existing_imports.set(package_name, it.import_path);
        existing_imports_r.set(it.import_path, package_name);
    }

    int var_index = 0;
    FOR_NODE_CHILDREN (argsnode) {
        defer { var_index++; };

        if (var_index) rend.write(", ");

        rend.write("v%d ", var_index);

        auto res = expr_to_evaled_type(it);
        if (!res) {
            rend.write("any");
            continue;
        };

        bool shit_done_fucked_up = false;

        rend.write_type(res->gotype, [&](auto rend, auto it) -> bool {
            ccstr import_path = NULL, name = NULL;

            switch (it->type) {
            case GOTYPE_ID: {
                name = it->id_name;

                auto declres = find_decl_of_id(name, it->id_pos, res->ctx);
                if (!declres) {
                    shit_done_fucked_up = true;
                    return false;
                }

                if (streq(declres->ctx->import_path, "@builtin")) {
                    rend->write(name);
                    return true;
                }

                import_path = res->ctx->import_path;
                break;
            }
            case GOTYPE_SEL:
                import_path = find_import_path_referred_to_by_id(it->sel_name, res->ctx);
                name = it->sel_sel;
                break;
            default:
                return false;
            }

            if (!import_path) {
                shit_done_fucked_up = true;
                return false;
            }

            if (streq(import_path, current_import_path)) {
                rend->write(name);
                return true;
            }

            auto pkgname = existing_imports_r.get(import_path);
            if (!pkgname) {
                auto pkg = find_up_to_date_package(import_path);
                if (!pkg) {
                    shit_done_fucked_up = true;
                    return false;
                }

                auto base_pkgname = pkg->package_name;
                ccstr name = NULL;

                for (int i = 0;; i++) {
                    name = i ? cp_sprintf("%s%d", base_pkgname, i) : base_pkgname;
                    if (!existing_imports.get(name)) break;
                }

                pkgname = name;

                existing_imports.set(pkgname, import_path);
                existing_imports_r.set(import_path, pkgname);

                ret->imports_needed->append(import_path);
                if (streq(pkgname, pkg->package_name))
                    ret->imports_needed_names->append((ccstr)NULL);
                else
                    ret->imports_needed_names->append(pkgname);
            }

            rend->write("%s.%s", pkgname, name);
            return true;
        });

        if (shit_done_fucked_up) return NULL;
    }

    rend.write(") {\n\tpanic(\"not implemented yet\")\n}");

    if (ret->imports_needed->len) {
        rend.write("\n/*\nimport (\n");
        Fori (ret->imports_needed) {
            auto name = ret->imports_needed_names->at(i);
            if (name)
                rend.write("\t%s \"%s\"\n", name, it);
            else
                rend.write("\t\"%s\"\n", it);
        }
        rend.write(")\n");
        rend.write("*/");
    }

    if (add_newlines_after)
        ret->insert_code = cp_sprintf("%s\n\n", rend.finish());
    else
        ret->insert_code = cp_sprintf("\n\n%s", rend.finish());

    cur2 code_end = ret->insert_pos;
    for (auto p = ret->insert_code; *p; p++) {
        if (*p == '\n') {
            code_end.y++;
            code_end.x = 0;
        } else {
            code_end.x++;
        }
    }

    if (add_newlines_after) {
        ret->jump_to_pos = ret->insert_pos;
        ret->highlight_start = ret->insert_pos;
        ret->highlight_end = code_end;
        ret->highlight_end.y--;
    } else {
        ret->jump_to_pos = new_cur2(0, ret->insert_pos.y+2);
        ret->highlight_start = new_cur2(0, ret->insert_pos.y+2);
        ret->highlight_end = code_end;
    }

    return ret;
}

Jump_To_Definition_Result* Go_Indexer::jump_to_definition(ccstr filepath, cur2 pos) {
    Timer t;
    t.init("jump_to_definition", false);

    reload_all_editors();

    t.log("reload dirty files");

    auto pf = parse_file(filepath, LANG_GO, true);
    if (!pf) return NULL;
    defer { free_parsed_file(pf); };

    auto file = pf->root;

    auto ctx = filepath_to_ctx(filepath);
    if (!ctx) return NULL;

    Jump_To_Definition_Result result; ptr0(&result);

    t.log("setup shit");

    find_nodes_containing_pos(file, pos, true, [&](auto node) -> Walk_Action {
        auto contains_pos = [&](Ast_Node *node) -> bool {
            return cmp_pos_to_node(pos, node) == 0;
        };

        switch (node->type()) {
        case TS_PACKAGE_CLAUSE: {
            auto name_node = node->child();
            if (contains_pos(name_node)) {
                result.file = filepath;
                result.pos = name_node->start();
            }
            return WALK_ABORT;
        }

        case TS_IMPORT_SPEC:
            result.file = filepath;
            result.pos = node->start();
            return WALK_ABORT;

        case TS_QUALIFIED_TYPE:
        case TS_SELECTOR_EXPRESSION: {
            auto sel_node = node->field(node->type() == TS_QUALIFIED_TYPE ? TSF_NAME : TSF_FIELD);
            if (!contains_pos(sel_node)) return WALK_CONTINUE;

            auto operand_node = node->field(node->type() == TS_QUALIFIED_TYPE ? TSF_PACKAGE : TSF_OPERAND);

            bool dontcare;
            auto results = get_node_dotprops(operand_node, &dontcare, ctx);
            if (results) {
                auto sel_name = sel_node->string();
                For (results) {
                    auto decl = it.decl;

                    if (streq(decl->name, sel_name)) {
                        result.pos = decl->name_start;
                        result.file = ctx_to_filepath(it.ctx);
                        result.decl = it.copy_decl();
                        return WALK_ABORT;
                    }
                }
            }
            return WALK_ABORT;
        }

        case TS_PACKAGE_IDENTIFIER:
        case TS_TYPE_IDENTIFIER:
        case TS_IDENTIFIER:
        case TS_LABEL_NAME:
        case TS_FIELD_IDENTIFIER: {
            auto is_struct_field_in_literal = [&]() -> Ast_Node* {
                auto p = node->parent();
                if (isastnull(p)) return NULL;
                if (p->type() != TS_LITERAL_ELEMENT) return NULL;

                // must be first literal element (second is value)
                if (!isastnull(p->prev())) return NULL;

                p = p->parent();
                if (isastnull(p)) return NULL;
                if (p->type() != TS_KEYED_ELEMENT) return NULL;

                p = p->parent();
                if (isastnull(p)) return NULL;
                if (p->type() != TS_LITERAL_VALUE) return NULL;

                p = p->parent();
                if (isastnull(p)) return NULL;
                if (p->type() != TS_COMPOSITE_LITERAL) return NULL;

                return p;
            };

            Goresult *declres = NULL;

            auto comp_literal = is_struct_field_in_literal();
            if (comp_literal) {
                do {
                    auto p = comp_literal->field(TSF_TYPE);
                    if (isastnull(p)) break;

                    auto gotype = node_to_gotype(p);
                    if (!gotype) break;

                    auto res = evaluate_type(gotype, ctx);
                    if (!res) break;

                    auto rres = resolve_type(res);
                    if (!rres) break;

                    rres = subst_generic_type(rres);
                    if (!rres) break;

                    rres = remove_override_ctx(rres->gotype, rres->ctx);

                    auto tmp = new_list(Goresult);

                    Actually_List_Dotprops_Opts opts; ptr0(&opts);
                    opts.out = tmp;
                    opts.depth = 0;
                    opts.seen_embeds.init();
                    opts.fields_only = true;
                    actually_list_dotprops(res, rres, &opts);

                    auto name = node->string();
                    For (tmp) {
                        if (streq(it.decl->name, name)) {
                            declres = &it;
                            break;
                        }
                    }
                } while (0);
            } else {
                declres = find_decl_of_id(node->string(), node->start(), ctx);
            }

            if (declres) {
                result.file = ctx_to_filepath(declres->ctx);
                result.decl = declres->copy_decl();
                if (declres->decl->name)
                    result.pos = declres->decl->name_start;
                else
                    result.pos = declres->decl->spec_start;
            }
            return WALK_ABORT;
        }
        }

        return WALK_CONTINUE;
    }, true);

    t.log("find declaration");

    if (!result.file) return NULL;

    if (result.pos.y == -1) {
        auto newpos = offset_to_cur(result.pos.x, result.file);
        if (newpos == NULL_CUR) return NULL;
        result.pos = newpos;
    }

    t.log("convert pos if needed");

    auto ret = new_object(Jump_To_Definition_Result);
    *ret = result;

    t.total();

    return ret;
}

cur2 offset_to_cur(int off, ccstr filepath) {
    auto fm = map_file_into_memory(filepath);
    if (!fm) return NULL_CUR;
    defer { fm->cleanup(); };

    cur2 newpos; ptr0(&newpos);
    for (u32 i = 0; i < fm->len && i < off; i++) {
        if (fm->data[i] == '\r') continue;
        if (fm->data[i] == '\n') {
            newpos.y++;
            newpos.x = 0;
            continue;
        }
        newpos.x++;
    }
    return newpos;
}

bool is_expression_node(Ast_Node *node) {
    switch (node->type()) {
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

char go_tsinput_buffer[1024];

bool Go_Indexer::truncate_parsed_file(Parsed_File *pf, cur2 end_pos, ccstr chars_to_append) {
    if (!pf->tree_belongs_to_editor) return false;
    if (pf->it->type != IT_BUFFER) return false;

    auto buf = pf->it->buffer_params.it.buf;
    auto eof_pos = new_cur2(buf->lines.last()->len, buf->lines.len - 1);

    TSInputEdit edit; ptr0(&edit);
    edit.start_byte = buf->cur_to_offset(end_pos);
    edit.start_point = cur_to_tspoint(end_pos);
    edit.old_end_byte = buf->cur_to_offset(eof_pos);
    edit.old_end_point = cur_to_tspoint(eof_pos);
    edit.new_end_byte = edit.start_byte;
    edit.new_end_point = edit.start_point;

    ts_tree_edit(pf->tree, &edit);

    auto &it = pf->it->buffer_params.it;
    it.has_fake_end = true;
    it.fake_end = end_pos;
    it.fake_end_offset = buf->cur_to_offset(it.fake_end);

    if (chars_to_append) {
        it.append_chars_to_end = true;
        it.chars_to_append_to_end = chars_to_append;
    }

    {
        SCOPED_FRAME();

        Buffer_It it2;
        memcpy(&it2, &it, sizeof(Buffer_It));

        auto ret = new_list(char);

        it2.pos = new_cur2(0, 0);
        while (!it2.eof())
            ret->append(it2.next());
        ret->append('\0');
    }

    TSInput input;
    input.payload = &it;
    input.encoding = TSInputEncodingUTF8;

    input.read = [](void *p, uint32_t off, TSPoint pos, uint32_t *read) -> const char* {
        auto it = (Buffer_It*)p;

        if (it->has_fake_end && off >= it->fake_end_offset) {
            it->pos = it->fake_end;
            it->pos.x += (off - it->fake_end_offset);
        } else {
            it->pos = it->buf->offset_to_cur(off);
        }

        u32 n = 0;

        while (!it->eof()) {
            auto uch = it->next();
            if (!uch) break;

            auto size = uchar_size(uch);
            if (n + size + 1 > _countof(go_tsinput_buffer)) break;

            n += uchar_to_cstr(uch, &go_tsinput_buffer[n]);
        }

        *read = n;
        go_tsinput_buffer[n] = '\0';
        return go_tsinput_buffer;
    };

    pf->tree = ts_parser_parse(pf->editor_parser, pf->tree, input);
    pf->root = new_ast_node(ts_tree_root_node(pf->tree), pf->it);
    return true;
}

ccstr get_postfix_completion_name(Postfix_Completion_Type type) {
    switch (type) {
    case PFC_ASSIGNAPPEND: return "aappend!";
    case PFC_APPEND: return "append!";
    case PFC_LEN: return "len!";
    case PFC_CAP: return "cap!";
    case PFC_FOR: return "for!";
    case PFC_FORKEY: return "forkey!";
    case PFC_FORVALUE: return "forvalue!";
    case PFC_NIL: return "nil!";
    case PFC_NOTNIL: return "notnil!";
    case PFC_NOT: return "not!";
    case PFC_EMPTY: return "empty!";
    case PFC_NOTEMPTY: return "notempty!";
    case PFC_IFEMPTY: return "ifempty!";
    case PFC_IFNOTEMPTY: return "ifnotempty!";
    case PFC_IF: return "if!";
    case PFC_IFNOT: return "ifnot!";
    case PFC_IFNIL: return "ifnil!";
    case PFC_IFNOTNIL: return "ifnotnil!";
    case PFC_CHECK: return "check!";
    case PFC_DEFSTRUCT: return "defstruct!";
    case PFC_DEFINTERFACE: return "definterface!";
    case PFC_SWITCH: return "switch!";
    }
    return NULL;

}

Gotype *Go_Indexer::get_closest_function(ccstr filepath, cur2 pos) {
    reload_all_editors();

    auto pf = parse_file(filepath, LANG_GO, true);
    if (!pf) return NULL;
    defer { free_parsed_file(pf); };

    if (!truncate_parsed_file(pf, pos, "_}}}}}}}}}}}}}}}}}")) return NULL;
    defer { ts_tree_delete(pf->tree); };

    Gotype *ret = NULL;

    auto cb = [&](Go_Scope_Op *it) -> bool {
        if (it->pos > pos) return false;

        if (it->type == GSOP_DECL)
            if (it->decl->decl_start <= pos && pos < it->decl->decl_end)
                if (it->decl->type != GODECL_IMPORT)
                    if (it->decl->gotype && it->decl->gotype->type == GOTYPE_FUNC)
                        ret = it->decl->gotype;

        return true;
    };

    iterate_over_scope_ops(pf->root, cb, cp_basename(filepath));
    return ret;
}

// this fills possible types
void Go_Indexer::fill_generate_implementation(List<Go_Symbol> *out, bool selected_interface) {
    reload_all_editors();

    For (index.packages) {
        if (it.status != GPS_READY) continue;

        if (selected_interface)
            if (!index.workspace->find_module_containing(it.import_path))
                continue;

        auto pkg = &it;

        Go_Ctx ctx;
        ctx.import_path = it.import_path;

        auto pkgname = it.package_name;
        For (it.files) {
            ctx.filename = it.filename;
            auto filehash = it.hash;
            auto filepath = ctx_to_filepath(&ctx);

            For (it.decls) {
                if (it.type != GODECL_TYPE) continue;
                if (!it.gotype) continue; // ???

                auto gotype_type = it.gotype->type;
                if (gotype_type == GOTYPE_BUILTIN) {
                    if (!it.gotype->builtin_underlying_base) continue;
                    gotype_type = it.gotype->builtin_underlying_base->type;
                }

                // if user selected interface, we're looking for any other
                // non-interface type
                if (selected_interface) {
                    if (gotype_type == GOTYPE_INTERFACE) continue;
                } else {
                    if (gotype_type != GOTYPE_INTERFACE) continue;
                }

                Go_Symbol sym;
                sym.pkgname = pkgname;
                sym.filepath = filepath;
                sym.name = it.name;
                sym.decl = make_goresult(&it, &ctx)->copy_decl();
                sym.filehash = filehash;
                out->append(&sym);
            }
        }
    }
}

void Go_Indexer::fill_goto_symbol(List<Go_Symbol> *out) {
    reload_all_editors();

    auto packages = new_list(Go_Package*);
    For (index.packages)
        if (index.workspace->find_module_containing(it.import_path))
            packages->append(&it);

    For (packages) {
        auto pkg = it;

        auto pkgname = pkg->package_name;
        auto import_path = pkg->import_path;

        For (pkg->files) {
            auto ctx = new_object(Go_Ctx);
            ctx->filename = it.filename;
            ctx->import_path = import_path;

            auto filepath = ctx_to_filepath(ctx);

            For (it.decls) {
                if (streq(it.name, "_")) continue;

                auto getrecv = [&]() -> ccstr {
                    if (it.type != GODECL_FUNC) return NULL;
                    if (!it.gotype) return NULL;
                    if (it.gotype->type != GOTYPE_FUNC) return NULL;

                    auto recv = it.gotype->func_recv;
                    if (!recv) return NULL;

                    recv = unpointer_type(recv);
                    if (!recv) return NULL;
                    if (recv->type == GOTYPE_GENERIC) recv = recv->base;
                    if (recv->type != GOTYPE_ID) return NULL;

                    return recv->id_name;
                };

                ccstr name = NULL;
                auto recvname = getrecv();
                if (recvname)
                    name = cp_sprintf("%s.%s", recvname, it.name);
                else
                    name = cp_sprintf("%s", it.name);

                Go_Symbol sym;
                sym.pkgname = pkgname;
                sym.filepath = filepath;
                sym.name = name;
                sym.decl = make_goresult(&it, ctx);
                sym.filehash = 0; // ???
                out->append(&sym);
            }
        }
    }
}

bool Go_Indexer::autocomplete(ccstr filepath, cur2 pos, bool triggered_by_period, Autocomplete *out) {
    Timer t;
    t.init("autocomplete", false);

    reload_all_editors();

    t.log("reload");

    auto pf = parse_file(filepath, LANG_GO, true);
    if (!pf) return false;
    defer { free_parsed_file(pf); };

    if (!truncate_parsed_file(pf, pos, "_}}}}}}}}}}}}}}}}}")) return false;
    defer { ts_tree_delete(pf->tree); };

    auto intelligently_move_cursor_backwards = [&]() -> cur2 {
        auto it = new_object(Parser_It);
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

    auto ctx = filepath_to_ctx(filepath);
    if (!ctx) return false;

    List<Goresult> results;
    results.init();
    bool found_something = false;

    enum Current_Situation {
        FOUND_JACK_SHIT,
        FOUND_DOT_COMPLETE,
        FOUND_DOT_COMPLETE_NEED_CRAWL,
        FOUND_LONE_IDENTIFIER,
        JUST_EXIT,
    };

    Current_Situation situation = FOUND_JACK_SHIT;
    Ast_Node *expr_to_analyze = NULL;
    cur2 keyword_start; ptr0(&keyword_start);
    ccstr prefix = NULL;
    Ast_Node *lone_identifier_struct_literal = NULL;

    // i believe right now there are three possibilities:
    //
    // 1) well-formed ts_selector_expression
    // 2) ill-formed ts_selector_expression in the making; we have a ts_anon_dot
    // 3) we have nothing, just do a keyword autocomplete

    t.log("setup shit");

    find_nodes_containing_pos(pf->root, intelligently_move_cursor_backwards(), false, [&](auto node) -> Walk_Action {
        switch (node->type()) {
        case TS_QUALIFIED_TYPE:
        case TS_SELECTOR_EXPRESSION: {
            auto operand_node = node->field(node->type() == TS_QUALIFIED_TYPE ? TSF_PACKAGE : TSF_OPERAND);
            if (isastnull(operand_node)) return WALK_ABORT;
            if (cmp_pos_to_node(pos, operand_node) == 0) return WALK_CONTINUE;

            auto sel_node = node->field(node->type() == TS_QUALIFIED_TYPE ? TSF_NAME : TSF_FIELD);

            bool dot_found = false;
            for (auto curr = node->child_all(); !isastnull(curr); curr = curr->next_all()) {
                if (curr->type() == TS_ANON_DOT) {
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
                keyword_start = sel_node->start();
                prefix = cp_strdup(sel_node->string());
                ((cstr)prefix)[pos.x - sel_node->start().x] = '\0';
                break;
            case 1: // pos is after sel
                // if it's directly to the right of sel, like foo.bar|,
                // then we treat cursor as being "in" sel
                if (pos == sel_node->end()) {
                    keyword_start = sel_node->start();
                    prefix = sel_node->string();
                    break;
                }
                return WALK_ABORT;
            }

            expr_to_analyze = operand_node->dup();
            situation = FOUND_DOT_COMPLETE;
            return WALK_ABORT;
        }

        case TS_PACKAGE_IDENTIFIER:
        case TS_TYPE_IDENTIFIER:
        case TS_IDENTIFIER:
        case TS_FIELD_IDENTIFIER:
            expr_to_analyze = node->dup();
            situation = FOUND_LONE_IDENTIFIER;
            keyword_start = node->start();
            prefix = cp_strdup(node->string());
            ((cstr)prefix)[pos.x - node->start().x] = '\0';
            {
                auto get_struct_literal_type = [&]() -> Ast_Node* {
                    auto curr = node->parent();
                    if (isastnull(curr)) return NULL;
                    if (curr->type() != TS_KEYED_ELEMENT && curr->type() != TS_LITERAL_ELEMENT) return NULL;
                    if (!isastnull(node->prev())) return NULL; // must be key, not value

                    curr = curr->parent();
                    if (isastnull(curr)) return NULL;
                    if (curr->type() != TS_LITERAL_VALUE) return NULL;

                    curr = curr->parent();
                    if (isastnull(curr)) return NULL;
                    if (curr->type() != TS_COMPOSITE_LITERAL) return NULL;

                    curr = curr->field(TSF_TYPE);
                    if (isastnull(curr)) return NULL;

                    return curr->dup();
                };
                lone_identifier_struct_literal = get_struct_literal_type();
            }
            return WALK_ABORT;

        case TS_ANON_DOT: {
            auto prev = node->prev();
            if (!isastnull(prev)) {
                expr_to_analyze = prev->dup();
                situation = FOUND_DOT_COMPLETE;
                keyword_start = pos;
                prefix = "";
            } else {
                auto parent = node->parent();
                if (!isastnull(parent) && parent->type() == TS_ERROR) {
                    auto expr = parent->prev();
                    if (!isastnull(expr)) {
                        expr_to_analyze = expr->dup();
                        situation = FOUND_DOT_COMPLETE_NEED_CRAWL;
                        keyword_start = pos;
                        prefix = "";
                    }
                }
            }
            return WALK_ABORT;
        }

        case TS_INT_LITERAL:
        case TS_FLOAT_LITERAL:
        case TS_IMAGINARY_LITERAL:
        case TS_RUNE_LITERAL:
        case TS_RAW_STRING_LITERAL:
        case TS_INTERPRETED_STRING_LITERAL:
            situation = JUST_EXIT;
            return WALK_ABORT;
        }
        return WALK_CONTINUE;
    }, true);

    t.log("find current node");

    List<AC_Result> *ac_results = NULL;
    Gotype *expr_to_analyze_gotype = NULL;
    bool expr_to_analyze_gotype_is_error = false;

    auto try_dot_complete = [&](Ast_Node *expr_to_analyze) -> bool {
        bool was_package = false;

        auto init_results = [&]() {
            if (!ac_results)
                ac_results = new_list(AC_Result);
        };

        // try normal dot completions
        do {
            auto results = get_node_dotprops(expr_to_analyze, &was_package, ctx);
            if (!results || !results->len) break;

            init_results();
            For (results) {
                auto r = ac_results->append();
                r->name = it.decl->name;
                r->type = ACR_DECLARATION;
                r->declaration_godecl = it.decl;
                r->declaration_import_path = it.ctx->import_path;
                r->declaration_filename = it.ctx->filename;

                auto res = evaluate_type(it.decl->gotype, it.ctx);
                if (res)
                    r->declaration_evaluated_gotype = res->gotype;
            }
        } while (0);

        // try postfix completions
        do {
            auto results = get_postfix_completions(expr_to_analyze, ctx);
            if (!results || !results->len) break;

            init_results();
            For (results) {
                auto name = get_postfix_completion_name(it);
                if (name) {
                    auto r = ac_results->append();
                    r->name = name;
                    r->type = ACR_POSTFIX;
                    r->postfix_operation = it;
                }
            }
        } while (0);

        if (!ac_results) return false;

        do {
            auto gotype = expr_to_gotype(expr_to_analyze);
            if (!gotype) break;

            auto res = evaluate_type(gotype, ctx);
            if (!res) break;

            bool iserror = is_gotype_error(res);

            auto rres = resolve_type(res);
            if (!rres) break;

            expr_to_analyze_gotype = rres->gotype;
            expr_to_analyze_gotype_is_error = iserror;
        } while (0);

        out->type = AUTOCOMPLETE_DOT_COMPLETE;
        return true;
    };

    switch (situation) {
    case JUST_EXIT:
        return false;
    case FOUND_DOT_COMPLETE_NEED_CRAWL:
        for (auto expr = expr_to_analyze; !try_dot_complete(expr);) {
            if (expr->type() == TS_PARENTHESIZED_EXPRESSION) return false;

            expr = expr->child();
            if (isastnull(expr)) return false;

            Ast_Node *next = NULL;
            while (!isastnull(next = expr->next())) expr = next;
        }
        break;
    case FOUND_DOT_COMPLETE:
        if (!try_dot_complete(expr_to_analyze)) return false;
        break;
    case FOUND_JACK_SHIT:
        if (triggered_by_period) return false;
        keyword_start = pos;
        prefix = "";
        // fallthrough
    case FOUND_LONE_IDENTIFIER: {
        String_Set seen_strings;
        seen_strings.init();
        ac_results = new_list(AC_Result);

        auto add_declaration_result = [&](ccstr name) -> AC_Result* {
            if (!name) return NULL;
            if (seen_strings.has(name)) return NULL;

            auto res = ac_results->append();
            res->name = name;
            res->type = ACR_DECLARATION;

            seen_strings.add(name);

            return res;
        };

        String_Set existing_imports; existing_imports.init();

        auto gofile = find_gofile_from_ctx(ctx);
        if (gofile) {
            SCOPED_FRAME_WITH_MEM(&scoped_table_mem);
            Scoped_Table<Go_Scope_Op*> table;
            {
                SCOPED_MEM(&scoped_table_mem);
                table.init();
            }
            defer { table.cleanup(); };

            For (gofile->scope_ops) {
                if (it.pos > pos) break;

                switch (it.type) {
                case GSOP_OPEN_SCOPE:
                    table.push_scope();
                    break;
                case GSOP_CLOSE_SCOPE:
                    table.pop_scope();
                    break;
                case GSOP_DECL:
                    table.set(it.decl->name, it.copy());
                    break;
                }
            }

            t.log("iterate over scope ops");

            auto entries = table.entries();
            For (entries) {
                auto r = add_declaration_result(it->name);
                if (r) {
                    r->declaration_godecl = it->value->decl;
                    r->declaration_import_path = ctx->import_path;
                    r->declaration_filename = ctx->filename;
                    r->declaration_is_own_file = true;
                    r->declaration_is_scopeop = true;
                    r->declaration_scopeop_depth = it->value->decl_scope_depth;

                    auto res = evaluate_type(it->value->decl->gotype, ctx);
                    if (res)
                        r->declaration_evaluated_gotype = res->gotype;
                }
            }

            t.log("iterate over table entries");

            For (gofile->imports) {
                auto package_name = get_import_package_name(&it);
                if (!package_name) continue;

                auto res = ac_results->append();
                res->name = package_name;
                res->type = ACR_IMPORT;
                res->import_path = it.import_path;
                res->import_is_existing = true;

                existing_imports.add(it.import_path);
            }

            t.log("add imports");

            For (gofile->imports) {
                if (it.package_name_type != GPN_DOT) continue;
                if (!it.import_path) continue;

                auto decls = list_package_decls(it.import_path, LISTDECLS_PUBLIC_ONLY | LISTDECLS_EXCLUDE_METHODS);

                For (decls) {
                    auto res = add_declaration_result(it.decl->name);
                    if (!res) continue;

                    res->declaration_godecl = it.decl;
                    res->declaration_import_path = it.ctx->import_path;
                    res->declaration_filename = it.ctx->filename;

                    auto eval = evaluate_type(it.decl->gotype, it.ctx);
                    if (eval) res->declaration_evaluated_gotype = eval->gotype;
                }
            }

            t.log("add decls from dot imports");

            Parser_It it = *pf->it;
            it.set_pos(keyword_start);

            bool has_three_letters = true;
            char first_three_letters[3];

            for (int i = 0; i < 3; i++) {
                if (it.get_pos() == pos) {
                    has_three_letters = false;
                    break;
                }
                first_three_letters[i] = it.next();
            }

            if (has_three_letters) {
                For (gofile->imports) {
                    if (it.package_name_type == GPN_DOT) continue;

                    auto pkgname = get_import_package_name(&it);
                    if (!pkgname) continue;

                    auto import_path = it.import_path;
                    if (!import_path) continue;

                    auto fuzzy_matches_first_three_letters = [&]() -> bool {
                        int i = 0;
                        for (int j = 0, len = strlen(import_path); j < len; j++)
                            if (import_path[j] == first_three_letters[i])
                                if (++i == 3)
                                    return true;
                        return false;
                    };

                    if (!fuzzy_matches_first_three_letters()) continue;

                    auto results = list_package_decls(it.import_path, LISTDECLS_EXCLUDE_METHODS);
                    if (results) {
                        int count = 0;
                        For (results) {
                            if (!it.decl->name) continue;
                            if (is_name_private(it.decl->name)) continue;

                            if (count++ > 500) break;

                            auto result = add_declaration_result(cp_sprintf("%s.%s", pkgname, it.decl->name));
                            if (result) {
                                result->declaration_godecl = it.decl;
                                result->declaration_import_path = it.ctx->import_path;
                                result->declaration_filename = it.ctx->filename;

                                // wait, is this guaranteed to just always be declaration_import_path
                                result->declaration_package = import_path;

                                auto res = evaluate_type(it.decl->gotype, it.ctx);
                                if (res)
                                    result->declaration_evaluated_gotype = res->gotype;
                            }
                        }
                    }
                }
            }
        }

        // workspace or are immediate deps?
        For (index.packages) {
            if (!it.import_path) continue;
            if (existing_imports.has(it.import_path)) continue;
            if (it.status != GPS_READY) continue;
            if (!it.package_name) continue;
            if (streq(it.import_path, ctx->import_path)) continue;

            // gofile->imports
            // TODO: check if import already exists in file

            if (!index_has_module_containing(it.import_path))
                if (is_import_path_internal(it.import_path))
                    continue;

            auto res = ac_results->append();
            res->name = it.package_name;
            res->type = ACR_IMPORT;
            res->import_path = it.import_path;
        }

        do {
            if (!lone_identifier_struct_literal) break;

            auto gotype = node_to_gotype(lone_identifier_struct_literal);
            if (!gotype) break;

            auto res = evaluate_type(gotype, ctx);
            if (!res) break;

            auto rres = resolve_type(res);
            if (!rres) break;

            auto tmp = new_list(Goresult);
            list_struct_fields(rres, tmp);

            For (tmp) {
                if (!streq(it.ctx->import_path, ctx->import_path))
                    if (!isupper(it.decl->name[0]))
                        continue;

                if (it.decl->type != GODECL_FIELD) continue;

                auto res = ac_results->append();
                res->type = ACR_DECLARATION;
                res->name = it.decl->name;

                res->declaration_godecl = it.decl;
                res->declaration_import_path = it.ctx->import_path;
                res->declaration_filename = it.ctx->filename;
                res->declaration_is_builtin = false;
                res->declaration_is_struct_literal_field = true;

                // struct field gotypes are already evaluated, i believe
                res->declaration_evaluated_gotype = it.decl->gotype;
            }
        } while (0);

        t.log("more crazy shit part 2");

        auto results = list_package_decls(ctx->import_path, LISTDECLS_EXCLUDE_METHODS);
        if (results) {
            For (results) {
                bool is_own_file = (gofile && streq(gofile->filename, it.ctx->filename));

                auto result = add_declaration_result(it.decl->name);
                if (result) {
                    result->declaration_godecl = it.decl;
                    result->declaration_import_path = it.ctx->import_path;
                    result->declaration_filename = it.ctx->filename;
                    result->declaration_is_own_file = is_own_file;

                    auto res = evaluate_type(it.decl->gotype, it.ctx);
                    if (res)
                        result->declaration_evaluated_gotype = res->gotype;
                }
            }
        }

        t.log("list package decls");

        // if the identifier isn't empty, show keywords & builtins
        if (keyword_start < pos) {
            ccstr keywords[] = {
                "package", "import", "const", "var", "func",
                "type", "struct", "interface", "map", "chan",
                "fallthrough", "break", "continue", "goto", "return",
                "go", "defer", "if", "else",
                "for", "range", "switch", "case",
                "default", "select", "new", "make", "iota",
            };

            For (&keywords) {
                auto res = ac_results->append();
                res->name = it;
                res->type = ACR_KEYWORD;
            }

            // add builtins
            {
                auto results = list_package_decls("@builtin", LISTDECLS_EXCLUDE_METHODS);
                if (results) {
                    For (results) {
                        auto res = add_declaration_result(it.decl->name); // i think this is enough?
                        if (res) {
                            res->declaration_godecl = it.decl;
                            res->declaration_import_path = it.ctx->import_path;
                            res->declaration_filename = it.ctx->filename;

                            auto gores = evaluate_type(it.decl->gotype, it.ctx);
                            if (gores)
                                res->declaration_evaluated_gotype = gores->gotype;
                        }
                    }
                }
            }

            t.log("add keywords & builtins");
        }

        if (!ac_results->len) return false;
        out->type = AUTOCOMPLETE_IDENTIFIER;
        break;
    }
    }

    t.log("done generating results");

    if (expr_to_analyze) {
        out->operand_start = expr_to_analyze->start();
        out->operand_end = expr_to_analyze->end();
        out->operand_gotype = expr_to_analyze_gotype;
        out->operand_is_error_type = expr_to_analyze_gotype_is_error;
    }

    out->keyword_start = keyword_start;
    out->keyword_end = pos;

    out->results = ac_results;
    out->prefix = prefix;

    return true;
}

bool Go_Indexer::is_import_path_internal(ccstr import_path) {
    auto parts = make_path(import_path)->parts;
    For (parts)
        if (streq(it, "internal"))
            return true;
    return false;
}

bool Go_Indexer::check_if_still_in_parameter_hint(ccstr filepath, cur2 cur, cur2 hint_start) {
    if (cur < hint_start) return false;

    reload_all_editors();

    auto pf = parse_file(filepath, LANG_GO, true);
    if (!pf) return false;
    defer { free_parsed_file(pf); };

    auto buf = pf->it->buffer_params.it.buf;

    // try to close string if we're in one
    char string_close_char = 0;

    cur2 bytecur = cur;
    bytecur.x = buf->idx_cp_to_byte(bytecur.y, bytecur.x);

    find_nodes_containing_pos(pf->root, bytecur, true, [&](auto it) -> Walk_Action {
        switch (it->type()) {
        case TS_RAW_STRING_LITERAL:
        case TS_INTERPRETED_STRING_LITERAL: {
            auto get_char_at_pos = [&](cur2 p) -> uchar {
                auto old = pf->it->get_pos();
                defer { pf->it->set_pos(old); };

                pf->it->set_pos(p);
                return pf->it->peek();
            };

            auto start_pos = it->start();
            auto start_ch = get_char_at_pos(start_pos);
            if (start_ch == '"' || start_ch == '`') {
                auto end_pos = it->end();
                auto last_pos = new_cur2(relu_sub(end_pos.x, 1), end_pos.y);

                auto last_ch = get_char_at_pos(last_pos);
                if (!(bytecur == end_pos && last_ch == start_ch && last_pos != start_pos))
                    string_close_char = start_ch;
            }
            return WALK_ABORT;
        }
        }
        return WALK_CONTINUE;
    }, true);

    ccstr suffix = "_";
    if (string_close_char)
        suffix = cp_sprintf("%c", string_close_char);
    suffix = cp_sprintf("%s)}}}}}}}}}}}}}}}}", suffix);

    if (!truncate_parsed_file(pf, cur, suffix)) return false;
    defer { ts_tree_delete(pf->tree); };

    bool ret = false;

    // hint_start is already in byte index

    find_nodes_containing_pos(pf->root, hint_start, true, [&](auto it) -> Walk_Action {
        if (it->start() == hint_start)
            if (it->type() == TS_ARGUMENT_LIST)
                if (bytecur < it->end()) {
                    ret = true;
                    return WALK_ABORT;
                }
        return WALK_CONTINUE;
    });

    return ret;
}

Parameter_Hint *Go_Indexer::parameter_hint(ccstr filepath, cur2 pos) {
    Timer t;
    t.init("parameter_hint");

    reload_all_editors();

    t.log("reload files");

    auto pf = parse_file(filepath, LANG_GO, true);
    if (!pf) return NULL;
    defer { free_parsed_file(pf); };

    if (pf->it->type != IT_BUFFER) {
        error("can only do parameter hint on buffer parser");
        return NULL;
    }

    if (!truncate_parsed_file(pf, pos, "_)}}}}}}}}}}}}}}}}")) return NULL;
    defer { ts_tree_delete(pf->tree); };

    auto go_back_until_non_space = [&]() -> cur2 {
        auto it = new_object(Parser_It);
        memcpy(it, pf->it, sizeof(Parser_It));
        it->set_pos(pos);
        while (!it->bof() && isspace(it->peek())) it->prev();
        return it->get_pos();
    };

    auto start_pos = go_back_until_non_space();

    auto buf = pf->it->buffer_params.it.buf;
    pos.x = buf->idx_cp_to_byte(pos.y, pos.x);
    start_pos.x = buf->idx_cp_to_byte(start_pos.y, start_pos.x);

    Ast_Node *func_expr = NULL;
    cur2 call_args_start;
    int current_param = -1;

    t.log("prepare shit");

    find_nodes_containing_pos(pf->root, start_pos, false, [&](auto node) -> Walk_Action {
        switch (node->type()) {
        case TS_TYPE_CONVERSION_EXPRESSION:
        case TS_CALL_EXPRESSION: {
            if (cmp_pos_to_node(pos, node) != 0) return WALK_ABORT;

            Ast_Node *func = NULL, *args = NULL;

            if (node->type() == TS_TYPE_CONVERSION_EXPRESSION) {
                func = node->field(TSF_TYPE);
                args = node->field(TSF_OPERAND);
            } else {
                func = node->field(TSF_FUNCTION);
                args = node->field(TSF_ARGUMENTS);
            }

            if (cmp_pos_to_node(pos, args) < 0) break;
            if (isastnull(func) || isastnull(args)) break;

            func_expr = func;

            if (node->type() == TS_TYPE_CONVERSION_EXPRESSION) {
                bool found = false;
                FOR_ALL_NODE_CHILDREN (node) {
                    if (it->type() == TS_LPAREN) {
                        call_args_start = it->start();
                        found = true;
                        break;
                    }
                }
                if (!found) break;

                if (cmp_pos_to_node(pos, args, true) == 0)
                    current_param = 0;
            } else {
                call_args_start = args->start();

                int i = 0;
                FOR_NODE_CHILDREN (args) {
                    if (cmp_pos_to_node(pos, it, true) == 0) {
                        current_param = i;
                        break;
                    }
                    i++;
                }
            }

            // don't abort, if there's a deeper func_expr we want to use that one
            break;
        }
        case TS_LPAREN: {
            auto prev = node->prev_all();
            if (!isastnull(prev)) {
                func_expr = prev;
                if (func_expr->type() == TS_ERROR) {
                    func_expr = func_expr->child();
                    for (Ast_Node *next; (next = func_expr->next());)
                        func_expr = next;
                }
                call_args_start = node->start();
            } else {
                auto parent = node->parent();
                if (!isastnull(parent) && parent->type() == TS_ERROR) {
                    auto parent_prev = parent->prev_all();
                    if (!isastnull(parent_prev)) {
                        func_expr = parent_prev;
                        call_args_start = node->start();
                    }
                }
            }
            return WALK_ABORT;
        }
        }
        return WALK_CONTINUE;
    });

    t.log("find function node");

    if (!func_expr) return NULL;

    auto ctx = filepath_to_ctx(filepath);
    if (!ctx) return NULL;

    auto gotype = expr_to_gotype(func_expr);
    if (!gotype) return NULL;

    auto res = evaluate_type(gotype, ctx);
    if (!res) return NULL;

    auto rres = resolve_type(res);
    if (!rres) return NULL;

    if (rres->gotype->type != GOTYPE_FUNC) return NULL;

    // should we try to "normalize" the types in res?  like say we have
    // packages foo and bar bar calls a func foo.Func foo includes package blah
    // and includes a type blah.Type in foo.Func if we just render "blah.Type"
    // it won't really be clear what blah refers to

    t.log("get type of function");

    auto hint = new_object(Parameter_Hint);
    hint->gotype = rres->gotype->copy();
    hint->call_args_start = call_args_start;
    hint->current_param = current_param;

    t.total();
    return hint;
}

void Go_Indexer::list_struct_fields(Goresult *type, List<Goresult> *ret) {
    auto t = type->gotype;
    switch (t->type) {
    case GOTYPE_POINTER:
        list_struct_fields(type->wrap(t->pointer_base), ret);
        break;
    case GOTYPE_STRUCT:
        For (t->struct_specs) {
            // recursively list methods for embedded fields
            if (it.field->field_is_embedded) {
                auto res = resolve_type(it.field->gotype, type->ctx);
                if (res)
                    list_struct_fields(res, ret);
            }
            ret->append(type->wrap(it.field));
        }
        break;
    }
}

bool Go_Indexer::list_interface_methods(Goresult *interface_type, List<Goresult> *out) {
    auto gotype = interface_type->gotype;
    if (gotype->type != GOTYPE_INTERFACE) return false;

    auto specs = gotype->interface_specs;
    if (!specs) return false;

    For (specs) {
        if (!it.field) return false; // this shouldn't happen

        auto method = it.field;
        if (method->field_is_embedded) {
            auto rres = resolve_type(method->gotype, interface_type->ctx);
            if (!rres) return false;
            if (!list_interface_methods(rres, out)) return false;
        } else {
            if (method->gotype->type != GOTYPE_FUNC) return false;
            out->append(interface_type->wrap(method));
        }
    }

    return true;
}

List<Goresult> *Go_Indexer::list_interface_methods(Goresult *interface_type) {
    Frame frame;

    auto ret = new_list(Goresult);
    if (!list_interface_methods(interface_type, ret)) {
        frame.restore();
        return NULL;
    }

    return ret;
}

bool Go_Indexer::list_type_methods(ccstr type_name, ccstr import_path, List<Goresult> *out) {
    auto results = list_package_decls(import_path);
    if (!results) return false;

    For (results) {
        auto decl = it.decl;

        if (decl->type != GODECL_FUNC) continue;

        auto functype = decl->gotype;
        if (!functype->func_recv) continue;

        auto recv = unpointer_type(functype->func_recv);
        if (!recv) continue;

        if (recv->type == GOTYPE_GENERIC) recv = recv->base;

        if (recv->type != GOTYPE_ID) continue;
        if (!streq(recv->id_name, type_name)) continue;

        out->append(&it);
    }

    return true;
}

void Go_Indexer::list_dotprops(Goresult *type_res, Goresult *resolved_type_res, List<Goresult> *ret) {
    auto tmp = new_list(Goresult);

    Actually_List_Dotprops_Opts opts; ptr0(&opts);
    opts.out = tmp;
    opts.depth = 0;
    opts.seen_embeds.init();

    actually_list_dotprops(type_res, resolved_type_res, &opts);

    String_Set seen; seen.init();
    auto ctx = type_res->ctx;

    for (int i = 0; i < tmp->len; i++) {
        auto &it = tmp->at(tmp->len - i - 1);

        if (!it.decl->name) continue;
        if (seen.has(it.decl->name)) continue;

        seen.add(it.decl->name);
        ret->append(&it);
    }
}

void Go_Indexer::actually_list_dotprops(Goresult *type_res, Goresult *resolved_type_res, Actually_List_Dotprops_Opts *opts) {
    auto resolve_embedded_field = [&](Gotype *gotype, Go_Ctx *ctx) -> Goresult* {
        bool is_alias = false;

        do {
            gotype = unpointer_type(gotype);

            List<Goresult*> *generic_args = NULL;
            if (gotype->type == GOTYPE_GENERIC) {
                generic_args = new_list(Goresult*);
                For (gotype->generic_args)
                    generic_args->append(make_goresult(it, ctx));
                gotype = gotype->generic_base;
            }

            if (!is_type_ident(gotype)) return NULL;

            auto res = resolve_type_to_decl(gotype, ctx);
            if (!res) return NULL;

            auto key = cp_sprintf("%s:%s", ctx_to_filepath(res->ctx), res->decl->name);
            if (opts->seen_embeds.has(key)) return NULL;
            opts->seen_embeds.add(key);

            auto decl = res->decl;
            is_alias = decl->is_alias;

            gotype = decl->gotype;
            if (generic_args && decl->type == GODECL_TYPE && decl->type_params) {
                auto newret = do_generic_subst(gotype, decl->type_params, generic_args);
                if (newret) gotype = newret;
            }
            ctx = res->ctx;
        } while (is_alias);

        return make_goresult(gotype, ctx);
    };

    auto process_field = [&](Godecl *field, Go_Ctx *ctx) {
        if (field->field_is_embedded) {
            auto rres = resolve_embedded_field(field->gotype, ctx);
            if (rres) {
                opts->depth++;
                actually_list_dotprops(make_goresult(field->gotype, ctx), rres, opts);
                opts->depth--;
            }
        }

        auto val = field->copy(); // is this gonna substantially slow things down?
        val->field_depth = opts->depth;
        opts->out->append(resolved_type_res->wrap(val));
    };

    auto resolved_type = resolved_type_res->gotype;
    switch (resolved_type->type) {
    case GOTYPE_POINTER:
    case GOTYPE_RECEIVE:
    case GOTYPE_ASSERTION: {
        auto new_resolved_type_res = resolved_type_res->wrap(resolved_type->base);

        opts->depth++;
        actually_list_dotprops(type_res, new_resolved_type_res, opts);
        opts->depth--;
        return;
    }

    case GOTYPE_CONSTRAINT: {
        // as far as i understand, if a type constraint is an interface
        // with methods, it can't be joined with other interfaces.
        if (resolved_type->constraint_terms->len != 1) break;

        auto term = resolved_type->constraint_terms->at(0);
        auto new_resolved_type_res = resolved_type_res->wrap(term);

        opts->depth++;
        actually_list_dotprops(type_res, new_resolved_type_res, opts);
        opts->depth--;
        return;
    }

    case GOTYPE_STRUCT: {
        For (resolved_type->struct_specs)
            process_field(it.field, resolved_type_res->ctx);
        break;
    }

    case GOTYPE_INTERFACE: {
        // if we have an GO_INTERFACE_SPEC_ELEM, i.e. a union with more than one term, then:
        //
        //  - unions can't have terms that specify methods
        //  - interfaces are the intersection of their elems
        //  - means the interface itself has no elements
        //  - so we have nothing to add, break out
        For (resolved_type->interface_specs)
            if (it.type == GO_INTERFACE_SPEC_ELEM)
                goto getout;

        // no elems found
        For (resolved_type->interface_specs)
            process_field(it.field, resolved_type_res->ctx);
        break;
    }
    }
getout:

    if (opts->fields_only) return;

    type_res = unpointer_type(type_res);

    // "unalias" type - if it is an alias, follow it
    while (true) {
        if (!is_type_ident(type_res->gotype)) break;

        auto res = resolve_type_to_decl(type_res->gotype, type_res->ctx);
        if (!res) break;

        if (res->decl->type != GODECL_TYPE) break;
        if (!res->decl->is_alias) break;

        // we found an alias! follow it
        type_res = res->wrap(res->decl->gotype);
    }

    auto type = type_res->gotype;
    ccstr type_name = NULL;
    ccstr target_import_path = NULL;

    switch (type->type) {
    case GOTYPE_GENERIC:
    case GOTYPE_ASSERTION:
    case GOTYPE_RECEIVE:
        type = type->base;
        break;
    }

    switch (type->type) {
    case GOTYPE_ID:
        // TODO: if decl of id is not a toplevel, exit (locally defined types won't have methods)
        type_name = type->id_name;
        target_import_path = type_res->ctx->import_path;
        break;
    case GOTYPE_SEL:
        type_name = type->sel_sel;
        target_import_path = find_import_path_referred_to_by_id(type->sel_name, type_res->ctx);
        break;
    default:
        return;
    }

    list_type_methods(type_name, target_import_path, opts->out);
}

ccstr remove_ats_from_path(ccstr s) {
    auto path = make_path(s);
    For (path->parts) {
        auto atpos = strchr((char*)it, '@');
        if (atpos) *atpos = '\0';
    }
    return path->str();
}

Go_Ctx *Go_Indexer::filepath_to_ctx(ccstr filepath) {
    if (!filepath) return NULL;

    auto import_path = filepath_to_import_path(cp_dirname(filepath));
    if (!import_path) return NULL;

    auto ret = new_object(Go_Ctx);
    ret->import_path = import_path;
    ret->filename = cp_basename(filepath);
    return ret;
}

ccstr Go_Indexer::ctx_to_filepath(Go_Ctx *ctx) {
    if (!ctx) return NULL;

    auto dir = get_package_path(ctx->import_path);
    if (!dir) return NULL;

    return path_join(dir, ctx->filename);
}

Go_File *Go_Indexer::find_gofile_from_ctx(Go_Ctx *ctx, Go_Package **out) {
    if (!ctx) return NULL;

    auto pkg = find_up_to_date_package(ctx->import_path);
    if (!pkg) return NULL;

    auto check = [&](auto it) { return streq(it->filename, ctx->filename); };
    auto ret = pkg->files->find(check);
    if (!ret) return NULL;

    if (out) *out = pkg;
    return ret;
}

ccstr Go_Indexer::filepath_to_import_path(ccstr path_str) {
    auto ret = module_resolver.resolved_path_to_import_path(path_str);
    if (ret) return ret;

    auto path = make_path(path_str);
    auto goroot_path = make_path(goroot);

    if (!goroot_path->contains(path)) return NULL;

    auto parts = new_list(ccstr, path->parts->len - goroot_path->parts->len);
    for (u32 i = goroot_path->parts->len; i < path->parts->len; i++)
        parts->append(path->parts->at(i));

    Path p;
    p.init(parts);
    return p.str('/');
}

void Go_Indexer::init() {
    Timer t; t.init("Go_Indexer::init");
    ptr0(this);

    mem.init("indexer_mem");
    final_mem.init("final_mem");
    ui_mem.init("ui_mem");
    scoped_table_mem.init("scoped_table_mem");

    SCOPED_MEM(&mem);

    message_queue.init();

    {
        SCOPED_FRAME();
        cp_strcpy_fixed(current_exe_path, cp_dirname(get_executable_path()));
    }

    auto init_buildenv = [&]() {
        Timer t; t.init("init_buildenv");
        if (!GHBuildEnvInit()) return false;
        if (!GHBuildEnvGoVersionSupported()) return false;
        return true;
    };

    if (!init_buildenv())
        cp_panic("Please make sure Go version 1.13+ is installed and accessible through your PATH.");

    auto copystr = [&](ccstr s) {
        auto ret = cp_strdup(s);
        GHFree((void*)s);
        return ret[0] == '\0' ? NULL : ret;
    };

    {
        auto vars = GHGetGoEnvVars();
        if (!vars) cp_panic("Unable to detect Go installation.");
        defer { GHFreeGoEnvVars(vars); };

        if (streq(vars->goroot, ""))
            cp_panic("Unable to detect GOROOT. Please make sure Go is installed and accessible through your PATH.");
        if (streq(vars->gomodcache, ""))
            cp_panic("Unable to detect GOMODCACHE. Please make sure Go is installed and accessible through your PATH.");

        goroot = cp_strdup(vars->goroot);
        gomodcache = cp_strdup(vars->gomodcache);

        auto goroot_without_src = goroot;
        goroot = path_join(goroot, "src");

        if (check_path(goroot) != CPR_DIRECTORY) {
            auto msg = cp_sprintf(
                "We found the following GOROOT:\n\n%s\n\nIt doesn't appear to be valid. The program will keep running, but code intelligence might not fully work.",
                goroot_without_src
            );
            // This is called from main thread, so we can just call tell_user().
            tell_user(msg, "Warning");
        }
    }

    lock.init();

    start_writing();
}

bool Go_Indexer::acquire_lock(Indexer_Status new_status, bool just_try) {
    go_print("[acquire] %s", indexer_status_str(new_status));

    if (status == new_status) {
        // read lock can only be acquired once
        if (status == IND_READING)
            return false;

        reacquires++;
        return true;
    }

    if (just_try) {
        if (!lock.try_enter())
            return false;
    } else {
        lock.enter();
    }

    status = new_status;
    reacquires++;
    return true;
}

void Go_Indexer::release_lock(Indexer_Status expected_status) {
    go_print("[release] %s", indexer_status_str(expected_status));

    if (status != expected_status) {
        auto msg = "Go_Indexer::release_lock() called with status mismatch (want %d, got %d)";
        msg = cp_sprintf(msg, expected_status, status);
        cp_panic(msg);
    }

    if (--reacquires == 0) {
        status = IND_READY;
        lock.leave();
    }
}

void Go_Indexer::start_writing(bool skip_if_already_started) {
    if (skip_if_already_started)
        if (status == IND_WRITING)
            return;

    acquire_lock(IND_WRITING);
}

void Go_Indexer::stop_writing() {
    release_lock(IND_WRITING);
}

// i don't think this is actually called right now...
// we're just letting the OS reclaim all this shit when the program exits
void Go_Indexer::cleanup() {
    if (bgthread) {
        kill_thread(bgthread);
        close_thread_handle(bgthread);
        bgthread = NULL;
    }

    mem.cleanup();
    final_mem.cleanup();
    ui_mem.cleanup();
    scoped_table_mem.cleanup();
    lock.cleanup();
}

List<Godecl> *Go_Indexer::parameter_list_to_fields(Ast_Node *params) {
    u32 count = 0;

    FOR_NODE_CHILDREN (params) {
        auto param = it;
        auto type_node = it->field(TSF_TYPE);

        u32 id_count = 0;
        FOR_NODE_CHILDREN (param) {
            if (it->eq(type_node)) break;
            id_count++;
        }

        if (!id_count) id_count = 1;
        count += id_count;
    }

    auto ret = new_list(Godecl, count);

    FOR_NODE_CHILDREN (params) {
        auto type_node = it->field(TSF_TYPE);
        auto param_node = it;
        bool is_variadic = (param_node->type() == TS_VARIADIC_PARAMETER_DECLARATION);
        bool id_added = false;

        FOR_NODE_CHILDREN (param_node) {
            if (it->eq(type_node)) break;

            id_added = true;

            auto field = ret->append();
            field->type = GODECL_PARAM;
            field->decl_start = param_node->start();
            field->decl_end = param_node->end();
            field->spec_start = param_node->start();
            field->name_start = it->start();
            field->name_end = it->end();
            field->name = it->string();
            field->gotype = node_to_gotype(type_node);

            if (is_variadic) {
                auto t = new_gotype(GOTYPE_SLICE);
                t->slice_base = field->gotype;
                t->slice_is_variadic = true;
                field->gotype = t;
            }
        }

        if (!id_added) {
            auto field = ret->append();
            field->type = GODECL_PARAM;
            field->decl_start = param_node->start();
            field->decl_end = param_node->end();
            field->spec_start = param_node->start();
            field->name_start = param_node->start();
            field->name_end = param_node->start();
            field->name = "_";
            field->gotype = node_to_gotype(type_node);

            if (is_variadic) {
                auto t = new_gotype(GOTYPE_SLICE);
                t->slice_base = field->gotype;
                t->slice_is_variadic = true;
                field->gotype = t;
            }
        }
    }

    return ret;
}

bool Go_Indexer::node_func_to_gotype_sig(Ast_Node *params, Ast_Node *result, Go_Func_Sig *sig) {
    if (params->type() != TS_PARAMETER_LIST) return false;

    sig->params = parameter_list_to_fields(params);

    if (!isastnull(result)) {
        if (result->type() == TS_PARAMETER_LIST) {
            sig->result = parameter_list_to_fields(result);
        } else {
            sig->result = new_list(Godecl, 1);
            auto field = sig->result->append();
            field->type = GODECL_PARAM;
            field->decl_start = result->start();
            field->decl_end = result->end();
            field->spec_start = result->start();
            field->name = NULL;
            field->gotype = node_to_gotype(result);
        }
    }

    return true;
}

Godecl *Go_Indexer::read_method_spec_into_field(Ast_Node *node, int field_order) {
    auto name_node = node->field(TSF_NAME);
    if (!name_node) return NULL;

    auto field = new_object(Godecl);
    field->type = GODECL_FIELD;
    field->name = name_node->string();
    field->gotype = new_gotype(GOTYPE_FUNC);
    field->decl_start = node->start();
    field->decl_end =  node->end();
    field->spec_start = node->start();
    field->name_start = name_node->start();
    field->name_end = name_node->end();
    field->field_order = field_order;

    node_func_to_gotype_sig(
        node->field(TSF_PARAMETERS),
        node->field(TSF_RESULT),
        &field->gotype->func_sig
    );
    return field;
}

List<Go_Struct_Spec> *Go_Indexer::read_struct_field_into_specs(Ast_Node *field_node, int starting_field_order, bool is_toplevel) {
    auto tag_node = field_node->field(TSF_TAG);
    auto type_node = field_node->field(TSF_TYPE);
    if (isastnull(type_node)) return NULL;

    auto field_type = node_to_gotype(type_node);
    if (!field_type) return NULL;

    auto ret = new_list(Go_Struct_Spec);

    if (!field_node->child()->eq(type_node)) {
        FOR_NODE_CHILDREN (field_node) {
            if (it->eq(type_node)) break;

            auto field = new_object(Godecl);
            field->type = GODECL_FIELD;
            field->is_toplevel = is_toplevel;
            field->gotype = field_type;
            field->name = it->string();
            field->name_start = it->start();
            field->name_end = it->end();
            field->spec_start = field_node->start();
            field->decl_start = field_node->start();
            field->decl_end = field_node->end();
            field->field_order = starting_field_order + ret->len;

            auto spec = ret->append();
            spec->field = field;
            if (!isastnull(tag_node))
                spec->tag = tag_node->string();
        }
    } else {
        auto unptr_type = unpointer_type(field_type);
        if (!unptr_type) return NULL;

        ccstr field_name = NULL;

        if (unptr_type->type == GOTYPE_GENERIC)
            unptr_type = unptr_type->generic_base;

        switch (unptr_type->type) {
        case GOTYPE_SEL:
            field_name = unptr_type->sel_sel;
            break;
        case GOTYPE_ID:
            field_name = unptr_type->id_name;
            break;
        }

        if (!field_name) return NULL;

        auto field = new_object(Godecl);
        field->type = GODECL_FIELD;
        field->is_toplevel = is_toplevel;
        field->field_is_embedded = true;
        field->gotype = field_type;
        field->spec_start = type_node->start();
        field->decl_start = type_node->start();
        field->decl_end = type_node->end();
        field->name = field_name;
        field->name_start = type_node->start();
        field->field_order = starting_field_order + ret->len;

        auto spec = ret->append();
        spec->field = field;
        if (!isastnull(tag_node))
            spec->tag = tag_node->string();
    }

    return ret;
}

Gotype *Go_Indexer::node_to_gotype(Ast_Node *node, bool toplevel) {
    if (isastnull(node)) return NULL;

    Gotype *ret = NULL;

    switch (node->type()) {
    // case TS_SIMPLE_TYPE:
    case TS_CONSTRAINT_ELEM: {
        ret = new_gotype(GOTYPE_CONSTRAINT);
        ret->constraint_terms = new_list(Gotype*);

        FOR_NODE_CHILDREN (node) {
            if (it->type() != TS_CONSTRAINT_TERM) continue; // ???

            auto child_node = it->child();
            if (isastnull(child_node)) continue; // ???

            auto term = node_to_gotype(child_node);
            if (!term) continue; // just break out of the whole thing?

            auto prev = child_node->prev_all();
            if (!isastnull(prev) && prev->type() == TS_TILDE) {
                auto tmp = new_gotype(GOTYPE_CONSTRAINT_UNDERLYING);
                tmp->constraint_underlying_base = term;
                term = tmp;
            }

            ret->constraint_terms->append(term);
        }
        break;
    }

    case TS_QUALIFIED_TYPE: {
        auto pkg_node = node->field(TSF_PACKAGE);
        if (isastnull(pkg_node)) break;
        auto name_node = node->field(TSF_NAME);
        if (isastnull(name_node)) break;

        ret = new_gotype(GOTYPE_SEL);
        ret->sel_name = pkg_node->string();
        ret->sel_sel = name_node->string();
        break;
    }

    case TS_TYPE_IDENTIFIER:
        ret = new_gotype(GOTYPE_ID);
        ret->id_name = node->string();
        ret->id_pos = node->start();
        break;

    case TS_POINTER_TYPE: {
        auto base = node_to_gotype(node->child());
        if (!base) break;

        ret = new_gotype(GOTYPE_POINTER);
        ret->pointer_base = base;
        break;
    }

    case TS_GENERIC_TYPE: {
        auto base = node_to_gotype(node->field(TSF_TYPE));
        if (!base) break;

        ret = new_gotype(GOTYPE_GENERIC);
        ret->generic_base = base;

        auto args_node = node->field(TSF_TYPE_ARGUMENTS);
        if (!isastnull(args_node)) {
            bool ok = true;
            auto args = new_list(Gotype*);

            FOR_NODE_CHILDREN (args_node) {
                auto arg_gotype = node_to_gotype(it);
                if (!arg_gotype) {
                    ok = false;
                    break;
                }
                args->append(arg_gotype);
            }

            if (!ok) return NULL;

            ret->generic_args = args;
        }
        break;
    }

    case TS_PARENTHESIZED_TYPE:
        ret = node_to_gotype(node->child());
        break;

    case TS_IMPLICIT_LENGTH_ARRAY_TYPE:
    case TS_ARRAY_TYPE:
    case TS_SLICE_TYPE: {
        auto base = node_to_gotype(node->field(TSF_ELEMENT));
        if (!base) break;

        switch (node->type()) {
        case TS_IMPLICIT_LENGTH_ARRAY_TYPE:
        case TS_ARRAY_TYPE:
            ret = new_gotype(GOTYPE_ARRAY);
            break;
        case TS_SLICE_TYPE:
            ret = new_gotype(GOTYPE_SLICE);
            break;
        }
        ret->base = base;
        break;
    }

    case TS_MAP_TYPE: {
        auto key = node_to_gotype(node->field(TSF_KEY));
        if (!key) break;
        auto value = node_to_gotype(node->field(TSF_VALUE));
        if (!value) break;

        ret = new_gotype(GOTYPE_MAP);
        ret->map_key = key;
        ret->map_value = value;
        break;
    }

    case TS_STRUCT_TYPE: {
        auto fieldlist_node = node->child();
        if (isastnull(fieldlist_node)) break;

        ret = new_gotype(GOTYPE_STRUCT);
        ret->struct_specs = new_list(Go_Struct_Spec);

        FOR_NODE_CHILDREN (fieldlist_node) {
            auto specs = read_struct_field_into_specs(it, ret->struct_specs->len, toplevel);
            if (!specs) continue;
            ret->struct_specs->concat(specs);
        }
        break;
    }

    case TS_INTERFACE_TYPE: {
        ret = new_object(Gotype);
        ret->type = GOTYPE_INTERFACE;
        ret->interface_specs = new_list(Go_Interface_Spec, node->child_count());

        int i = 0;
        FOR_NODE_CHILDREN (node) {
            defer { i++; };

            switch (it->type()) {
            case TS_METHOD_SPEC: {
                auto field = read_method_spec_into_field(it, ret->interface_specs->len);
                if (!field) break;

                auto spec = ret->interface_specs->append();
                spec->type = GO_INTERFACE_SPEC_METHOD;
                spec->field = field;
                break;
            }
            case TS_CONSTRAINT_ELEM:
            case TS_INTERFACE_TYPE_NAME: {
                auto node = it;
                if (node->type() == TS_INTERFACE_TYPE_NAME)
                    node = node->child();

                if (isastnull(node)) continue;

                auto gotype = node_to_gotype(node);
                if (!gotype) break;

                auto field = new_object(Godecl);
                field->type = GODECL_FIELD;
                field->name = NULL;
                field->field_is_embedded = true;
                field->gotype = gotype;
                field->spec_start = node->start();
                field->decl_start = node->start();
                field->decl_end = node->end();
                field->field_order = i;

                auto spec = ret->interface_specs->append();
                if (node->type() == TS_CONSTRAINT_ELEM)
                    spec->type = GO_INTERFACE_SPEC_ELEM;
                else
                    spec->type = GO_INTERFACE_SPEC_EMBEDDED;
                spec->field = field;
                break;
            }
            }
        }
        break;
    }

    case TS_CHANNEL_TYPE: {
        auto base = node_to_gotype(node->field(TSF_VALUE));
        if (!base) break;

        ret = new_gotype(GOTYPE_CHAN);
        ret->chan_base = base;
        break;
    }

    case TS_FUNCTION_TYPE:
    case TS_FUNC_LITERAL:
        ret = new_gotype(GOTYPE_FUNC);
        if (!node_func_to_gotype_sig(node->field(TSF_PARAMETERS), node->field(TSF_RESULT), &ret->func_sig))
            ret = NULL;
        break;
    }

    return ret;
}

void Go_Indexer::import_spec_to_decl(Ast_Node *spec_node, Godecl *decl) {
    decl->type = GODECL_IMPORT;
    decl->spec_start = spec_node->start();

    auto name_node = spec_node->field(TSF_NAME);
    if (!isastnull(name_node)) {
        decl->name = name_node->string();
        decl->name_start = name_node->start();
        decl->name_end = name_node->end();
    }

    auto path_node = spec_node->field(TSF_PATH);
    if (!isastnull(path_node))
        decl->import_path = path_node->string();
}

bool Go_Indexer::assignment_to_decls(List<Ast_Node*> *lhs, List<Ast_Node*> *rhs, New_Godecl_Func new_godecl, bool range) {
    if (!lhs->len || !rhs->len) return false;

    if (rhs->len == 1) {
        if (range) {
            auto range_base = expr_to_gotype(rhs->at(0));

            Fori (lhs) {
                if (i >= 2) break;

                if (!it) continue;
                if (it->type() != TS_IDENTIFIER) continue;

                auto gotype = new_gotype(GOTYPE_LAZY_RANGE);
                gotype->lazy_range_base = range_base;
                gotype->lazy_range_is_index = (!i);

                auto decl = new_godecl();
                decl->name = it->string();
                decl->name_start = it->start();
                decl->name_end = it->end();
                decl->gotype = gotype;
            }
            return true;
        }

        auto multi_type = expr_to_gotype(rhs->at(0));
        if (!multi_type) return false;

        /*
        if (multi_type->type != GOTYPE_MULTI) {
            if (lhs->len != 1) {
                // TODO: are there legitimate cases where this will happen?
                return false;
            }

            auto it = lhs->at(0);

            auto decl = new_godecl();
            decl->name = it->string();
            decl->name_start = it->start();
            decl->name_end = it->end();
            decl->gotype = multi_type;
            return true;
        }
        */

        u32 index = 0;
        For (lhs) {
            defer { index++; };

            if (it->type() != TS_IDENTIFIER) continue;

            auto name = it->string();
            if (streq(name, "_")) continue;

            auto gotype = new_gotype(GOTYPE_LAZY_ONE_OF_MULTI);
            gotype->lazy_one_of_multi_base = multi_type;
            gotype->lazy_one_of_multi_index = index;
            gotype->lazy_one_of_multi_is_single = (lhs->len == 1);

            auto decl = new_godecl();
            decl->name = it->string();
            decl->name_start = it->start();
            decl->name_end = it->end();
            decl->gotype = gotype;
        }

        return true;
    }

    if (lhs->len == rhs->len) {
        for (u32 i = 0; i < lhs->len; i++) {
            auto name_node = lhs->at(i);
            if (name_node->type() != TS_IDENTIFIER) continue;

            auto gotype = expr_to_gotype(rhs->at(i));
            if (!gotype) continue;

            if (gotype->type == GOTYPE_MULTI || gotype->type == GOTYPE_RANGE)
                continue;

            if (gotype->type == GOTYPE_ASSERTION || gotype->type == GOTYPE_RECEIVE)
                gotype = gotype->base;

            auto decl = new_godecl();
            decl->name = name_node->string();
            decl->name_start = name_node->start();
            decl->name_end = name_node->end();
            decl->gotype = gotype;
        }

        return true;
    }

    return false;
}

// @Functional I should start labeling which functions are "functional" in the
// sense that they don't affect any state in Go_Indexer, and basically just
// input-output.  This function is like that.
void Go_Indexer::node_to_decls(Ast_Node *node, List<Godecl> *results, ccstr filename, Pool *target_pool) {
    auto parent = node->parent();
    bool is_toplevel = (!isastnull(parent) && parent->type() == TS_SOURCE_FILE);

    auto new_result = [&]() -> Godecl * {
        auto decl = results->append();
        decl->decl_start = node->start();
        decl->decl_end = node->end();

        decl->is_toplevel = is_toplevel;

        return decl;
    };

    auto save_decl = [&](Godecl *decl) {
        if (!target_pool) return;
        SCOPED_MEM(target_pool);
        memcpy(decl, decl->copy(), sizeof(Godecl));
    };

    auto parse_type_params = [&](Ast_Node *node) -> List<Godecl> * {
        if (isastnull(node)) return NULL;

        auto ret = new_list(Godecl);
        FOR_NODE_CHILDREN (node) {
            auto name_node = it->field(TSF_NAME);
            if (isastnull(name_node)) return NULL;

            auto type_node = it->field(TSF_TYPE);
            if (isastnull(type_node)) return NULL;

            auto obj = ret->append();
            obj->type = GODECL_TYPE_PARAM;
            obj->name = name_node->string();
            obj->name_start = name_node->start();
            obj->name_end = name_node->end();
            obj->decl_start = node->start();
            obj->decl_end = node->end();
            obj->spec_start = node->start();
            obj->is_toplevel = false;
            obj->gotype = node_to_gotype(type_node, is_toplevel);
        }
        return ret;
    };

    auto node_type = node->type();
    switch (node_type) {
    case TS_FUNCTION_DECLARATION:
    case TS_METHOD_DECLARATION: {
        auto name_node = node->field(TSF_NAME);
        if (isastnull(name_node)) break;

        auto name = name_node->string();

        if (node_type == TS_FUNCTION_DECLARATION)
            if (is_toplevel && streq(name, "init"))
                break;

        auto type_params = parse_type_params(node->field(TSF_TYPE_PARAMETERS));

        auto params_node = node->field(TSF_PARAMETERS);
        auto result_node = node->field(TSF_RESULT);

        auto gotype = new_gotype(GOTYPE_FUNC);

        if (!node_func_to_gotype_sig(params_node, result_node, &gotype->func_sig)) break;

        bool ok = true;
        do {
            if (node->type() != TS_METHOD_DECLARATION) break;

            auto recv_node = node->field(TSF_RECEIVER);
            if (isastnull(recv_node)) break;

            auto child = recv_node->child();
            if (isastnull(child)) break;

            auto recv_type = child->field(TSF_TYPE);
            if (isastnull(recv_type)) break;

            auto func_recv = node_to_gotype(recv_type);
            if (!func_recv) {
                ok = false;
                break;
            }

            gotype->func_recv = func_recv;
        } while (0);

        if (!ok) break;

        auto decl = new_result();
        decl->type = GODECL_FUNC;
        decl->spec_start = node->start();
        decl->name = name;
        decl->name_start = name_node->start();
        decl->name_end = name_node->end();
        decl->gotype = gotype;
        decl->type_params = type_params;
        save_decl(decl);
        break;
    }

    case TS_LABELED_STATEMENT: {
        auto label = node->field(TSF_LABEL);
        if (isastnull(label)) break;

        auto decl = new_result();
        decl->type = GODECL_LABEL;
        decl->name = label->string();
        decl->spec_start = node->start();
        decl->name_start = label->start();
        decl->name_end = label->end();
        decl->gotype = NULL;
        save_decl(decl);
        break;
    }

    case TS_TYPE_DECLARATION:
        for (auto spec = node->child(); !isastnull(spec); spec = spec->next()) {
            auto name_node = spec->field(TSF_NAME);
            if (isastnull(name_node)) continue;

            auto type_params = parse_type_params(spec->field(TSF_TYPE_PARAMETERS));

            auto name = name_node->string();
            if (streq(name, "_")) continue;

            auto type_node = spec->field(TSF_TYPE);
            if (isastnull(type_node)) continue;

            auto gotype = node_to_gotype(type_node, is_toplevel);
            if (!gotype) continue;

            auto decl = new_result();
            decl->type = GODECL_TYPE;
            decl->spec_start = spec->start();
            decl->name = name;
            decl->name_start = name_node->start();
            decl->name_end = name_node->end();
            decl->gotype = gotype;
            decl->type_params = type_params;
            decl->is_alias = (spec->type() == TS_TYPE_ALIAS);
            save_decl(decl);
        }
        break;

    case TS_TYPE_PARAMETER_DECLARATION: {
        auto type_node = node->field(TSF_TYPE);
        if (isastnull(type_node)) break;

        auto type_node_gotype = node_to_gotype(type_node);
        if (!type_node_gotype) break;

        FOR_NODE_CHILDREN (node) {
            if (it->eq(type_node)) break;
            if (it->type() != TS_IDENTIFIER) continue;

            auto decl = new_result();
            decl->type = GODECL_TYPE_PARAM;
            decl->spec_start = node->start();
            decl->name = it->string();
            decl->name_start = it->start();
            decl->name_end = it->end();
            decl->gotype = type_node_gotype;
            save_decl(decl);
        }
        break;
    }

    case TS_PARAMETER_LIST:
    case TS_CONST_DECLARATION:
    case TS_VAR_DECLARATION: {
        List<Gotype*> *saved_iota_types = NULL;

        FOR_NODE_CHILDREN (node) {
            auto spec = it;
            auto type_node = spec->field(TSF_TYPE);
            auto value_node = spec->field(TSF_VALUE);

            bool is_error = false;
            FOR_NODE_CHILDREN (spec) {
                if (it->type() == TS_ERROR) {
                    is_error = true;
                    break;
                }
            }

            if (is_error) continue;

            // !type && !value      try to used saved iota expression
            // !type && value       infer types from values, try to save iota
            // type && value        save type from type, try to save iota
            // type && !value       save type from type

            if (isastnull(type_node) && isastnull(value_node)) {
                do {
                    if (!saved_iota_types) break;

                    auto ntype = node->type();
                    if (ntype != TS_CONST_DECLARATION && ntype != TS_VAR_DECLARATION)
                        break;

                    int i = 0;
                    FOR_NODE_CHILDREN (spec) {
                        if (i >= saved_iota_types->len) break;
                        auto saved_gotype = saved_iota_types->at(i++);

                        auto name = it->string();
                        if (streq(name, "_")) continue;

                        auto decl = new_result();
                        decl->type = (ntype == TS_CONST_DECLARATION ?  GODECL_CONST : GODECL_VAR);
                        decl->spec_start = spec->start();
                        decl->name = it->string();
                        decl->name_start = it->start();
                        decl->name_end = it->end();
                        decl->gotype = saved_gotype;
                        save_decl(decl);
                    }
                } while (0);

                continue;
            }

            // at this point, !isastnull(type_node) || !isastnull(value_node)

            auto has_iota = [&](Ast_Node *node) -> bool {
                bool ret = false;
                walk_ast_node(node, true, [&](Ast_Node *it, Ts_Field_Type, int) -> Walk_Action {
                    if (it->type() != TS_IOTA_LITERAL) return WALK_CONTINUE;

                    ret = true;
                    return WALK_ABORT;
                });
                return ret;
            };

            bool should_save_iota_types = (
                !saved_iota_types
                && !isastnull(value_node)
                && has_iota(value_node)
            );

            Gotype *type_node_gotype = NULL;
            if (!isastnull(type_node)) {
                type_node_gotype = node_to_gotype(type_node);
                if (!type_node_gotype) continue;

                if (node->type() == TS_PARAMETER_LIST && spec->type() == TS_VARIADIC_PARAMETER_DECLARATION) {
                    auto t = new_gotype(GOTYPE_SLICE);
                    t->slice_base = type_node_gotype;
                    t->slice_is_variadic = true;
                    type_node_gotype = t;
                }

                if (should_save_iota_types)
                    saved_iota_types = new_list(Gotype*);

                FOR_NODE_CHILDREN (spec) {
                    if (it->eq(type_node) || it->eq(value_node)) break;

                    auto decl_gotype = type_node_gotype;

                    auto decl = new_result();
                    switch (node->type()) {
                    case TS_PARAMETER_LIST: decl->type = GODECL_PARAM; break;
                    case TS_CONST_DECLARATION: decl->type = GODECL_CONST; break;
                    case TS_VAR_DECLARATION: decl->type = GODECL_VAR; break;
                    }
                    decl->spec_start = spec->start();
                    decl->name = it->string();
                    decl->name_start = it->start();
                    decl->name_end = it->end();
                    decl->gotype = decl_gotype;
                    save_decl(decl);

                    do {
                        if (node->type() != TS_PARAMETER_LIST) break;
                        if (spec->type() != TS_PARAMETER_DECLARATION) break;

                        auto method_decl = node->parent();
                        if (!method_decl) break;
                        if (isastnull(method_decl)) break;
                        if (method_decl->type() != TS_METHOD_DECLARATION) break;
                        auto recv = method_decl->field(TSF_RECEIVER);
                        if (isastnull(recv) || !recv->eq(node)) break;

                        auto gotype = unpointer_type(decl_gotype);
                        if (!gotype) break;

                        if (gotype->type != GOTYPE_GENERIC) break;

                        decl_gotype = gotype->base;

                        // ---
                        // try to grab the type arguments and create decls from them

                        auto generic_type_node = type_node;
                        while (!isastnull(generic_type_node) && generic_type_node->type() == TS_POINTER_TYPE)
                            generic_type_node = generic_type_node->child();

                        // should this be an assert? gotype->type == GOTYPE_GENERIC at this point
                        if (generic_type_node->type() != TS_GENERIC_TYPE) break;

                        auto base_type_node = generic_type_node->field(TSF_TYPE);
                        if (isastnull(base_type_node)) break;
                        if (base_type_node->type() != TS_TYPE_IDENTIFIER) break;

                        auto base_type = node_to_gotype(base_type_node);
                        if (!base_type) break;

                        auto args_node = generic_type_node->field(TSF_TYPE_ARGUMENTS);
                        if (isastnull(args_node)) break;

                        FOR_NODE_CHILDREN (args_node) {
                            if (it->type() != TS_TYPE_IDENTIFIER) continue;

                            auto child_gotype = node_to_gotype(it);
                            if (!child_gotype) continue;

                            auto decl = results->append(); // don't call new_result(), we need to set everything manually
                            decl->decl_start = it->start();
                            decl->decl_end = it->end();
                            decl->type = GODECL_METHOD_RECEIVER_TYPE_PARAM;
                            decl->spec_start = it->start();
                            decl->name = it->string();
                            decl->name_start = it->start();
                            decl->name_end = it->end();
                            decl->gotype = child_gotype;
                            decl->base = base_type;
                            save_decl(decl);
                        }
                    } while (0);

                    if (should_save_iota_types)
                        saved_iota_types->append(type_node_gotype);
                }
            } else {
                cp_assert(value_node->type() == TS_EXPRESSION_LIST);

                u32 lhs_count = 0;
                FOR_NODE_CHILDREN (spec) {
                    if (it->eq(type_node) || it->eq(value_node)) break;
                    lhs_count++;
                }

                auto lhs = new_list(Ast_Node*, lhs_count);
                auto rhs = new_list(Ast_Node*, value_node->child_count());

                FOR_NODE_CHILDREN (spec) {
                    if (it->eq(type_node) || it->eq(value_node)) break;
                    lhs->append(it);
                }

                FOR_NODE_CHILDREN (value_node) rhs->append(it);

                auto new_godecl = [&]() -> Godecl * {
                    auto decl = new_result();
                    decl->spec_start = spec->start();

                    switch (node->type()) {
                    case TS_PARAMETER_LIST: decl->type = GODECL_PARAM; break;
                    case TS_CONST_DECLARATION: decl->type = GODECL_CONST; break;
                    case TS_VAR_DECLARATION: decl->type = GODECL_VAR; break;
                    }

                    return decl;
                };

                if (should_save_iota_types)
                    saved_iota_types = new_list(Gotype*);

                auto old_len = results->len;
                assignment_to_decls(lhs, rhs, new_godecl);
                for (u32 i = old_len; i < results->len; i++) {
                    auto it = results->items + i;
                    save_decl(it);
                    if (should_save_iota_types)
                        saved_iota_types->append(it->gotype);
                }
            }
        }
        break;
    }

    case TS_RANGE_CLAUSE: {
        auto left = node->field(TSF_LEFT);
        auto right = node->field(TSF_RIGHT);

        if (!left) break;
        if (!right) break;

        if (left->type() != TS_EXPRESSION_LIST) break;

        auto lhs = new_list(Ast_Node*, left->child_count());
        auto rhs = new_list(Ast_Node*, 1);

        FOR_NODE_CHILDREN (left) lhs->append(it);
        rhs->append(right);

        auto new_godecl = [&]() -> Godecl * {
            auto decl = new_result();
            decl->spec_start = node->start();
            decl->type = GODECL_VAR; // do we need GODECL_RANGE?
            return decl;
        };

        auto old_len = results->len;
        assignment_to_decls(lhs, rhs, new_godecl, true);
        for (u32 i = old_len; i < results->len; i++)
            save_decl(results->items + i);
        break;
    }

    case TS_RECEIVE_STATEMENT:
    case TS_SHORT_VAR_DECLARATION: {
        List<Ast_Node*> *lhs = NULL;
        List<Ast_Node*> *rhs = NULL;

        if (node_type == TS_RECEIVE_STATEMENT) {
            bool isdecl = false;
            FOR_ALL_NODE_CHILDREN (node) {
                if (it->type() == TS_COLON_EQ) {
                    isdecl = true;
                    break;
                }
            }

            if (!isdecl) break;

            auto left = node->field(TSF_LEFT);
            if (isastnull(left)) break;
            if (left->type() != TS_EXPRESSION_LIST) break;

            auto right = node->field(TSF_RIGHT);
            if (isastnull(right)) break;
            if (!is_expression_node(right)) break;

            lhs = new_list(Ast_Node*, left->child_count());
            FOR_NODE_CHILDREN (left) lhs->append(it);

            rhs = new_list(Ast_Node*, 1);
            rhs->append(right);
        } else {
            auto left = node->field(TSF_LEFT);
            auto right = node->field(TSF_RIGHT);

            if (isastnull(left)) break;
            if (isastnull(right)) break;

            if (left->type() != TS_EXPRESSION_LIST) break;
            if (right->type() != TS_EXPRESSION_LIST) break;

            lhs = new_list(Ast_Node*, left->child_count());
            rhs = new_list(Ast_Node*, right->child_count());

            FOR_NODE_CHILDREN (left) lhs->append(it);
            FOR_NODE_CHILDREN (right) rhs->append(it);
        }

        auto new_godecl = [&]() -> Godecl * {
            auto decl = new_result();
            decl->spec_start = node->start();
            decl->type = GODECL_SHORTVAR;
            return decl;
        };

        auto old_len = results->len;
        assignment_to_decls(lhs, rhs, new_godecl);
        for (u32 i = old_len; i < results->len; i++)
            save_decl(results->items + i);

        break;
    }
    }
}

Gotype* walk_gotype_and_replace(Gotype *gotype, walk_gotype_and_replace_cb cb) {
    return _walk_gotype_and_replace(gotype->copy(), cb);
}

Gotype* walk_gotype_and_replace_ids(Gotype *gotype, Table<Goresult*> *lookup, bool *pfound = NULL) {
    bool found = false;

    auto ret = walk_gotype_and_replace(gotype, [&](auto it) -> Gotype* {
        if (it->type != GOTYPE_ID) return NULL;

        auto repl = lookup->get(it->id_name);
        if (!repl) return NULL;

        found = true;

        auto ret = new_gotype(GOTYPE_OVERRIDE_CTX);
        ret->override_ctx_base = repl->gotype;
        ret->override_ctx_ctx = repl->ctx;
        return ret;
    });

    if (pfound) *pfound = found;
    return ret;
}

Gotype* _walk_gotype_and_replace(Gotype *gotype, walk_gotype_and_replace_cb cb) {
    if (!gotype) return NULL;

    auto repl = cb(gotype);
    if (repl) return repl;

#define recur_raw(x) _walk_gotype_and_replace(x, cb)
#define recur(x) gotype->x = recur_raw(gotype->x)

    auto recur_arr = [&](auto arr) {
        for (int i = 0; i < arr->len; i++)
            arr->items[i] = recur_raw(arr->items[i]);
    };

    auto recur_decl = [&](auto decl) {
        if (decl->gotype)
            decl->gotype = recur_raw(decl->gotype);
    };

    switch (gotype->type) {
    case GOTYPE_POINTER:
    case GOTYPE_SLICE:
    case GOTYPE_ARRAY:
    case GOTYPE_CHAN:
    case GOTYPE_ASSERTION:
    case GOTYPE_RECEIVE:
    case GOTYPE_RANGE:
    case GOTYPE_CONSTRAINT_UNDERLYING:
    case GOTYPE_BUILTIN:
        recur(base);
        break;
    case GOTYPE_MAP:
        recur(map_key);
        recur(map_value);
        break;
    case GOTYPE_STRUCT:
        For (gotype->struct_specs)
            recur_decl(it.field);
        break;
    case GOTYPE_INTERFACE:
        For (gotype->interface_specs)
            recur_decl(it.field);
        break;
    case GOTYPE_FUNC:
        For (gotype->func_sig.params)
            recur_decl(&it);
        if (gotype->func_sig.result)
            For (gotype->func_sig.result)
                recur_decl(&it);
        break;
    case GOTYPE_MULTI:
        recur_arr(gotype->multi_types);
        break;
    case GOTYPE_CONSTRAINT:
        recur_arr(gotype->constraint_terms);
        break;
    case GOTYPE_GENERIC:
        recur(generic_base);
        recur_arr(gotype->generic_args);
        break;
    }

#undef recur
#undef recur_raw

    return gotype;
}

Gotype* Go_Indexer::do_generic_subst(Gotype *base, List<Godecl> *params, List<Goresult*> *args) {
    if (!params) return NULL;
    if (!args) return NULL;
    if (params->len != args->len) return NULL;

    Table<Goresult*> lookup; lookup.init();
    Fori (params) {
        lookup.set(it.name, args->at(i));
    }
    return walk_gotype_and_replace_ids(base, &lookup);
}

Goresult *Go_Indexer::_subst_generic_type(Gotype *type, Go_Ctx *ctx) {
    cp_assert(type->type == GOTYPE_GENERIC);

    auto base = type->generic_base;
    if (!is_type_ident(base)) return NULL;

    auto res = resolve_type_to_decl(base, ctx);
    if (!res) return NULL;

    auto decl = res->decl;
    if (decl->type != GODECL_TYPE) return NULL;

    auto args = new_list(Goresult*);
    For (type->generic_args) {
        args->append(make_goresult(it, ctx));
    }

    auto ret = do_generic_subst(decl->gotype, decl->type_params, args);
    if (!ret) return NULL;

    return make_goresult(ret, ctx);
}

Goresult *Go_Indexer::subst_generic_type(Goresult *res) {
    if (res->gotype->type == GOTYPE_POINTER) {
        auto base = subst_generic_type(res->wrap(res->gotype->base));
        if (!base) return NULL;

        auto ret = res->gotype->copy();
        ret->base = base->gotype;
        return res->wrap(ret);
    }

    while (res && res->gotype->type == GOTYPE_GENERIC)
        res = _subst_generic_type(res->gotype, res->ctx);
    return res;
}

Goresult *Go_Indexer::subst_generic_type(Gotype *type, Go_Ctx *ctx) {
    return subst_generic_type(make_goresult(type, ctx));
}

Goresult *Go_Indexer::unpointer_type(Goresult *res) {
    return unpointer_type(res->gotype, res->ctx);
}

Gotype *Go_Indexer::unpointer_type(Gotype *type) {
    while (type && type->type == GOTYPE_POINTER)
        type = type->pointer_base;
    return type;
}

Goresult *Go_Indexer::unpointer_type(Gotype *type, Go_Ctx *ctx) {
    auto ret = unpointer_type(type);
    if (!ret) return NULL;
    return make_goresult(ret, ctx);
}

List<Goresult> *Go_Indexer::list_package_decls(ccstr import_path, int flags) {
    auto pkg = find_up_to_date_package(import_path);
    if (!pkg) return NULL;

    auto ret = new_list(Goresult);

    For (pkg->files) {
        auto filename = it.filename;

        For (it.decls) {
            if (!it.name) continue;

            if (flags & LISTDECLS_PUBLIC_ONLY)
                if (is_name_private(it.name))
                    continue;

            if (flags & LISTDECLS_EXCLUDE_METHODS)
                if (it.type == GODECL_FUNC && it.gotype->func_recv)
                    continue;

            auto ctx = new_object(Go_Ctx);
            ctx->import_path = import_path;
            ctx->filename = filename;
            ret->append(make_goresult(&it, ctx));
        }
    }

    return ret;
}

Goresult *Go_Indexer::find_decl_in_package(ccstr id, ccstr import_path) {
    auto results = list_package_decls(import_path, LISTDECLS_EXCLUDE_METHODS);
    if (!results) return NULL;

    // in the future we can sort this shit
    For (results)
        if (streq(it.decl->name, id))
            return &it;
    return NULL;
}

Gotype *Go_Indexer::expr_to_gotype(Ast_Node *expr) {
    if (isastnull(expr)) return NULL;

    Gotype *ret = NULL; // so we don't have to declare inside switch

    switch (expr->type()) {
    case TS_PARENTHESIZED_EXPRESSION:
        return expr_to_gotype(expr->child());

    case TS_NIL_LITERAL: return new_primitive_type("Type"); // what is the nil type again?
    case TS_TRUE_LITERAL: return new_primitive_type("bool");
    case TS_FALSE_LITERAL: return new_primitive_type("bool");

    case TS_INT_LITERAL: return new_primitive_type("int");
    case TS_FLOAT_LITERAL: return new_primitive_type("float64");
    case TS_IMAGINARY_LITERAL: return new_primitive_type("complex128");
    case TS_RUNE_LITERAL: return new_primitive_type("rune");

    case TS_RAW_STRING_LITERAL:
    case TS_INTERPRETED_STRING_LITERAL:
        return new_primitive_type("string");

    case TS_VARIADIC_ARGUMENT: {
        auto ret = new_gotype(GOTYPE_SLICE);
        ret->slice_base = expr_to_gotype(expr->child());
        ret->slice_is_variadic = true;
        return ret;
    }

    case TS_UNARY_EXPRESSION:
        switch (expr->field(TSF_OPERATOR)->type()) {
        // case TS_RANGE: // idk even what to do here
        case TS_PLUS: // add
        case TS_DASH: // sub
        case TS_CARET: // xor
            return new_primitive_type("int");
        case TS_BANG: // not
            return new_primitive_type("bool");
        case TS_STAR: // mul
            ret = new_gotype(GOTYPE_LAZY_DEREFERENCE);
            ret->lazy_dereference_base = expr_to_gotype(expr->field(TSF_OPERAND));
            return ret;
        case TS_AMP: // and
            ret = new_gotype(GOTYPE_LAZY_REFERENCE);
            ret->lazy_reference_base = expr_to_gotype(expr->field(TSF_OPERAND));
            return ret;
        case TS_LT_DASH: // arrow
            ret = new_gotype(GOTYPE_LAZY_ARROW);
            ret->lazy_arrow_base = expr_to_gotype(expr->field(TSF_OPERAND));
            return ret;
        }
        break;

    case TS_BINARY_EXPRESSION:
        // TODO: these operators should return bool: || && == != < <= > >=
        return new_primitive_type("int");

    case TS_CALL_EXPRESSION: {
        // detect make(...) calls
        do {
            auto func = expr->field(TSF_FUNCTION);
            if (func->type() != TS_IDENTIFIER) break;
            if (!streq(func->string(), "make") && !streq(func->string(), "new")) break;

            auto args = expr->field(TSF_ARGUMENTS);
            if (isastnull(args)) break;
            if (args->type() != TS_ARGUMENT_LIST) break;

            auto firstarg = args->child();
            if (isastnull(firstarg)) break;

            ret = node_to_gotype(firstarg);
            if (streq(func->string(), "new")) {
                auto newret = new_gotype(GOTYPE_POINTER);
                newret->pointer_base = ret;
                ret = newret;
            }
            return ret;
        } while (0);

        Frame frame;
        List<Gotype*> *args = new_list(Gotype*);
        List<Gotype*> *type_args = NULL;

        auto type_args_node = expr->field(TSF_TYPE_ARGUMENTS);
        if (!isastnull(type_args_node)) {
            type_args = new_list(Gotype*);
            FOR_NODE_CHILDREN (type_args_node) {
                auto gotype = node_to_gotype(it);
                if (!gotype) {
                    frame.restore();
                    return NULL;
                }
                type_args->append(gotype);
            }
        }

        auto args_node = expr->field(TSF_ARGUMENTS);
        FOR_NODE_CHILDREN (args_node) {
            auto gotype = expr_to_gotype(it);
            if (!gotype) {
                frame.restore();
                return NULL;
            }
            args->append(gotype);
        }

        auto base = expr_to_gotype(expr->field(TSF_FUNCTION));
        if (!base) return NULL;

        if (type_args) {
            auto newbase = new_gotype(GOTYPE_LAZY_INSTANCE);
            newbase->lazy_instance_base = base;
            newbase->lazy_instance_args = type_args;
            base = newbase;
        }

        ret = new_gotype(GOTYPE_LAZY_CALL);
        ret->lazy_call_base = base;
        ret->lazy_call_args = args;
        return ret;
    }

    case TS_INDEX_EXPRESSION:
        ret = new_gotype(GOTYPE_LAZY_INDEX);
        ret->lazy_index_base = expr_to_gotype(expr->field(TSF_OPERAND));
        ret->lazy_index_key = expr_to_gotype(expr->field(TSF_INDEX));
        return ret;

    case TS_INSTANCE_EXPRESSION: {
        auto args_node = expr->field(TSF_TYPE_ARGUMENTS);
        if (isastnull(args_node)) return NULL; // ?

        auto args = new_list(Gotype*);
        FOR_NODE_CHILDREN (args_node) {
            auto arg = node_to_gotype(it);
            if (!arg) return NULL; // ?

            args->append(arg);
        }

        ret = new_gotype(GOTYPE_LAZY_INSTANCE);
        ret->lazy_instance_base = expr_to_gotype(expr->field(TSF_OPERAND));
        ret->lazy_instance_args = args;
        return ret;
    }

    case TS_QUALIFIED_TYPE:
    case TS_SELECTOR_EXPRESSION: {
        auto operand_node = expr->field(expr->type() == TS_QUALIFIED_TYPE ? TSF_PACKAGE : TSF_OPERAND);
        if (isastnull(operand_node)) return NULL;

        auto field_node = expr->field(expr->type() == TS_QUALIFIED_TYPE ? TSF_NAME : TSF_FIELD);
        if (isastnull(field_node)) return NULL;

        ret = new_gotype(GOTYPE_LAZY_SEL);
        ret->lazy_sel_sel = field_node->string();
        ret->lazy_sel_base = expr_to_gotype(operand_node);
        return ret;
    }

    case TS_SLICE_EXPRESSION:
        return expr_to_gotype(expr->field(TSF_OPERAND));

    case TS_TYPE_CONVERSION_EXPRESSION:
    case TS_COMPOSITE_LITERAL:
        return node_to_gotype(expr->field(TSF_TYPE));

    case TS_TYPE_ASSERTION_EXPRESSION:
        ret = new_gotype(GOTYPE_ASSERTION);
        ret->assertion_base = node_to_gotype(expr->field(TSF_TYPE));
        return ret;

    case TS_PACKAGE_IDENTIFIER:
    case TS_TYPE_IDENTIFIER:
    case TS_IDENTIFIER:
    case TS_FIELD_IDENTIFIER:
        ret = new_gotype(GOTYPE_LAZY_ID);
        ret->lazy_id_name = expr->string();
        ret->lazy_id_pos = expr->start();
        return ret;

    case TS_FUNC_LITERAL:
        return node_to_gotype(expr);
    }

    return NULL;
}

Goresult *Go_Indexer::evaluate_type(Goresult *res, Godecl** outdecl) {
    return evaluate_type(res->gotype, res->ctx, outdecl);
}

Goresult *Go_Indexer::remove_override_ctx(Gotype *gotype, Go_Ctx *ctx) {
    auto res = make_goresult(gotype, ctx);
    gotype = res->gotype;
    ctx = res->ctx;

    auto ret = walk_gotype_and_replace(gotype, [&](Gotype *it) -> Gotype* {
        return it->type == GOTYPE_OVERRIDE_CTX ? it->base : NULL;
    });
    return make_goresult(ret, ctx);
}

Goresult *Go_Indexer::evaluate_type(Gotype *gotype, Go_Ctx *ctx, Godecl** outdecl) {
    auto ret = _evaluate_type(gotype, ctx, outdecl);
    if (!ret) return NULL;

    return remove_override_ctx(ret->gotype, ret->ctx);
}

Goresult *Go_Indexer::_evaluate_type(Goresult *res, Godecl** outdecl) {
    return _evaluate_type(res->gotype, res->ctx, outdecl);
}

Goresult *Go_Indexer::_evaluate_type(Gotype *gotype, Go_Ctx *ctx, Godecl** outdecl) {
    if (!gotype) return NULL;

    enum {
        U_EVAL = 1 << 0,
        U_RESOLVE = 1 << 1,
        U_SUBST = 1 << 2,
        U_UNPOINTER = 1 << 3,
        U_ALL = U_EVAL | U_RESOLVE | U_SUBST | U_UNPOINTER,
    };

    auto unwrap_type = [&](Gotype *base, int flags = U_ALL) -> Goresult* {
        if (!base) return NULL;

        auto res = make_goresult(base, ctx);

        if (flags & U_EVAL) {
            res = _evaluate_type(res, outdecl);
            if (!res) return NULL;
        }

        if (flags & U_RESOLVE) {
            res = resolve_type(res);
            if (!res) return NULL;
        }

        if (flags & U_SUBST) {
            res = subst_generic_type(res);
            if (!res) return NULL;
        }

        if (flags & U_UNPOINTER) {
            res = unpointer_type(res);
            if (!res) return NULL;
        }

        return res;
    };

    // A GOTYPE_LAZY_INDEX or GOTYPE_LAZY_INSTANCE Gotype possibly refers to a
    // generic function. This procedure takes such a Gotype and returns the
    // target Gotype (the GOTYPE_FUNC), the params, and the args for the caller
    // to do type substitution.
    //
    // Why don't we just do the type substitution here? Because sometimes we do
    // it using just the type args provided, and sometimes we need the func
    // args to do type inference.

    struct Prepare_Lazy_Subst_Result {
        Goresult *res;
        Godecl *decl;
        Gotype *target;
        List<Godecl> *params;
        List<Gotype*> *args;
    };

    auto prepare_lazy_subst = [&](Gotype *lazy) -> Prepare_Lazy_Subst_Result* {
        Godecl *decl = NULL;
        Goresult *res = NULL;

        if (lazy->type == GOTYPE_LAZY_INDEX || lazy->type == GOTYPE_LAZY_INSTANCE) {
            auto base = lazy->base;
            if (base->type != GOTYPE_LAZY_ID && base->type != GOTYPE_LAZY_SEL)
                return NULL;
            res = _evaluate_type(base, ctx, &decl);
        } else {
            res = _evaluate_type(lazy, ctx, &decl);
        }

        if (!res) return NULL;
        if (!decl) return NULL;
        if (decl->type != GODECL_FUNC) return NULL;
        if (res->gotype->type != GOTYPE_FUNC) return NULL;
        if (!decl->type_params) return NULL;

        List<Gotype*> *args = NULL;

        switch (lazy->type) {
        case GOTYPE_LAZY_INDEX: {
            auto key = lazy->lazy_index_key;
            Gotype *newtype = NULL;

            switch (key->type) {
            case GOTYPE_LAZY_ID:
                newtype = new_gotype(GOTYPE_ID);
                newtype->id_name = key->lazy_id_name;
                newtype->id_pos = key->lazy_id_pos;
                break;
            case GOTYPE_LAZY_SEL:
                if (!key->lazy_sel_base) return NULL;
                if (key->lazy_sel_base->type != GOTYPE_ID) return NULL;

                newtype = new_gotype(GOTYPE_SEL);
                newtype->sel_name = key->lazy_sel_base->id_name;
                newtype->sel_sel = key->lazy_sel_sel;
                break;
            default:
                return NULL;
            }

            args = new_list(Gotype*);
            args->append(newtype);
            break;
        }
        case GOTYPE_LAZY_INSTANCE:
            args = lazy->lazy_instance_args;
            break;
        case GOTYPE_LAZY_ID:
        case GOTYPE_LAZY_SEL:
            args = new_list(Gotype*);
            break;
        }

        auto ret = new_object(Prepare_Lazy_Subst_Result);
        ret->target = res->gotype;
        ret->params = decl->type_params;
        ret->res = res;
        ret->decl = decl;
        ret->args = args;
        return ret;
    };

    auto try_evaluating_index = [&](Gotype *base) -> Goresult* {
        auto res = unwrap_type(base);
        if (!res) return NULL;

        auto base_type = res->gotype;
        if (base_type->type == GOTYPE_MULTI)
            if (base_type->multi_types && base_type->multi_types->len == 1)
                base_type = base_type->multi_types->at(0);

        switch (base_type->type) {
        case GOTYPE_ARRAY:
        case GOTYPE_SLICE:
            return _evaluate_type(res->wrap(base_type->base), outdecl);

        case GOTYPE_ID:
            if (streq(base_type->id_name, "string"))
                return new_primitive_type_goresult("rune");
            break;
        case GOTYPE_MAP: {
            auto res2 = _evaluate_type(base_type->map_value, res->ctx, outdecl);
            if (!res2) return NULL;

            auto ret = new_gotype(GOTYPE_ASSERTION);
            ret->assertion_base = res2->gotype;
            return res2->wrap(ret);
        }
        }

        return NULL;
    };

    auto try_evaluating_instance = [&](Gotype *lazy, Go_Ctx *ctx) -> Goresult* {
        auto subst = prepare_lazy_subst(lazy);
        if (!subst) return NULL;

        auto args = new_list(Goresult*);
        For (subst->args) {
            args->append(make_goresult(it, ctx));
        }

        auto ret = do_generic_subst(subst->target, subst->params, args);
        if (!ret) return NULL;

        return subst->res->wrap(ret);
    };

    switch (gotype->type) {
    case GOTYPE_LAZY_RANGE: {
        auto res = unwrap_type(gotype->lazy_range_base);
        if (!res) return NULL;

        auto base_type = res->gotype;
        if (base_type->type == GOTYPE_MULTI)
            if (base_type->multi_types && base_type->multi_types->len == 1)
                base_type = base_type->multi_types->at(0);

        switch (res->gotype->type) {
        case GOTYPE_MAP:
            if (gotype->lazy_range_is_index)
                return res->wrap(res->gotype->map_key);
            return res->wrap(res->gotype->map_value);
        case GOTYPE_SLICE:
            if (gotype->lazy_range_is_index)
                return new_primitive_type_goresult("int");
            return res->wrap(res->gotype->slice_base);
        case GOTYPE_ARRAY:
            if (gotype->lazy_range_is_index)
                return new_primitive_type_goresult("int");
            return res->wrap(res->gotype->array_base);
        case GOTYPE_CHAN:
            if (gotype->lazy_range_is_index)
                return res->wrap(res->gotype->chan_base);
            break;
        }
        return NULL;
    }

    // ======================================
    // START OF CASES WITH TYPE INSTANTIATION
    // ======================================

    case GOTYPE_LAZY_INDEX: {
        // try as index
        auto ret = try_evaluating_index(gotype->lazy_index_base);
        if (ret) return ret;

        // then as instance
        return try_evaluating_instance(gotype, ctx);
    }

    case GOTYPE_LAZY_INSTANCE: {
        // try as instance
        auto ret = try_evaluating_instance(gotype, ctx);
        if (ret) return ret;

        // then as index
        if (gotype->lazy_instance_args->len != 1) return NULL;
        auto arg = gotype->lazy_instance_args->at(0);
        if (!is_type_ident(arg)) return NULL;
        return try_evaluating_index(gotype->lazy_instance_base);
    }

    case GOTYPE_LAZY_CALL: {
        auto try_with_type_args = [&]() -> Goresult* {
            auto base = gotype->lazy_call_base;
            if (!base) return NULL;

            switch (base->type) {
            case GOTYPE_LAZY_INDEX:
            case GOTYPE_LAZY_INSTANCE:
            case GOTYPE_LAZY_ID:
            case GOTYPE_LAZY_SEL:
                break;
            default:
                return NULL;
            }

            auto result = prepare_lazy_subst(base);
            if (!result) return NULL;
            if (result->args->len > result->params->len) return NULL;

            auto func_args = gotype->lazy_call_args;
            auto func_ctx = ctx;
            auto func_params = result->target->func_sig.params;

            // handle gotype_multi (e.g. `foo(bar())`, `foo` takes 2 args, `bar()` returns 2 vals)
            do {
                if (func_args->len != 1) break;

                auto res = _evaluate_type(func_args->at(0), ctx);
                if (!res) break;
                if (res->gotype->type != GOTYPE_MULTI) break;

                func_ctx = res->ctx;
                func_args = res->gotype->multi_types;
            } while (0);

            auto arity_matches = [&]() {
                if (func_params->len) {
                    auto param = func_params->at(func_params->len-1);
                    if (param.gotype->type == GOTYPE_SLICE)
                        if (param.gotype->slice_is_variadic)
                            return func_args->len >= func_params->len - 1;
                }
                return func_args->len == func_params->len;
            };

            if (!arity_matches()) return NULL;

            auto func_result = result->target->func_sig.result;
            if (!func_result) return NULL;

            Table<Goresult*> lookup; lookup.init();
            int types_found = 0;

            Fori (result->params) {
                Goresult *arg = NULL;
                if (i < result->args->len)
                    arg = make_goresult(result->args->at(i), ctx);
                lookup.set(it.name, arg);
            }

            fn<bool(Goresult*, Goresult*)> unify_types = [&](auto ares, auto bres) {
                cp_assert(ares);
                cp_assert(bres);

                auto a = ares->gotype;
                auto b = bres->gotype;

                if (!a && !b) return true;
                if (!a || !b) return false;

                auto get_type_param = [&](Goresult *res, bool *pfound) -> Goresult* {
                    if (!are_ctxs_equal(res->ctx, result->res->ctx)) return NULL;

                    auto gotype = res->gotype;
                    if (gotype->type != GOTYPE_ID) return NULL;

                    // check if id pos is in func params, func result, or func type params
                    auto check_id_pos = [&]() {
                        auto pos = gotype->id_pos;
                        auto check_range = [&](auto decls) {
                            For (decls)
                                if (it.decl_start <= pos && pos < it.decl_end)
                                    return true;
                            return false;
                        };

                        if (check_range(result->target->func_sig.params)) return true;
                        if (check_range(result->target->func_sig.result)) return true;
                        if (check_range(result->decl->type_params)) return true;
                        return false;
                    };

                    if (!check_id_pos()) return NULL;

                    return lookup.get(gotype->id_name, pfound);
                };

                {
                    bool afound = false;
                    bool bfound = false;
                    auto atype = get_type_param(ares, &afound);
                    auto btype = get_type_param(bres, &bfound);

                    if ((afound && !bfound) || (!afound && bfound)) {
                        Goresult *existing_type = (afound ? atype : btype);
                        auto other_res = (afound ? bres : ares);

                        if (existing_type)
                            return unify_types(existing_type, other_res);

                        ccstr name = (afound ? ares : bres)->gotype->id_name;
                        lookup.set(name, other_res);
                        types_found++;
                        return true;
                    }

                    if (afound && bfound) // can't use another type param as constraint
                        return false;
                }

                if (is_type_ident(a) && is_type_ident(b)) {
                    auto resa = resolve_type_to_decl(a, ares->ctx);
                    auto resb = resolve_type_to_decl(b, bres->ctx);
                    return are_decls_equal(resa, resb);
                }

                if (a->type != b->type) return false;

                auto _recur_arr = [&](auto aarr, auto actx, auto barr, auto bctx) {
                    if (!aarr && !barr) return true;
                    if (!aarr || !barr) return false;
                    if (aarr->len != barr->len) return false;

                    Fori (aarr) {
                        auto &a2 = it;
                        auto &b2 = barr->at(i);
                        if (!unify_types(make_goresult(a2, actx), make_goresult(b2, bctx))) return false;
                    }
                    return true;
                };

#define recur(field) if (!unify_types(ares->wrap(a->field), bres->wrap(b->field))) return false
#define recur_arr(field) if (!_recur_arr(a->field, ares->ctx, b->field, bres->ctx)) return false

                switch (a->type) {
                case GOTYPE_ID: {
                    // type params have already been handled
                    auto adeclres = find_decl_of_id(a->id_name, a->id_pos, ares->ctx);
                    auto bdeclres = find_decl_of_id(b->id_name, b->id_pos, bres->ctx);
                    // when would this not be sufficient?
                    return are_ctxs_equal(adeclres->ctx, bdeclres->ctx);
                }

                case GOTYPE_SEL: {
                    auto aimp = find_import_path_referred_to_by_id(a->sel_name, ares->ctx);
                    auto bimp = find_import_path_referred_to_by_id(b->sel_name, bres->ctx);
                    if (!aimp || !bimp) return false;
                    if (!streq(aimp, bimp)) return false;
                    if (!streq(a->sel_sel, b->sel_sel)) return false;
                    break;
                }

                case GOTYPE_POINTER:
                case GOTYPE_SLICE:
                case GOTYPE_ARRAY:
                case GOTYPE_CHAN:
                case GOTYPE_ASSERTION:
                case GOTYPE_RECEIVE:
                case GOTYPE_RANGE:
                case GOTYPE_CONSTRAINT_UNDERLYING:
                    recur(base);
                    break;
                case GOTYPE_BUILTIN: {
                    bool abase = a->base;
                    bool bbase = b->base;
                    if (abase != bbase) return false;
                    if (a->base) recur(base);
                    break;
                }
                case GOTYPE_MAP:
                    recur(map_key);
                    recur(map_value);
                    break;
                case GOTYPE_STRUCT: {
                    auto &aspecs = a->struct_specs;
                    auto &bspecs = b->struct_specs;

                    if (aspecs->len != bspecs->len) return false;

                    Fori (aspecs) {
                        auto &a2 = it.field->gotype;
                        auto &b2 = bspecs->at(i).field->gotype;
                        if (!unify_types(ares->wrap(a2), bres->wrap(b2))) return false;
                    }
                    break;
                }
                case GOTYPE_INTERFACE: {
                    auto &aspecs = a->interface_specs;
                    auto &bspecs = b->interface_specs;

                    if (aspecs->len != bspecs->len) return false;

                    Fori (aspecs) {
                        auto &a2 = it.field->gotype;
                        auto &b2 = bspecs->at(i).field->gotype;
                        if (!unify_types(ares->wrap(a2), bres->wrap(b2))) return false;
                    }
                    break;
                }
                case GOTYPE_FUNC: {
                    auto aparams = a->func_sig.params;
                    auto bparams = b->func_sig.params;

                    auto aresult = a->func_sig.result;
                    auto bresult = b->func_sig.result;

                    if (aparams->len != bparams->len) return false;
                    if (aresult->len != bresult->len) return false;

                    Fori (aparams) {
                        auto &a2 = it;
                        auto &b2 = bparams->at(i);
                        if (!unify_types(ares->wrap(a2.gotype), bres->wrap(b2.gotype))) return false;
                    }

                    Fori (aresult) {
                        auto &a2 = it;
                        auto &b2 = bresult->at(i);
                        if (!unify_types(ares->wrap(a2.gotype), bres->wrap(b2.gotype))) return false;
                    }
                    break;
                }
                case GOTYPE_MULTI:
                    recur_arr(multi_types);
                    break;
                case GOTYPE_CONSTRAINT:
                    recur_arr(constraint_terms);
                    break;
                case GOTYPE_GENERIC:
                    recur(generic_base);
                    recur_arr(generic_args);
                    break;
                }

#undef recur_arr
#undef recur
                return true;
            };

            bool ran_constraint_type_inference = false;

            enum Type_Node_Type {
                TYPE_NODE_UNION,
                TYPE_NODE_INTERSECT,
                TYPE_NODE_TERM,
            };

            struct Type_Node {
                Type_Node_Type type;
                union {
                    List<Type_Node*> *children;
                    struct {
                        Goresult *term; // gotype
                        bool is_underlying;
                    };
                };
            };

            auto new_type_node = [&](Type_Node_Type type) {
                auto ret = new_object(Type_Node);
                ret->type = type;
                return ret;
            };

            fn<Type_Node*(Gotype*, Go_Ctx*)> gotype_to_type_node = [&](auto gotype, auto ctx) -> Type_Node* {
                Type_Node *ret = NULL;

                switch (gotype->type) {
                case GOTYPE_INTERFACE:
                    ret = new_type_node(TYPE_NODE_INTERSECT);
                    ret->children = new_list(Type_Node*);
                    For (gotype->interface_specs) {
                        if (it.type != GO_INTERFACE_SPEC_EMBEDDED && it.type != GO_INTERFACE_SPEC_ELEM)
                            continue;
                        auto child = gotype_to_type_node(it.field->gotype, ctx);
                        if (!child) return NULL;
                        ret->children->append(child);
                    }
                    if (ret->children->len == 1)
                        return ret->children->at(0);
                    return ret;

                case GOTYPE_CONSTRAINT:
                    ret = new_type_node(TYPE_NODE_UNION);
                    ret->children = new_list(Type_Node*);
                    For (gotype->constraint_terms) {
                        auto child = gotype_to_type_node(it, ctx);
                        if (!child) return NULL;
                        ret->children->append(child);
                    }
                    return ret;

                case GOTYPE_CONSTRAINT_UNDERLYING:
                    ret = gotype_to_type_node(gotype->constraint_underlying_base, ctx);
                    if (!ret) return NULL;
                    // if (ret->term->gotype->type != GOTYPE_BUILTIN) return NULL;
                    ret->is_underlying = true;
                    return ret;

                case GOTYPE_ID:
                case GOTYPE_SEL: {
                    auto res = resolve_type_to_decl(gotype, ctx);
                    if (!res) return NULL;

                    auto other_gotype = res->decl->gotype;
                    if (other_gotype->type == GOTYPE_BUILTIN)
                        other_gotype = other_gotype->base;

                    if (other_gotype->type == GOTYPE_INTERFACE)
                        return gotype_to_type_node(other_gotype, res->ctx);

                    ret = new_type_node(TYPE_NODE_TERM);
                    ret->term = make_goresult(gotype, ctx);
                    return ret;
                }

                case GOTYPE_BUILTIN:
                case GOTYPE_MAP:
                case GOTYPE_STRUCT:
                case GOTYPE_POINTER:
                case GOTYPE_FUNC:
                case GOTYPE_SLICE:
                case GOTYPE_ARRAY:
                case GOTYPE_CHAN:
                case GOTYPE_GENERIC:
                    ret = new_type_node(TYPE_NODE_TERM);
                    ret->term = make_goresult(gotype, ctx);
                    return ret;
                }

                return NULL;
            };

            // mutates node
            fn<void(Type_Node*)> flatten_type_node = [&](auto node) {
                if (node->type != TYPE_NODE_INTERSECT && node->type != TYPE_NODE_UNION)
                    return;

                auto children = new_list(Type_Node*);
                For (node->children) {
                    auto child = it;
                    flatten_type_node(child);
                    if (child->type == node->type) {
                        For (child->children) {
                            children->append(it);
                        }
                    } else {
                        children->append(child);
                    }
                }
                node->children = children;
            };

            auto empty_intersection = [&]() {
                return new_type_node(TYPE_NODE_INTERSECT);
            };

            auto is_empty_intersection = [&](Type_Node *it) {
                return it->type == TYPE_NODE_INTERSECT && isempty(it->children);
            };

            auto get_intersection_of_two_terms = [&](Type_Node *a, Type_Node *b) -> Type_Node* {
                cp_assert(a->type == TYPE_NODE_TERM);
                cp_assert(b->type == TYPE_NODE_TERM);

                // if both underlying or both not underlying
                if (a->is_underlying == b->is_underlying)
                    return are_gotypes_equal(a->term, b->term) ? a : empty_intersection();

                // at this point, a->is_underlying != b->is_underlying

                if (b->is_underlying) SWAP(a, b);

                // at this point, a is underlying, b is not

                auto underlying_type = a->term->gotype;
                Goresult *resolved_gotype = NULL;

                // TODO: resolve aliases
                if (underlying_type->type == GOTYPE_ID) {
                    auto res = find_decl_of_id(underlying_type->id_name, underlying_type->id_pos, a->term->ctx);
                    if (!res) return NULL;
                    if (res->gotype->type != GOTYPE_BUILTIN) return NULL;
                    resolved_gotype = res;
                } else {
                    resolved_gotype = a->term;
                }

                auto res = resolve_type(b->term);
                if (!res) return NULL;

                return are_gotypes_equal(resolved_gotype, res) ? b : empty_intersection();
            };

            fn<Type_Node*(Type_Node*)> simplify_type_node = [&](auto node) -> Type_Node* {
                switch (node->type) {
                case TYPE_NODE_INTERSECT: {
                    if (!node->children->len) return empty_intersection();

                    auto current_union = new_list(Type_Node*);
                    auto temp_union = new_list(Type_Node*);

                    For (node->children) {
                        Type_Node* child = it; // without reference
                        child = simplify_type_node(child);

                        List<Type_Node*> *new_union = NULL;
                        if (child->type == TYPE_NODE_UNION) {
                            new_union = child->children;
                        } else {
                            new_union = new_list(Type_Node*);
                            new_union->append(child);
                        }

                        cp_assert(new_union->len);

                        if (!current_union->len) {
                            For (new_union) current_union->append(it);
                            continue;
                        }

                        temp_union->len = 0;

                        For (current_union) {
                            auto curr = it;
                            For (new_union) {
                                auto term = get_intersection_of_two_terms(curr, it);
                                if (!term) return NULL;
                                if (is_empty_intersection(term)) continue;

                                cp_assert(term->type == TYPE_NODE_TERM);
                                temp_union->append(term);
                            }
                        }

                        if (!temp_union->len) return empty_intersection();

                        current_union->len = 0;
                        For (temp_union) current_union->append(it);
                    }

                    auto ret = new_type_node(TYPE_NODE_UNION);
                    ret->children = current_union;
                    return ret;
                }
                case TYPE_NODE_UNION: {
                    auto new_union = new_list(Type_Node*);

                    For (node->children) {
                        Type_Node *child = it; // without reference
                        child = simplify_type_node(child);

                        if (is_empty_intersection(child)) continue;

                        if (child->type == TYPE_NODE_UNION) {
                            For (child->children) new_union->append(it);
                        } else {
                            cp_assert(child->type == TYPE_NODE_TERM);
                            new_union->append(child);
                        }
                    }

                    auto ret = new_type_node(TYPE_NODE_UNION);
                    ret->children = new_union;
                    return ret;
                }
                }
                return node;
            };

            auto simplify_union = [&](Type_Node *tn) -> Type_Node* {
                cp_assert(tn->type != TYPE_NODE_INTERSECT);

                if (tn->type == TYPE_NODE_TERM) return tn;

                auto children = new_list(Type_Node*);

                For (tn->children) {
                    auto curr = it;
                    cp_assert(curr->type == TYPE_NODE_TERM);

                    if (curr->is_underlying) {
                        // skip if any underlying from children equal to this
                        // remove any non-underlying from children that belong to this
                        auto underlying = curr->term;

                        bool already_in_set = false;
                        For (children) {
                            if (!it->is_underlying) continue;

                            if (are_gotypes_equal(it->term, underlying)) {
                                already_in_set = true;
                                break;
                            }
                        }

                        if (already_in_set) continue;

                        for (int i = 0; i < children->len; i++) {
                            auto &it = children->at(i);
                            if (it->is_underlying) continue;

                            auto res = resolve_type(it->term);
                            if (!res) return NULL;

                            if (are_gotypes_equal(res, underlying))
                                children->remove(i--);
                        }

                        children->append(curr);
                    } else {
                        // skip if any non-underlying equal to this
                        // skip if any underlying is superset of us
                        auto underlying = resolve_type(curr->term);
                        if (!underlying) return NULL;

                        bool already_in_set = false;
                        For (children) {
                            if (are_gotypes_equal(it->term, it->is_underlying ? underlying : curr->term)) {
                                already_in_set = true;
                                break;
                            }
                        }

                        if (!already_in_set) children->append(it);
                    }
                }

                if (children->len == 1) return children->at(0);

                auto ret = new_type_node(TYPE_NODE_UNION);
                ret->children = children;
                return ret;
            };

            auto get_core_type = [&](Gotype *gotype, Go_Ctx *ctx) -> Goresult* {
                auto tn = gotype_to_type_node(gotype, ctx);
                if (!tn) return NULL;

                flatten_type_node(tn);

                // after flatten_type_node(), intersections and unions no
                // longer have intersections and unions respectively as
                // children

                tn = simplify_type_node(tn);

                // after simplify_type_node(), all intersections are
                // eliminated, tn is now either a union of terms, or a single
                // term
                cp_assert(tn->type != TYPE_NODE_INTERSECT);

                if (tn->type == TYPE_NODE_UNION)
                    tn = simplify_union(tn);

                // tn is now either a union of terms, or a single term
                // union will always contain multiple terms, otherwise it'll be a term
                cp_assert(!(tn->type == TYPE_NODE_UNION && tn->children->len == 1));

                List<Type_Node*> *children = NULL;
                if (tn->type == TYPE_NODE_TERM) {
                    children = new_list(Type_Node*);
                    children->append(tn);
                } else {
                    children = tn->children;
                }

                Goresult *first = NULL;
                For (children) {
                    Goresult *underlying = NULL;
                    if (it->is_underlying) {
                        underlying = it->term;
                    } else {
                        underlying = resolve_type(it->term);
                        if (!underlying) return NULL;
                    }

                    if (!first) {
                        first = underlying;
                        continue;
                    }

                    if (!are_gotypes_equal(underlying, first)) return NULL;
                }

                return first;
            };

            auto constraint_type_inference = [&]() {
                ran_constraint_type_inference = true;

                For (result->params) {
                    auto res = result->res;

                    auto core_type = get_core_type(it.gotype, res->ctx);
                    if (!core_type) continue;

                    auto idtype = new_gotype(GOTYPE_ID);
                    idtype->id_name = it.name;
                    idtype->id_pos = it.name_start;

                    if (!unify_types(res->wrap(idtype), core_type)) return false;
                }

                auto do_substitution = [&]() {
                    bool found_something = false;
                    For (result->params) {
                        auto res = lookup.get(it.name);
                        if (!res) continue;

                        bool found = false;
                        auto newtype = walk_gotype_and_replace_ids(res->gotype, &lookup, &found);
                        if (found) found_something = true;

                        lookup.set(it.name, res->wrap(newtype));
                    }
                    return found_something;
                };

                while (do_substitution()) continue;

                return true;
            };

            bool added_new_types = false;

            auto is_func_arg_untyped = [&](Gotype *gotype) {
                if (gotype->type != GOTYPE_ID) return false;

                if (streq(gotype->id_name, "int")) return true;
                if (streq(gotype->id_name, "bool")) return true;
                if (streq(gotype->id_name, "float64")) return true;
                if (streq(gotype->id_name, "complex128")) return true;
                if (streq(gotype->id_name, "string")) return true;

                return false;
            };

            auto func_arg_type_inference = [&](bool untyped) {
                added_new_types = false;
                auto old = types_found;

                Fori (func_params) {
                    auto &arg = func_args->at(i);
                    if (is_func_arg_untyped(arg) != untyped) continue;

                    auto res = make_goresult(arg, ctx);

                    if (!untyped) {
                        res = _evaluate_type(arg, func_ctx);
                        if (!res) return false;
                        // res = resolve_type(res);
                        // if (!res) return false;
                    }

                    if (!unify_types(res, result->res->wrap(it.gotype)))
                        return false;
                }

                if (types_found > old) added_new_types = true;

                return true;
            };

            auto is_done = [&]() {
                For (result->params) {
                    auto val = lookup.get(it.name);
                    if (!val) return false;

                    bool references_other_type_param = false;

                    walk_gotype_and_replace(val->gotype, [&](Gotype* it) -> Gotype* {
                        if (it->type == GOTYPE_ID) {
                            bool found = false;
                            lookup.get(it->id_name, &found);
                            if (found) references_other_type_param = true;
                        }
                        return NULL;
                    });

                    if (references_other_type_param) return false;
                }
                return true;
            };

            // apply function argument type inference to typed ordinary function arguments
            if (!is_done() && !func_arg_type_inference(false))
                return NULL;

            // apply constraint type inference
            if (!is_done() && added_new_types)
                if (!constraint_type_inference())
                    return NULL;

            // apply function argument type inference to untyped ordinary function arguments
            if (!is_done() && !func_arg_type_inference(true))
                return NULL;

            // apply constraint type inference
            if (!is_done() && (added_new_types || !ran_constraint_type_inference))
                if (!constraint_type_inference())
                    return NULL;

            if (!is_done()) return NULL;

            Gotype *ret = NULL;
            if (func_result->len == 1) {
                ret = func_result->at(0).gotype;
            } else {
                ret = new_gotype(GOTYPE_MULTI);
                ret->multi_types = new_list(Gotype*, func_result->len);
                For (func_result) ret->multi_types->append(it.gotype);
            }

            return result->res->wrap(walk_gotype_and_replace_ids(ret, &lookup));
        };

        {
            auto ret = try_with_type_args();
            if (ret) return ret;
        }

        // didn't work with type args, treat as normal function call.

        Godecl *decl = NULL;
        auto res = _evaluate_type(make_goresult(gotype->lazy_call_base, ctx), &decl);
        if (outdecl) *outdecl = decl;
        if (!res) return NULL;

        if (decl && decl->type == GODECL_TYPE) {
            // we have a Type(x) or pkg.Type(x)
            // right now it thinks Type/pkg.Type are lazy_id/lazy_sel
            // convert to a regular id/sel, since Type is the type itself
            auto base = gotype->lazy_call_base;
            switch (base->type) {
            case GOTYPE_LAZY_ID: {
                auto gotype = new_gotype(GOTYPE_ID);
                gotype->id_name = base->lazy_id_name;
                gotype->id_pos = base->lazy_id_pos;
                return make_goresult(gotype, ctx);
            }
            case GOTYPE_LAZY_SEL: {
                // this should be true, but check anyway
                if (base->lazy_sel_base->type != GOTYPE_LAZY_ID) break;

                auto gotype = new_gotype(GOTYPE_SEL);
                gotype->sel_name = base->lazy_sel_base->lazy_id_name;
                gotype->sel_sel = base->lazy_sel_sel;
                return make_goresult(gotype, ctx);
            }
            }
        }

        res = resolve_type(res);       if (!res) return NULL;
        res = subst_generic_type(res); if (!res) return NULL;
        res = unpointer_type(res);     if (!res) return NULL;

        if (res->gotype->type != GOTYPE_FUNC) return NULL;

        auto result = res->gotype->func_sig.result;
        if (!result) return NULL;

        // if the result contains type parameters, get out
        if (decl && (decl->type == GODECL_FUNC || decl->type == GODECL_TYPE) && decl->type_params) {
            For (result) {
                bool bad = false;
                walk_gotype_and_replace(it.gotype, [&](Gotype *it) -> Gotype* {
                    if (bad) return NULL;
                    if (it->type != GOTYPE_ID) return NULL;

                    auto name = it->id_name;
                    For (decl->type_params) {
                        if (streq(it.name, name)) {
                            bad = true;
                            break;
                        }
                    }
                    return NULL;
                });
                if (bad) return NULL;
            }
        }

        if (result->len == 1) return res->wrap(result->at(0).gotype);

        auto ret = new_gotype(GOTYPE_MULTI);
        ret->multi_types = new_list(Gotype*, result->len);
        For (result) ret->multi_types->append(it.gotype);
        return res->wrap(ret);
    }

    // =========================
    // END OF TYPE INSTANTIATION
    // =========================

    case GOTYPE_LAZY_DEREFERENCE: {
        auto res = unwrap_type(gotype->lazy_dereference_base, U_ALL & ~U_UNPOINTER);
        if (!res) return NULL;

        if (res->gotype->type != GOTYPE_POINTER) return NULL;
        return _evaluate_type(res->gotype->pointer_base, res->ctx, outdecl);
    }

    case GOTYPE_LAZY_REFERENCE: {
        auto res = unwrap_type(gotype->lazy_reference_base, U_EVAL | U_SUBST);
        if (!res) return NULL;

        auto type = new_gotype(GOTYPE_POINTER);
        type->pointer_base = res->gotype;
        return res->wrap(type);
    }

    case GOTYPE_LAZY_ARROW: {
        auto res = unwrap_type(gotype->lazy_arrow_base);
        if (!res) return NULL;
        if (res->gotype->type != GOTYPE_CHAN) return NULL;

        res = _evaluate_type(res->gotype->chan_base, res->ctx, outdecl);
        if (!res) return NULL;

        auto ret = new_gotype(GOTYPE_RECEIVE);
        ret->receive_base = res->gotype;
        return res->wrap(ret);
    }

    case GOTYPE_LAZY_ID: {
        auto res = find_decl_of_id(gotype->lazy_id_name, gotype->lazy_id_pos, ctx);
        if (!res) return NULL;
        if (res->decl->type == GODECL_IMPORT) return NULL;
        // strictly speaking, GOTYPE_LAZY_ID refers to an expression, so the
        // decl type must not be a type, otherwise we're directly naming the
        // type (and it's a GOTYPE_ID). should we explicitly disallow here?
        //
        // a couple places call _evaluate_type on an id that possibly refers to
        // a type, and it's useful to get the GODECL_TYPE back.
        if (!res->decl->gotype) return NULL;

        if (outdecl) *outdecl = res->decl;
        return _evaluate_type(res->decl->gotype, res->ctx, outdecl);
    }

    case GOTYPE_LAZY_SEL: {
        do {
            if (!gotype->lazy_sel_base) break;
            if (gotype->lazy_sel_base->type != GOTYPE_LAZY_ID) break;

            auto base = gotype->lazy_sel_base;
            Go_Import *gi = NULL;

            auto decl_res = find_decl_of_id(base->lazy_id_name, base->lazy_id_pos, ctx, &gi);
            if (!decl_res) break;
            if (!gi) break;

            auto res = find_decl_in_package(gotype->lazy_sel_sel, gi->import_path);
            if (!res) return NULL;

            auto ext_decl = res->decl;
            switch (ext_decl->type) {
            case GODECL_VAR:
            case GODECL_CONST:
            case GODECL_FUNC:
            case GODECL_TYPE: // this wasn't added before, why?
                if (outdecl) *outdecl = ext_decl;
                return _evaluate_type(ext_decl->gotype, res->ctx, outdecl);
            default:
                return NULL;
            }
        } while (0);

        auto res = _evaluate_type(gotype->lazy_sel_base, ctx);
        if (!res) return NULL;

        auto rres = resolve_type(res);
        if (!rres) return NULL;

        rres = subst_generic_type(rres);
        if (!rres) return NULL;

        rres = unpointer_type(rres);
        if (!rres) return NULL;

        List<Goresult> results;
        results.init();
        list_dotprops(res, rres, &results);

        // ---
        // find the method & correct its type
        // ---

        Gotype *field_type = NULL;
        Godecl *field_decl = NULL;
        Go_Ctx *field_ctx = NULL;

        For (&results) {
            if (streq(it.decl->name, gotype->lazy_sel_sel)) {
                field_type = it.decl->gotype;
                field_decl = it.decl;
                field_ctx = it.ctx;
                break;
            }
        }

        if (!field_type) break;

        auto handle_generics = [&]() {
            if (field_type->type != GOTYPE_FUNC) return;

            auto recv = field_type->func_recv;
            if (!recv) return;
            recv = unpointer_type(recv);
            if (!recv) return;
            if (recv->type != GOTYPE_GENERIC) return;
            if (!recv->generic_args) return;

            auto object_res = res;
            auto object_type = res->gotype;

            if (object_type->type == GOTYPE_POINTER)
                object_type = object_type->base;

            if (object_type->type != GOTYPE_GENERIC) {
                // Here, we are unlucky and did not get the straightforward
                // case where the object type was itself the generic. Instead,
                // it's probably a struct or something that embeds a generic
                // type.
                //
                // Fortunately, we know what to do. We have the field, which is
                // a func. Inside that is the receiver in `func_recv`. We just
                // need to find the embedded struct inside the struct that that
                // func_recv points to. If that is a generic, we're golden --
                // just use it.

                auto recv_base = recv->generic_base;
                if (recv_base->type != GOTYPE_ID) return; // ??? this can't even happen can it
                auto recv_name = recv_base->id_name;

                typedef fn<bool(Goresult*)> fn_type;

                // so basically, now we need to find an embedded type with name recv_name
                // whose decl is located in same ctx as the recv's ctx, i.e. field_ctx
                fn_type find_embedded_generic = [&](auto res) {
                    auto gotype = unpointer_type(res->gotype);
                    if (!gotype) return false;

                    if (gotype->type == GOTYPE_GENERIC) {
                        auto base = gotype->generic_base;
                        if (!is_type_ident(base)) return false;

                        do {
                            // we now have a gotype_id or gotype_sel
                            // we need to know if it refers to the same as the recv

                            // find what the generic in the struct points to
                            auto a = resolve_type_to_decl(base, res->ctx);
                            if (!a) break;

                            // find what the generic in the recv points to
                            auto b = resolve_type_to_decl(recv_base, field_ctx);
                            if (!b) break;

                            if (!are_decls_equal(a, b)) break;

                            object_res = res;
                            object_type = gotype;
                            return true;
                        } while (0);

                        // if the generic wasn't a match, grab its base and try it as an id/sel
                        gotype = base;
                    }

                    switch (gotype->type) {
                    case GOTYPE_ID:
                    case GOTYPE_SEL: {
                        auto newres = resolve_type_to_decl(gotype, res->ctx);
                        if (!newres) break;
                        return find_embedded_generic(newres->wrap(newres->decl->gotype));
                    }
                    case GOTYPE_STRUCT:
                        For (gotype->struct_specs)
                            if (it.field->field_is_embedded)
                                if (find_embedded_generic(res->wrap(it.field->gotype)))
                                    return true;
                        break;
                    }
                    return false;
                };

                if (!find_embedded_generic(object_res->wrap(object_type)))
                    return;
            }

            if (!object_type->generic_args->len) return;
            if (recv->generic_args->len != object_type->generic_args->len) return;

            Table<Goresult*> lookup; lookup.init();
            Fori (recv->generic_args) {
                if (it->type != GOTYPE_ID)
                    return;
                lookup.set(it->id_name, object_res->wrap(object_type->generic_args->at(i)));
            }
            field_type = walk_gotype_and_replace_ids(field_type, &lookup);
        };

        handle_generics();

        if (outdecl) *outdecl = field_decl;
        return _evaluate_type(field_type, field_ctx);
    }

    case GOTYPE_LAZY_ONE_OF_MULTI: {
        auto res = unwrap_type(gotype->lazy_one_of_multi_base, U_EVAL);
        if (!res) return NULL;

        bool is_single = gotype->lazy_one_of_multi_is_single;
        int index = gotype->lazy_one_of_multi_index;
        Gotype *base = gotype->lazy_one_of_multi_base;

        switch (res->gotype->type) {
        case GOTYPE_MULTI: {
            auto types = res->gotype->multi_types;
            if (index >= types->len) return NULL;
            return _evaluate_type(types->at(index), res->ctx);
        }

        case GOTYPE_RECEIVE:
            if (index == 0) return _evaluate_type(res->gotype->receive_base, res->ctx);
            if (index == 1) return new_primitive_type_goresult("bool");
            break;

        case GOTYPE_ASSERTION:
            if (index == 0) return _evaluate_type(res->gotype->assertion_base, res->ctx);
            if (index == 1) return new_primitive_type_goresult("bool");
            break;

        case GOTYPE_RANGE:
            switch (res->gotype->range_base->type) {
            case GOTYPE_MAP:
                if (index == 0) return _evaluate_type(res->gotype->range_base->map_key, res->ctx);
                if (index == 1) return _evaluate_type(res->gotype->range_base->map_value, res->ctx);
                break;

            case GOTYPE_ARRAY:
            case GOTYPE_SLICE:
                if (index == 0)
                    return new_primitive_type_goresult("int");
                if (index == 1) {
                    auto base = res->gotype->type == GOTYPE_ARRAY ? res->gotype->range_base->array_base : res->gotype->range_base->slice_base;
                    return _evaluate_type(base, res->ctx);
                }
                break;

            case GOTYPE_ID:
                if (!streq(res->gotype->id_name, "string")) break;
                if (index == 0) return new_primitive_type_goresult("int");
                if (index == 1) return new_primitive_type_goresult("rune");
                break;
            }
            break;
        }

        if (gotype->lazy_one_of_multi_is_single) return res;
        break;
    }

    default: return make_goresult(gotype, ctx);
    }

    return NULL;
}

Goresult *make_goresult_from_pointer(void *ptr, Go_Ctx *ctx) {
    cp_assert(ptr);

    auto ret = new_object(Goresult);
    ret->ptr = ptr;
    ret->ctx = ctx;
    return ret;
}

Goresult *make_goresult(Gotype *gotype, Go_Ctx *ctx) {
    if (gotype->type == GOTYPE_OVERRIDE_CTX) {
        ctx = gotype->override_ctx_ctx;
        gotype = gotype->override_ctx_base;
    }
    return make_goresult_from_pointer(gotype, ctx);
}

Goresult *make_goresult(Godecl *decl, Go_Ctx *ctx) {
    return make_goresult_from_pointer(decl, ctx);
}

// resolves a GOTYPE_ID or GOTYPE_SEL to the decl it points to.
Goresult *Go_Indexer::resolve_type_to_decl(Gotype *type, Go_Ctx *ctx) {
    Goresult *res = NULL;

    switch (type->type) {
    case GOTYPE_ID:
        res = find_decl_of_id(type->id_name, type->id_pos, ctx);
        break;

    case GOTYPE_SEL: {
        auto import_path = find_import_path_referred_to_by_id(type->sel_name, ctx);
        if (!import_path) return NULL;

        res = find_decl_in_package(type->sel_sel, import_path);
        break;
    }
    }

    if (!res) return NULL;
    if (res->decl->type != GODECL_TYPE && res->decl->type != GODECL_TYPE_PARAM)
        return NULL;
    return res;
}

Goresult *Go_Indexer::resolve_type(Goresult *res) {
    return resolve_type(res->gotype, res->ctx);
}

Goresult *Go_Indexer::resolve_type(Gotype *type, Go_Ctx *ctx) {
    String_Set seen; seen.init();
    return resolve_type(type, ctx, &seen);
}

Goresult *Go_Indexer::resolve_type(Gotype *type, Go_Ctx *ctx, String_Set *seen) {
    if (!type) return NULL;

    switch (type->type) {
    case GOTYPE_BUILTIN: // pending decision: should we do this here?
        if (dont_resolve_builtin) break;
        if (!type->builtin_underlying_base) break;
        return resolve_type(type->builtin_underlying_base, ctx, seen);

    // is this a hack? does it work?
    case GOTYPE_CONSTRAINT: {
        // as far as i understand, if a type constraint is an interface
        // with methods, it can't be joined with other interfaces.
        if (type->constraint_terms->len != 1) break;

        auto term = type->constraint_terms->at(0);
        return resolve_type(term, ctx, seen);
    }

    case GOTYPE_RECEIVE:
    case GOTYPE_ASSERTION:
    case GOTYPE_POINTER: {
        auto b = type->base;

        // if it's an id or sel, manually resolve it in order to check if it's already in table
        // (we need to resolve because the table key needs to know the go_ctx too)
        // if so, break out, otherwise, proceed as usual
        // this does mean we'll be resolving twice, but who cares
        if (is_type_ident(b)) {
            auto res = resolve_type_to_decl(b, ctx);
            if (!res) return NULL;

            auto name = b->type == GOTYPE_ID ? b->id_name : b->sel_sel;
            auto key = cp_sprintf("%s:%s", ctx_to_filepath(res->ctx), name);

            // pointer base is an id we've already seen, break out to return type as is
            if (seen->has(key)) break;
        }

        auto res = resolve_type(b, ctx, seen);
        if (!res) return NULL;

        auto ret = type->copy();
        ret->base = res->gotype;
        return res->wrap(ret);
    }

    case GOTYPE_ID:
    case GOTYPE_SEL: {
        auto res = resolve_type_to_decl(type, ctx);
        if (!res) return NULL;

        auto name = type->type == GOTYPE_ID ? type->id_name : type->sel_sel;
        auto key = cp_sprintf("%s:%s", ctx_to_filepath(res->ctx), name);
        if (seen->has(key)) return NULL;
        seen->add(key);

        return resolve_type(res->decl->gotype, res->ctx, seen);
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
        // it is important that cb fills in node->it for the user
        wrapped_node.init(node, NULL);

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

ccstr _path_join(ccstr a, ...) {
    va_list vl, vlcount;
    va_start(vl, a);
    va_copy(vlcount, vl);

    auto ret = new_list(char);

    while (true) {
        ccstr val = NULL;
        if (a) {
            val = a;
            a = NULL;
        } else {
            val = va_arg(vlcount, ccstr);
        }
        if (!val) break;
        if (val[0] == '\0') continue;

        auto len = strlen(val);
        if (len > 0 && is_sep(val[len-1])) len--;

        // copy val into ret, normalizing path separators
        for (u32 j = 0; j < len; j++) {
            auto ch = val[j];
            if (is_sep(ch)) ch = PATH_SEP;
            ret->append(ch);
        }

        ret->append(PATH_SEP);
    }

    if (ret->len > 0) ret->len--; // remove last PATH_SEP

    ret->append('\0');

    va_end(vl);
    va_end(vlcount);
    return ret->items;
}

ccstr case_style_pretty_str(int x) {
    return case_style_pretty_str((Case_Style)x);
}

ccstr case_style_pretty_str(Case_Style x) {
    switch (x) {
    case CASE_SNAKE: return "Snake case (looks_like_this)";
    case CASE_PASCAL: return "Pascal case (LooksLikeThis)";
    case CASE_CAMEL: return "Camel case (looksLikeThis)";
    }
    return NULL;
}

bool is_type_ident(Gotype *x) {
    switch (x->type) {
    case GOTYPE_ID:
    case GOTYPE_SEL:
        return true;
    }
    return false;
}

// -----
// read functions

#define depointer(x) std::remove_pointer<x>::type
#define READ_STR(x) x = s->readstr()
#define READ_OBJ(x) x = read_object<depointer(decltype(x))>(s)
#define READ_LIST(x) x = read_list<depointer(decltype(x))::type>(s)
#define READ_LISTP(x) x = read_listp<depointer(depointer(decltype(x))::type)>(s)

// ---

void Go_Workspace::read(Index_Stream *s) {
    READ_LIST(modules);
}

void Go_Work_Module::read(Index_Stream *s) {
    READ_STR(import_path);
    READ_STR(resolved_path);
}

void Godecl::read(Index_Stream *s) {
    READ_STR(name);

    if (type == GODECL_IMPORT) {
        READ_STR(import_path);
    } else {
        READ_OBJ(gotype);
        switch (type) {
        case GODECL_FUNC:
        case GODECL_TYPE:
            READ_LIST(type_params);
            break;
        case GODECL_METHOD_RECEIVER_TYPE_PARAM:
            READ_OBJ(base);
            break;
        }
    }
}

void Go_Struct_Spec::read(Index_Stream *s) {
    READ_STR(tag);
    READ_OBJ(field);
}

void Go_Interface_Spec::read(Index_Stream *s) {
    READ_OBJ(field);
}

void Go_Import::read(Index_Stream *s) {
    READ_STR(package_name);
    READ_STR(import_path);
    READ_OBJ(decl);
}

void Go_Reference::read(Index_Stream *s) {
    if (is_sel) {
        READ_OBJ(x);
        READ_STR(sel);
    } else {
        READ_STR(name);
    }
}

void Gotype::read(Index_Stream *s) {
    switch (type) {
    case GOTYPE_GENERIC:
        READ_OBJ(generic_base);
        READ_LISTP(generic_args);
        break;
    case GOTYPE_LAZY_INSTANCE:
        READ_OBJ(lazy_instance_base);
        READ_LISTP(lazy_instance_args);
        break;
    case GOTYPE_CONSTRAINT:
        READ_LISTP(constraint_terms);
        break;
    case GOTYPE_CONSTRAINT_UNDERLYING:
        READ_OBJ(constraint_underlying_base);
        break;
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
        break;
    case GOTYPE_MULTI: {
        // can't use READ_LIST here because multi_types contains pointers
        // (instead of the objects themselves)
        auto len = s->read4();
        multi_types = new_list(Gotype*, len);
        for (u32 i = 0; i < len; i++) {
            auto gotype = read_object<Gotype>(s);
            multi_types->append(gotype);
        }
        break;
    }
    case GOTYPE_ASSERTION:
        READ_OBJ(assertion_base);
        break;
    case GOTYPE_RECEIVE:
        READ_OBJ(receive_base);
        break;
    case GOTYPE_RANGE:
        READ_OBJ(range_base);
        break;
    case GOTYPE_BUILTIN:
        READ_OBJ(builtin_underlying_base);
        break;
    case GOTYPE_LAZY_INDEX:
        READ_OBJ(lazy_index_base);
        READ_OBJ(lazy_index_key);
        break;
    case GOTYPE_LAZY_CALL:
        READ_OBJ(lazy_call_base);
        READ_LISTP(lazy_call_args);
        break;
    case GOTYPE_LAZY_DEREFERENCE:
        READ_OBJ(lazy_dereference_base);
        break;
    case GOTYPE_LAZY_REFERENCE:
        READ_OBJ(lazy_reference_base);
        break;
    case GOTYPE_LAZY_ARROW:
        READ_OBJ(lazy_arrow_base);
        break;
    case GOTYPE_LAZY_ID:
        READ_STR(lazy_id_name);
        break;
    case GOTYPE_LAZY_SEL:
        READ_OBJ(lazy_sel_base);
        READ_STR(lazy_sel_sel);
        break;
    case GOTYPE_LAZY_ONE_OF_MULTI:
        READ_OBJ(lazy_one_of_multi_base);
        break;
    case GOTYPE_LAZY_RANGE:
        READ_OBJ(lazy_range_base);
        break;
    case GOTYPE_OVERRIDE_CTX:
        cp_panic("invalid type GOTYPE_OVERRIDE_CTX");
        break;
    }
}

void Go_Scope_Op::read(Index_Stream *s) {
    if (type == GSOP_DECL)
        READ_OBJ(decl);
}

void Go_File::read(Index_Stream *s) {
    pool.init();
    SCOPED_MEM(&pool);

    READ_STR(filename);
    READ_LIST(scope_ops);
    READ_LIST(decls);
    READ_LIST(imports);
    READ_LIST(references);
}

void Go_Package::read(Index_Stream *s) {
    READ_STR(import_path);
    READ_STR(package_name);
    READ_LIST(files);
}

void Go_Index::read(Index_Stream *s) {
    READ_OBJ(workspace);
    READ_LIST(packages);
}

// ---

#define WRITE_STR(x) s->writestr(x)
#define WRITE_OBJ(x) write_object<std::remove_pointer<decltype(x)>::type>(x, s)
#define WRITE_LIST(x) write_list<decltype(x)>(x, s)
#define WRITE_LISTP(x) write_listp<decltype(x)>(x, s)

void Go_Workspace::write(Index_Stream *s) {
    WRITE_LIST(modules);
}

void Go_Work_Module::write(Index_Stream *s) {
    WRITE_STR(import_path);
    WRITE_STR(resolved_path);
}

void Godecl::write(Index_Stream *s) {
    WRITE_STR(name);

    if (type == GODECL_IMPORT) {
        WRITE_STR(import_path);
    } else {
        WRITE_OBJ(gotype);
        switch (type) {
        case GODECL_FUNC:
        case GODECL_TYPE:
            WRITE_LIST(type_params);
            break;
        case GODECL_METHOD_RECEIVER_TYPE_PARAM:
            WRITE_OBJ(base);
            break;
        }
    }
}

void Go_Struct_Spec::write(Index_Stream *s) {
    WRITE_STR(tag);
    WRITE_OBJ(field);
}

void Go_Interface_Spec::write(Index_Stream *s) {
    WRITE_OBJ(field);
}

void Go_Import::write(Index_Stream *s) {
    WRITE_STR(package_name);
    WRITE_STR(import_path);
    WRITE_OBJ(decl);
}

void Go_Reference::write(Index_Stream *s) {
    if (is_sel) {
        WRITE_OBJ(x);
        WRITE_STR(sel);
    } else {
        WRITE_STR(name);
    }
}

void Gotype::write(Index_Stream *s) {
    switch (type) {
    case GOTYPE_GENERIC:
        WRITE_OBJ(generic_base);
        WRITE_LISTP(generic_args);
        break;
    case GOTYPE_LAZY_INSTANCE:
        WRITE_OBJ(lazy_instance_base);
        WRITE_LISTP(lazy_instance_args);
        break;
    case GOTYPE_CONSTRAINT:
        WRITE_LISTP(constraint_terms);
        break;
    case GOTYPE_CONSTRAINT_UNDERLYING:
        WRITE_OBJ(constraint_underlying_base);
        break;
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
        break;
    case GOTYPE_MULTI:
        WRITE_LISTP(multi_types);
        break;
    case GOTYPE_ASSERTION:
        WRITE_OBJ(assertion_base);
        break;
    case GOTYPE_RECEIVE:
        WRITE_OBJ(receive_base);
        break;
    case GOTYPE_RANGE:
        WRITE_OBJ(range_base);
        break;
    case GOTYPE_BUILTIN:
        WRITE_OBJ(builtin_underlying_base);
        break;
    case GOTYPE_LAZY_INDEX:
        WRITE_OBJ(lazy_index_base);
        WRITE_OBJ(lazy_index_key);
        break;
    case GOTYPE_LAZY_CALL:
        WRITE_OBJ(lazy_call_base);
        WRITE_LISTP(lazy_call_args);
        break;
    case GOTYPE_LAZY_DEREFERENCE:
        WRITE_OBJ(lazy_dereference_base);
        break;
    case GOTYPE_LAZY_REFERENCE:
        WRITE_OBJ(lazy_reference_base);
        break;
    case GOTYPE_LAZY_ARROW:
        WRITE_OBJ(lazy_arrow_base);
        break;
    case GOTYPE_LAZY_ID:
        WRITE_STR(lazy_id_name);
        break;
    case GOTYPE_LAZY_SEL:
        WRITE_OBJ(lazy_sel_base);
        WRITE_STR(lazy_sel_sel);
        break;
    case GOTYPE_LAZY_ONE_OF_MULTI:
        WRITE_OBJ(lazy_one_of_multi_base);
        break;
    case GOTYPE_LAZY_RANGE:
        WRITE_OBJ(lazy_range_base);
        break;
    case GOTYPE_OVERRIDE_CTX:
        cp_panic("invalid type GOTYPE_OVERRIDE_CTX");
        break;
    }
}

void Go_Scope_Op::write(Index_Stream *s) {
    if (type == GSOP_DECL)
        WRITE_OBJ(decl);
}

void Go_File::write(Index_Stream *s) {
    WRITE_STR(filename);
    WRITE_LIST(scope_ops);
    WRITE_LIST(decls);
    WRITE_LIST(imports);
    WRITE_LIST(references);
}

void Go_Package::write(Index_Stream *s) {
    WRITE_STR(import_path);
    WRITE_STR(package_name);
    WRITE_LIST(files);
}

void Go_Index::write(Index_Stream *s) {
    WRITE_OBJ(workspace);
    WRITE_LIST(packages);
}
