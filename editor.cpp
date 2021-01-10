#include "editor.hpp"
#include "os.hpp"
#include "world.hpp"
#include "ui.hpp"

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
    buf.init();

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

    if (world.use_nvim) {
        // nvim_data.status = ENS_BUF_PENDING;
        nvim_data.file_handle = f;

        auto& nv = world.nvim;

        {
            auto msgid = nv.start_request_message("nvim_create_buf", 2);
            nv.save_request(NVIM_REQ_CREATE_BUF, msgid, id);

            nv.writer.write_bool(false);
            nv.writer.write_bool(true);
            nv.end_message();
        }
    } else if (f != NULL) {
        buf.read(f);
        fclose(f);
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
    if (!f.init(path, FILE_MODE_READ, FILE_OPEN_EXISTING)) {
        print("error: %s", get_last_error());
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
    auto newpath = (cstr)our_strcpy("c:/users/brandon/life");
    strcpy_safe(path, _countof(path), normalize_path_separator(newpath));
#else
    Select_File_Opts opts;
    opts.buf = path;
    opts.bufsize = _countof(path);
    opts.folder = true;
    opts.save = false;
    let_user_select_file(&opts);
#endif

    // try to parse gomod
    {
        SCOPED_FRAME();
        auto gomod_path = path_join(path, "go.mod");

        if (check_path(gomod_path) == CPR_FILE) {
            go_mod_exists = true;
            parse_gomod_file(gomod_path);
        }
    }
}

bool Workspace::parse_gomod_file(ccstr path) {
    world.gomod_parser_arena.cleanup();
    world.gomod_parser_arena.init();
    SCOPED_ARENA(&world.gomod_parser_arena);

    FILE* f = fopen(path, "r");
    if (f == NULL) return false;
    defer { fclose(f); };

    Parser_It it;
    it.type = IT_FILE;
    it.file_params.file = f;

    Go_Mod_Parser p;
    p.it = &it;

    go_mod_info.directives.cleanup();
    ptr0(&go_mod_info);
    go_mod_info.directives.init(LIST_MALLOC, 128);

    p.parse(&go_mod_info);
    return true;
}

void Workspace::activate_pane(u32 idx) {
    if (idx > panes.len) return;

    if (idx == panes.len) {
        float total = 0;
        auto widths = alloc_array(float, panes.cap);

        {
            u32 i = 0;
            For (panes) {
                total += it.width;
                widths[i++] = it.width;
            }
        }

        auto pane = panes.append();
        pane->init();
        pane->width = world.display_size.x / panes.len;

        if (panes.len > 1) {
            UI ui;
            ui.init();

            float new_total = world.display_size.x - pane->width;
            for (u32 i = 0; i < panes.len - 1; i++)
                panes[i].width = widths[i] / total * new_total;
        }
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
    arena.init();
    highlights_lock.init();
}

void Editor::cleanup() {
    arena.cleanup();
    highlights->cleanup();
    highlights_lock.cleanup();
}

ccstr hl_type_str(HlType type) {
    switch (type) {
        define_str_case(HL_NONE);
        define_str_case(HL_COMMENT);
        define_str_case(HL_STATEMENT);
        define_str_case(HL_TYPE);
        define_str_case(HL_CONSTANT);
    }
    return NULL;
}

typedef fn<void(Parser*)> parser_cb;

void with_parser_at_location(ccstr filepath, cur2 location, parser_cb cb) {
    auto iter = file_loader(filepath);
    defer{ iter->cleanup(); };
    if (iter->it == NULL) return;
    iter->it->set_pos(location);

    Parser p;
    p.init(iter->it);
    defer{ p.cleanup(); };

    cb(&p);
}

void Editor::on_type() {
    auto last_character = buf.iter(buf.dec_cur(cur)).peek();

    /*
    I think the plan is to build this out in stages:

        stage 1 (noob): given a current buffer state and cursor position, set up
        autocomplete/hints to have the correct state.

    This seems like it'd be kind of slow. Probably not an issue; my computer
    spins THREE BILLION TIMES a second. But ideally we'd have:

        stage 2 (advanced): take into account previous state and make only the
        changes we need from the last character.
    */

    switch (last_character) {
        case '.':
            // TODO: what happens if we backspace into a '.'? e.g.
            //     foo.bar|
            // and we backspace the r, a, and b
            // we probably shouldn't trigger autocomplete 
            // though honestly it's probably fine if we can get autocomplete faster lol
            trigger_autocomplete(true);
            break;
        case '(':
            trigger_parameter_hint(true);
            break;
    }

    if (autocomplete.ac.results != NULL) {
        auto& ac = autocomplete.ac;

        auto passed_start = [&]() {
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

    // reset parameter hint when cursor goes before hint start
    auto& hint = parameter_hint;
    if (hint.params != NULL) {
        auto should_close_hints = [&]() {
            if (cur < hint.start) return true;

            bool ret = false;
            with_parser_at_location(filepath, hint.start, [&](Parser* p) {
                auto call_args = p->parse_call_args();
                ret = (cur > call_args->end);
            });
            return ret;
        };

        if (should_close_hints()) {
            hint.params = NULL;
        } else {
            with_parser_at_location(filepath, hint.start, [&](Parser* p) {
                auto call_args = p->parse_call_args();
                u32 idx = 0;
                For (call_args->call_args.args->list) {
                    // When parser parses something invalid, most commonly lack of
                    // TOK_RPAREN, like when user starts typing "foo(", it adds NULL to
                    // args; just ignore it.
                    if (it == NULL) continue;

                    if (locate_pos_relative_to_ast(cur, it) == 0) {
                        hint.current_param = idx;
                        break;
                    }
                    idx++;
                }
            });
        }
    }
}

// OK, I think the most likely cause here is world.parser_mem being reused.
// investigate that shit

void Editor::trigger_autocomplete(bool triggered_by_dot) {
    ptr0(&autocomplete);

    SCOPED_MEM(&world.parser_mem);
    SCOPED_FRAME();

    Golang go;
    Autocomplete ac;

    world.autocomplete_mem.sp = 0;

    if (!go.autocomplete(filepath, cur, triggered_by_dot, &ac)) return;

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
        SCOPED_MEM(&world.parser_mem);
        SCOPED_FRAME();

        auto id = parse_autocomplete_id(ac);
        if (id == NULL) break;

        if (id->id.bad) {
            autocomplete.prefix[0] = '\0';
        } else {
            assert(id->start.y == cur.y);
            auto prefix_len = cur.x - id->start.x;

            prefix_len = min(prefix_len, _countof(autocomplete.prefix) - 1);
            if (prefix_len == 0)
                autocomplete.prefix[0] = '\0';
            else
                strncpy(autocomplete.prefix, id->id.lit, prefix_len);
        }

        prefix_found = true;
    } while (0);

    if (!prefix_found) return;

    auto match = [&](AC_Result& res) -> bool {
        auto prefix_len = strlen(autocomplete.prefix);
        if (prefix_len == 0) return true;

        int pi = 0;
        for (ccstr p = res.name; *p != '\0'; p++)
            if (tolower(*p) == tolower(autocomplete.prefix[pi]))
                if (++pi == prefix_len)
                    return true;
        return false;
    };

    autocomplete.filtered_results->len = 0;
    auto results = ac->results;
    for (int i = 0; i < results->len; i++)
        if (match(results->at(i)))
            autocomplete.filtered_results->append(i);
}

struct Type_Renderer : public Text_Renderer {
    void write_type(Ast* t) {
        switch (t->type) {
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

    SCOPED_MEM(&world.parser_mem);
    SCOPED_FRAME();

    Golang go;
    auto hint = go.parameter_hint(filepath, cursor, triggered_by_paren);
    if (hint == NULL) return;

    SCOPED_MEM(&world.parameter_hint_mem);

    world.parameter_hint_mem.sp = 0;

    auto fields = hint->signature->signature.params->parameters.fields;
    auto params = alloc_list<ccstr>(fields->list.len);

    For (fields->list) {
        Type_Renderer rend;
        rend.init();

        auto& ids = it->field.ids->list;
        for (int i = 0; i < ids.len; i++) {
            rend.write("%s ", ids[i]->id.lit);
            if (i < ids.len - 1)
                rend.write(", ");
        }
        rend.write_type(it->field.type);
        params->append(rend.finish());
    }

    out->start = hint->call_args->start;
    out->params = params;
}
