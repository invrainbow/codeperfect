#include "go.hpp"
#include "utils.hpp"
#include "world.hpp"
#include "mem.hpp"
#include "os.hpp"
#include "set.hpp"
#include "editor.hpp"
#include "meow_hash.hpp"

// TODO: dynamically determine this
static const char GOROOT[] = "c:\\go";
static const char GOPATH[] = "c:\\users\\brandon\\go";
const char TEST_PATH[] = "c:\\users\\brandon\\life";

// -----

s32 num_index_stream_opens = 0;
s32 num_index_stream_closes = 0;

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

void Module_Resolver::init(ccstr current_module_filepath) {
    ptr0(this);

    mem.init();

    SCOPED_MEM(&mem);

    root_import_to_resolved = alloc_object(Node);
    root_resolved_to_import = alloc_object(Node);

    Process proc;
    proc.init();
    proc.dir = current_module_filepath;
    if (!proc.run("go list -mod=mod -m all")) return;
    defer { proc.cleanup(); };

    List<char> line;
    line.init();
    char ch;

    do {
        line.len = 0;
        for (ch = '\0'; proc.read1(&ch) && ch != '\n'; ch = '\0')
            line.append(ch);
        line.append('\0');

        auto parts = split_string(line.items, ' ');
        if (parts->len == 1) {
            module_path = our_strcpy(parts->at(0));
            add_path(module_path, current_module_filepath);
        } else if (parts->len == 2) {
            auto import_path = parts->at(0);
            auto version = parts->at(1);
            auto subpath = normalize_path_in_module_cache(our_sprintf("%s@%s", import_path, version));
            auto path = path_join(GOPATH, "pkg/mod", subpath);
            add_path(import_path, path);
        } else if (parts->len == 5) {
            auto import_path = parts->at(0);
            auto new_import_path = parts->at(3);
            auto version = parts->at(4);
            auto subpath = normalize_path_in_module_cache(our_sprintf("%s@%s", new_import_path, version));
            auto path = path_join(GOPATH, "pkg/mod", subpath);
            add_path(import_path, path);
        }
    } while (ch != '\0');
}

void Module_Resolver::cleanup() {
    // ???
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

        if (it->type == IT_MMAP) {
            if (n + 2 > bufsize) break;
            buf[n++] = (char)uch;
        } else if (it->type == IT_BUFFER) {
            auto size = uchar_size(uch);
            if (n + size + 1 > bufsize) break;

            uchar_to_cstr(uch, &buf[n], &size);
            n += size;
        }
    }

    *read = n;
    buf[n] = '\0';
    return buf;
}

void Parser::init(Parser_It* _it, ccstr _filepath) {
    ptr0(this);
    it = _it;
    filepath = _filepath;
}

bool isid(int c) { return (isalnum(c) || c == '_'); }

void Parser::lex() {
    // tok.start_before_leading_whitespace = it->get_pos();

    while (isspace(it->peek()) && !it->eof())
        it->next();

    auto get_type = [&]() -> Tok_Type {
        if (it->eof()) return TOK_EOF;

        auto ch = it->next();
        switch (ch) {
        case '/':
            switch (it->peek()) {
            case '/':
                while (it->peek() != '\n')
                    it->next();
                return TOK_COMMENT;
            case '*':
                {
                    bool last_was_star = false;
                    while (true) {
                        auto ch = it->next();
                        if (ch == '/' && last_was_star) break;
                        last_was_star = (ch == '*');
                    }
                }
                return TOK_COMMENT;
            }
            break;
        }

        if (!isid(ch)) return TOK_ILLEGAL;

        while (isid(it->peek()))
            it->next();
        return TOK_ID;
    };

    tok.start = it->get_pos();
    tok.type = get_type();
    tok.end = it->get_pos();
}

// we could just do streq(get_token_string(tok), str) but this doesn't require memory
bool Parser::match_token_to_string(ccstr str) {
    auto len = strlen(str);
    if (tok.end.y != tok.start.y) return false;
    if (tok.end.x - tok.start.x != len) return false;

    auto old_pos = it->get_pos();
    defer { it->set_pos(old_pos); };

    it->set_pos(tok.start);
    for (u32 i = 0; i < len; i++)
        if (it->next() != str[i])
            return false;
    return true;
}

ccstr Parser::get_token_string() {
    if (tok.start.y != tok.end.y) return NULL;

    auto old_pos = it->get_pos();
    defer { it->set_pos(old_pos); };

    auto len = tok.end.x - tok.start.x;
    auto ret = alloc_array(char, len + 1);

    it->set_pos(tok.start);
    for (u32 i = 0; i < len; i++)
        ret[i] = it->next();
    ret[len] = '\0';
    return ret;
}

ccstr Parser::get_package_name() {
    do { lex(); } while (tok.type == TOK_COMMENT);

    if (tok.type != TOK_ID) return NULL;
    if (!match_token_to_string("package")) return NULL;
    lex();

    if (tok.type != TOK_ID) return NULL;
    return get_token_string();
}

Go_File *get_ready_file_in_package(Go_Package *pkg, ccstr filename) {
    auto file = pkg->files->find([&](Go_File *it) { return streq(filename, it->filename); });

    if (file == NULL) {
        file = pkg->files->append();
        file->pool.init("file pool", 1024); // tweak this
    } else {
        file->pool.reset(); // should we cleanup/init instead?
    }

    {
        SCOPED_MEM(&file->pool);
        file->filename = our_strcpy(filename);
        file->scope_ops = alloc_list<Go_Scope_Op>();
        file->decls = alloc_list<Godecl>();
        file->imports = alloc_list<Go_Import>();
    }

    return file;
}

void Go_Indexer::fill_package_hash(Go_Package *pkg) {
    pkg->hash = hash_package(pkg->import_path);
}

/*
open challenges:
 - when go.mod changed, what do?
*/

/*
granularize our background thread loop
either:
    - look at tree-sitter and come up with our own incremental implementation for go_file
    - or, just resign ourselves to creating a new go_file every autocomplete, param hint, jump to def, etc.
        - honestly, wouldn't this slow things down?
*/

void Go_Indexer::reload_all_dirty_files() {
    For (world.wksp.panes) {
        For (it.editors) {
            if (!it.index_dirty) continue;
            it.index_dirty = false;

            SCOPED_FRAME();

            auto filename = our_basename(it.filepath);

            auto import_path = filepath_to_import_path(our_dirname(it.filepath));
            auto pkg = find_package_in_index(import_path);
            if (pkg == NULL) continue;

            auto file = get_ready_file_in_package(pkg, filename);

            auto iter = alloc_object(Parser_It);
            iter->init(&it.buf);
            auto root_node = new_ast_node(ts_tree_root_node(it.tree), iter);

            ccstr package_name = NULL;
            process_tree_into_gofile(file, root_node, it.filepath, &package_name);
            replace_package_name(pkg, package_name);
        }
    }
}

bool is_git_folder(ccstr path) {
    SCOPED_FRAME();
    auto pathlist = make_path(path);
    return pathlist->parts->find([&](ccstr *it) { return streqi(*it, ".git"); }) != NULL;
}

Editor *get_open_editor(ccstr filepath) {
    For (world.wksp.panes)
        For (it.editors)
            if (are_filepaths_same_file(it.filepath, filepath))
                return &it;
    return NULL;
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

void Go_Indexer::replace_package_name(Go_Package *pkg, ccstr package_name) {
    if (package_name == NULL) return;

    if (pkg->package_name != NULL)
        if (streq(pkg->package_name, package_name))
            return;

    {
        SCOPED_MEM(&final_mem);
        pkg->package_name = our_strcpy(package_name);
    }
}

void Go_Indexer::background_thread() {
    // === create pools that we'll need ===

    // mem containing info for orchestrating background thread
    Pool bg_orch_mem;
    bg_orch_mem.init("bg_orch_mem");
    defer { bg_orch_mem.cleanup(); };

    // other initialization
    // ===

    SCOPED_MEM(&mem);

    use_pool_for_tree_sitter = true;

    List<ccstr> queue;

    {
        SCOPED_MEM(&bg_orch_mem);
        module_resolver.init(TEST_PATH);
        package_lookup.init();
        queue.init();
        // TODO: when do we clean this stuff up?
    }

    auto append_to_queue = [&](ccstr import_path) {
        SCOPED_MEM(&bg_orch_mem);
        queue.append(our_strcpy(import_path));
    };

    // try to read in index from disk
    // ===

    do {
        print("reading...");

        Index_Stream s;
        if (s.open(path_join(TEST_PATH, "db"), FILE_MODE_READ, FILE_OPEN_EXISTING) != FILE_RESULT_SUCCESS) break;
        defer { s.cleanup(); };

        {
            SCOPED_MEM(&final_mem);
            memcpy(&index, read_object<Go_Index>(&s), sizeof(Go_Index));
        }

        print("successfully read index from disk, final_mem.size = %d", final_mem.mem_allocated);
    } while (0);

    // initialize index
    // ===

    {
        SCOPED_MEM(&final_mem);
        if (index.current_path == NULL)
            index.current_path = our_strcpy(TEST_PATH);
        if (index.current_import_path == NULL)
            index.current_import_path = our_strcpy(get_workspace_import_path());
        if (index.packages == NULL)
            index.packages = alloc_list<Go_Package>();
    }

    // add existing packages to package lookup table
    // ===

    if (index.packages != NULL)
        For (*index.packages)
            package_lookup.set(it.import_path, &it);

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

            bool already_in_index = (find_up_to_date_package(import_path) != NULL);
            bool is_go_package = false;

            list_directory(resolved_path, [&](Dir_Entry *ent) {
                if (ent->type == DIRENT_FILE) {
                    if (!already_in_index && !is_go_package)
                        if (str_ends_with(ent->name, ".go"))
                            if (is_file_included_in_build(path_join(resolved_path, ent->name)))
                                is_go_package = true;
                    return;
                }

                if (streq(ent->name, "vendor")) return;
                if (streq(ent->name, ".git")) return;

                import_paths_queue->append(normalize_path_sep(path_join(import_path, ent->name), '/'));
                resolved_paths_queue->append(path_join(resolved_path, ent->name));
            });

            if (!already_in_index) {
                if (is_go_package || streq(import_path, index.current_import_path))
                    append_to_queue(import_path);
            }
        }
    }

    /*
    // TEST: remove fmt from packages, check that it gets picked up below
    {
        bool found = false;
        auto pkg = package_lookup.get("fmt", &found);
        our_assert(found, "unable to find \"fmt\" in index???");

        index.packages->remove(pkg);
        package_lookup.remove("fmt");
    }
    */

    // if we have any ready packages, see if they have any imports that were missed
    // ===

    {
        SCOPED_FRAME();

        String_Set seen;
        seen.init();
        defer { seen.cleanup(); };

        For (*index.packages) {
            if (it.status != GPS_READY) {
                append_to_queue(it.import_path);
                continue;
            }

            if (it.files == NULL) continue; // when would this happen?

            For (*it.files) {
                if (it.imports == NULL) continue;
                For (*it.imports) {
                    if (seen.has(it.import_path)) continue;
                    seen.add(it.import_path);

                    if (get_package_status(it.import_path) != GPS_READY)
                        append_to_queue(it.import_path);
                }
            }
        }
    }

    auto invalidate_packages_with_outdated_hash = [&]() {
        For (*index.packages) {
            if (it.status != GPS_READY) continue;
            // if hash has changed, mark outdated & queue for re-processing
            auto package_path = get_package_path(it.import_path);
            if (it.hash == hash_package(package_path)) continue;

            it.status = GPS_OUTDATED;
            append_to_queue(it.import_path);
        }
    };

    invalidate_packages_with_outdated_hash();

    // main loop
    // 1) process items in queue
    // 2) process filesystem events
    // 3) clean up memory by transferring index from final_mem to new pool, and swapping
    // 4) i guess write index to disk? tho shouldn't this happen in a more explicit way, instead of on an interval
    // ===

    int last_final_mem_allocated = final_mem.mem_allocated;
    u64 last_write_time = 0;

    for (;; Sleep(100)) {
        bool force_write_this_time = false;

        // process handle_gomod_changed request
        // ===

        {
            bool is_set = false;
            {
                SCOPED_LOCK(&handle_gomod_changed_lock);
                is_set = flag_handle_gomod_changed;
                if (is_set)
                    flag_handle_gomod_changed = false;
            }

            if (is_set) {
                module_resolver.cleanup();
                module_resolver.init(TEST_PATH);
                invalidate_packages_with_outdated_hash();
            }
        }

        // process filesystem events
        // ---

        Fs_Event event;
        for (u32 items_processed = 0; items_processed < 10 && wksp_watch.next_event(&event); items_processed++) {
            scratch_mem.init("scratch_mem");
            defer { scratch_mem.cleanup(); };
            SCOPED_MEM(&scratch_mem);

            if (is_git_folder(event.filepath)) continue;

            if (event.type == FSEVENT_RENAME)
                print("%s: %s -> %s", fs_event_type_str(event.type), event.filepath, event.new_filepath);
            else
                print("%s: %s", fs_event_type_str(event.type), event.filepath);

            auto handle_gofile_deleted = [&](ccstr filepath) {
                auto pkg = find_package_in_index(filepath_to_import_path(our_dirname(filepath)));
                if (pkg == NULL) return;
                if (pkg->files == NULL) return;

                auto filename = our_basename(filepath);
                auto file = pkg->files->find([&](Go_File *it) { return streq(filename, it->filename); });
                if (file == NULL) return;

                pkg->files->remove(file);
            };

            auto handle_gofile_created = [&](ccstr filepath) {
                // i have a great fucking idea
                // why don't we just exclude any file here that's open in an editor
                // since files open in editor will get picked up by reload_all_dirty_files() anyway
                if (get_open_editor(filepath) != NULL) return;

                auto pkg = find_package_in_index(filepath_to_import_path(our_dirname(filepath)));
                if (pkg == NULL) return;

                auto filename = our_basename(event.filepath);
                auto file = get_ready_file_in_package(pkg, filename);

                auto pf = parse_file(filepath);
                if (pf == NULL) return;
                defer { free_parsed_file(pf); };

                ccstr package_name = NULL;
                process_tree_into_gofile(file, pf->root, event.filepath, &package_name);
                replace_package_name(pkg, package_name);

                // queue up any new imports
                For (*file->imports) {
                    auto status = get_package_status(it.import_path);
                    if (status == GPS_OUTDATED)
                        append_to_queue(it.import_path);
                }

                /*
                auto editor = get_open_editor(filepath);
                if (editor != NULL) {
                    // TODO: there was supposed to be a lock here before
                    // the whole thing got commented out
                    auto msg = editor->msg_queue.append();
                    msg->type = EMSG_RELOAD;
                }
                */

                /*
                if file changed from outside
                    update index
                    if editor exists, reload it, discard changes
                if file changed from editor
                    update on save
                */
            };

            switch (event.type) {
            case FSEVENT_DELETE:
                {
                    auto filepath = path_join(index.current_path, event.filepath);
                    // try treating filepath as a directory
                    auto pkg = find_package_in_index(filepath_to_import_path(filepath));
                    if (pkg != NULL) {
                        index.packages->remove(pkg);
                    } else if (str_ends_with(event.filepath, ".go")) {
                        // try treating filepath as a file
                        handle_gofile_deleted(filepath);
                    }
                }
                break;

            case FSEVENT_CHANGE:
                {
                    auto filepath = path_join(index.current_path, event.filepath);
                    if (check_path(filepath) != CPR_FILE) break;
                    if (!str_ends_with(filepath, ".go")) break;

                    // handled same way as file creation
                    handle_gofile_created(filepath);
                }
                break;

            case FSEVENT_CREATE:
                {
                    auto filepath = path_join(index.current_path, event.filepath);
                    switch (check_path(filepath)) {
                    case CPR_DIRECTORY:
                        {
                            // at this point, there will already exist a folder full of files
                            // FSEVENT_CREATE events won't be created for the individual files.

                            auto import_path = filepath_to_import_path(filepath);
                            auto pkg = find_package_in_index(import_path);

                            if (pkg == NULL) {
                                SCOPED_MEM(&final_mem);
                                pkg = index.packages->append();
                                pkg->status = GPS_OUTDATED;
                                pkg->files = alloc_list<Go_File>();
                                pkg->import_path = our_strcpy(import_path);
                                package_lookup.set(pkg->import_path, pkg);
                            }

                            pkg->status = GPS_UPDATING;
                            pkg->files->len = 0;

                            auto source_files = list_source_files(filepath, false);
                            For (*source_files) {
                                Pool tmp_mem;
                                tmp_mem.init();
                                defer { tmp_mem.cleanup(); };

                                SCOPED_MEM(&tmp_mem);

                                auto full_filepath = path_join(filepath, it);

                                auto pf = parse_file(full_filepath);
                                if (pf == NULL) continue;
                                defer { free_parsed_file(pf); };

                                auto file = get_ready_file_in_package(pkg, it);

                                ccstr package_name = NULL;
                                process_tree_into_gofile(file, pf->root, full_filepath, &package_name);
                                replace_package_name(pkg, package_name);

                                For (*file->imports) {
                                    auto status = get_package_status(it.import_path);
                                    if (status == GPS_OUTDATED)
                                        append_to_queue(it.import_path);
                                }
                            }

                            fill_package_hash(pkg);
                            pkg->status = GPS_READY;
                        }
                        break;
                    case CPR_FILE:
                        {
                            if (!str_ends_with(filepath, ".go")) break;
                            handle_gofile_created(filepath);
                        }
                        break;
                    }
                }
                break;

            case FSEVENT_RENAME:
                {
                    auto filepath = path_join(index.current_path, event.new_filepath);
                    switch (check_path(filepath)) {
                    case CPR_DIRECTORY:
                        {
                            auto import_path = filepath_to_import_path(filepath);
                            auto pkg = find_package_in_index(import_path);

                            package_lookup.remove(pkg->import_path);
                            package_lookup.set(pkg->import_path, pkg);

                            auto new_filepath = path_join(index.current_path, event.new_filepath);
                            auto new_import_path = filepath_to_import_path(new_filepath);

                            {
                                SCOPED_MEM(&final_mem);
                                pkg->import_path = our_strcpy(new_import_path);
                            }
                        }
                        break;
                    case CPR_FILE:
                        {
                            bool old_is_gofile = str_ends_with(event.filepath, ".go");
                            bool new_is_gofile = str_ends_with(event.new_filepath, ".go");

                            if (old_is_gofile && !new_is_gofile) {
                                // same as deleting
                                handle_gofile_deleted(path_join(index.current_path, event.filepath));
                            } else if (!old_is_gofile && new_is_gofile) {
                                // same as creating
                                handle_gofile_created(filepath);
                            } else if (old_is_gofile && new_is_gofile) {
                                auto old_filepath = path_join(index.current_path, event.filepath);
                                auto pkg = find_package_in_index(filepath_to_import_path(our_dirname(old_filepath)));
                                if (pkg == NULL) break;

                                auto filename = our_basename(event.filepath);
                                auto file = pkg->files->find([&](Go_File *it) -> bool {
                                    return streq(filename, it->filename);
                                });

                                if (file == NULL) break;

                                {
                                    SCOPED_MEM(&final_mem);
                                    file->filename = our_basename(event.new_filepath);
                                }
                            }
                        }
                        break;
                    }
                }
                break;
            }
        }

        // process items in queue
        // ---

        bool queue_had_stuff = (queue.len > 0);

        for (int items_processed = 0; items_processed < 10 && queue.len > 0; items_processed++) {
            scratch_mem.init("scratch_mem");
            defer { scratch_mem.cleanup(); };
            SCOPED_MEM(&scratch_mem);

            auto import_path = *queue.last();
            queue.len--;

            auto resolved_path = get_package_path(import_path);
            if (resolved_path == NULL) continue;

            auto pkg = find_package_in_index(import_path);
            if (pkg != NULL) {
                if (pkg->status == GPS_READY) continue;
            } else {
                SCOPED_MEM(&final_mem);
                pkg = index.packages->append();
                pkg->status = GPS_OUTDATED;
                pkg->files = alloc_list<Go_File>();
                pkg->import_path = our_strcpy(import_path);
                package_lookup.set(pkg->import_path, pkg);
            }

            print("processing %s -> %s", import_path, resolved_path);

            pkg->status = GPS_UPDATING;

            auto source_files = list_source_files(resolved_path, false);
            For (*source_files) {
                auto filename = it;

                // TODO: refactor this block too, pretty sure we're doing same
                // thing somewhere else

                SCOPED_FRAME();

                auto filepath = path_join(resolved_path, filename);

                auto pf = parse_file(filepath);
                if (pf == NULL) continue;
                defer { free_parsed_file(pf); };

                auto file = get_ready_file_in_package(pkg, filename);

                ccstr package_name = NULL;
                process_tree_into_gofile(file, pf->root, filepath, &package_name);
                replace_package_name(pkg, package_name);

                For (*file->imports) {
                    auto status = get_package_status(it.import_path);
                    if (status == GPS_OUTDATED)
                        append_to_queue(it.import_path);
                }
            }

            fill_package_hash(pkg);
            pkg->status = GPS_READY;
        }

        if (queue_had_stuff && queue.len == 0)
            force_write_this_time = true;

        // clean up memory if it's getting out of control
        // ---

        // clean up every 50 megabytes
        if (final_mem.mem_allocated - last_final_mem_allocated > 1024 * 1024 * 50) {
            Pool new_pool;
            new_pool.init();

            {
                SCOPED_MEM(&new_pool);
                memcpy(&index, index.copy(), sizeof(index));
            }

            final_mem.cleanup();
            memcpy(&final_mem, &new_pool, sizeof(Pool));
            last_final_mem_allocated = final_mem.mem_allocated;
        }

        // write index to disk
        // ---

        auto time = current_time_in_nanoseconds();

        auto should_write = [&]() -> bool {
            if (force_write_this_time) return true;

            // if there's still stuff in queue, don't write yet, we'll trigger
            // a write when we clear out the queue
            if (queue.len > 0) return false;

            auto ten_minutes_in_ns = (u64)10 * 60 * 1000 * 1000 * 1000;
            if (time - last_write_time >= ten_minutes_in_ns) return true;

            return false;
        };

        do {
            if (!should_write()) break;

            print("writing index to disk");

            // Set last_write_time, even if the write operation itself later fails.
            // This way we're not stuck trying over and over to write.
            last_write_time = time;

            {
                Index_Stream s;
                if (s.open(path_join(TEST_PATH, "db.tmp"), FILE_MODE_WRITE, FILE_CREATE_NEW) != FILE_RESULT_SUCCESS) break;
                defer { s.cleanup(); };
                write_object<Go_Index>(&index, &s);
            }

            move_file_atomically(path_join(TEST_PATH, "db.tmp"), path_join(TEST_PATH, "db"));
        } while (0);
    }
}

bool Go_Indexer::start_background_thread() {
    SCOPED_MEM(&mem);
    auto fn = [](void* param) {
        auto indexer = (Go_Indexer*)param;
        indexer->background_thread();
    };
    return (bgthread = create_thread(fn, this)) != NULL;
}

Parsed_File *Go_Indexer::parse_file(ccstr filepath) {
    Parsed_File *ret = NULL;

    auto editor = get_open_editor(filepath);
    if (editor != NULL) {
        auto it = alloc_object(Parser_It);
        it->init(&editor->buf);

        ret = alloc_object(Parsed_File);
        ret->tree_belongs_to_editor = true;
        ret->it = it;
        ret->tree = editor->tree;
    } else {
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

    ret->root = new_ast_node(ts_tree_root_node(ret->tree), ret->it);
    return ret;
}

Ast_Node *new_ast_node(TSNode node, Parser_It *it) {
    auto ret = alloc_object(Ast_Node);
    ret->init(node, it);
    return ret;
}

void Go_Indexer::free_parsed_file(Parsed_File *file) {
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

    walk_ts_cursor(&cursor, abstract_only, [&](Ast_Node *it, Ts_Field_Type type, int depth) -> Walk_Action {
        it->it = node->it;
        return cb(it, type, depth);
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

// -----

Ast_Node *Ast_Node::dup(TSNode new_node) {
    return new_ast_node(new_node, it);
}

ccstr Ast_Node::string() {
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

Gotype *Go_Indexer::new_primitive_type(ccstr name) {
    auto ret = new_gotype(GOTYPE_ID);
    ret->id_name = name;
    return ret;
}

Go_Package *Go_Indexer::find_package_in_index(ccstr import_path) {
    bool found = false;
    auto ret = package_lookup.get(import_path, &found);
    return found ? ret : NULL;

    /*
    if (index.packages == NULL) return NULL;
    return index.packages->find([&](Go_Package *it) -> bool {
        return streq(it->import_path, import_path);
    });
    */
}

Go_Package *Go_Indexer::find_up_to_date_package(ccstr import_path) {
    auto pkg = find_package_in_index(import_path);
    if (pkg != NULL)
        if (pkg->status != GPS_OUTDATED)
            return pkg;
    return NULL;
}

Go_Package_Status Go_Indexer::get_package_status(ccstr import_path) {
    auto pkg = find_package_in_index(import_path);
    return pkg == NULL ? GPS_OUTDATED : pkg->status;
}

ccstr Go_Indexer::find_import_path_referred_to_by_id(ccstr id, Go_Ctx *ctx) {
    auto pkg = find_up_to_date_package(ctx->import_path);
    if (pkg == NULL) return NULL;

    For (*pkg->files) {
        if (streq(it.filename, ctx->filename)) {
            For (*it.imports)
                if (it.package_name != NULL && streq(it.package_name, id))
                        return it.import_path;
            break;
        }
    }

    return NULL;
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

void Go_Indexer::iterate_over_scope_ops(Ast_Node *root, fn<bool(Go_Scope_Op*)> cb, ccstr filename) {
    List<int> open_scopes;
    open_scopes.init();

    auto scope_ops_decls = alloc_list<Godecl>();

    walk_ast_node(root, true, [&](Ast_Node* node, Ts_Field_Type, int depth) -> Walk_Action {
        for (; open_scopes.len > 0 && depth <= *open_scopes.last(); open_scopes.len--) {
            Go_Scope_Op op;
            op.type = GSOP_CLOSE_SCOPE;
            op.pos = node->start;
            if (!cb(&op)) return WALK_ABORT;
        }

        switch (node->type) {
        case TS_IF_STATEMENT:
        case TS_FOR_STATEMENT:
        case TS_TYPE_SWITCH_STATEMENT:
        case TS_EXPRESSION_SWITCH_STATEMENT:
        case TS_BLOCK:
        case TS_METHOD_DECLARATION:
        case TS_FUNCTION_DECLARATION:
            {
                open_scopes.append(depth);

                Go_Scope_Op op;
                op.type = GSOP_OPEN_SCOPE;
                op.pos = node->start;
                if (!cb(&op)) return WALK_ABORT;
            }
            break;

        case TS_PARAMETER_LIST:
        case TS_SHORT_VAR_DECLARATION:
        case TS_CONST_DECLARATION:
        case TS_VAR_DECLARATION:
            {
                scope_ops_decls->len = 0;
                node_to_decls(node, scope_ops_decls, filename);

                for (u32 i = 0; i < scope_ops_decls->len; i++) {
                    Go_Scope_Op op;
                    op.type = GSOP_DECL;
                    op.decl = &scope_ops_decls->items[i];
                    op.pos = scope_ops_decls->items[i].decl_start;
                    if (!cb(&op)) return WALK_ABORT;
                }
            }
            break;
        }

        return WALK_CONTINUE;
    });
}

/*
 - adds decls to package->decls
 - adds scope ops to the right entry in package->files
 - adds import paths to individual_imports
*/
void Go_Indexer::process_tree_into_gofile(
    Go_File *file,
    Ast_Node *root,
    ccstr filepath,
    ccstr *package_name
) {
    auto filename = our_basename(filepath);

    // add decls
    // ---------

    file->decls->len = 0;

    FOR_NODE_CHILDREN (root) {
        switch (it->type) {
        case TS_PACKAGE_CLAUSE:
            if (package_name != NULL)
                if (*package_name == NULL)
                    *package_name = it->child()->string();
            break;
        case TS_VAR_DECLARATION:
        case TS_CONST_DECLARATION:
        case TS_FUNCTION_DECLARATION:
        case TS_METHOD_DECLARATION:
        case TS_TYPE_DECLARATION:
        case TS_SHORT_VAR_DECLARATION:
            node_to_decls(it, file->decls, filename, &file->pool);
            break;
        }
    }

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

    // add import info
    // ---------------

    file->imports->len = 0;

    bool imports_seen = false;

    FOR_NODE_CHILDREN (root) {
        auto decl_node = it;

        if (decl_node->type != TS_IMPORT_DECLARATION) {
            if (imports_seen)
                break;
            else
                continue;
        }

        imports_seen = true;

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

            auto new_import_path = parse_go_string(path_node->string());

            auto ri = resolve_import(new_import_path);
            if (ri == NULL) {
                ri = resolve_import(new_import_path);
                continue;
            }

            // decl
            auto decl = alloc_object(Godecl);
            decl->file = filename;
            decl->decl_start = decl_node->start;
            import_spec_to_decl(it, decl);

            // import
            auto imp = file->imports->append();
            imp->decl = decl;
            imp->import_path = new_import_path;
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

            {
                SCOPED_MEM(&file->pool);
                memcpy(imp, imp->copy(), sizeof(Go_Import));
            }
        }
    }

    file->hash = hash_file(filepath);
}

void Go_Indexer::handle_error(ccstr err) {
    // TODO: What's our error handling strategy?
    error("%s", err);
}

u64 Go_Indexer::hash_file(ccstr filepath) {
    auto f = read_entire_file(filepath);
    if (f == NULL) return 0;
    defer { free_entire_file(f); };

    u64 ret = 0;
    auto name = our_basename(filepath);
    ret ^= meow_hash(f->data, f->len);
    ret ^= meow_hash((void*)name, strlen(name));
    return ret;
}

u64 Go_Indexer::hash_package(ccstr resolved_package_path) {
    u64 ret = 0;
    ret ^= meow_hash((void*)resolved_package_path, strlen(resolved_package_path));

    {
        SCOPED_FRAME();

        auto files = list_source_files(resolved_package_path, false);
        if (files == NULL) return 0;
        For (*files)
            ret ^= hash_file(path_join(resolved_package_path, it));
    }

    return ret;
}

bool Go_Indexer::is_file_included_in_build(ccstr path) {
    auto resp = run_gohelper_command(GH_OP_CHECK_INCLUDED_IN_BUILD, path, NULL);
    return streq(resp, "true");
}

ccstr Go_Indexer::run_gohelper_command(Gohelper_Op op, ...) {
    va_list vl;
    va_start(vl, op);

    gohelper_proc.writestr(our_sprintf("%d", op));
    gohelper_proc.write1('\n');

    ccstr param = NULL;
    while ((param = va_arg(vl, ccstr)) != NULL) {
        gohelper_proc.writestr(param);
        gohelper_proc.write1('\n');
    }

    auto read_line = [&]() -> ccstr {
        auto ret = alloc_list<char>();
        char ch;
        while (true) {
            our_assert(gohelper_proc.read1(&ch), "gohelper crashed, we can't do anything anymore");
            if (ch == '\n') break;
            ret->append(ch);
        }
        ret->append('\0');
        return ret->items;
    };

    auto ret = read_line();
    if (streq(ret, "error")) {
        gohelper_returned_error = true;
        return read_line();
    }

    gohelper_returned_error = false;
    return ret;
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
    char buf[256];

    {
        SCOPED_FRAME();

        auto ef = read_entire_file(filepath);
        if (ef == NULL) return NULL;

        auto it = alloc_object(Parser_It);
        it->init(ef);

        Parser p;
        p.init(it, filepath);
        auto name = p.get_package_name();
        if (name == NULL) {
            return NULL;
        }

        strcpy_safe(buf, _countof(buf), name);
    }

    return our_strcpy(buf);
}

ccstr Go_Indexer::get_package_name(ccstr path) {
    if (check_path(path) != CPR_DIRECTORY) return NULL;

    auto files = list_source_files(path, false);
    if (files == NULL) return NULL;

    For (*files) {
        auto filepath = path_join(path, it);
        auto pkgname = get_package_name_from_file(filepath);
        if (pkgname != NULL)
            return pkgname;
        else
            continue;
    }

    return NULL;
}

ccstr Go_Indexer::get_package_path(ccstr import_path) {
    auto ret = module_resolver.resolve_import(import_path);
    if (ret != NULL) return ret;

    auto path = path_join(GOROOT, "src", import_path);
    if (check_path(path) != CPR_DIRECTORY) return NULL;
    bool is_package = false;
    list_directory(path, [&](Dir_Entry *ent) {
        if (!is_package)
            if (ent->type == DIRENT_FILE)
                if (str_ends_with(ent->name, ".go"))
                    is_package = true;
    });
    return is_package ? path : NULL;
}

Resolved_Import* Go_Indexer::resolve_import(ccstr import_path) {
    auto path = get_package_path(import_path);
    if (path == NULL) return NULL;

    auto name = get_package_name(path);
    if (name == NULL) return NULL;

    auto ret = alloc_object(Resolved_Import);
    ret->path = path;
    ret->package_name = name;
    return ret;
}

ccstr Go_Indexer::get_workspace_import_path() {
    return module_resolver.module_path;
}

Goresult *Go_Indexer::find_decl_of_id(ccstr id_to_find, cur2 id_pos, Go_Ctx *ctx, Go_Import **single_import) {
    auto pkg = find_up_to_date_package(ctx->import_path);
    if (pkg != NULL) {
        auto check = [&](Go_File *it) { return streq(it->filename, ctx->filename); };
        auto file = pkg->files->find(check);
        if (file != NULL) {
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
                case GSOP_OPEN_SCOPE: table.push_scope(); break;
                case GSOP_CLOSE_SCOPE: table.pop_scope(); break;
                case GSOP_DECL: table.set(it.decl->name, it.decl); break;
                }
            }

            auto decl = table.get(id_to_find);
            if (decl != NULL) return make_goresult(decl, ctx);
        }

        For (*pkg->files) {
            if (streq(it.filename, ctx->filename)) {
                For (*it.imports) {
                    if (it.package_name != NULL && streq(it.package_name, id_to_find)) {
                        if (single_import != NULL)
                            *single_import = &it;
                        return make_goresult(it.decl, ctx);
                    }
                }
                break;
            }
        }
    }

    return find_decl_in_package(id_to_find, ctx->import_path);
}

List<Goresult> *Go_Indexer::get_possible_dot_completions(Ast_Node *operand_node, bool *was_package, Go_Ctx *ctx) {
    switch (operand_node->type) {
    case TS_IDENTIFIER:
    case TS_FIELD_IDENTIFIER:
    case TS_PACKAGE_IDENTIFIER:
    case TS_TYPE_IDENTIFIER:
        do {
            auto import_path = find_import_path_referred_to_by_id(operand_node->string(), ctx);
            if (import_path == NULL) break;

            auto ret = get_package_decls(import_path, GETDECLS_PUBLIC_ONLY | GETDECLS_EXCLUDE_METHODS);
            if (ret != NULL)  {
                *was_package = true;
                return ret;
            }
        } while (0);
        break;
    }

    auto gotype = expr_to_gotype(operand_node);
    if (gotype == NULL) return NULL;

    auto res = evaluate_type(gotype, ctx);
    if (res == NULL) return NULL;

    auto resolved_res = resolve_type(res->gotype, res->ctx);
    if (resolved_res == NULL) return NULL;

    auto tmp = alloc_list<Goresult>();
    list_fields_and_methods(res, resolved_res, tmp);

    auto results = alloc_list<Goresult>();
    For (*tmp) {
        if (!streq(it.ctx->import_path, ctx->import_path))
            if (!isupper(it.decl->name[0]))
                continue;
        results->append(&it);
    }

    *was_package = false;
    return results;
}

Jump_To_Definition_Result* Go_Indexer::jump_to_definition(ccstr filepath, cur2 pos) {
    reload_all_dirty_files();

    auto pf = parse_file(filepath);
    if (pf == NULL) return NULL;
    defer { free_parsed_file(pf); };

    auto file = pf->root;

    Go_Ctx ctx = {0};
    ctx.import_path = filepath_to_import_path(our_dirname(filepath));
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
    reload_all_dirty_files();

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

    auto import_path = filepath_to_import_path(our_dirname(filepath));
    if (import_path == NULL) return false;

    Go_Ctx ctx = {0};
    ctx.import_path = import_path;
    ctx.filename = our_basename(filepath);

    List<Goresult> results;
    results.init();
    bool found_something = false;

    enum Current_Situation {
        FOUND_JACK_SHIT,
        FOUND_DOT_COMPLETE,
        FOUND_DOT_COMPLETE_NEED_CRAWL,
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
                            expr_to_analyze = expr;
                            situation = FOUND_DOT_COMPLETE_NEED_CRAWL;
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

    auto try_dot_complete = [&](Ast_Node *expr_to_analyze) -> bool {
        bool was_package = false;
        auto results = get_possible_dot_completions(expr_to_analyze, &was_package, &ctx);
        if (results == NULL || results->len == 0) return false;

        ac_results = alloc_list<AC_Result>(results->len);
        For (*results) ac_results->append()->name = it.decl->name;

        out->type = (was_package ? AUTOCOMPLETE_PACKAGE_EXPORTS : AUTOCOMPLETE_FIELDS_AND_METHODS);
        return true;
    };

    switch (situation) {
    case FOUND_PROBLEM_NEED_TO_EXIT:
        return false;
    case FOUND_DOT_COMPLETE_NEED_CRAWL:
        for (auto expr = expr_to_analyze; !try_dot_complete(expr);) {
            if (expr->type == TS_PARENTHESIZED_EXPRESSION) return false;

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
    case FOUND_LONE_IDENTIFIER:
        {
            String_Set seen_strings;
            seen_strings.init();
            ac_results = alloc_list<AC_Result>();

            auto add_result = [&](ccstr name) {
                if (!seen_strings.has(name)) {
                    ac_results->append()->name = name;
                    seen_strings.add(name);
                }
            };

            SCOPED_FRAME_WITH_MEM(&scoped_table_mem);
            Scoped_Table<bool> table;
            {
                SCOPED_MEM(&scoped_table_mem);
                table.init();
            }
            defer { table.cleanup(); };

            iterate_over_scope_ops(pf->root, [&](Go_Scope_Op *it) -> bool {
                if (it->pos > pos) {
                    // iterate over hash table
                    return false;
                }

                switch (it->type) {
                case GSOP_OPEN_SCOPE:
                    table.push_scope();
                    break;
                case GSOP_CLOSE_SCOPE:
                    table.pop_scope();
                    break;
                case GSOP_DECL:
                    table.set(it->decl->name, true);
                    break;
                }
                return true;
            }, ctx.filename);

            auto entries = table.entries();
            For (*entries) add_result(it->name);

            // imports now?
            bool imports_seen = false;
            FOR_NODE_CHILDREN (pf->root) {
                auto decl_node = it;

                if (decl_node->type != TS_IMPORT_DECLARATION) {
                    if (imports_seen) break; else continue;
                }

                imports_seen = true;

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

                    if (name_node == NULL || name_node->null) {
                        auto new_import_path = parse_go_string(path_node->string());
                        auto ri = resolve_import(new_import_path);
                        if (ri == NULL) continue;
                        add_result(ri->package_name);
                    } else if (name_node->type != TS_DOT && name_node->type != TS_BLANK_IDENTIFIER) {
                        add_result(name_node->string());
                    }
                }
            }

            /* to read imports from the index:
            auto pkg = find_up_to_date_package(ctx.import_path);
            auto check = [&](Go_File *it) { return streq(it->filename, ctx.filename); };
            auto file = pkg->files->find(check);
            For (*file->imports) it.package_name; */

            auto results = get_package_decls(ctx.import_path, GETDECLS_EXCLUDE_METHODS);
            if (results != NULL)
                For (*results)
                    add_result(it.decl->name);

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
    reload_all_dirty_files();

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
                if (!prev->null) {
                    func_expr = prev;
                    call_args_start = node->start;
                } else {
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
    ctx.import_path = filepath_to_import_path(our_dirname(filepath));
    ctx.filename = our_basename(filepath);

    auto gotype = expr_to_gotype(func_expr);
    if (gotype == NULL) return NULL;

    auto res = evaluate_type(gotype, &ctx);
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
        break;

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

    type_res = unpointer_type(type_res->gotype, type_res->ctx);

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
        break;
    }

    if (type_name == NULL || target_import_path == NULL) return;

    auto results = get_package_decls(target_import_path);
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

#define EVENT_DEBOUNCE_DELAY_MS 3000 // wait 3 seconds (arbitrary)

ccstr remove_ats_from_path(ccstr s) {
    auto path = make_path(s);
    For (*path->parts) {
        auto atpos = strchr((char*)it, '@');
        if (atpos != NULL) *atpos = '\0';
    }
    return path->str();
}

ccstr Go_Indexer::get_filepath_from_ctx(Go_Ctx *ctx) {
    auto dir = get_package_path(ctx->import_path);
    if (dir == NULL) return NULL;
    return path_join(dir, ctx->filename);
}

ccstr Go_Indexer::filepath_to_import_path(ccstr path_str) {
    auto ret = module_resolver.resolved_path_to_import_path(path_str);
    if (ret != NULL) return ret;

    auto path = make_path(path_str);
    auto goroot = make_path(GOROOT);

    if (!goroot->contains(path)) return NULL;

    auto parts = alloc_list<ccstr>(path->parts->len - goroot->parts->len);
    for (u32 i = goroot->parts->len; i < path->parts->len; i++)
        parts->append(path->parts->at(i));

    Path p;
    p.init(parts);
    return p.str();
}

void Go_Indexer::init() {
    ptr0(this);

    mem.init("indexer_mem");
    final_mem.init("final_mem");
    ui_mem.init("ui_mem");
    scratch_mem.init("scratch_mem");
    scoped_table_mem.init("scoped_table_mem");

    SCOPED_MEM(&mem);

    {
        SCOPED_FRAME();
        GetModuleFileNameA(NULL, current_exe_path, _countof(current_exe_path));
        auto path = our_dirname(current_exe_path);
        strcpy_safe(current_exe_path, _countof(current_exe_path), path);
    }

    gohelper_proc.dir = TEST_PATH;
    gohelper_proc.use_stdin = true;
    gohelper_proc.run(our_sprintf("go run %s", path_join(current_exe_path, "helper/helper.go")));

    wksp_watch.init(TEST_PATH);

    handle_gomod_changed_lock.init();
    // files_to_ignore_fsevents_on
}

void Go_Indexer::cleanup() {
    if (bgthread != NULL) {
        kill_thread(bgthread);
        close_thread_handle(bgthread);
        bgthread = NULL;
    }

    gohelper_proc.cleanup();
    mem.cleanup();
    final_mem.cleanup();
    ui_mem.cleanup();
    scratch_mem.cleanup();
    scoped_table_mem.cleanup();
    handle_gomod_changed_lock.init();
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

        if (id_count == 0) id_count = 1;
        count += id_count;
    }

    auto ret = alloc_list<Godecl>(count);

    FOR_NODE_CHILDREN (params) {
        auto type_node = it->field(TSF_TYPE);
        auto param_node = it;
        bool is_variadic = (param_node->type == TS_VARIADIC_PARAMETER_DECLARATION);
        bool id_added = false;

        FOR_NODE_CHILDREN (param_node) {
            if (it->eq(type_node)) break;

            id_added = true;

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

        if (!id_added) {
            auto field = ret->append();
            field->type = GODECL_FIELD;
            field->decl_start = param_node->start;
            field->spec_start = param_node->start;
            field->name_start = param_node->start;
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

bool Go_Indexer::assignment_to_decls(List<Ast_Node*> *lhs, List<Ast_Node*> *rhs, New_Godecl_Func new_godecl) {
    if (lhs->len == 0 || rhs->len == 0) return false;

    if (rhs->len == 1) {
        auto multi_type = expr_to_gotype(rhs->at(0));

        u32 index = 0;
        For (*lhs) {
            defer { index++; };

            if (it->type != TS_IDENTIFIER) continue;

            auto name = it->string();
            if (streq(name, "_")) continue;

            auto gotype = new_gotype(GOTYPE_LAZY_ONE_OF_MULTI);
            gotype->lazy_one_of_multi_base = multi_type;
            gotype->lazy_one_of_multi_index = index;
            gotype->lazy_one_of_multi_is_single = (lhs->len == 1);

            auto decl = new_godecl();
            decl->name = it->string();
            decl->name_start = it->start;
            decl->gotype = gotype;
        }
        return true;

    }

    if (lhs->len == rhs->len) {
        for (u32 i = 0; i < lhs->len; i++) {
            auto name_node = lhs->at(i);
            if (name_node->type != TS_IDENTIFIER) continue;

            auto gotype = expr_to_gotype(rhs->at(i));
            if (gotype == NULL) continue;

            if (gotype->type == GOTYPE_MULTI || gotype->type == GOTYPE_RANGE)
                continue;

            if (gotype->type == GOTYPE_ASSERTION)
                gotype = gotype->assertion_base;

            auto decl = new_godecl();
            decl->name = name_node->string();
            decl->name_start = name_node->start;
            decl->gotype = gotype;
        }

        return true;
    }

    return false;
}

void Go_Indexer::node_to_decls(Ast_Node *node, List<Godecl> *results, ccstr filename, Pool *target_pool) {
    auto new_result = [&]() -> Godecl * {
        auto decl = results->append();
        decl->file = filename;
        decl->decl_start = node->start;
        return decl;
    };

    auto save_decl = [&](Godecl *decl) {
        if (target_pool == NULL) return;
        SCOPED_MEM(target_pool);
        memcpy(decl, decl->copy(), sizeof(Godecl));
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

            auto decl = new_result();
            decl->type = GODECL_FUNC;
            decl->spec_start = node->start;
            decl->name = name->string();
            decl->gotype = gotype;
            save_decl(decl);
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

            auto decl = new_result();
            decl->type = GODECL_TYPE;
            decl->spec_start = spec->start;
            decl->name = name_node->string();
            decl->name_start = name_node->start;
            decl->gotype = gotype;
            save_decl(decl);
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

                    auto decl = new_result();

                    switch (node->type) {
                    case TS_PARAMETER_LIST: decl->type = GODECL_FIELD; break;
                    case TS_CONST_DECLARATION: decl->type = GODECL_CONST; break;
                    case TS_VAR_DECLARATION: decl->type = GODECL_VAR; break;
                    }

                    decl->spec_start = spec->start;
                    decl->name = it->string();
                    decl->name_start = it->start;
                    decl->gotype = type_node_gotype;
                    save_decl(decl);
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

                auto new_godecl = [&]() -> Godecl * {
                    auto decl = new_result();
                    decl->spec_start = spec->start;

                    switch (node->type) {
                    case TS_PARAMETER_LIST: decl->type = GODECL_FIELD; break;
                    case TS_CONST_DECLARATION: decl->type = GODECL_CONST; break;
                    case TS_VAR_DECLARATION: decl->type = GODECL_VAR; break;
                    }

                    return decl;
                };

                auto old_len = results->len;
                assignment_to_decls(lhs, rhs, new_godecl);
                for (u32 i = old_len; i < results->len; i++)
                    save_decl(results->items + i);
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

            auto new_godecl = [&]() -> Godecl * {
                auto decl = new_result();
                decl->spec_start = node->start;
                decl->type = GODECL_SHORTVAR;
                return decl;
            };

            auto old_len = results->len;
            assignment_to_decls(lhs, rhs, new_godecl);
            for (u32 i = old_len; i < results->len; i++)
                save_decl(results->items + i);
        }
        break;
    }
}

Goresult *Go_Indexer::unpointer_type(Gotype *type, Go_Ctx *ctx) {
    while (type->type == GOTYPE_POINTER)
        type = type->pointer_base;
    return make_goresult(type, ctx);
}

List<Goresult> *Go_Indexer::get_package_decls(ccstr import_path, int flags) {
    if (index.packages == NULL) return NULL;
    For (*index.packages) {
        if (it.status == GPS_OUTDATED) continue;
        if (!streq(it.import_path, import_path)) continue;

        u32 len = 0;
        For (*it.files) {
            For (*it.decls) {
                if (flags & GETDECLS_PUBLIC_ONLY)
                    if (!(it.name != NULL && isupper(it.name[0])))
                        continue;
                len++;
            }
        }

        auto ret = alloc_list<Goresult>(len);

        For (*it.files) {
            For (*it.decls) {
                if (streq(it.name, "JSON"))
                    print("breakpoint");

                if (flags & GETDECLS_PUBLIC_ONLY)
                    if (!(it.name != NULL && isupper(it.name[0])))
                        continue;

                if (flags & GETDECLS_EXCLUDE_METHODS)
                    if (it.type == GODECL_FUNC && it.gotype->func_recv != NULL)
                        continue;

                auto ctx = alloc_object(Go_Ctx);
                ctx->import_path = import_path;
                ctx->filename = it.file;
                ret->append(make_goresult(&it, ctx));
            }
        }

        return ret;
    }

    return NULL;
}

Goresult *Go_Indexer::find_decl_in_package(ccstr id, ccstr import_path) {
    auto results = get_package_decls(import_path, GETDECLS_EXCLUDE_METHODS);
    if (results == NULL) return NULL;

    // in the future we can sort this shit
    For (*results)
        if (streq(it.decl->name, id))
            return &it;
    return NULL;
}

Gotype *Go_Indexer::expr_to_gotype(Ast_Node *expr) {
    if (expr->null) return NULL;

    Gotype *ret = NULL; // so we don't have to declare inside switch

    switch (expr->type) {
    case TS_UNARY_EXPRESSION:
        switch (expr->field(TSF_OPERATOR)->type) {
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
        // TODO
        break;

    case TS_CALL_EXPRESSION:
        ret = new_gotype(GOTYPE_LAZY_CALL);
        ret->lazy_call_base = expr_to_gotype(expr->field(TSF_FUNCTION));
        return ret;

    case TS_INDEX_EXPRESSION:
        ret = new_gotype(GOTYPE_LAZY_INDEX);
        ret->lazy_index_base = expr_to_gotype(expr->field(TSF_OPERAND));
        return ret;

    case TS_QUALIFIED_TYPE:
    case TS_SELECTOR_EXPRESSION:
        {
            auto operand_node = expr->field(expr->type == TS_QUALIFIED_TYPE ? TSF_PACKAGE : TSF_OPERAND);
            auto field_node = expr->field(expr->type == TS_QUALIFIED_TYPE ? TSF_NAME : TSF_FIELD);

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
        ret->lazy_id_pos = expr->start;
        return ret;
    }

    return NULL;
}

Goresult *Go_Indexer::evaluate_type(Gotype *gotype, Go_Ctx *ctx) {
    switch (gotype->type) {
    case GOTYPE_LAZY_INDEX:
        {
            auto res = evaluate_type(gotype->lazy_index_base, ctx);
            if (res == NULL) return NULL;

            res = resolve_type(res->gotype, res->ctx);
            res = unpointer_type(res->gotype, res->ctx);

            auto operand_type = res->gotype;
            switch (operand_type->type) {
            case GOTYPE_ARRAY: return evaluate_type(operand_type->array_base, res->ctx);
            case GOTYPE_SLICE: return evaluate_type(operand_type->slice_base, res->ctx);
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
            return NULL;
        }

    case GOTYPE_LAZY_CALL:
        {
            auto res = evaluate_type(gotype->lazy_call_base, ctx);
            if (res == NULL) break;

            res = resolve_type(res->gotype, res->ctx);
            res = unpointer_type(res->gotype, res->ctx);

            if (res->gotype->type != GOTYPE_FUNC) return NULL;

            auto result = res->gotype->func_sig.result;
            if (result == NULL) return NULL;

            auto ret = new_gotype(GOTYPE_MULTI);
            ret->multi_types = alloc_list<Gotype*>(result->len);
            For (*result) ret->multi_types->append(it.gotype);
            return make_goresult(ret, res->ctx);
        }

    case GOTYPE_LAZY_DEREFERENCE:
        {
            auto res = evaluate_type(gotype->lazy_dereference_base, ctx);
            if (res == NULL) return NULL;

            res = resolve_type(res->gotype, res->ctx);
            if (res == NULL) return NULL;

            if (res->gotype->type != GOTYPE_POINTER) return NULL;
            return evaluate_type(res->gotype->pointer_base, res->ctx);
        }

    case GOTYPE_LAZY_REFERENCE:
        {
            auto res = evaluate_type(gotype->lazy_reference_base, ctx);
            if (res == NULL) return NULL;

            auto type = new_gotype(GOTYPE_POINTER);
            type->pointer_base = res->gotype;
            return make_goresult(type, res->ctx);
        }

    case GOTYPE_LAZY_ARROW:
        {
            auto res = evaluate_type(gotype->lazy_arrow_base, ctx);
            if (res == NULL) return NULL;
            if (res->gotype->type != GOTYPE_CHAN) return NULL;

            return evaluate_type(res->gotype->chan_base, res->ctx);
        }

    case GOTYPE_LAZY_ID:
        {
            auto res = find_decl_of_id(gotype->lazy_id_name, gotype->lazy_id_pos, ctx);
            if (res == NULL) return NULL;
            if (res->decl->gotype == NULL) return NULL;
            return evaluate_type(res->decl->gotype, res->ctx);
        }

    case GOTYPE_LAZY_SEL:
        {
            do {
                if (gotype->lazy_sel_base->type != GOTYPE_LAZY_ID) break;

                auto base = gotype->lazy_sel_base;
                Go_Import *gi = NULL;

                auto decl_res = find_decl_of_id(base->lazy_id_name, base->lazy_id_pos, ctx, &gi);
                if (decl_res == NULL) break;
                if (gi == NULL) break;

                auto res = find_decl_in_package(gotype->lazy_sel_sel, gi->import_path);
                if (res == NULL) return NULL;

                auto ext_decl = res->decl;
                switch (ext_decl->type) {
                case GODECL_VAR:
                case GODECL_CONST:
                case GODECL_FUNC:
                    return evaluate_type(ext_decl->gotype, res->ctx);
                default:
                    return NULL;
                }
            } while (0);

            auto res = evaluate_type(gotype->lazy_sel_base, ctx);
            if (res == NULL) return NULL;

            auto resolved_res = resolve_type(res->gotype, res->ctx);
            resolved_res = unpointer_type(resolved_res->gotype, resolved_res->ctx);

            List<Goresult> results;
            results.init();
            list_fields_and_methods(res, resolved_res, &results);

            For (results)
                if (streq(it.decl->name, gotype->lazy_sel_sel))
                    return evaluate_type(it.decl->gotype, it.ctx);
        }
        break;

    case GOTYPE_LAZY_ONE_OF_MULTI:
        {
            auto res = evaluate_type(gotype->lazy_one_of_multi_base, ctx);
            if (res == NULL) return NULL;

            if (res->gotype->type != GOTYPE_MULTI) {
                // means we got foo := bar (lhs.len == 1, rhs.len == 1), just return type of bar here
                if (gotype->lazy_one_of_multi_is_single)
                    return res;
                return NULL;
            }

            auto ret = res->gotype->multi_types->at(gotype->lazy_one_of_multi_index);
            return evaluate_type(ret, res->ctx);

            // TODO: there's some other logic here around other multi types
            /*
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
            */
        }

    default: return make_goresult(gotype, ctx);
    }
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
            auto import_path = find_import_path_referred_to_by_id(type->sel_name, ctx);
            if (import_path == NULL) break;

            auto res = find_decl_in_package(type->sel_sel, import_path);
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
    case GOTYPE_LAZY_INDEX:
        ret->lazy_index_base = copy_object(lazy_index_base);
        break;
    case GOTYPE_LAZY_CALL:
        ret->lazy_call_base = copy_object(lazy_call_base);
        break;
    case GOTYPE_LAZY_DEREFERENCE:
        ret->lazy_dereference_base = copy_object(lazy_dereference_base);
        break;
    case GOTYPE_LAZY_REFERENCE:
        ret->lazy_reference_base = copy_object(lazy_reference_base);
        break;
    case GOTYPE_LAZY_ARROW:
        ret->lazy_arrow_base = copy_object(lazy_arrow_base);
        break;
    case GOTYPE_LAZY_ID:
        ret->lazy_id_name = our_strcpy(lazy_id_name);
        break;
    case GOTYPE_LAZY_SEL:
        ret->lazy_sel_base = copy_object(lazy_sel_base);
        ret->lazy_sel_sel = our_strcpy(lazy_sel_sel);
        break;
    case GOTYPE_LAZY_ONE_OF_MULTI:
        ret->lazy_one_of_multi_base = copy_object(lazy_one_of_multi_base);
        break;
    }
    return ret;
}

Go_Package *Go_Package::copy() {
    auto ret = clone(this);
    ret->import_path = our_strcpy(import_path);
    ret->package_name = our_strcpy(package_name);

    // leave files alone, since they have their own pool.
    // just copy the list over, and re-point the list pools
    auto new_files = alloc_object(List<Go_File>);
    new_files->init(LIST_POOL, max(ret->files->len, 1));
    For (*ret->files) {
        auto new_file = new_files->append();
        memcpy(new_file, &it, sizeof(Go_File));

        new_file->scope_ops->pool = &new_file->pool;
        new_file->decls->pool = &new_file->pool;
        new_file->imports->pool = &new_file->pool;
    }
    ret->files = new_files;

    return ret;
}

Go_Import *Go_Import::copy() {
    auto ret = clone(this);
    ret->package_name = our_strcpy(package_name);
    ret->import_path = our_strcpy(import_path);
    ret->decl = copy_object(decl);
    return ret;
}

Go_Scope_Op *Go_Scope_Op::copy() {
    auto ret = clone(this);
    if (type == GSOP_DECL)
        ret->decl = copy_object(decl);
    return ret;
}

/*
Go_File *Go_File::copy() {
    auto ret = clone(this);
    ret->filename = our_strcpy(filename);
    ret->scope_ops = copy_list(scope_ops);
    ret->decls = copy_list(decls);
    ret->imports = copy_list(imports);
    return ret;
}
*/

Go_Index *Go_Index::copy() {
    auto ret = clone(this);
    ret->current_path = our_strcpy(current_path);
    ret->current_import_path = our_strcpy(current_import_path);
    ret->packages = copy_list(packages);
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

void Go_Import::read(Index_Stream *s) {
    READ_STR(package_name);
    READ_STR(import_path);
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
        break;
    case GOTYPE_LAZY_INDEX:
        READ_OBJ(lazy_index_base);
        break;
    case GOTYPE_LAZY_CALL:
        READ_OBJ(lazy_call_base);
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

void Go_Import::write(Index_Stream *s) {
    WRITE_STR(package_name);
    WRITE_STR(import_path);
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
        break;
    case GOTYPE_LAZY_INDEX:
        WRITE_OBJ(lazy_index_base);
        break;
    case GOTYPE_LAZY_CALL:
        WRITE_OBJ(lazy_call_base);
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

ccstr gotype_type_str(Gotype_Type type) {
    switch (type) {
    define_str_case(GOTYPE_ID);
    define_str_case(GOTYPE_SEL);
    define_str_case(GOTYPE_MAP);
    define_str_case(GOTYPE_STRUCT);
    define_str_case(GOTYPE_INTERFACE);
    define_str_case(GOTYPE_POINTER);
    define_str_case(GOTYPE_FUNC);
    define_str_case(GOTYPE_SLICE);
    define_str_case(GOTYPE_ARRAY);
    define_str_case(GOTYPE_CHAN);
    define_str_case(GOTYPE_MULTI);
    define_str_case(GOTYPE_VARIADIC);
    define_str_case(GOTYPE_ASSERTION);
    define_str_case(GOTYPE_RANGE);
    define_str_case(GOTYPE_LAZY_INDEX);
    define_str_case(GOTYPE_LAZY_CALL);
    define_str_case(GOTYPE_LAZY_DEREFERENCE);
    define_str_case(GOTYPE_LAZY_REFERENCE);
    define_str_case(GOTYPE_LAZY_ARROW);
    define_str_case(GOTYPE_LAZY_ID);
    define_str_case(GOTYPE_LAZY_SEL);
    define_str_case(GOTYPE_LAZY_ONE_OF_MULTI);
    }
    return NULL;
}

ccstr godecl_type_str(Godecl_Type type) {
    switch (type) {
    define_str_case(GODECL_IMPORT);
    define_str_case(GODECL_VAR);
    define_str_case(GODECL_CONST);
    define_str_case(GODECL_TYPE);
    define_str_case(GODECL_FUNC); // should we have GODECL_METHOD too? can just check gotype->func_recv
    define_str_case(GODECL_FIELD);
    define_str_case(GODECL_SHORTVAR);
    }
    return NULL;
}
