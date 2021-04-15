#include "editor.hpp"
#include "os.hpp"
#include "world.hpp"
#include "ui.hpp"
#include "go.hpp"
#include "fzy_match.h"
#include "tree_sitter_crap.hpp"

void Editor::raw_move_cursor(cur2 c) {
    if (c.y == -1) c = buf.offset_to_cur(c.x);

    if (c.y < 0 || c.y >= buf.lines.len) return;

    cur = c;

    auto& line = buf.lines[c.y];

    u32 vx = 0;
    for (u32 i = 0; i < c.x; i++)
        vx += line[i] == '\t' ? TAB_SIZE : 1;

    if (vx < view.x)
        view.x = vx;
    if (vx >= view.x + view.w)
        view.x = vx - view.w + 1;
    if (cur.y < view.y)
        view.y = cur.y;
    if (cur.y >= view.y + view.h)
        view.y = cur.y - view.h + 1;
}

void Editor::update_lines(int firstline, int lastline, List<uchar*> *new_lines, List<s32> *line_lengths) {
    if (lastline == -1) lastline = buf.lines.len;

    auto start_cur = new_cur2(0, (i32)firstline);
    auto old_end_cur = new_cur2(0, (i32)lastline);
    if (lastline == buf.lines.len) {
        start_cur = buf.dec_cur(start_cur);
        old_end_cur = new_cur2((i32)buf.lines.last()->len, (i32)buf.lines.len - 1);
    }

    TSInputEdit tsedit = {0};

    if (is_go_file && tree != NULL) {
        tsedit.start_byte = cur_to_offset(start_cur);
        tsedit.start_point = cur_to_tspoint(start_cur);
        tsedit.old_end_byte = cur_to_offset(old_end_cur);
        tsedit.old_end_point = cur_to_tspoint(old_end_cur);
    }

    buf.delete_lines(firstline, lastline);
    for (u32 i = 0; i < new_lines->len; i++) {
        auto line = new_lines->at(i);
        auto len = line_lengths->at(i);
        buf.insert_line(firstline + i, line, len);
    }

    if (is_go_file) {
        if (tree != NULL) {
            auto new_end_cur = new_cur2(0, firstline + new_lines->len);
            if (firstline + new_lines->len == buf.lines.len) {
                if (buf.lines.len == 0)
                    new_end_cur = new_cur2(0, 0);
                else
                    new_end_cur = new_cur2((i32)buf.lines.last()->len, (i32)buf.lines.len - 1);
            }

            tsedit.new_end_byte = cur_to_offset(new_end_cur);
            tsedit.new_end_point = cur_to_tspoint(new_end_cur);
            ts_tree_edit(tree, &tsedit);
        }

        update_tree();
    }
}

void Editor::update_tree() {
    if (!is_go_file) return;

    TSInput input;
    input.payload = this;
    input.encoding = TSInputEncodingUTF8;

    input.read = [](void *p, uint32_t off, TSPoint pos, uint32_t *read) -> const char* {
        Editor *editor = (Editor*)p;
        auto it = editor->buf.iter(editor->offset_to_cur(off));
        u32 n = 0;

        while (!it.eof()) {
            auto uch = it.next();
            if (uch == 0) {
                break;
            }
            auto size = uchar_size(uch);
            if (n + size + 1 > _countof(tsinput_buffer)) break;
            uchar_to_cstr(uch, &editor->tsinput_buffer[n], &size);
            n += size;
        }

        *read = n;
        editor->tsinput_buffer[n] = '\0';
        return editor->tsinput_buffer;
    };

    tree = ts_parser_parse(parser, tree, input);
    index_dirty = true;
}

void Editor::move_cursor(cur2 c, int save_nvim_request) {
    if (world.nvim.mode == VI_INSERT) {
        nvim_data.waiting_for_move_cursor = true;
        nvim_data.move_cursor_to = c;
        nvim_data.move_cursor_save_nvim_request = save_nvim_request;

        world.nvim.start_request_message("nvim_input", 1);
        world.nvim.writer.write_string("<Esc>");
        world.nvim.end_message();
        return;
    }

    raw_move_cursor(c);

    if (world.use_nvim) {
        auto& nv = world.nvim;
        auto msgid = nv.start_request_message("nvim_win_set_cursor", 2);
        if (save_nvim_request != 0)
            nv.save_request((Nvim_Request_Type)save_nvim_request, msgid, id);
        nv.writer.write_int(nvim_data.win_id);
        {
            nv.writer.write_array(2);
            nv.writer.write_int(c.y + 1);
            nv.writer.write_int(c.x);
        }
        nv.end_message();
    }
}

void Editor::reset_state() {
    cur.x = 0;
    cur.y = 0;
}

// I'm just going to make this a separate function from load_file(), since it is doing mostly a different thing.
void Editor::reload_file() {
    auto f = fopen(filepath, "r");
    if (f == NULL) {
        error("unable to open %s for reading: %s", filepath, strerror(errno));
        return;
    }
    defer { fclose(f); };

    TSInputEdit tsedit = {0};

    if (tree != NULL) {
        cur2 start = new_cur2(0, 0);
        cur2 old_end = new_cur2((i32)buf.lines.last()->len, buf.lines.len-1);

        tsedit.start_byte = cur_to_offset(start);
        tsedit.start_point = cur_to_tspoint(start);
        tsedit.old_end_byte = cur_to_offset(old_end);
        tsedit.old_end_point = cur_to_tspoint(old_end);
    }

    if (buf.initialized)
        buf.cleanup();
    buf.init(&mem);
    buf.read(f);

    if (tree != NULL) {
        cur2 new_end = new_cur2((i32)buf.lines.last()->len, buf.lines.len-1);

        tsedit.new_end_byte = cur_to_offset(new_end);
        tsedit.new_end_point = cur_to_tspoint(new_end);

        ts_tree_edit(tree, &tsedit);
    }

    update_tree();
}

bool Editor::load_file(ccstr new_filepath) {
    reset_state();

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
        defer { fclose(f); };
        buf.read(f);
    } else {
        is_untitled = true;
        uchar tmp = 0;
        buf.insert_line(0, &tmp, 0);
    }

    // TODO: when untitled file is saved, set is_go_file
    is_go_file = (new_filepath != NULL && str_ends_with(new_filepath, ".go"));

    if (world.use_nvim) {
        auto& nv = world.nvim;
        auto msgid = nv.start_request_message("nvim_create_buf", 2);
        nv.save_request(NVIM_REQ_CREATE_BUF, msgid, id);
        nv.writer.write_bool(false);
        nv.writer.write_bool(true);
        nv.end_message();
    }

    update_tree();
    return true;
}

Editor* Pane::open_empty_editor() {
    auto ed = editors.append();
    ed->init();
    ed->load_file(NULL);
    ui.recalculate_view_sizes(true);
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

Editor* Pane::focus_editor(ccstr path) {
    return focus_editor(path, new_cur2(-1, -1));
}

Editor* Pane::focus_editor(ccstr path, cur2 pos) {
    u32 i = 0;
    For (editors) {
        if (are_filepaths_same_file(path, it.filepath))
            return focus_editor_by_index(i, pos);
        i++;
    }

    /*
    TODO: check this elsewhere
    if (!check_file_dimensions(path)) {
        error("Unfortunately, we currently don't support files with more than 65,000 lines, or lines with more than 65,000 characters.");
        return NULL;
    }
    */

    auto ed = editors.append();
    ed->init();
    ed->load_file(path);
    ui.recalculate_view_sizes(true);
    return focus_editor_by_index(editors.len - 1, pos);
}

Editor *Pane::focus_editor_by_index(u32 idx) {
    return focus_editor_by_index(idx, new_cur2(-1, -1));
}

bool Editor::trigger_escape() {
    bool handled = false;

    if (autocomplete.ac.results != NULL) {
        handled = true;
        autocomplete.ac.results = NULL;
    }

    if (parameter_hint.gotype != NULL) {
        handled = true;
        parameter_hint.gotype = NULL;
    }

    if (world.nvim.mode == VI_INSERT) {
        auto& nv = world.nvim;
        auto msgid = nv.start_request_message("nvim_buf_get_changedtick", 1);
        nv.save_request(NVIM_REQ_POST_INSERT_GETCHANGEDTICK, msgid, id);
        nv.writer.write_int(nvim_data.buf_id);
        nv.end_message();

        handled = true;
    }

    return handled;
}

Editor *Pane::focus_editor_by_index(u32 idx, cur2 pos) {
    if (current_editor != idx) {
        auto e = world.get_current_editor();
        if (e != NULL) e->trigger_escape();
    }

    current_editor = idx;

    auto &editor = editors[idx];

    if (pos.x != -1) {
        if (editor.is_nvim_ready()) {
            editor.move_cursor(pos);
        } else {
            editor.nvim_data.need_initial_pos_set = true;
            editor.nvim_data.initial_pos = pos;
        }
    }

    world.nvim.waiting_focus_window = editor.id;

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
    // strcpy_safe(path, _countof(path), normalize_path_sep("c:/users/brandon/ide/helper"));
    strcpy_safe(path, _countof(path), normalize_path_sep(TEST_PATH));
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
}

void Workspace::activate_pane(u32 idx) {
    if (idx > panes.len) return;

    if (idx == panes.len) {
        auto panes_width = ui.get_panes_area().w;

        float new_width = panes_width;
        if (panes.len > 0)
            new_width /= panes.len;

        auto pane = panes.append();
        pane->init();
        pane->width = new_width;
    }

    if (current_pane != idx) {
        auto e = world.get_current_editor();
        if (e != NULL) e->trigger_escape();
    }

    current_pane = idx;

    if (world.use_nvim) {
        auto pane = get_current_pane();
        if (pane->current_editor != -1)
            pane->focus_editor_by_index(pane->current_editor);

        /*
        auto editor = pane->get_current_editor();
        if (editor != NULL)
            world.nvim.set_current_window(editor);
        */
    }
}

Pane* Workspace::get_current_pane() {
    if (panes.len == 0) return NULL;

    return &panes[current_pane];
}

bool Editor::is_nvim_ready() {
    return world.nvim.is_ui_attached
        && nvim_data.is_buf_attached
        && (nvim_data.buf_id != 0)
        && (nvim_data.win_id != 0);
}

void Editor::init() {
    ptr0(this);
    id = ++world.next_editor_id;

    mem.init("editor mem");
    SCOPED_MEM(&mem);

    parser = new_ts_parser();
}

void Editor::cleanup() {
    if (parser != NULL)
        ts_parser_delete(parser);
    if (tree != NULL)
        ts_tree_delete(tree); // i remember this being super slow, is it still if it's just one tree?

    // TODO: delete nvim resources
    buf.cleanup();
    mem.cleanup();
}

// basically the rule is, if autocomplete comes up empty ON FIRST OPEN, then keep it closed

void Editor::trigger_autocomplete(bool triggered_by_dot) {
    ptr0(&autocomplete);

    SCOPED_MEM(&world.indexer.ui_mem);
    defer { world.indexer.ui_mem.reset(); };

    if (!world.indexer.ready) return; // strictly we can just call try_enter(), but want consistency with UI, which is based on `ready`
    if (!world.indexer.lock.try_enter()) return;
    defer { world.indexer.lock.leave(); };

    bool was_already_open = (autocomplete.ac.results != NULL);

    Autocomplete ac = {0};
    if (!world.indexer.autocomplete(filepath, cur, triggered_by_dot, &ac)) return;

    {
        // use autocomplete_mem
        world.autocomplete_mem.reset();
        SCOPED_MEM(&world.autocomplete_mem);

        // copy results
        auto new_results = alloc_list<AC_Result>(ac.results->len);
        For (*ac.results) new_results->append()->name = our_strcpy(it.name);

        // copy ac over to autocomplete.ac
        memcpy(&autocomplete.ac, &ac, sizeof(Autocomplete));
        autocomplete.filtered_results = alloc_list<int>();
        autocomplete.ac.prefix = our_strcpy(ac.prefix);
        autocomplete.ac.results = new_results;

        auto prefix = autocomplete.ac.prefix;
        auto results = autocomplete.ac.results;

        // OPTIMIZATION: if we added characters to prefix, then we only need to
        // search through the existing filtered results

        for (int i = 0; i < results->len; i++)
            if (fzy_has_match(prefix, results->at(i).name))
                autocomplete.filtered_results->append(i);

        if (!was_already_open && autocomplete.filtered_results->len == 0) {
            autocomplete.ac.results = NULL;
            return;
        }

        autocomplete.filtered_results->sort([&](int *ia, int *ib) -> int {
            auto a = fzy_match(prefix, autocomplete.ac.results->at(*ia).name);
            auto b = fzy_match(prefix, autocomplete.ac.results->at(*ib).name);
            return a < b ? 1 : (a > b ? -1 : 0); // reverse
        });
    }
}

struct Type_Renderer : public Text_Renderer {
    void write_type(Gotype *t, bool parameter_hint_root = false) {
        switch (t->type) {
        case GOTYPE_ID:
            write("%s", t->id_name);
            break;
        case GOTYPE_SEL:
            write("%s.%s", t->sel_name, t->sel_sel);
            break;
        case GOTYPE_MAP:
            write("map[");
            write_type(t->map_key);
            write("]");
            write_type(t->map_value);
            break;
        case GOTYPE_STRUCT:
            write("struct");
            break;
        case GOTYPE_INTERFACE:
            write("interface");
            break;
        case GOTYPE_VARIADIC:
            write("...");
            write_type(t->variadic_base);
            break;
        case GOTYPE_POINTER:
            write("*");
            write_type(t->pointer_base);
            break;
        case GOTYPE_FUNC:
            {
                if (!parameter_hint_root)
                    write("func");

                auto write_params = [&](List<Godecl> *params) {
                    write("(");

                    u32 i = 0;
                    For (*params) {
                        write("%s ", it.name);
                        write_type(it.gotype);
                        if (i < params->len - 1)
                            write(", ");
                        i++;
                    }

                    write(")");
                };

                auto &sig = t->func_sig;
                write_params(sig.params);

                auto result = sig.result;
                if (result != NULL && result->len > 0) {
                    if (result->len == 1 && result->at(0).name == NULL) {
                        write(" ");
                        write_type(result->at(0).gotype);
                    } else {
                        write_params(result);
                    }
                }
            }
            break;
        case GOTYPE_SLICE:
            write("[]");
            write_type(t->slice_base);
            break;
        case GOTYPE_ARRAY:
            write("[]");
            write_type(t->array_base);
            break;
        case GOTYPE_CHAN:
            if (t->chan_direction == CHAN_RECV)
                write("<-");
            write("chan");
            write_type(t->chan_base);
            if (t->chan_direction == CHAN_SEND)
                write("<-");
            break;
        case GOTYPE_MULTI:
            write("(multi type?)");
        }
    }
};

void Editor::trigger_parameter_hint(bool triggered_by_paren) {
    ptr0(&parameter_hint);

    {
        SCOPED_MEM(&world.indexer.ui_mem);
        defer { world.indexer.ui_mem.reset(); };

        if (!world.indexer.ready) return; // strictly we can just call try_enter(), but want consistency with UI, which is based on `ready`
        if (!world.indexer.lock.try_enter()) return;
        defer { world.indexer.lock.leave(); };

        auto hint = world.indexer.parameter_hint(filepath, cur, triggered_by_paren);
        if (hint == NULL) return;

        {
            SCOPED_MEM(&world.parameter_hint_mem);
            world.parameter_hint_mem.reset();

            parameter_hint.gotype = hint->gotype->copy();
            parameter_hint.start = hint->call_args_start;

            Type_Renderer rend;
            rend.init();
            rend.write_type(parameter_hint.gotype, true);
            parameter_hint.help_text = rend.finish();
        }
    }
}

void Editor::type_char(char ch) {
    uchar uch = ch;
    buf.insert(cur, &uch, 1);
    raw_move_cursor(buf.inc_cur(cur));
}

void Editor::update_autocomplete() {
    if (autocomplete.ac.results == NULL) return;

    trigger_autocomplete(false);

    /*
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
    */
}

void Editor::update_parameter_hint() {
    // reset parameter hint when cursor goes before hint start
    auto& hint = parameter_hint;
    if (hint.gotype == NULL) return;

    auto should_close_hints = [&]() {
        if (cur < hint.start) return true;

        auto root = new_ast_node(ts_tree_root_node(tree), NULL);

        bool ret = false;
        find_nodes_containing_pos(root, hint.start, true, [&](Ast_Node *it) -> Walk_Action {
            if (it->start == hint.start)
                if (it->type == TS_ARGUMENT_LIST)
                    if (cur >= it->end) {
                        ret = true;
                        return WALK_ABORT;
                    }
            return WALK_CONTINUE;
        });

        return ret;
    };

    if (should_close_hints()) {
        hint.gotype = NULL;
    } else {
        /*
        hint.current_param = -1;
        with_parser_at_location(filepath, hint.start, [&](Parser* p) {
            auto call_args = p->parse_call_args();
            u32 idx = 0;
            For (call_args->call_args.args->list) {
                if (cur <= it->end) {
                    hint.current_param = idx;
                    break;
                }
                idx++;
            }
        });
        */
    }
}

void Editor::start_change() {
    if (!is_go_file) return;

    ptr0(&curr_change);
    curr_change.start_point = cur_to_tspoint(cur);
    curr_change.start_byte = cur_to_offset(cur);

    curr_change.old_end_point = curr_change.start_point;
    curr_change.old_end_byte = curr_change.start_byte;
}

void Editor::end_change() {
    if (!is_go_file) return;

    auto end = cur_to_offset(cur);

    if (end < curr_change.start_byte) {
        curr_change.start_byte = end;
        curr_change.start_point = cur_to_tspoint(cur);
    }

    curr_change.new_end_byte = end;
    curr_change.new_end_point = cur_to_tspoint(cur);

    ts_tree_edit(tree, &curr_change);
    update_tree();
}

void Editor::type_char_in_insert_mode(char ch) {
    start_change();
    type_char(ch);
    end_change();

    // at this point, tree is up to date! we can simply walk, don't need to re-parse :)

    bool did_autocomplete = false;
    bool did_parameter_hint = false;

    switch (ch) {
    case '.':
        trigger_autocomplete(true);
        did_autocomplete = true;
        break;
    case '(':
        trigger_parameter_hint(true);
        did_parameter_hint = true;
        break;

    case '}':
    case ')':
    case ']':
        {
            if (cur.x == 0) break;

            auto rbrace_pos = buf.dec_cur(cur);

            Ts_Ast_Type brace_type = TS_ERROR, other_brace_type = TS_ERROR;
            switch (ch) {
            case '}':
                brace_type = TS_RBRACE;
                other_brace_type = TS_LBRACE;
                break;
            case ')':
                brace_type = TS_RPAREN;
                other_brace_type = TS_LPAREN;
                break;
            case ']':
                brace_type = TS_RBRACK;
                other_brace_type = TS_LBRACK;
                break;
            }

            auto &line = buf.lines[rbrace_pos.y];
            bool starts_with_spaces = true;
            for (u32 x = 0; x < rbrace_pos.x; x++) {
                if (line[x] != ' ' && line[x] != '\t') {
                    starts_with_spaces = false;
                    break;
                }
            }

            if (!starts_with_spaces) break;

            Parser_It it;
            it.init(&buf);

            auto root_node = new_ast_node(ts_tree_root_node(tree), &it);

            Ast_Node *rbrace_node = alloc_object(Ast_Node);

            find_nodes_containing_pos(root_node, rbrace_pos, false, [&](Ast_Node *it) {
                if (it->type == brace_type) {
                    memcpy(rbrace_node, it, sizeof(Ast_Node));
                    return WALK_ABORT;
                }
                return WALK_CONTINUE;
            });

            auto curr = rbrace_node->prev_all();
            int depth = 1;

            i32 lbrace_line = -1;

            fn<void(Ast_Node*)> process_node = [&](Ast_Node* node) {
                if (lbrace_line != -1) return;

                SCOPED_FRAME();
                auto children = alloc_list<Ast_Node*>(node->child_count);
                FOR_ALL_NODE_CHILDREN (node) children->append(it);
                for (; children->len > 0; children->len--)
                    process_node(*children->last());

                if (node->type == brace_type)
                    depth++;
                if (node->type == other_brace_type) {
                    depth--;
                    if (depth == 0) {
                        lbrace_line = node->start.y;
                        return;
                    }
                }
            };

            for (; !curr->null && lbrace_line == -1; curr = curr->parent()) {
                while (lbrace_line == -1) {
                    process_node(curr);
                    auto prev = curr->prev_all();
                    if (prev->null) break;
                    curr = prev;
                }
            }

            if (lbrace_line == -1) break;

            auto indentation = alloc_list<uchar>();
            For (buf.lines[lbrace_line]) {
                if (it == '\t' || it == ' ')
                    indentation->append(it);
                else
                    break;
            }

            auto start = new_cur2(0, rbrace_pos.y);
            if (start < nvim_insert.backspaced_to)
                nvim_insert.backspaced_to = start;

            start_change();
            buf.remove(start, rbrace_pos);
            buf.insert(start, indentation->items, indentation->len);
            cur = new_cur2(indentation->len + 1, rbrace_pos.y);
            end_change();
        }
    }

    if (!did_autocomplete) update_autocomplete();
    if (!did_parameter_hint) update_parameter_hint();
}

void Editor::format_on_save() {
    auto old_cur = cur;

    auto &proc = goimports_proc;
    proc.cleanup();
    proc.init();
    proc.use_stdin = true;
    proc.run("goimports");
    saving = true;

    For (buf.lines) {
        // TODO(unicode)
        For (it) proc.write1((char)it);
        proc.write1('\n');
    }
    proc.done_writing();

    Buffer swapbuf;
    swapbuf.init(MEM);
    swapbuf.read([&](char* out) { return proc.read1(out); });
    defer { swapbuf.cleanup(); };

    while (proc.status() == PROCESS_WAITING) continue;
    bool success = (proc.exit_code == 0);
    proc.cleanup();

    if (success) {
        buf.copy_from(&swapbuf);

        if (tree != NULL) {
            ts_tree_delete(tree);
            tree = NULL;
        }
        update_tree();

        auto &nv = world.nvim;
        auto msgid = nv.start_request_message("nvim_buf_get_changedtick", 1);
        auto req = nv.save_request(NVIM_REQ_POST_SAVE_GETCHANGEDTICK, msgid, id);
        req->post_save_getchangedtick.cur = old_cur;
        nv.writer.write_int(nvim_data.buf_id);
        nv.end_message();
    }
}

void go_to_error(int index) {
    auto &b = world.build;
    if (index < 0 || index >= b.errors.len) return;

    auto &error = b.errors[index];

    SCOPED_FRAME();
    auto path = path_join(world.wksp.path, error.file);
    auto pos = new_cur2(error.col-1, error.row-1);
    world.get_current_pane()->focus_editor(path, pos);
}
