#include "editor.hpp"
#include "os.hpp"
#include "world.hpp"
#include "ui.hpp"
#include "go.hpp"
#include "fzy_match.h"

void Editor::raw_move_cursor(cur2 c) {
    cur = c;

#if 0
    if (reset_autocomplete)
        autocomplete.results = NULL;
#endif

    if (cur.x < view.x)
        view.x = cur.x;
    if (cur.x >= view.x + view.w)
        view.x = cur.x - view.w + 1;
    if (cur.y < view.y)
        view.y = cur.y;
    if (cur.y >= view.y + view.h)
        view.y = cur.y - view.h + 1;
}

void Editor::move_cursor(cur2 c) {
    raw_move_cursor(c);

    if (world.use_nvim) {
        auto& nv = world.nvim;

        {
            auto msgid = nv.start_request_message("nvim_win_set_cursor", 2);
            nv.save_request(NVIM_REQ_SET_CURSOR, msgid, id);
            nv.writer.write_int(nvim_data.win_id);
            {
                nv.writer.write_array(2);
                nv.writer.write_int(c.y + 1);
                nv.writer.write_int(c.x);
            }
            nv.end_message();
        }
    }
}

void Editor::reset_state() {
    cur.x = 0;
    cur.y = 0;
}

bool Editor::load_file(ccstr new_filepath) {
    if (buf.initialized)
        buf.cleanup();
    buf.init(&mem);

    FILE* f = NULL;
    if (new_filepath != NULL) {
        u32 result = get_normalized_path(new_filepath, filepath, _countof(filepath));
        if (result == 0)
            return error("unable to normalize filepath"), false;
        if (result + 1 > _countof(filepath))
            return error("filepath too big"), false;
        f = fopen(filepath, "r");
        if (f == NULL)
            return error("unable to open %s for reading: %s", filepath, strerror(errno)), false;
    } else {
        is_untitled = true;
    }

    reset_state();
    buf.read(f);
    fclose(f);

    if (world.use_nvim) {
        auto& nv = world.nvim;
        auto msgid = nv.start_request_message("nvim_create_buf", 2);
        nv.save_request(NVIM_REQ_CREATE_BUF, msgid, id);
        nv.writer.write_bool(false);
        nv.writer.write_bool(true);
        nv.end_message();
    }

    return true;
}

Editor* Pane::open_empty_editor() {
    auto ed = editors.append();
    ed->init();
    ed->load_file(NULL);
    return focus_editor_by_index(editors.len - 1);
}

bool Editor::save_file() {
    FILE* f = fopen(filepath, "w");
    if (f == NULL)
        return error("unable to open %s for writing", filepath), false;
    defer { fclose(f); };

    buf.write(f);
    return true;
}

i32 Editor::cur_to_offset(cur2 c) {
    return buf.cur_to_offset(c);
}

i32 Editor::cur_to_offset() { return cur_to_offset(cur); }

cur2 Editor::offset_to_cur(i32 offset) {
    for (i32 y = 0; y < buf.lines.len; y++) {
        auto len = buf.lines[y].len;
        if (offset <= len)
            return new_cur2(offset, y);
        offset -= (len + 1);
    }
    return new_cur2(-1, -1);
}

int Editor::get_indent_of_line(int y) {
    auto& line = buf.lines[y];

    int indent = 0;
    for (; indent < line.len; indent++)
        if (line[indent] != '\t')
            break;
    return indent;
}

Buffer_It Editor::iter() { return iter(cur); }
Buffer_It Editor::iter(cur2 _cur) { return buf.iter(_cur); }

void Pane::init() {
    ptr0(this);
    editors.init(LIST_MALLOC, 8);
    current_editor = -1;
}

void Pane::cleanup() {
    For (editors) {
        it.cleanup();
    }
    editors.cleanup();
}

bool check_file_dimensions(ccstr path) {
    File f;
    if (f.init(path, FILE_MODE_READ, FILE_OPEN_EXISTING) != FILE_RESULT_SUCCESS) {
        print("Error opening file while checknig file dimensions: %s", get_last_error());
        return false;
    }
    defer { f.cleanup(); };

    u32 line_count = 1;
    u32 max_line_len = 0;
    u32 line_len = 0;
    char next_ch = 0;

    while (f.read(&next_ch, 1)) {
        if (next_ch == '\n') {
            if (line_count++ > 65000)
                return false;
            line_len = 0;
        } else if (line_len++ > 65000) {
            return false;
        }
    }
    return true;
}

Editor* Pane::focus_editor(ccstr path) {
    u32 i = 0;
    For (editors) {
        if (are_filepaths_same_file(path, it.filepath))
            return focus_editor_by_index(i);
        i++;
    }

    if (!check_file_dimensions(path)) {
        error("Unfortunately, we currently don't support files with more than 65,000 lines, or lines with more than 65,000 characters.");
        return NULL;
    }

    auto ed = editors.append();
    ed->init();
    ed->load_file(path);
    return focus_editor_by_index(editors.len - 1);
}

Editor *Pane::focus_editor_by_index(u32 idx) {
    current_editor = idx;

    auto &editor = editors[idx];

    world.nvim_data.waiting_focus_window = editor.id;

    if (editor.nvim_data.win_id != 0)
        world.nvim.set_current_window(&editor);

    return &editor;
}

Editor* Pane::get_current_editor() {
    if (editors.len == 0) return NULL;
    if (current_editor == -1) return NULL;

    return &editors[current_editor];
}

void Workspace::init() {
    ptr0(this);

    resizing_pane = -1;

    panes.init(LIST_FIXED, _countof(_panes), _panes);

#if 1
    auto newpath = (cstr)our_strcpy("c:/users/brandon/ide/helper");
    strcpy_safe(path, _countof(path), normalize_path_separator(newpath));
#else
    Select_File_Opts opts;
    opts.buf = path;
    opts.bufsize = _countof(path);
    opts.folder = true;
    opts.save = false;
    let_user_select_file(&opts);
#endif

    git_buf root = {0};
    if (git_repository_discover(&root, path, 0, NULL) == 0) {
        git_repository_open(&git_repo, root.ptr);
        git_buf_free(&root);
    }

    // try to parse gomod
    {
        SCOPED_FRAME();
        auto gomod_path = path_join(path, "go.mod");

        if (check_path(gomod_path) == CPR_FILE) {
            gomod_exists = true;
            parse_gomod_file(gomod_path);
        }
    }
}

bool Workspace::parse_gomod_file(ccstr path) {
    world.gomod_parser_mem.cleanup();
    world.gomod_parser_mem.init("gomod_parser_mem");
    SCOPED_MEM(&world.gomod_parser_mem);

    auto ef = read_entire_file(path);
    if (ef == NULL) return false;
    defer { free_entire_file(ef); };

    Parser_It it;
    it.init(ef);

    Gomod_Parser p;
    p.it = &it;

    gomod_info.directives.cleanup();
    ptr0(&gomod_info);
    gomod_info.directives.init(LIST_POOL, 128);

    p.parse(&gomod_info);
    return true;
}

void Workspace::activate_pane(u32 idx) {
    if (idx > panes.len) return;

    if (idx == panes.len) {
        auto panes_width = ui.get_panes_area().w;

        auto new_width = panes_width - (panes_width / (panes.len + 1));
        ui.resize_panes_proportionally(new_width);

        auto pane = panes.append();
        pane->init();
        pane->width = panes_width / panes.len;

        ui.recalculate_view_sizes();
    }

    current_pane = idx;

    if (world.use_nvim) {
        auto pane = get_current_pane();
        auto editor = pane->get_current_editor();
        if (editor != NULL)
            world.nvim.set_current_window(editor);
    }
}

Pane* Workspace::get_current_pane() {
    if (panes.len == 0) return NULL;

    return &panes[current_pane];
}

bool Editor::is_nvim_ready() {
    return world.nvim_data.is_ui_attached
        && nvim_data.is_buf_attached
        && (nvim_data.buf_id != 0)
        && (nvim_data.win_id != 0);
}

void Editor::init() {
    ptr0(this);
    id = ++world.next_editor_id;
    mem.init("editor mem");
}

void Editor::cleanup() {
    mem.cleanup();
}

void Editor::trigger_autocomplete(bool triggered_by_dot) {
    ptr0(&autocomplete);

    SCOPED_MEM(&world.index.main_thread_mem);
    defer { world.index.main_thread_mem.reset(); };

    Autocomplete ac;
    world.autocomplete_mem.reset();

    if (!world.index.autocomplete(filepath, cur, triggered_by_dot, &ac)) return;

    {
        SCOPED_MEM(&world.autocomplete_mem);
        autocomplete.filtered_results = alloc_list<int>(ac.results->len);
        filter_autocomplete_results(&ac);
    }

    memcpy(&autocomplete.ac, &ac, sizeof(ac));
}

Ast* Editor::parse_autocomplete_id(Autocomplete* ac) {
    auto pos = ac->keyword_start_position;
    if (ac->type == AUTOCOMPLETE_STRUCT_FIELDS || ac->type == AUTOCOMPLETE_PACKAGE_EXPORTS)
        pos.x++;

    Ast* id = NULL;
    with_parser_at_location(filepath, pos, [&](Parser* p) { id = p->parse_id(); });
    return id;
}

void Editor::filter_autocomplete_results(Autocomplete* ac) {
    bool prefix_found = false;

    do {
        SCOPED_MEM(&world.index.main_thread_mem);
        defer { world.index.main_thread_mem.reset(); };

        auto id = parse_autocomplete_id(ac);
        if (id == NULL) break;

        if (id->id.bad) {
            autocomplete.prefix[0] = '\0';
        } else {
            if (cur < id->start) {
                // happens if we're at "foo.    bar"
                //                           ^
                // there's whitespace between period and sel, and we're in it
                autocomplete.prefix[0] = '\0';
            } else if (cur > id->end) {
                break;  // this shouldn't happen
            } else {
                auto prefix_len = cur.x - id->start.x;
                prefix_len = min(prefix_len, _countof(autocomplete.prefix) - 1);
                if (prefix_len == 0) {
                    autocomplete.prefix[0] = '\0';
                } else {
                    strncpy(autocomplete.prefix, id->id.lit, prefix_len);
                    autocomplete.prefix[prefix_len] = '\0';
                }
            }
        }

        prefix_found = true;
    } while (0);

    if (!prefix_found) return;

    auto prefix = autocomplete.prefix;

    autocomplete.filtered_results->len = 0;
    auto results = ac->results;
    for (int i = 0; i < results->len; i++)
        if (fzy_has_match(prefix, results->at(i).name))
            autocomplete.filtered_results->append(i);

    autocomplete.filtered_results->sort([&](int *ia, int *ib) -> int {
        auto a = fzy_match(prefix, ac->results->at(*ia).name);
        auto b = fzy_match(prefix, ac->results->at(*ib).name);

        // reverse
        return a < b ? 1 : (a > b ? -1 : 0);
    });
}

struct Type_Renderer : public Text_Renderer {
    void write_type(Ast* t) {
        switch (t->type) {
            case AST_SELECTOR_EXPR:
                write_type(t->selector_expr.x);
                writechar('.');
                write_type(t->selector_expr.sel);
                break;
            case AST_ELLIPSIS:
                write("...");
                write_type(t->ellipsis.type);
                break;
            case AST_ID:
                write("%s", t->id.lit);
                break;
            case AST_SLICE_TYPE:
                write("[]");
                write_type(t->slice_type.base_type);
                break;
            case AST_ARRAY_TYPE:
                write("[]");
                write_type(t->array_type.base_type);
                break;
            case AST_POINTER_TYPE:
                write("*");
                write_type(t->pointer_type.base_type);
                break;
            case AST_MAP_TYPE:
                write("map[");
                write_type(t->map_type.key_type);
                write("]");
                write_type(t->map_type.value_type);
                break;
            case AST_CHAN_TYPE:
                if (t->chan_type.direction == AST_CHAN_RECV)
                    write("<-");
                write("chan");
                write_type(t->chan_type.base_type);
                if (t->chan_type.direction == AST_CHAN_SEND)
                    write("<-");
                break;
            case AST_STRUCT_TYPE:
                write("struct");
                break;
            case AST_INTERFACE_TYPE:
                write("interface");
                break;
            case AST_FUNC_TYPE:
                write("func");
                break;
        }
    }
};

void Editor::trigger_parameter_hint(bool triggered_by_paren) {
    parameter_hint.params = NULL;

    auto cursor = cur;
    auto out = &parameter_hint;

    SCOPED_MEM(&world.index.main_thread_mem);
    defer { world.index.main_thread_mem.reset(); };

    auto hint = world.index.parameter_hint(filepath, cursor, triggered_by_paren);
    if (hint == NULL) return;

    SCOPED_MEM(&world.parameter_hint_mem);
    world.parameter_hint_mem.reset();

    auto fields = hint->signature->signature.params->parameters.fields;
    auto params = alloc_list<ccstr>(fields->list.len);

    For (fields->list) {
        Type_Renderer rend;
        rend.init();

        auto& ids = it->field.ids->list;
        for (int i = 0; i < ids.len; i++) {
            rend.writestr(ids[i]->id.lit);
            if (i < ids.len - 1)
                rend.write(", ");
        }
        rend.writechar(' ');
        rend.write_type(it->field.type);
        params->append(rend.finish());
    }

    out->start = hint->call_args->start;
    out->params = params;
}

void Editor::type_char(char ch) {
    uchar uch = ch;
    buf.insert(cur, &uch, 1);
    raw_move_cursor(buf.inc_cur(cur));
}

void Editor::update_autocomplete() {
    if (autocomplete.ac.results != NULL) {
        auto& ac = autocomplete.ac;

        auto passed_start = [&]() {
            auto& ac = autocomplete.ac;
            if (ac.type == AUTOCOMPLETE_PACKAGE_EXPORTS || ac.type == AUTOCOMPLETE_STRUCT_FIELDS)
                return cur <= ac.keyword_start_position;
            return cur < ac.keyword_start_position;
        };

        auto passed_end = [&]() {
            auto id = parse_autocomplete_id(&autocomplete.ac);
            return id != NULL && cur > id->end;
        };

        if (passed_start() || passed_end())
            autocomplete.ac.results = NULL;
        else
            filter_autocomplete_results(&autocomplete.ac);
    }
}


void Editor::update_parameter_hint() {
    // reset parameter hint when cursor goes before hint start
    auto& hint = parameter_hint;
    if (hint.params != NULL) {
        auto should_close_hints = [&]() {
            if (cur < hint.start) return true;

            bool ret = false;
            with_parser_at_location(filepath, hint.start, [&](Parser* p) {
                auto call_args = p->parse_call_args();
                ret = (cur >= call_args->end);
            });
            return ret;
        };

        if (should_close_hints()) {
            hint.params = NULL;
        } else {
            hint.current_param = -1;
            with_parser_at_location(filepath, hint.start, [&](Parser* p) {
                auto call_args = p->parse_call_args();
                u32 idx = 0;
                For (call_args->call_args.args->list) {
                    // When parser parses something invalid, most commonly lack of
                    // TOK_RPAREN, like when user starts typing "foo(", it adds NULL to
                    // args; just ignore it.
                    if (it == NULL) continue;

                    // if (locate_pos_relative_to_ast(cur, it) == 0) {
                    if (cur <= it->end) {
                        hint.current_param = idx;
                        break;
                    }
                    idx++;
                }
            });
        }
    }
}

void Editor::type_char_in_insert_mode(char ch) {
    type_char(ch);

    switch (ch) {
        case '.': trigger_autocomplete(true); break;
        case '(': trigger_parameter_hint(true); break;
    }

    update_autocomplete();
    update_parameter_hint();
}
