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

#if OS_WIN
#include <windows.h>
#elif OS_MAC
#include <dlfcn.h>
#endif

#define GO_DEBUG 0

#if GO_DEBUG
#define go_print(fmt, ...) print("[go] " fmt, ##__VA_ARGS__)
#else
#define go_print(fmt, ...)
#endif

const int GO_INDEX_MAGIC_NUMBER = 0x49fa98;
// version 16: add Go_Reference
// version 17: change GO_INDEX_MAGIC_NUMBER
// version 18: change Go_Reference
// version 19: fix Go_File::read()/write() not saving Go_Reference
// version 20: fix Go_Reference not using correct pool
// version 21: remove array_size from Gotype
// version 22: sort references
// version 23: rename @builtins to @builtin
// version 24: don't include "_" decls
// version 25: fix selector references being counted a second time as single ident
// version 26: fix scope ops not handling TS_FUNC_LITERAL
// version 27: fix parser handling newlines and idents wrong in interface specs
// version 28: upgrade tree-sitter-go
// version 29: generics
// version 30: generics cont
const int GO_INDEX_VERSION = 30;

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
    if (!size) return NULL;

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

    // custom handle
    if (custom_handler(this, t)) return;

    auto recur = [&](Gotype *t) { write_type(t, custom_handler); };

    switch (t->type) {
    case GOTYPE_BUILTIN:
        switch (t->builtin_type) {
        case GO_BUILTIN_COMPLEXTYPE: write("ComplexType"); break;
        case GO_BUILTIN_FLOATTYPE: write("FloatType"); break;
        case GO_BUILTIN_INTEGERTYPE: write("IntegerType"); break;
        case GO_BUILTIN_TYPE: write("Type"); break;
        case GO_BUILTIN_TYPE1: write("Type1"); break;
        case GO_BUILTIN_BOOL: write("bool"); break;
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
        For (*t->struct_specs) {
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
        For (*t->interface_specs) {
            // TODO: branch on it.field->is_embedded
            if (it.field->is_embedded) continue;

            write("%s ", it.field->name);
            write_type(it.field->gotype, custom_handler, true);
            write("\n");
        }
        write("}");
        break;
    case GOTYPE_VARIADIC:
        write("...");
        recur(t->variadic_base);
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
            For (*params) {
                if (is_goident_empty(it.name)) {
                    if (!is_result)
                        write("_ ");
                } else {
                    write("%s ", it.name);
                }
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
        write("(multi type?)");
    }
}

void Module_Resolver::init(ccstr current_module_filepath, ccstr _gomodcache) {
    ptr0(this);

    mem.init();

    SCOPED_MEM(&mem);

    gomodcache = cp_strdup(_gomodcache);

    root_import_to_resolved = alloc_object(Node);
    root_resolved_to_import = alloc_object(Node);

    Process proc;
    proc.init();
    proc.dir = current_module_filepath;
    proc.skip_shell = true;
    if (!proc.run(cp_sprintf("%s list -mod=mod -m all", world.go_binary_path))) return;
    defer { proc.cleanup(); };

    List<char> line;
    line.init();
    char ch;

    do {
        line.len = 0;
        for (ch = '\0'; proc.read1(&ch) && ch != '\n'; ch = '\0')
            line.append(ch);
        line.append('\0');

        // print("%s", line.items);

        ccstr import_path = NULL;
        ccstr resolved_path = NULL;

        auto parts = split_string(line.items, ' ');
        if (parts->len == 1) {
            module_path = cp_strdup(parts->at(0));
            import_path = module_path;
            resolved_path = current_module_filepath;
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
    } while (ch != '\0');

    if (!module_path) {
        // TODO
        cp_panic("Sorry, currently only modules are supported.");
    }
}

void Module_Resolver::cleanup() {
    // ???
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
        file->scope_ops = alloc_list<Go_Scope_Op>();
        file->decls = alloc_list<Godecl>();
        file->imports = alloc_list<Go_Import>();
        file->references = alloc_list<Go_Reference>();
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

    auto pf = parse_file(filepath, false);
    if (!pf) return;
    defer { free_parsed_file(pf); };

    ccstr package_name = NULL;
    process_tree_into_gofile(file, pf->root, filepath, &package_name);
    replace_package_name(pkg, package_name);
}

void Go_Indexer::reload_editor(void *editor) {
    auto it = (Editor*)editor;

    SCOPED_FRAME();

    Timer t;
    t.init(cp_sprintf("reload %s", cp_basename(it->filepath)));

    auto filename = cp_basename(it->filepath);

    auto import_path = filepath_to_import_path(cp_dirname(it->filepath));
    auto pkg = find_package_in_index(import_path);
    if (!pkg) return;

    t.log("get package");

    auto file = get_ready_file_in_package(pkg, filename);

    t.log("get file");

    auto iter = alloc_object(Parser_It);
    iter->init(it->buf);
    auto root_node = new_ast_node(ts_tree_root_node(it->buf->tree), iter);

    ccstr package_name = NULL;
    process_tree_into_gofile(file, root_node, it->filepath, &package_name);
    replace_package_name(pkg, package_name);

    t.log("process tree");

    it->buf->tree_dirty = false;
}

// @Write
// Should only be called from main thread.
void Go_Indexer::reload_all_editors(bool force) {
    For (world.panes) {
        For (it.editors) {
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

    ccstr fake_filename = "this is a fake file";

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
            func_type->func_sig.params = alloc_list<Godecl>();
            func_type->func_sig.result = alloc_list<Godecl>();
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
            auto ret = new_gotype(GOTYPE_VARIADIC);
            ret->variadic_base = base;
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

        // error interface
        {
            SCOPED_MEM(&final_mem);

            start();
            add_result(builtin("string"));

            auto field = alloc_object(Godecl);
            field->type = GODECL_PARAM;
            field->name = "Error";
            field->gotype = func_type->copy();

            auto error_interface = new_gotype(GOTYPE_INTERFACE);
            error_interface->interface_specs = alloc_list<Go_Interface_Spec>(1);

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
        add_param("v", new_gotype(GOTYPE_INTERFACE));
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
        add_result(new_gotype(GOTYPE_INTERFACE));
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
        For (*file->imports) {
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

        for (int i = 0; i < index.packages->len; i++) {
            auto &it = index.packages->at(i);
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

        pkg->cleanup_files();
        index.packages->remove(pkg);
        rebuild_package_lookup();
    };

    // random shit

    // try to read in index from disk
    // ===

    do {
        index_print("Reading existing database...");

        Index_Stream s;
        if (!s.open(path_join(world.current_path, ".cpdb"))) {
            index_print("No database found (or couldn't open).");
            break;
        }

        defer { s.cleanup(); };

        {
            SCOPED_MEM(&final_mem);

            auto obj = s.read_index();
            if (!s.ok) {
                index_print("Unable to read database file.");
                break;
            }

            memcpy(&index, obj, sizeof(Go_Index));
        }

#ifdef DEBUG_MODE
        index_print("Successfully read database from disk, final_mem.size = %d", final_mem.mem_allocated);
#else
        index_print("Successfully read database from disk.");
#endif
    } while (0);

    // initialize index
    // ===

    auto init_index = [&](bool force_reset_index) {
        bool reset_index = false;

        if (force_reset_index)
            reset_index = true;

        SCOPED_MEM(&final_mem);
        if (index.current_path && !streq(index.current_path, world.current_path))
            reset_index = true;

        auto workspace_import_path = module_resolver.module_path;
        if (index.current_import_path && !streq(index.current_import_path, workspace_import_path))
            reset_index = true;

        index.current_path = cp_strdup(world.current_path);
        index.current_import_path = cp_strdup(workspace_import_path);

        if (!index.packages || reset_index)
            index.packages = alloc_list<Go_Package>();
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

            auto import_paths_queue = alloc_list<ccstr>();
            auto resolved_paths_queue = alloc_list<ccstr>();

            import_paths_queue->append(index.current_import_path);
            resolved_paths_queue->append(index.current_path);

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
                                if (str_ends_with(ent->name, ".go"))
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

                if (!already_in_index) {
                    if (is_go_package || streq(import_path, index.current_import_path))
                        enqueue_package(import_path);
                }
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

            For (*index.packages) {
                auto &pkg = it;

                if (pkg.status != GPS_READY) {
                    enqueue_package(pkg.import_path);
                    continue;
                }

                if (pkg.files)
                    For (*pkg.files)
                        enqueue_imports_from_file(&it);
            }
        }

        // mark all packages for outdated hash check
        // ===
        For (*index.packages) it.checked_for_outdated_hash = false;
    };

    rescan_everything(); // kick off rescan

    auto handle_fsevent = [&](ccstr filepath) {
        filepath = path_join(index.current_path, filepath);

        auto import_path = filepath_to_import_path(filepath);
        auto res = check_path(filepath);

        switch (res) {
        case CPR_DIRECTORY:
            if (is_go_package(filepath)) {
                start_writing(true);
                mark_package_for_reprocessing(import_path);
            }
            break;
        case CPR_FILE:
            if (is_go_package(cp_dirname(filepath)) && str_ends_with(filepath, ".go")) {
                start_writing(true);
                mark_package_for_reprocessing(cp_dirname(import_path));
            }
            if (are_filepaths_equal(path_join(index.current_path, "go.mod"), filepath)) {
                if (status == IND_READY) {
                    start_writing();
                    rescan_gomod(false);
                    rescan_everything();
                }
            }
            break;
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
            For (*msgs) process_message(&it);
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
                pkg->files = alloc_list<Go_File>();
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

            Timer t;
            t.init();

            auto source_files = list_source_files(resolved_path, true);
            if (!source_files) continue;
            if (!source_files->len) continue;

            create_package_if_null();

            pkg->status = GPS_UPDATING;
            pkg->cleanup_files();

            ccstr package_name = NULL;
            ccstr test_package_name = NULL;

            For (*source_files) {
                auto filename = it;

                // TODO: refactor, pretty sure we're doing same thing elsewhere

                SCOPED_FRAME();

                auto filepath = path_join(resolved_path, filename);

                auto pf = parse_file(filepath);
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
                last_package_processed = cp_strdup(import_path);
                packages_processed_since_last_write++;
            }

            index_print("Processed %s in %dms.", import_path, t.read_time() / 1000000);
        }

        if (!package_queue.len) {
            int i = 0;
            int num_checked = 0;

            if (queue_had_stuff) {
                index_print("Scanning packages for changes...");
                try_write_after_checking_hashes = true;
            }

            auto to_remove = alloc_list<int>();

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
#ifdef DEBUG_MODE
                    cp_panic("we have a package that wasn't found in package_lookup");
#endif
                } else {
                    if (true_copy != i) {
#ifdef DEBUG_MODE
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
            For (*to_remove) {
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

Parsed_File *Go_Indexer::parse_file(ccstr filepath, bool use_latest) {
    Parsed_File *ret = NULL;

    if (use_latest) {
        auto editor = find_editor_by_filepath(filepath);
        if (!editor) return NULL;
        if (!editor->buf->tree) return NULL;

        auto it = alloc_object(Parser_It);
        it->init(editor->buf);

        ret = alloc_object(Parsed_File);
        ret->tree_belongs_to_editor = true;
        ret->editor_parser = editor->buf->parser;
        ret->it = it;
        ret->tree = ts_tree_copy(editor->buf->tree);
    } else {
        auto fm = map_file_into_memory(filepath);
        if (!fm) return NULL;

        auto it = alloc_object(Parser_It);
        it->init(fm);

        Parser_Input pinput;
        pinput.indexer = this;
        pinput.it = it;

        TSInput input;
        input.payload = &pinput;
        input.encoding = TSInputEncodingUTF8;
        input.read = read_from_parser_input;

        auto tree = ts_parser_parse(new_ts_parser(), NULL, input);
        if (!tree) return NULL;

        ret = alloc_object(Parsed_File);
        ret->tree_belongs_to_editor = false;
        ret->editor_parser = NULL;
        ret->it = pinput.it;
        ret->tree = tree;
    }

    ret->root = new_ast_node(ts_tree_root_node(ret->tree), ret->it);
    return ret;
}

Ast_Node *new_ast_node(TSNode node, Parser_It *it) {
    auto ret = alloc_object(Ast_Node);
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
        it->set_pos(new_cur2((i32)start_byte(), (i32)-1));

        auto ret = alloc_array(char, len + 1);
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

        auto ret = alloc_list<char>();
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

Gotype *Go_Indexer::new_gotype(Gotype_Type type) {
    // maybe optimize size of the object we allocate, like we did with Ast
    auto ret = alloc_object(Gotype);
    ret->type = type;
    return ret;
}

Gotype *Go_Indexer::new_primitive_type(ccstr name) {
    auto ret = new_gotype(GOTYPE_ID);
    ret->id_name = name;
    return ret;
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
    if (it->package_name_type == GPN_EXPLICIT)
        if (it->package_name)
            return it->package_name;

    auto pkg = find_up_to_date_package(it->import_path);
    if (pkg)
        return pkg->package_name;

    return NULL;
}

ccstr Go_Indexer::find_import_path_referred_to_by_id(ccstr id, Go_Ctx *ctx) {
    auto pkg = find_up_to_date_package(ctx->import_path);
    if (!pkg) return NULL;

    For (*pkg->files) {
        if (streq(it.filename, ctx->filename)) {
            For (*it.imports) {
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
    List<int> open_scopes;
    open_scopes.init();

    auto scope_ops_decls = alloc_list<Godecl>();

    walk_ast_node(root, true, [&](Ast_Node* node, Ts_Field_Type, int depth) -> Walk_Action {
        for (; open_scopes.len > 0 && depth <= *open_scopes.last(); open_scopes.len--) {
            Go_Scope_Op op;
            op.type = GSOP_CLOSE_SCOPE;
            op.pos = node->start();
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
            open_scopes.append(depth);

            Go_Scope_Op op;
            op.type = GSOP_OPEN_SCOPE;
            op.pos = node->start();
            if (!cb(&op)) return WALK_ABORT;
            break;
        }

        case TS_TYPE_CASE:
        case TS_DEFAULT_CASE: {
            auto parent = node->parent();
            if (parent->null) break;
            if (parent->type() != TS_TYPE_SWITCH_STATEMENT) break;

            auto alias = parent->field(TSF_ALIAS);
            if (alias->null) break;
            if (alias->type() != TS_EXPRESSION_LIST) break;

            FOR_NODE_CHILDREN (alias) {
                if (it->type() != TS_IDENTIFIER) break;

                auto decl = alloc_object(Godecl);
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
                    gotype->interface_specs = alloc_list<Go_Interface_Spec>(0);
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

        // TODO: handle TS_TYPE_ALIAS here
        case TS_METHOD_DECLARATION:
        case TS_FUNCTION_DECLARATION:
        case TS_TYPE_DECLARATION:
        case TS_PARAMETER_LIST:
        case TS_SHORT_VAR_DECLARATION:
        case TS_CONST_DECLARATION:
        case TS_VAR_DECLARATION:
        case TS_RANGE_CLAUSE:
        case TS_RECEIVE_STATEMENT:
        case TS_PARAMETER_DECLARATION:
        case TS_TYPE_PARAMETER_DECLARATION: {
            if (node_type == TS_METHOD_DECLARATION || node_type == TS_FUNCTION_DECLARATION) {
                open_scopes.append(depth);

                Go_Scope_Op op;
                op.type = GSOP_OPEN_SCOPE;
                op.pos = node->start();
                if (!cb(&op)) return WALK_ABORT;
            }

            if (node_type == TS_RECEIVE_STATEMENT) {
                bool ok = false;
                do {
                    auto parent = node->parent();
                    if (parent->null) continue;
                    if (parent->type() != TS_COMMUNICATION_CASE) continue;
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
        }

        return WALK_CONTINUE;
    });

    For (open_scopes) {
        Go_Scope_Op op;
        op.type = GSOP_CLOSE_SCOPE;
        op.pos = root->end();
        cb(&op);
    }
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
        case TS_TYPE_ALIAS:
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

        walk_ast_node(root, true, [&](auto it, auto, auto) {
            Ast_Node *x = NULL, *sel = NULL;

            switch (it->type()) {
            case TS_IDENTIFIER:
            case TS_FIELD_IDENTIFIER:
            case TS_PACKAGE_IDENTIFIER:
            case TS_TYPE_IDENTIFIER: {
                /*
                auto parent = it->parent();
                if (!parent->null) {
                    if (parent->type() == TS_QUALIFIED_TYPE)
                        return WALK_SKIP_CHILDREN;
                    if (parent->type() == TS_SELECTOR_EXPRESSION)
                        return WALK_SKIP_CHILDREN;
                }
                */

                Go_Reference ref;
                ref.is_sel = false;
                ref.start = it->start();
                ref.end = it->end();
                ref.name = it->string();

                {
                    SCOPED_MEM(&file->pool);
                    file->references->append(ref.copy());
                }
                break;
            }

            case TS_QUALIFIED_TYPE:
            case TS_SELECTOR_EXPRESSION: {
                Ast_Node *x = NULL, *sel = NULL;

                if (it->type() == TS_QUALIFIED_TYPE) {
                    x = it->field(TSF_PACKAGE);
                    sel = it->field(TSF_NAME); // TODO: this is wrong, look at astviewer
                } else {
                    x = it->field(TSF_OPERAND);
                    /*
                    switch (x->type()) {
                    case TS_IDENTIFIER:
                    case TS_FIELD_IDENTIFIER:
                    case TS_PACKAGE_IDENTIFIER:
                    case TS_TYPE_IDENTIFIER:
                        break;
                    default:
                        return WALK_CONTINUE;
                    }
                    */
                    sel = it->field(TSF_FIELD);
                }

                auto xtype = expr_to_gotype(x);
                if (!xtype) break;

                Go_Reference ref;
                ref.is_sel = true;
                ref.x = expr_to_gotype(x);
                ref.x_start = x->start();
                ref.x_end = x->end();
                ref.sel = sel->string();
                ref.sel_start = sel->start();
                ref.sel_end = sel->end();

                {
                    SCOPED_MEM(&file->pool);
                    file->references->append(ref.copy());
                }

                /*
                switch (x->type()) {
                case TS_IDENTIFIER:
                case TS_FIELD_IDENTIFIER:
                case TS_PACKAGE_IDENTIFIER:
                case TS_TYPE_IDENTIFIER:
                    return WALK_SKIP_CHILDREN;
                }
                */
                return WALK_SKIP_CHILDREN;
            }
            }
            return WALK_CONTINUE;
        });

        file->references->sort([&](auto pa, auto pb) {
            auto a = pa->true_start();
            auto b = pb->true_start();

            if (a == b) return 0; // should this ever happen? no right?

            return a < b ? -1 : 1;
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

        import_decl_to_goimports(decl_node, filename, file->imports);

        {
            SCOPED_MEM(&file->pool);
            for (int i = 0; i < file->imports->len; i++) {
                auto imp = &file->imports->items[i];
                memcpy(imp, imp->copy(), sizeof(Go_Import));
            }
        }
    }

    if (time) t.log("get import info");

    file->hash = hash_file(filepath);
}

void Go_Indexer::import_decl_to_goimports(Ast_Node *decl_node, ccstr filename, List<Go_Import> *out) {
    auto speclist_node = decl_node->child();
    FOR_NODE_CHILDREN (speclist_node) {
        Ast_Node *name_node = NULL;
        Ast_Node *path_node = NULL;

        if (it->type() == TS_IMPORT_SPEC) {
            path_node = it->field(TSF_PATH);
            name_node = it->field(TSF_NAME);
        } else if (it->type() == TS_INTERPRETED_STRING_LITERAL) {
            path_node = it;
            name_node = NULL;
        } else {
            continue;
        }

        auto new_import_path = parse_go_string(path_node->string());
        if (!new_import_path) continue;

        // decl
        auto decl = alloc_object(Godecl);
        decl->decl_start = decl_node->start();
        decl->decl_end = decl_node->end();
        import_spec_to_decl(it, decl);

        // import
        auto imp = out->append();
        imp->decl = decl;
        imp->import_path = new_import_path;
        if (!name_node || name_node->null)
            imp->package_name_type = GPN_IMPLICIT;
        else if (name_node->type() == TS_DOT)
            imp->package_name_type = GPN_DOT;
        else if (name_node->type() == TS_BLANK_IDENTIFIER)
            imp->package_name_type = GPN_BLANK;
        else
            imp->package_name_type = GPN_EXPLICIT;

        if (imp->package_name_type == GPN_EXPLICIT)
            imp->package_name = name_node->string();
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
        For (*files)
            ret ^= hash_file(path_join(resolved_package_path, it));
    }

    return ret;
}

bool Go_Indexer::is_file_included_in_build(ccstr path) {
    return GHBuildEnvIsFileIncluded((char*)path);
}

List<ccstr>* Go_Indexer::list_source_files(ccstr dirpath, bool include_tests) {
    auto ret = alloc_list<ccstr>();

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
    auto pkg = find_up_to_date_package(ctx->import_path);
    if (pkg) {
        auto check = [&](Go_File *it) { return streq(it->filename, ctx->filename); };
        auto file = pkg->files->find(check);
        if (file) {
            // see if we're on a struct field declaration
            For (*file->decls) {
                if (it.type != GODECL_TYPE) continue;
                if (!(it.decl_start <= id_pos && id_pos < it.decl_end)) continue;
                if (it.decl_start > id_pos) break;

                auto gotype = it.gotype;

                auto check = [&](auto field) -> Goresult* {
                    if (!field->is_embedded)
                        if (field->name_start <= id_pos && id_pos < field->name_end)
                            return make_goresult(field, ctx);
                    return NULL;
                };

                if (gotype->type == GOTYPE_STRUCT) {
                    For (*gotype->struct_specs) {
                        auto ret = check(it.field);
                        if (ret) return ret;
                    }
                } else if (gotype->type == GOTYPE_INTERFACE) {
                    For (*gotype->interface_specs) {
                        if (it.type != GO_INTERFACE_SPEC_METHOD) continue;
                        auto ret = check(it.field);
                        if (ret) return ret;
                    }
                }
            }

            auto scope_ops = file->scope_ops;

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
                case GSOP_OPEN_SCOPE:
                    table.push_scope();
                    break;
                case GSOP_CLOSE_SCOPE:
                    table.pop_scope();
                    break;
                case GSOP_DECL:
                    if (it.decl->decl_start <= id_pos && id_pos < it.decl->decl_end)
                        if (it.decl_scope_depth == table.frames.len)
                            if (!(it.decl->name_start <= id_pos && id_pos < it.decl->name_end))
                                break;
                    table.set(it.decl->name, it.decl);
                    break;
                }
            }

            auto decl = table.get(id_to_find);
            if (decl) return make_goresult(decl, ctx);
        }

        For (*pkg->files) {
            if (streq(it.filename, ctx->filename)) {
                For (*it.imports) {
                    auto package_name = get_import_package_name(&it);
                    if (package_name && streq(package_name, id_to_find)) {
                        if (single_import)
                            *single_import = &it;
                        return make_goresult(it.decl, ctx);
                    }
                }
                break;
            }
        }
    }

    auto ret = find_decl_in_package(id_to_find, ctx->import_path);
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

    auto ret = alloc_list<Postfix_Completion_Type>();

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
            ret->append(PFC_IFNIL);
            ret->append(PFC_IFNOTNIL);
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
                For (*basetype->multi_types) {
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
        ret->append(PFC_IFEMPTY);
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

    auto tmp = alloc_list<Goresult>();
    list_dotprops(res, rres, tmp);

    auto results = alloc_list<Goresult>();
    For (*tmp) {
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

bool Go_Indexer::are_decls_equal(Goresult *adecl, Goresult *bdecl) {
    if (!adecl) return false;
    if (!adecl->ctx) return false;
    if (!bdecl) return false;
    if (!bdecl->ctx) return false;

    auto afile = ctx_to_filepath(adecl->ctx);
    if (!afile) return false;

    auto bfile = ctx_to_filepath(bdecl->ctx);
    if (!bfile) return false;

    if (!streq(afile, bfile)) return false;
    if (adecl->decl->name_start != bdecl->decl->name_start) return false;

    return true;
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
        auto a_is_ref = (a->type == GOTYPE_ID || a->type == GOTYPE_SEL);
        auto b_is_ref = (b->type == GOTYPE_ID || b->type == GOTYPE_SEL);

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
            For (*meths) {
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

    case GOTYPE_ASSERTION: {
        auto abase = ra->wrap(a->assertion_base);
        auto bbase = rb->wrap(b->assertion_base);
        return are_gotypes_equal(abase, bbase);
        break;
    }

    case GOTYPE_STRUCT: {
        auto sa = a->struct_specs;
        auto sb = a->struct_specs;
        if (sa->len != sb->len) return false;

        for (int i = 0; i < sa->len; i++) {
            auto &ita = sa->at(i);
            auto &itb = sb->at(i);

            if (ita.tag || itb.tag) {
                if (!ita.tag || !itb.tag) return false;
                if (!streq(ita.tag, itb.tag)) return false;
            }

            if (ita.field->is_embedded != itb.field->is_embedded) return false;
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

    case GOTYPE_POINTER: {
        auto abase = ra->wrap(a->pointer_base);
        auto bbase = rb->wrap(b->pointer_base);
        return are_gotypes_equal(abase, bbase);
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
            for (int i = 0; i < fa.params->len; i++) {
                auto ga = ra->wrap(fa.params->at(i).gotype);
                auto gb = rb->wrap(fb.params->at(i).gotype);
                if (!are_gotypes_equal(ga, gb)) return false;
            }
        }

        if (mismatch(fa.result, fb.result)) return false;

        if (!isempty(fa.result)) {
            for (int i = 0; i < fa.result->len; i++) {
                auto ga = ra->wrap(fa.result->at(i).gotype);
                auto gb = rb->wrap(fb.result->at(i).gotype);
                if (!are_gotypes_equal(ga, gb)) return false;
            }
        }

        return true;
    }

    case GOTYPE_SLICE: {
        auto abase = ra->wrap(a->slice_base);
        auto bbase = rb->wrap(b->slice_base);
        return are_gotypes_equal(abase, bbase);
    }

    case GOTYPE_ARRAY: {
        auto abase = ra->wrap(a->array_base);
        auto bbase = rb->wrap(b->array_base);

        // Strictly speaking, we should check the array size too. But that
        // would require us to evaluate the actual *value* of expressions,
        // something we can't do yet (right now we only deal with types).
        //
        // If it becomes important enough, we can do it.

        // return a->array_size == b->array_size && are_gotypes_equal(abase, bbase);
        return are_gotypes_equal(abase, bbase);
    }

    case GOTYPE_CHAN: {
        auto abase = ra->wrap(a->chan_base);
        auto bbase = rb->wrap(b->chan_base);
        return are_gotypes_equal(abase, bbase);
    }

    case GOTYPE_MULTI:
        return false; // this isn't a real go type, it's for our purposes

    case GOTYPE_VARIADIC: {
        auto abase = ra->wrap(a->variadic_base);
        auto bbase = rb->wrap(b->variadic_base);
        return are_gotypes_equal(abase, bbase);
    }

    case GOTYPE_RANGE: {
        auto abase = ra->wrap(a->range_base);
        auto bbase = rb->wrap(b->range_base);
        return are_gotypes_equal(abase, bbase);
    }

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

    auto methods = alloc_list<Goresult>();
    if (!list_type_methods(target->decl->name, target->ctx->import_path, methods))
        return NULL;

    For (*methods) {
        auto filepath = ctx_to_filepath(it.ctx);
        auto decl = it.decl;

        print("%s %s %s", filepath, decl->decl_start.str(), decl->name);
    }

    auto ret = alloc_list<Find_Decl>();

    For (*index.packages) {
        if (it.status != GPS_READY) continue;

        auto import_path = it.import_path;
        auto package_name = it.package_name;

        if (!search_everywhere)
            if (!path_has_descendant(index.current_import_path, import_path))
                continue;

        For (*it.files) {
            auto ctx = alloc_object(Go_Ctx);
            ctx->import_path = import_path;
            ctx->filename = it.filename;

            auto filepath = ctx_to_filepath(ctx);

            For (*it.decls) {
                if (it.type != GODECL_TYPE) continue;

                auto gotype = it.gotype;
                if (gotype->type != GOTYPE_INTERFACE) continue;

                // TODO: validate methods

                auto match = [&]() {
                    auto imethods = list_interface_methods(make_goresult(gotype, ctx));
                    if (!imethods) return false;

                    // test that all(<methods contains i> for i in imethods)
                    For (*imethods) {
                        auto imeth_name = it.decl->name;
                        if (!imeth_name) return false;

                        auto imeth = make_goresult(it.decl->gotype, ctx);
                        bool found = false;

                        For (*methods) {
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
    auto methods = alloc_list<Goresult>();
    if (!list_interface_methods(target->wrap(target->decl->gotype), methods))
        return NULL;
    // For (*target->decl->gotype->interface_specs) methods->append(it.field);

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
            ret = alloc_object(Type_Info);
            ret->methods_matched = alloc_array(bool, methods->len);
            huge_table.set(key, ret);
        }
        return ret;
    };

    For (*index.packages) {
        if (it.status != GPS_READY) continue;

        auto import_path = it.import_path;
        auto package_name = it.package_name;

        if (!search_everywhere)
            if (!path_has_descendant(index.current_import_path, import_path))
                continue;

        For (*it.files) {
            auto ctx = alloc_object(Go_Ctx);
            ctx->import_path = import_path;
            ctx->filename = it.filename;

            For (*it.decls) {
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

                for (int i = 0; i < methods->len; i++) {
                    auto &it = methods->at(i);

                    if (!streq(it.decl->name, method_name)) continue;
                    if (!are_gotypes_equal(it.wrap(it.decl->gotype), make_goresult(gotype, ctx)))
                        continue; // break here

                    auto type_info = get_type_info(type_name);
                    type_info->methods_matched[i] = true;

                    bool found = false;
                    auto info = huge_table.get(type_name, &found);
                    if (!found) {
                        info = alloc_object(Type_Info);
                        huge_table.set(type_name, info);
                    }

                    info->methods_matched[i] = true;
                    break;
                }
            }
        }
    }

    auto ret = alloc_list<Find_Decl>();

    auto entries = huge_table.entries();

    for (int i = 0; i < entries->len; i++) {
        auto &it = entries->at(i);
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

    recv = unpointer_type(recv, NULL)->gotype;
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
        pkgname = alloc_array(char, len+1);
        strncpy(pkgname, symbol, len);
        pkgname[len] = '\0';
    }

    auto base_path = make_path(index.current_import_path);

    For (*index.packages) {
        if (it.status != GPS_READY) continue;

        {
            SCOPED_FRAME();
            if (!base_path->contains(make_path(it.import_path)))
                continue;
        }

        if (!streq(it.package_name, pkgname)) continue;

        auto import_path = it.import_path;
        For (*it.files) {
            auto filename = it.filename;
            For (*it.decls) {
                auto key = it.name;
                auto recvname = get_godecl_recvname(&it);
                if (recvname)
                    key = cp_sprintf("%s.%s", recvname, it.name);

                if (!streq(key, rest)) continue;

                Go_Ctx ctx; ptr0(&ctx);
                ctx.filename = filename;
                ctx.import_path = import_path;
                auto filepath = ctx_to_filepath(&ctx);

                auto ret = alloc_object(Jump_To_Definition_Result);
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

    auto ret = alloc_list<Call_Hier_Node>();
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

    For (*ref_files) {
        auto filepath = it.filepath;
        auto ctx = filepath_to_ctx(filepath);
        Go_Package *pkg = NULL;
        auto file = find_gofile_from_ctx(ctx, &pkg);

        if (!file || !pkg) continue;

        For (*it.references) {
            // get the bounds of the reference
            cur2 start, end;
            if (it.is_sel) {
                start = it.x_start;
                end = it.sel_end;
            } else {
                start = it.start;
                end = it.end;
            }

            auto enclosing_decl = find_toplevel_containing(file, start, end);
            if (!enclosing_decl)
                continue;

            auto declres = make_goresult(enclosing_decl, ctx);

            auto fd = alloc_object(Find_Decl);
            fd->filepath = filepath;
            fd->decl = declres;
            fd->package_name = pkg->package_name;

            Call_Hier_Node node;
            node.decl = fd;
            node.children = alloc_list<Call_Hier_Node>();
            node.ref = &it;
            out->append(&node);

            if (enclosing_decl->gotype)
                if (enclosing_decl->gotype->type == GOTYPE_FUNC)
                    actually_generate_caller_hierarchy(declres, node.children);
        }
    }
}

List<Call_Hier_Node>* Go_Indexer::generate_callee_hierarchy(Goresult *declres) {
    reload_all_editors();

    auto ret = alloc_list<Call_Hier_Node>();
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
        seen = alloc_list<Seen_Callee_Entry>();

    auto find_seen = [&](Goresult *res) -> Call_Hier_Node* {
        For (*seen) {
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
        if (!path_has_descendant(index.current_import_path, res->ctx->import_path)) continue;

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

        auto fd = alloc_object(Find_Decl);
        fd->filepath = ctx_to_filepath(res->ctx);
        fd->decl = res;
        fd->package_name = pkg->package_name;

        Call_Hier_Node node; ptr0(&node);
        node.decl = fd;
        node.children = alloc_list<Call_Hier_Node>();
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

    For (*results)
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

    auto ret = alloc_list<Find_References_File>();

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

        auto references = alloc_list<Go_Reference>();

        bool same_package = streq(pkg->import_path, ctx->import_path);

        ccstr package_name_we_want;
        if (!same_package && case_type == CASE_NORMAL) {
            For (*file->imports) {
                // TODO: handle dot
                if (it.package_name_type == GPN_DOT) continue;
                if (it.package_name_type == GPN_BLANK) continue;

                if (streq(it.import_path, ctx->import_path)) {
                    package_name_we_want = get_import_package_name(&it);
                    break;
                }
            }
        }

        auto process_ref = [&](Go_Reference *it) {
            if (!streq(it->is_sel ? it->sel : it->name, decl_name))
                return;

            auto is_self = [&]() {
                if (!same_file_as_decl) return false;
                if (it->is_sel) return false;
                if (it->start != decl->name_start) return false;
                if (it->end != decl->name_end) return false;

                return true;
            };

            if (!include_self && is_self())
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
                    if (!it->is_sel) return;
                    if (it->x->type != GOTYPE_LAZY_ID) return;
                    if (!streq(it->x->lazy_id_name, package_name_we_want)) return;
                } else {
                    if (it->is_sel) return;
                }
                break;
            case CASE_METHOD:
                if (!it->is_sel) return;
                break;
            }

            auto decl = get_reference_decl(it, &ctx2);
            if (decl && is_match(decl))
                references->append(it);
        };

        For (*file->references) process_ref(&it);

        if (references->len > 0) {
            Find_References_File out;
            out.filepath = cp_strdup(path_join(get_package_path(pkg->import_path), file->filename));
            out.references = references;
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
        For (*pkg->files) process(pkg, &it);
    } else {
        For (*index.packages) {
            if (it.status != GPS_READY) continue;
            if (!path_has_descendant(index.current_import_path, it.import_path))
                continue;
            auto &pkg = it;
            For (*it.files) process(&pkg, &it);
        }
    }

    return ret;
}

List<Go_Import> *Go_Indexer::optimize_imports(ccstr filepath) {
    Timer t; t.init("list_missing_imports");

    auto editor = find_editor_by_filepath(filepath);
    if (editor->buf->tree_dirty)
        reload_editor(editor);

    t.log("reload");

    auto pf = parse_file(filepath, true);
    if (!pf) return NULL;
    defer { free_parsed_file(pf); };

    t.log("parse_file");

    auto ctx = filepath_to_ctx(filepath);
    if (!ctx) return NULL;

    String_Set package_refs; package_refs.init();
    String_Set full_refs; full_refs.init();

    /*
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

        if (x->null || sel->null) return WALK_CONTINUE;

        auto node = alloc_object(Ast_Node);
        memcpy(node, x, sizeof(Ast_Node));

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

    auto pkgs = alloc_list<Ref_Package>();
    String_Set referenced_package_names; referenced_package_names.init();

    // put this earlier maybe?
    auto gofile = find_gofile_from_ctx(ctx);
    if (!gofile) return NULL;

    For (*gofile->references) {
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
    auto ret = alloc_list<Go_Import>();

    if (gofile->imports) {
        For (*gofile->imports) {
            auto package_name = get_import_package_name(&it);

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

    For (*pkgs) {
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

    For (*index.packages) {
        if (it.status != GPS_READY) continue;
        if (!it.package_name) continue;
        if (!streq(it.package_name, package_name)) continue;

        if (!path_has_descendant(index.current_import_path, it.import_path))
            if (is_import_path_internal(it.import_path))
                continue;

        candidates.append(&it);
        indexes.append(indexes.len);
    }

    if (!candidates.len) return NULL;

    String_Set all_identifiers; all_identifiers.init();
    For (*identifiers) all_identifiers.add(it);

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
            For (*it->files) {
                if (!it.decls) continue;
                For (*it.decls) {
                    if (it.type == GODECL_FUNC)
                        if (!it.gotype->func_recv)
                            continue;
                    if (all_identifiers.has(it.name))
                        score.matching_idents++;
                }
            }
        }

        if (path_has_descendant(index.current_import_path, it->import_path))
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

Jump_To_Definition_Result* Go_Indexer::jump_to_definition(ccstr filepath, cur2 pos) {
    Timer t;
    t.init("jump_to_definition");

    reload_all_editors();

    t.log("reload dirty files");

    auto pf = parse_file(filepath, true);
    if (!pf) return NULL;
    defer { free_parsed_file(pf); };

    auto file = pf->root;

    auto ctx = filepath_to_ctx(filepath);
    if (!ctx) return NULL;

    Jump_To_Definition_Result result; ptr0(&result);

    t.log("setup shit");

    find_nodes_containing_pos(file, pos, false, [&](auto node) -> Walk_Action {
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
                For (*results) {
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
        case TS_FIELD_IDENTIFIER: {
            auto is_struct_field_in_literal = [&]() -> Ast_Node *{
                auto p = node->parent();
                if (p->null) return NULL;
                if (p->type() != TS_KEYED_ELEMENT) return NULL;

                // field must be first child
                if (!node->prev()->null) return NULL;

                p = p->parent();
                if (p->null) return NULL;
                if (p->type() != TS_LITERAL_VALUE) return NULL;

                p = p->parent();
                if (p->null) return NULL;
                if (p->type() != TS_COMPOSITE_LITERAL) return NULL;

                return p;
            };

            Goresult *declres = NULL;

            auto comp_literal = is_struct_field_in_literal();
            if (comp_literal) {
                do {
                    auto p = comp_literal->field(TSF_TYPE);
                    if (p->null) break;

                    auto gotype = expr_to_gotype(p);
                    if (!gotype) break;

                    auto res = evaluate_type(gotype, ctx);
                    if (!res) break;

                    auto rres = resolve_type(res);
                    if (!rres) break;

                    auto tmp = alloc_list<Goresult>();
                    list_struct_fields(rres, tmp);

                    auto name = node->string();
                    For (*tmp) {
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
        auto fm = map_file_into_memory(result.file);
        if (!fm) return NULL;
        defer { fm->cleanup(); };

        cur2 newpos; ptr0(&newpos);
        for (u32 i = 0; i < fm->len && i < result.pos.x; i++) {
            if (fm->data[i] == '\r') continue;
            if (fm->data[i] == '\n') {
                newpos.y++;
                newpos.x = 0;
                continue;
            }
            newpos.x++;
        }
        result.pos = newpos;
    }

    t.log("convert pos if needed");

    auto ret = alloc_object(Jump_To_Definition_Result);
    *ret = result;

    t.total();

    return ret;
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
    auto eof_pos = new_cur2((i32)buf->lines.last()->len, (i32)buf->lines.len - 1);

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

        auto ret = alloc_list<char>();

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
    case PFC_IFEMPTY: return "ifempty!";
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

    auto pf = parse_file(filepath, true);
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

    auto base_path = make_path(index.current_import_path);

    For (*index.packages) {
        if (it.status != GPS_READY) continue;

        if (selected_interface) {
            SCOPED_FRAME();
            if (!base_path->contains(make_path(it.import_path)))
                continue;
        }

        auto pkg = &it;

        Go_Ctx ctx;
        ctx.import_path = it.import_path;

        auto pkgname = it.package_name;
        For (*it.files) {
            ctx.filename = it.filename;
            auto filehash = it.hash;
            auto filepath = ctx_to_filepath(&ctx);

            For (*it.decls) {
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

    auto base_path = make_path(index.current_import_path);
    auto packages = alloc_list<Go_Package*>();

    For (*index.packages) {
        {
            SCOPED_FRAME();
            if (!base_path->contains(make_path(it.import_path)))
                continue;
        }
        packages->append(&it);
    }

    For (*packages) {
        auto pkg = it;

        auto pkgname = pkg->package_name;
        auto import_path = pkg->import_path;

        For (*pkg->files) {
            auto ctx = alloc_object(Go_Ctx);
            ctx->filename = it.filename;
            ctx->import_path = import_path;

            auto filepath = ctx_to_filepath(ctx);

            For (*it.decls) {
                if (streq(it.name, "_")) continue;

                auto getrecv = [&]() -> ccstr {
                    if (it.type != GODECL_FUNC) return NULL;
                    if (!it.gotype) return NULL;
                    if (it.gotype->type != GOTYPE_FUNC) return NULL;

                    auto recv = it.gotype->func_recv;
                    if (!recv) return NULL;

                    recv = unpointer_type(recv, NULL)->gotype;
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
    t.init("autocomplete");

    reload_all_editors();

    t.log("reload");

    auto pf = parse_file(filepath, true);
    if (!pf) return false;
    defer { free_parsed_file(pf); };

    if (!truncate_parsed_file(pf, pos, "_}}}}}}}}}}}}}}}}}")) return false;
    defer { ts_tree_delete(pf->tree); };

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

    auto copy_node = [&](Ast_Node *node) -> Ast_Node* {
        auto ret = alloc_object(Ast_Node);
        memcpy(ret, node, sizeof(Ast_Node));
        return ret;
    };

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
            if (operand_node->null) return WALK_ABORT;
            if (cmp_pos_to_node(pos, operand_node) == 0) return WALK_CONTINUE;

            auto sel_node = node->field(node->type() == TS_QUALIFIED_TYPE ? TSF_NAME : TSF_FIELD);

            bool dot_found = false;
            for (auto curr = node->child_all(); !curr->null; curr = curr->next_all()) {
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

            expr_to_analyze = copy_node(operand_node);
            situation = FOUND_DOT_COMPLETE;
            return WALK_ABORT;
        }

        case TS_PACKAGE_IDENTIFIER:
        case TS_TYPE_IDENTIFIER:
        case TS_IDENTIFIER:
        case TS_FIELD_IDENTIFIER:
            expr_to_analyze = copy_node(node);
            situation = FOUND_LONE_IDENTIFIER;
            keyword_start = node->start();
            prefix = cp_strdup(node->string());
            ((cstr)prefix)[pos.x - node->start().x] = '\0';
            {
                auto get_struct_literal_type = [&]() -> Ast_Node* {
                    auto curr = node->parent();
                    if (curr->null) return NULL;
                    if (curr->type() != TS_KEYED_ELEMENT && curr->type() != TS_LITERAL_ELEMENT) return NULL;
                    if (!node->prev()->null) return NULL; // must be key, not value

                    curr = curr->parent();
                    if (curr->null) return NULL;
                    if (curr->type() != TS_LITERAL_VALUE) return NULL;

                    curr = curr->parent();
                    if (curr->null) return NULL;
                    if (curr->type() != TS_COMPOSITE_LITERAL) return NULL;

                    curr = curr->field(TSF_TYPE);
                    if (curr->null) return NULL;

                    return copy_node(curr);
                };
                lone_identifier_struct_literal = get_struct_literal_type();
            }
            return WALK_ABORT;

        case TS_ANON_DOT: {
            auto prev = node->prev();
            if (!prev->null) {
                expr_to_analyze = copy_node(prev);
                situation = FOUND_DOT_COMPLETE;
                keyword_start = pos;
                prefix = "";
            } else {
                auto parent = node->parent();
                if (!parent->null && parent->type() == TS_ERROR) {
                    auto expr = parent->prev();
                    if (!expr->null) {
                        expr_to_analyze = copy_node(expr);
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
                ac_results = alloc_list<AC_Result>();
        };

        // try normal dot completions
        do {
            auto results = get_node_dotprops(expr_to_analyze, &was_package, ctx);
            if (!results || !results->len) break;

            init_results();
            For (*results) {
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
            For (*results) {
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
            if (expr->null) return false;

            Ast_Node *next = NULL;
            while (!(next = expr->next())->null) expr = next;
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
        ac_results = alloc_list<AC_Result>();

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
            Scoped_Table<Godecl*> table;
            {
                SCOPED_MEM(&scoped_table_mem);
                table.init();
            }
            defer { table.cleanup(); };

            For (*gofile->scope_ops) {
                if (it.pos > pos) break;

                switch (it.type) {
                case GSOP_OPEN_SCOPE:
                    table.push_scope();
                    break;
                case GSOP_CLOSE_SCOPE:
                    table.pop_scope();
                    break;
                case GSOP_DECL:
                    table.set(it.decl->name, it.decl->copy());
                    break;
                }
            }

            t.log("iterate over scope ops");

            auto entries = table.entries();
            For (*entries) {
                auto r = add_declaration_result(it->name);
                if (r) {
                    r->declaration_godecl = it->value;
                    r->declaration_import_path = ctx->import_path;
                    r->declaration_filename = ctx->filename;

                    auto res = evaluate_type(it->value->gotype, ctx);
                    if (res)
                        r->declaration_evaluated_gotype = res->gotype;
                }
            }

            t.log("iterate over table entries");

            For (*gofile->imports) {
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
                For (*gofile->imports) {
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
                        For (*results) {
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
        For (*index.packages) {
            if (!it.import_path) continue;
            if (existing_imports.has(it.import_path)) continue;
            if (it.status != GPS_READY) continue;
            if (!it.package_name) continue;
            if (streq(it.import_path, ctx->import_path)) continue;

            // gofile->imports
            // TODO: check if import already exists in file

            if (!path_has_descendant(index.current_import_path, it.import_path))
                if (is_import_path_internal(it.import_path))
                    continue;

            auto res = ac_results->append();
            res->name = it.package_name;
            res->type = ACR_IMPORT;
            res->import_path = it.import_path;
        }

        t.log("iterate over packages");


        t.log("crazy shit");

        do {
            if (!lone_identifier_struct_literal) break;

            auto gotype = node_to_gotype(lone_identifier_struct_literal);
            if (!gotype) break;

            auto res = evaluate_type(gotype, ctx);
            if (!res) break;

            auto rres = resolve_type(res);
            if (!rres) break;

            auto tmp = alloc_list<Goresult>();
            list_struct_fields(rres, tmp);

            For (*tmp) {
                if (!streq(it.ctx->import_path, ctx->import_path))
                    if (!isupper(it.decl->name[0]))
                        continue;

                if (it.decl->type != GODECL_FIELD) continue;
                if (it.decl->is_embedded) continue;

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
            For (*results) {
                auto result = add_declaration_result(it.decl->name);
                if (result) {
                    result->declaration_godecl = it.decl;
                    result->declaration_import_path = it.ctx->import_path;
                    result->declaration_filename = it.ctx->filename;

                    auto res = evaluate_type(it.decl->gotype, it.ctx);
                    if (res)
                        result->declaration_evaluated_gotype = res->gotype;
                }
            }
        }

        t.log("list package decls");

        ccstr keywords[] = {
            "package", "import", "const", "var", "func",
            "type", "struct", "interface", "map", "chan",
            "fallthrough", "break", "continue", "goto", "return",
            "go", "defer", "if", "else",
            "for", "range", "switch", "case",
            "default", "select", "new", "make", "iota",
        };

        For (keywords) {
            auto res = ac_results->append();
            res->name = it;
            res->type = ACR_KEYWORD;
        }

        // add builtins
        {
            auto results = list_package_decls("@builtin", LISTDECLS_EXCLUDE_METHODS);
            if (results) {
                For (*results) {
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
    For (*parts)
        if (streq(it, "internal"))
            return true;
    return false;
}

bool Go_Indexer::check_if_still_in_parameter_hint(ccstr filepath, cur2 cur, cur2 hint_start) {
    if (cur < hint_start) return false;

    reload_all_editors();

    auto pf = parse_file(filepath, true);
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
                auto last_pos = new_cur2((i32)relu_sub(end_pos.x, 1), (i32)end_pos.y);

                auto last_ch = get_char_at_pos(last_pos);
                if (!(bytecur == end_pos && last_ch == start_ch && last_pos != start_pos))
                    string_close_char = start_ch;
            }
            return WALK_ABORT;
        }
        }
        return WALK_CONTINUE;
    }, true);

    ccstr suffix = "";
    if (string_close_char)
        suffix = cp_sprintf("%c", string_close_char);
    suffix = cp_sprintf("%s)}}}}}}}}}}}}}}}}", suffix);

    if (!truncate_parsed_file(pf, cur, suffix)) return NULL;
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

    auto pf = parse_file(filepath, true);
    if (!pf) return NULL;
    defer { free_parsed_file(pf); };

    if (pf->it->type != IT_BUFFER) {
        error("can only do parameter hint on buffer parser");
        return NULL;
    }

    if (!truncate_parsed_file(pf, pos, "_)}}}}}}}}}}}}}}}}")) return NULL;
    defer { ts_tree_delete(pf->tree); };

    auto go_back_until_non_space = [&]() -> cur2 {
        auto it = alloc_object(Parser_It);
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
            if (func->null || args->null) break;

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
            if (!prev->null) {
                func_expr = prev;
                if (func_expr->type() == TS_ERROR) {
                    func_expr = func_expr->child();
                    for (Ast_Node *next; (next = func_expr->next());)
                        func_expr = next;
                }
                call_args_start = node->start();
            } else {
                auto parent = node->parent();
                if (!parent->null && parent->type() == TS_ERROR) {
                    auto parent_prev = parent->prev_all();
                    if (!parent_prev->null) {
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

    auto hint = alloc_object(Parameter_Hint);
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
        For (*t->struct_specs) {
            // recursively list methods for embedded fields
            if (it.field->is_embedded) {
                auto res = resolve_type(it.field->gotype, type->ctx);
                if (res)
                    list_struct_fields(res, ret);
            }
            ret->append(type->wrap(it.field));
        }
        break;
    }
}

bool Go_Indexer::list_interface_methods(Goresult *interface, List<Goresult> *out) {
    auto gotype = interface->gotype;
    if (gotype->type != GOTYPE_INTERFACE) return false;

    auto specs = gotype->interface_specs;
    if (!specs) return false;

    For (*specs) {
        if (!it.field) return false; // this shouldn't happen

        auto method = it.field;
        if (method->is_embedded) {
            auto rres = resolve_type(method->gotype, interface->ctx);
            if (!rres) return false;
            if (!list_interface_methods(rres, out)) return false;
        } else {
            if (method->gotype->type != GOTYPE_FUNC) return false;
            out->append(interface->wrap(method));
        }
    }

    return true;
}

List<Goresult> *Go_Indexer::list_interface_methods(Goresult *interface) {
    Frame frame;

    auto ret = alloc_list<Goresult>();
    if (!list_interface_methods(interface, ret)) {
        frame.restore();
        return NULL;
    }

    return ret;
}

bool Go_Indexer::list_type_methods(ccstr type_name, ccstr import_path, List<Goresult> *out) {
    auto results = list_package_decls(import_path);
    if (!results) return false;

    For (*results) {
        auto decl = it.decl;

        if (decl->type != GODECL_FUNC) continue;

        auto functype = decl->gotype;
        if (!functype->func_recv) continue;

        auto recv = unpointer_type(functype->func_recv, NULL)->gotype;

        if (recv->type != GOTYPE_ID) continue;
        if (!streq(recv->id_name, type_name)) continue;

        out->append(&it);
    }

    return true;
}

void Go_Indexer::list_dotprops(Goresult *type_res, Goresult *resolved_type_res, List<Goresult> *ret) {
    auto tmp = alloc_list<Goresult>();
    actually_list_dotprops(type_res, resolved_type_res, tmp);

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

void Go_Indexer::actually_list_dotprops(Goresult *type_res, Goresult *resolved_type_res, List<Goresult> *ret) {
    auto resolved_type = resolved_type_res->gotype;
    switch (resolved_type->type) {
    case GOTYPE_POINTER: {
        auto new_resolved_type_res = resolved_type_res->wrap(resolved_type->pointer_base);
        actually_list_dotprops(type_res, new_resolved_type_res, ret);
        return;
    }

    case GOTYPE_STRUCT: {
        For (*resolved_type->struct_specs) {
            // recursively list methods for embedded fields
            if (it.field->is_embedded) {
                auto embedded_type = it.field->gotype;
                auto res = resolve_type(embedded_type, resolved_type_res->ctx);
                if (!res) continue;
                if (res->gotype->type != resolved_type->type) continue; // this is technically an error, should we surface it here?

                actually_list_dotprops(resolved_type_res->wrap(embedded_type), res, ret);
            }
            ret->append(resolved_type_res->wrap(it.field));
        }
        break;
    }

    case GOTYPE_INTERFACE: {
        // if we have an GO_INTERFACE_SPEC_ELEM, i.e. a union with more than one term, then:
        //
        //  - unions can't have terms that specify methods
        //  - interfaces are the intersection of their elems
        //  - means the interface itself has no elements
        //  - so we have nothing to add, break out
        For (*resolved_type->interface_specs)
            if (it.type == GO_INTERFACE_SPEC_ELEM)
                goto getout;

        // no elems found
        For (*resolved_type->interface_specs) {
            // recursively list methods for embedded fields
            if (it.field->is_embedded) {
                auto embedded_type = it.field->gotype;
                auto res = resolve_type(embedded_type, resolved_type_res->ctx);
                if (!res) continue;
                if (res->gotype->type != resolved_type->type) continue; // this is technically an error, should we surface it here?

                actually_list_dotprops(resolved_type_res->wrap(embedded_type), res, ret);
            }
            ret->append(resolved_type_res->wrap(it.field));
        }
        break;
    }
    }
getout:

    type_res = unpointer_type(type_res);

    auto type = type_res->gotype;
    ccstr type_name = NULL;
    ccstr target_import_path = NULL;

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

    list_type_methods(type_name, target_import_path, ret);
}

ccstr remove_ats_from_path(ccstr s) {
    auto path = make_path(s);
    For (*path->parts) {
        auto atpos = strchr((char*)it, '@');
        if (atpos) *atpos = '\0';
    }
    return path->str();
}

Go_Ctx *Go_Indexer::filepath_to_ctx(ccstr filepath) {
    auto import_path = filepath_to_import_path(cp_dirname(filepath));
    if (!import_path) return NULL;

    auto ret = alloc_object(Go_Ctx);
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

    auto parts = alloc_list<ccstr>(path->parts->len - goroot_path->parts->len);
    for (u32 i = goroot_path->parts->len; i < path->parts->len; i++)
        parts->append(path->parts->at(i));

    Path p;
    p.init(parts);
    return p.str('/');
}

void Go_Indexer::init() {
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
        if (!GHBuildEnvInit())
            return false;
        if (!GHBuildEnvGoVersionSupported())
            return false;
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
        goroot = copystr(GHGetGoroot());
        if (!goroot)
            cp_panic("Unable to detect GOROOT. Please make sure Go is installed and accessible through your PATH.");

        auto goroot_without_src = goroot;
        goroot = path_join(goroot, "src");

        if (check_path(goroot) != CPR_DIRECTORY) {
            // This is called from main thread, so we can just call tell_user().
            tell_user(
                cp_sprintf(
                    "We found the following GOROOT:\n\n%s\n\nIt doesn't appear to be valid. The program will keep running, but code intelligence might not fully work.",
                    goroot_without_src
                ),
                "Warning"
            );
        }

        gomodcache = copystr(GHGetGomodcache());
        if (!gomodcache)
            cp_panic("Unable to detect GOMODCACHE. Please make sure Go is installed and accessible through your PATH.");
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

bool Go_Indexer::release_lock(Indexer_Status expected_status) {
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

    auto ret = alloc_list<Godecl>(count);

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
                auto t = new_gotype(GOTYPE_VARIADIC);
                t->variadic_base = field->gotype;
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
                auto t = new_gotype(GOTYPE_VARIADIC);
                t->variadic_base = field->gotype;
                field->gotype = t;
            }
        }
    }

    return ret;
}

bool Go_Indexer::node_func_to_gotype_sig(Ast_Node *params, Ast_Node *result, Go_Func_Sig *sig) {
    if (params->type() != TS_PARAMETER_LIST) return false;

    sig->params = parameter_list_to_fields(params);

    if (!result->null) {
        if (result->type() == TS_PARAMETER_LIST) {
            sig->result = parameter_list_to_fields(result);
        } else {
            sig->result = alloc_list<Godecl>(1);
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

Gotype *Go_Indexer::node_to_gotype(Ast_Node *node, bool toplevel) {
    if (node->null) return NULL;

    Gotype *ret = NULL;

    switch (node->type()) {
    // case TS_SIMPLE_TYPE:
    case TS_CONSTRAINT_ELEM: {
        ret = new_gotype(GOTYPE_CONSTRAINT);
        ret->constraint_terms = alloc_list<Gotype*>();

        FOR_NODE_CHILDREN (node) {
            if (it->type() != TS_CONSTRAINT_TERM) continue; // ???

            auto child_node = it->child();
            if (!child_node || child_node->null) continue; // ???

            auto term = node_to_gotype(child_node);
            if (!term) continue; // just break out of the whole thing?

            auto prev = child_node->prev_all();
            if (prev && !prev->null && prev->type() == TS_TILDE) {
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
        if (pkg_node->null) break;
        auto name_node = node->field(TSF_NAME);
        if (name_node->null) break;

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

    case TS_POINTER_TYPE:
        ret = new_gotype(GOTYPE_POINTER);
        ret->pointer_base = node_to_gotype(node->child());
        break;

    case TS_GENERIC_TYPE: {
        ret = new_gotype(GOTYPE_GENERIC);
        ret->generic_base = node_to_gotype(node->field(TSF_TYPE));

        auto args_node = node->field(TSF_TYPE_ARGUMENTS);
        if (!args_node->null) {
            bool ok = true;
            auto args = alloc_list<Gotype*>();

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
        ret = new_gotype(GOTYPE_ARRAY);
        ret->array_base = node_to_gotype(node->field(TSF_ELEMENT));
        break;

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

    case TS_STRUCT_TYPE: {
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
            if (!total) total++;

            num_children += total;
        }

        ret->struct_specs = alloc_list<Go_Struct_Spec>(num_children);

        FOR_NODE_CHILDREN (fieldlist_node) {
            auto field_node = it;

            auto tag_node = field_node->field(TSF_TAG);
            auto type_node = field_node->field(TSF_TYPE);
            if (type_node->null) continue;

            auto field_type = node_to_gotype(type_node);
            if (!field_type) continue;

            bool names_found = false;
            FOR_NODE_CHILDREN (field_node) {
                names_found = !it->eq(type_node);
                break;
            }

            if (names_found) {
                FOR_NODE_CHILDREN (field_node) {
                    if (it->eq(type_node)) break;

                    auto field = alloc_object(Godecl);
                    field->type = GODECL_FIELD;
                    field->is_toplevel = toplevel;
                    field->gotype = field_type;
                    field->name = it->string();
                    field->name_start = it->start();
                    field->name_end = it->end();
                    field->spec_start = field_node->start();
                    field->decl_start = field_node->start();
                    field->decl_end = field_node->end();

                    auto spec = ret->struct_specs->append();
                    spec->field = field;
                    if (!tag_node->null)
                        spec->tag = tag_node->string();
                }
            } else {
                auto unptr_type = unpointer_type(field_type, NULL)->gotype;
                ccstr field_name = NULL;

                switch (unptr_type->type) {
                case GOTYPE_SEL:
                    field_name = unptr_type->sel_sel;
                    break;
                case GOTYPE_ID:
                    field_name = unptr_type->id_name;
                    break;
                }

                if (!field_name) continue;

                auto field = alloc_object(Godecl);
                field->type = GODECL_FIELD;
                field->is_toplevel = toplevel;
                field->is_embedded = true;
                field->gotype = field_type;
                field->spec_start = type_node->start();
                field->decl_start = type_node->start();
                field->decl_end = type_node->end();
                field->name = field_name;

                auto spec = ret->struct_specs->append();
                spec->field = field;
                if (!tag_node->null)
                    spec->tag = tag_node->string();
            }
        }
        break;
    }

    case TS_CHANNEL_TYPE:
        ret = new_gotype(GOTYPE_CHAN);
        ret->chan_base = node_to_gotype(node->field(TSF_VALUE));
        break;

    case TS_INTERFACE_TYPE: {
        ret = alloc_object(Gotype);
        ret->type = GOTYPE_INTERFACE;
        ret->interface_specs = alloc_list<Go_Interface_Spec>(node->child_count());

        FOR_NODE_CHILDREN (node) {
            Godecl *field = NULL;

            switch (it->type()) {
            case TS_METHOD_SPEC: {
                auto name_node = it->field(TSF_NAME);

                field = alloc_object(Godecl);
                field->type = GODECL_FIELD;
                field->name = name_node->string();
                field->gotype = new_gotype(GOTYPE_FUNC);
                field->decl_start = it->start();
                field->decl_end =  it->end();
                field->spec_start = it->start();
                field->name_start = name_node->start();
                field->name_end = name_node->end();

                node_func_to_gotype_sig(
                    it->field(TSF_PARAMETERS),
                    it->field(TSF_RESULT),
                    &field->gotype->func_sig
                );

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

                if (node->null) continue;

                auto gotype = node_to_gotype(node);
                if (!gotype) break;

                field = alloc_object(Godecl);
                field->type = GODECL_FIELD;
                field->name = NULL;
                field->is_embedded = true;
                field->gotype = gotype;
                field->spec_start = node->start();
                field->decl_start = node->start();
                field->decl_end = node->end();

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
    if (!name_node->null) {
        decl->name = name_node->string();
        decl->name_start = name_node->start();
        decl->name_end = name_node->end();
    }

    auto path_node = spec_node->field(TSF_PATH);
    if (!path_node->null)
        decl->import_path = path_node->string();
}

bool Go_Indexer::assignment_to_decls(List<Ast_Node*> *lhs, List<Ast_Node*> *rhs, New_Godecl_Func new_godecl, bool range) {
    if (!lhs->len || !rhs->len) return false;

    if (rhs->len == 1) {
        if (range) {
            auto range_base = expr_to_gotype(rhs->at(0));

            for (int i = 0; i < 2 && i < lhs->len; i++) {
                auto id = lhs->at(i);
                if (!id) continue;
                if (id->type() != TS_IDENTIFIER) continue;

                auto gotype = new_gotype(GOTYPE_LAZY_RANGE);
                gotype->lazy_range_base = range_base;
                gotype->lazy_range_is_index = (!i);

                auto decl = new_godecl();
                decl->name = id->string();
                decl->name_start = id->start();
                decl->name_end = id->end();
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
        For (*lhs) {
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

            if (gotype->type == GOTYPE_ASSERTION)
                gotype = gotype->assertion_base;

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
    bool is_toplevel = (!parent->null && parent->type() == TS_SOURCE_FILE);

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

    auto parse_type_params = [&](Ast_Node *node) -> List<Go_Type_Parameter> * {
        if (node->null) return NULL;

        auto ret = alloc_list<Go_Type_Parameter>();
        FOR_NODE_CHILDREN (node) {
            auto name_node = it->field(TSF_NAME);
            if (name_node->null) return NULL;
            auto type_node = it->field(TSF_TYPE);
            if (type_node->null) return NULL;

            auto obj = ret->append();
            obj->name = name_node->string();
            obj->constraint = node_to_gotype(type_node, is_toplevel);
        }
        return ret;
    };

    auto node_type = node->type();
    switch (node_type) {
    case TS_FUNCTION_DECLARATION:
    case TS_METHOD_DECLARATION: {
        auto name_node = node->field(TSF_NAME);
        if (name_node->null) break;

        auto name = name_node->string();

        if (node_type == TS_FUNCTION_DECLARATION)
            if (is_toplevel && streq(name, "init"))
                break;

        auto type_params = parse_type_params(node->field(TSF_TYPE_PARAMETERS));

        auto params_node = node->field(TSF_PARAMETERS);
        auto result_node = node->field(TSF_RESULT);

        auto gotype = new_gotype(GOTYPE_FUNC);

        if (!node_func_to_gotype_sig(params_node, result_node, &gotype->func_sig)) break;

        if (node->type() == TS_METHOD_DECLARATION) {
            auto recv_node = node->field(TSF_RECEIVER);
            auto recv_type = recv_node->child()->field(TSF_TYPE);
            if (!recv_type->null) {
                gotype->func_recv = node_to_gotype(recv_type);
                if (!gotype->func_recv) break;
            }
        }

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

    case TS_TYPE_DECLARATION:
        // TODO: handle TS_TYPE_ALIAS here.
        for (auto spec = node->child(); !spec->null; spec = spec->next()) {
            auto name_node = spec->field(TSF_NAME);
            if (name_node->null) continue;

            auto type_params = parse_type_params(spec->field(TSF_TYPE_PARAMETERS));

            auto name = name_node->string();
            if (streq(name, "_")) continue;

            auto type_node = spec->field(TSF_TYPE);
            if (type_node->null) continue;

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
            save_decl(decl);
        }
        break;

    case TS_TYPE_PARAMETER_DECLARATION: {
        auto type_node = node->field(TSF_TYPE);
        if (type_node->null) break;

        auto type_node_gotype = node_to_gotype(type_node);
        if (!type_node_gotype) break;

        FOR_NODE_CHILDREN (node) {
            if (it->eq(type_node)) break;
            if (!it->type() == TS_IDENTIFIER) continue;

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

            if (type_node->null && value_node->null) {
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

            // at this point, !type_node->null || !value_node->null

            auto has_iota = [&](Ast_Node *node) -> bool {
                bool ret = false;
                walk_ast_node(node, true, [&](Ast_Node *it, Ts_Field_Type, int) -> Walk_Action {
                    switch (it->type()) {
                    case TS_IDENTIFIER:
                    case TS_FIELD_IDENTIFIER:
                    case TS_PACKAGE_IDENTIFIER:
                    case TS_TYPE_IDENTIFIER:
                        if (streq(it->string(), "iota")) {
                            ret = true;
                            return WALK_ABORT;
                        }
                        break;
                    }
                    return WALK_CONTINUE;
                });
                return ret;
            };

            bool should_save_iota_types = (
                !saved_iota_types
                && !value_node->null
                && has_iota(value_node)
            );

            Gotype *type_node_gotype = NULL;
            if (!type_node->null) {
                type_node_gotype = node_to_gotype(type_node);
                if (!type_node_gotype) continue;

                if (node->type() == TS_PARAMETER_LIST && spec->type() == TS_VARIADIC_PARAMETER_DECLARATION) {
                    auto t = new_gotype(GOTYPE_VARIADIC);
                    t->variadic_base = type_node_gotype;
                    type_node_gotype = t;
                }

                if (should_save_iota_types)
                    saved_iota_types = alloc_list<Gotype*>();

                FOR_NODE_CHILDREN (spec) {
                    if (it->eq(type_node) || it->eq(value_node)) break;

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
                    decl->gotype = type_node_gotype;
                    save_decl(decl);

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

                auto lhs = alloc_list<Ast_Node*>(lhs_count);
                auto rhs = alloc_list<Ast_Node*>(value_node->child_count());

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
                    saved_iota_types = alloc_list<Gotype*>();

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
        if (left->type() != TS_EXPRESSION_LIST) break;

        auto lhs = alloc_list<Ast_Node*>(left->child_count());
        auto rhs = alloc_list<Ast_Node*>(1);

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
            if (left->null) break;
            if (left->type() != TS_EXPRESSION_LIST) break;

            auto right = node->field(TSF_RIGHT);
            if (right->null) break;
            if (!is_expression_node(right)) break;

            lhs = alloc_list<Ast_Node*>(left->child_count());
            FOR_NODE_CHILDREN (left) lhs->append(it);

            rhs = alloc_list<Ast_Node*>(1);
            rhs->append(right);
        } else {
            auto left = node->field(TSF_LEFT);
            auto right = node->field(TSF_RIGHT);

            if (left->null) break;
            if (right->null) break;

            if (left->type() != TS_EXPRESSION_LIST) break;
            if (right->type() != TS_EXPRESSION_LIST) break;

            lhs = alloc_list<Ast_Node*>(left->child_count());
            rhs = alloc_list<Ast_Node*>(right->child_count());

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

Gotype* walk_gotype_and_replace(Gotype *gotype, fn<Gotype*(Gotype*)> cb) {
    return _walk_gotype_and_replace(gotype->copy(), cb);
}

Gotype* _walk_gotype_and_replace(Gotype *gotype, fn<Gotype*(Gotype*)> cb) {
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
    case GOTYPE_VARIADIC:
    case GOTYPE_POINTER:
    case GOTYPE_SLICE:
    case GOTYPE_ARRAY:
    case GOTYPE_CHAN:
    case GOTYPE_ASSERTION:
    case GOTYPE_RANGE:
    case GOTYPE_CONSTRAINT_UNDERLYING:
    case GOTYPE_BUILTIN:
        recur(general_base);
        break;
    case GOTYPE_MAP:
        recur(map_key);
        recur(map_value);
        break;
    case GOTYPE_STRUCT:
        For (*gotype->struct_specs)
            recur_decl(it.field);
        break;
    case GOTYPE_INTERFACE:
        For (*gotype->interface_specs)
            recur_decl(it.field);
        break;
    case GOTYPE_FUNC:
        For (*gotype->func_sig.params) recur_decl(&it);
        For (*gotype->func_sig.result) recur_decl(&it);
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
    // i don't think we need to handle lazy types yet?
    // this is for going through a GOTYPE_GENERIC and replacing type parameters with arguments
    // case GOTYPE_LAZY_INDEX:
    // case GOTYPE_LAZY_CALL:
    // case GOTYPE_LAZY_DEREFERENCE:
    // case GOTYPE_LAZY_REFERENCE:
    // case GOTYPE_LAZY_ARROW:
    // case GOTYPE_LAZY_ID:
    // case GOTYPE_LAZY_SEL:
    // case GOTYPE_LAZY_ONE_OF_MULTI:
    // case GOTYPE_LAZY_RANGE:
    }
#undef recur
#undef recur_raw

    return gotype;
}

Goresult *Go_Indexer::_subst_generic_type(Gotype *type, Go_Ctx *ctx) {
    cp_assert(type->type == GOTYPE_GENERIC);

    auto base = type->generic_base;
    switch (base->type) {
    case GOTYPE_ID:
    case GOTYPE_SEL: {
        Goresult *res = NULL;
        if (base->type == GOTYPE_ID) {
            res = find_decl_of_id(base->id_name, base->id_pos, ctx);
        } else {
            auto import_path = find_import_path_referred_to_by_id(base->sel_name, ctx);
            if (!import_path) break;
            res = find_decl_in_package(base->sel_sel, import_path);
        }

        if (!res) break;

        auto decl = res->decl;

        // check requirements
        if (decl->type != GODECL_TYPE) break;
        if (decl->type_params == NULL) break;
        if (decl->type_params->len != type->generic_args->len) break;

        auto gotype = decl->gotype;

        // table of name to gotype
        Table<Gotype*> arguments; arguments.init();
        for (int i = 0; i < decl->type_params->len; i++) {
            auto &param = decl->type_params->at(i);
            auto arg = type->generic_args->at(i);
            arguments.set(param.name, arg);
        }

        auto ret = walk_gotype_and_replace(decl->gotype, [&](auto it) -> Gotype* {
            if (it->type == GOTYPE_ID)
                return arguments.get(it->id_name);
            return NULL;
        });

        // should we keep ctxc or use decl->ctx? how is this result used?
        return make_goresult(ret, ctx);
    }
    }
    return NULL;
}

Goresult *Go_Indexer::subst_generic_type(Goresult *res) {
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

Goresult *Go_Indexer::unpointer_type(Gotype *type, Go_Ctx *ctx) {
    while (type->type == GOTYPE_POINTER)
        type = type->pointer_base;
    return make_goresult(type, ctx);
}

List<Goresult> *Go_Indexer::list_package_decls(ccstr import_path, int flags) {
    auto pkg = find_up_to_date_package(import_path);
    if (!pkg) return NULL;

    auto ret = alloc_list<Goresult>();

    For (*pkg->files) {
        auto filename = it.filename;

        For (*it.decls) {
            if (!it.name) continue;

            if (flags & LISTDECLS_PUBLIC_ONLY)
                if (is_name_private(it.name))
                    continue;

            if (flags & LISTDECLS_EXCLUDE_METHODS)
                if (it.type == GODECL_FUNC && it.gotype->func_recv)
                    continue;

            auto ctx = alloc_object(Go_Ctx);
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
    For (*results)
        if (streq(it.decl->name, id))
            return &it;
    return NULL;
}

Gotype *Go_Indexer::expr_to_gotype(Ast_Node *expr) {
    if (expr->null) return NULL;

    Gotype *ret = NULL; // so we don't have to declare inside switch

    switch (expr->type()) {
    case TS_PARENTHESIZED_EXPRESSION:
        return expr_to_gotype(expr->child());

    case TS_INT_LITERAL: return new_primitive_type("int");
    case TS_FLOAT_LITERAL: return new_primitive_type("float64");
    case TS_IMAGINARY_LITERAL: return new_primitive_type("complex128");
    case TS_RUNE_LITERAL: return new_primitive_type("rune");

    case TS_RAW_STRING_LITERAL:
    case TS_INTERPRETED_STRING_LITERAL:
        return new_primitive_type("string");

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
            if (args->null) break;
            if (args->type() != TS_ARGUMENT_LIST) break;

            auto firstarg = args->child();
            if (firstarg->null) break;

            ret = node_to_gotype(firstarg);
            if (streq(func->string(), "new")) {
                auto newret = new_gotype(GOTYPE_POINTER);
                newret->pointer_base = ret;
                ret = newret;
            }
            return ret;
        } while (0);

        Frame frame;
        List<Gotype*> *args = alloc_list<Gotype*>();
        List<Gotype*> *type_args = NULL;

        auto type_args_node = expr->field(TSF_TYPE_ARGUMENTS);
        if (!type_args_node->null) {
            type_args = alloc_list<Gotype*>();
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
        if (type_args) {
            auto newbase = new_gotype(GOTYPE_GENERIC);
            newbase->generic_base = base;
            newbase->generic_args = type_args;
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
        return ret;

    case TS_QUALIFIED_TYPE:
    case TS_SELECTOR_EXPRESSION: {
        auto operand_node = expr->field(expr->type() == TS_QUALIFIED_TYPE ? TSF_PACKAGE : TSF_OPERAND);
        auto field_node = expr->field(expr->type() == TS_QUALIFIED_TYPE ? TSF_NAME : TSF_FIELD);

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

Goresult *Go_Indexer::evaluate_type(Gotype *gotype, Go_Ctx *ctx, Godecl** outdecl) {
    if (!gotype) return NULL;

    enum {
        U_EVAL = 1 << 0,
        U_RESOLVE = 1 << 1,
        U_SUBST = 1 << 2,
        U_UNPOINTER = 1 << 3,
        U_ALL = U_EVAL | U_RESOLVE | U_SUBST | U_UNPOINTER,
    };

    auto unwrap_type = [&](Gotype *base, int flags = U_ALL) -> Goresult* {
        auto res = make_goresult(base, ctx);

        if (flags & U_EVAL) {
            res = evaluate_type(res, outdecl);
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
                return make_goresult(new_primitive_type("int"), NULL);
            return res->wrap(res->gotype->slice_base);
        case GOTYPE_ARRAY:
            if (gotype->lazy_range_is_index)
                return make_goresult(new_primitive_type("int"), NULL);
            return res->wrap(res->gotype->array_base);
        }
        return NULL;
    }

    /*
    The fundamental problem is that given a generic type, we need to
    access the instantiated type in order to do anything with it. E.g. if
    we have a GOTYPE_LAZY_INDEX, we need the instantiated type to know how
    to index into it.
    */

    case GOTYPE_LAZY_INDEX: {
        auto res = unwrap_type(gotype->lazy_index_base);
        if (!res) return NULL;

        auto base_type = res->gotype;

        if (base_type->type == GOTYPE_MULTI)
            if (base_type->multi_types && base_type->multi_types->len == 1)
                base_type = base_type->multi_types->at(0);

        switch (base_type->type) {
        case GOTYPE_ARRAY: return evaluate_type(base_type->array_base, res->ctx, outdecl);
        case GOTYPE_SLICE: return evaluate_type(base_type->slice_base, res->ctx, outdecl);
        case GOTYPE_ID:
            if (streq(base_type->id_name, "string"))
                return make_goresult(new_primitive_type("rune"), NULL);
            break;
        case GOTYPE_MAP: {
            auto ret = new_gotype(GOTYPE_ASSERTION);
            ret->assertion_base = base_type->map_value;
            return res->wrap(ret);
        }
        }
        return NULL;
    }

    case GOTYPE_LAZY_CALL: {
        Godecl *decl = NULL;

        auto res = evaluate_type(gotype->lazy_call_base, ctx, &decl);
        if (!res) return NULL;

        res = resolve_type(res);
        if (!res) return NULL;

        // TODO: type inference
        {
            // res = subst_generic_type(res);
            // if (!res) return NULL;
        }

        res = unpointer_type(res);
        if (!res) return NULL;

        if (res->gotype->type != GOTYPE_FUNC) return NULL;

        auto result = res->gotype->func_sig.result;
        if (!result) return NULL;

        if (result->len == 1)
            return res->wrap(result->at(0).gotype);

        auto ret = new_gotype(GOTYPE_MULTI);
        ret->multi_types = alloc_list<Gotype*>(result->len);
        For (*result) ret->multi_types->append(it.gotype);
        return res->wrap(ret);
    }

    case GOTYPE_LAZY_DEREFERENCE: {
        auto res = unwrap_type(gotype->lazy_dereference_base, U_ALL & ~U_UNPOINTER);
        if (!res) return NULL;

        if (res->gotype->type != GOTYPE_POINTER) return NULL;
        return evaluate_type(res->gotype->pointer_base, res->ctx, outdecl);
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

        return evaluate_type(res->gotype->chan_base, res->ctx, outdecl);
    }

    case GOTYPE_LAZY_ID: {
        auto res = find_decl_of_id(gotype->lazy_id_name, gotype->lazy_id_pos, ctx);
        if (!res) return NULL;
        if (res->decl->type == GODECL_IMPORT) return NULL;
        if (!res->decl->gotype) return NULL;

        if (outdecl) *outdecl = res->decl;
        return evaluate_type(res->decl->gotype, res->ctx, outdecl);
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
                return evaluate_type(ext_decl->gotype, res->ctx, outdecl);
            default:
                return NULL;
            }
        } while (0);

        auto res = evaluate_type(gotype->lazy_sel_base, ctx);
        if (!res) return NULL;

        auto rres = resolve_type(res);
        if (!rres) return NULL;

        rres = unpointer_type(rres);

        List<Goresult> results;
        results.init();
        list_dotprops(res, rres, &results);

        For (results)
            if (streq(it.decl->name, gotype->lazy_sel_sel))
                return evaluate_type(it.decl->gotype, it.ctx);
        break;
    }

    case GOTYPE_LAZY_ONE_OF_MULTI: {
        auto res = unwrap_type(gotype->lazy_one_of_multi_base);
        if (!res) return NULL;

        bool is_single = gotype->lazy_one_of_multi_is_single;
        int index = gotype->lazy_one_of_multi_index;
        Gotype *base = gotype->lazy_one_of_multi_base;

        switch (res->gotype->type) {
        case GOTYPE_MULTI:
            return evaluate_type(res->gotype->multi_types->at(index), res->ctx);

        case GOTYPE_ASSERTION:
            if (!index)
                return evaluate_type(res->gotype->assertion_base, res->ctx);
            if (index == 1)
                return res->wrap(new_primitive_type("bool"));
            break;

        case GOTYPE_RANGE:
            switch (res->gotype->range_base->type) {
            case GOTYPE_MAP:
                if (!index)
                    return evaluate_type(res->gotype->range_base->map_key, res->ctx);
                if (index == 1)
                    return evaluate_type(res->gotype->range_base->map_value, res->ctx);
                break;

            case GOTYPE_ARRAY:
            case GOTYPE_SLICE:
                if (!index)
                    return res->wrap(new_primitive_type("int"));
                if (index == 1) {
                    auto base = res->gotype->type == GOTYPE_ARRAY ? res->gotype->array_base : res->gotype->slice_base;
                    return evaluate_type(base, res->ctx);
                }
                break;

            case GOTYPE_ID:
                if (!streq(res->gotype->id_name, "string")) break;
                if (!index)
                    return res->wrap(new_primitive_type("int"));
                if (index == 1)
                    return res->wrap(new_primitive_type("rune"));
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

    auto ret = alloc_object(Goresult);
    ret->ptr = ptr;
    ret->ctx = ctx;
    return ret;
}

Goresult *make_goresult(Gotype *gotype, Go_Ctx *ctx) { return make_goresult_from_pointer(gotype, ctx); }
Goresult *make_goresult(Godecl *decl, Go_Ctx *ctx) { return make_goresult_from_pointer(decl, ctx); }

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
    if (res->decl->type != GODECL_TYPE) return NULL;

    return res;
}

Goresult *Go_Indexer::resolve_type(Goresult *res) {
    return resolve_type(res->gotype, res->ctx);
}

Goresult *Go_Indexer::resolve_type(Gotype *type, Go_Ctx *ctx) {
    if (!type) return NULL;

    switch (type->type) {
    case GOTYPE_BUILTIN: // pending decision: should we do this here?
        if (!type->builtin_underlying_base)
            return NULL;
        return resolve_type(type->builtin_underlying_base, ctx);

    case GOTYPE_POINTER: {
        auto res = resolve_type(type->pointer_base, ctx);
        if (!res) return NULL;

        auto ret = new_gotype(GOTYPE_POINTER);
        ret->pointer_base = res->gotype;

        return res->wrap(ret);
    }

    case GOTYPE_ID:
    case GOTYPE_SEL: {
        auto res = resolve_type_to_decl(type, ctx);
        if (!res) return NULL;
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

    auto ret = alloc_list<char>();

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

TSParser *new_ts_parser() {
    auto ret = ts_parser_new();
    ts_parser_set_language(ret, tree_sitter_go());
    return ret;
}

// -----
// read functions

#define depointer(x) std::remove_pointer<x>::type
#define READ_STR(x) x = s->readstr()
#define READ_OBJ(x) x = read_object<depointer(decltype(x))>(s)
#define READ_LIST(x) x = read_list<depointer(decltype(x))::type>(s)
#define READ_LISTP(x) x = read_listp<depointer(depointer(decltype(x))::type)>(s)

// ---

void Go_Type_Parameter::read(Index_Stream *s) {
    READ_STR(name);
    READ_OBJ(constraint);
}

void Godecl::read(Index_Stream *s) {
    READ_STR(name);

    if (type == GODECL_IMPORT) {
        READ_STR(import_path);
    } else {
        READ_OBJ(gotype);
        READ_LIST(type_params);
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
        multi_types = alloc_list<Gotype*>(len);
        for (u32 i = 0; i < len; i++) {
            auto gotype = read_object<Gotype>(s);
            multi_types->append(gotype);
        }
        break;
    }
    case GOTYPE_VARIADIC:
        READ_OBJ(variadic_base);
        break;
    case GOTYPE_ASSERTION:
        READ_OBJ(assertion_base);
        break;
    case GOTYPE_RANGE:
        READ_OBJ(range_base);
        break;
    case GOTYPE_BUILTIN:
        READ_OBJ(builtin_underlying_base);
        break;
    case GOTYPE_LAZY_INDEX:
        READ_OBJ(lazy_index_base);
        break;
    case GOTYPE_LAZY_CALL:
        READ_OBJ(lazy_call_base);
        READ_LISTP(lazy_call_type_args);
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
    READ_STR(current_path);
    READ_STR(current_import_path);
    READ_LIST(packages);
}

// ---

#define WRITE_STR(x) s->writestr(x)
#define WRITE_OBJ(x) write_object<std::remove_pointer<decltype(x)>::type>(x, s)
#define WRITE_LIST(x) write_list<decltype(x)>(x, s)
#define WRITE_LISTP(x) write_listp<decltype(x)>(x, s)

void Go_Type_Parameter::write(Index_Stream *s) {
    WRITE_STR(name);
    WRITE_OBJ(constraint);
}

void Godecl::write(Index_Stream *s) {
    WRITE_STR(name);

    if (type == GODECL_IMPORT) {
        WRITE_STR(import_path);
    } else {
        WRITE_OBJ(gotype);
        WRITE_LIST(type_params);
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
    case GOTYPE_VARIADIC:
        WRITE_OBJ(variadic_base);
        break;
    case GOTYPE_ASSERTION:
        WRITE_OBJ(assertion_base);
        break;
    case GOTYPE_RANGE:
        WRITE_OBJ(range_base);
        break;
    case GOTYPE_BUILTIN:
        WRITE_OBJ(builtin_underlying_base);
        break;
    case GOTYPE_LAZY_INDEX:
        WRITE_OBJ(lazy_index_base);
        break;
    case GOTYPE_LAZY_CALL:
        WRITE_OBJ(lazy_call_base);
        WRITE_LISTP(lazy_call_type_args);
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
    WRITE_STR(current_path);
    WRITE_STR(current_import_path);
    WRITE_LIST(packages);
}
