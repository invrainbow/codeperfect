#include "editor.hpp"
#include "os.hpp"
#include "world.hpp"
#include "ui.hpp"
#include "go.hpp"
#include "fzy_match.h"
#include "tree_sitter_crap.hpp"
#include "settings.hpp"

ccstr Editor::get_autoindent(int for_y) {
    auto y = relu_sub(for_y, 1);

    while (true) {
        auto& line = buf.lines[y];
        for (u32 x = 0; x < line.len; x++)
            if (!isspace(line[x]))
                goto done;
        if (y == 0) break;
        y--;
    }
done:

    auto& line = buf.lines[y];
    u32 copy_spaces_until = 0;
    {
        u32 x = 0;
        for (; x < line.len; x++)
            if (line[x] != ' ' && line[x] != '\t')
                break;
        if (x == line.len)  // all spaces
            x = 0;
        copy_spaces_until = x;
    }

    // copy first `copy_spaces_until` chars of line y
    auto ret = alloc_list<char>();
    for (u32 x = 0; x < copy_spaces_until; x++)
        ret->append((char)line[x]); // has to be ' ' or '\t'

    if (is_go_file) {
        for (i32 x = line.len-1; x >= 0; x--) {
            if (isspace(line[x])) continue;

            switch (line[x]) {
            case '{':
            case '(':
            case '[':
                ret->append('\t');
                break;
            }
            break;
        }
    }

    ret->append('\0');
    return ret->items;
}

void Editor::insert_text_in_insert_mode(ccstr s) {
    auto len = strlen(s);
    if (len == 0) return;

    Cstr_To_Ustr conv;
    conv.init();

    for (int i = 0; i < len; i++)
        conv.count(s[i]);

    auto ulen = conv.len;

    SCOPED_FRAME();

    auto text = alloc_array(uchar, ulen);
    conv.init();
    for (u32 i = 0, j = 0; i < len; i++) {
        bool found = false;
        auto uch = conv.feed(s[i], &found);
        if (found)
            text[j++] = uch;
    }

    buf.insert(cur, text, ulen);
    auto c = cur;
    for (u32 i = 0; i < ulen; i++)
        c = buf.inc_cur(c);
    raw_move_cursor(c);
}

void Editor::perform_autocomplete(AC_Result *result) {
    auto& ac = autocomplete.ac;

    switch (result->type) {
    case ACR_POSTFIX:
        {
            // TODO: this currently only works for insert mode; support normal mode
            // also what if we just forced you to be in insert mode for autocomplete lol

            // remove everything but the operator
            raw_move_cursor(ac.operand_end);
            buf.remove(ac.operand_end, ac.keyword_end);

            // nice little dsl here lol

            ccstr autoindent_chars = NULL;
            ccstr operand_text = NULL;
            Postfix_Info *curr_postfix = NULL;

            auto insert_text = [&](ccstr fmt, ...) {
                SCOPED_FRAME();

                va_list vl;
                va_start(vl, fmt);
                insert_text_in_insert_mode(our_vsprintf(fmt, vl));
                va_end(vl);
            };

            auto save_autoindent = [&]() {
                auto& line = buf.lines[cur.y];
                u32 copy_spaces_until = 0;
                {
                    u32 x = 0;
                    for (; x < line.len; x++)
                        if (line[x] != ' ' && line[x] != '\t')
                            break;
                    if (x == line.len)  // all spaces
                        x = 0;
                    copy_spaces_until = x;
                }

                auto ret = alloc_list<char>(copy_spaces_until + 1);

                // copy first `copy_spaces_until` chars of line y
                for (u32 x = 0; x < copy_spaces_until; x++)
                    ret->append((char)line[x]); // has to be ' ' or '\t'

                ret->append('\0');
                autoindent_chars = ret->items;
            };

            auto insert_autoindent = [&](int add = 0) {
                SCOPED_FRAME();

                insert_text("%s", autoindent_chars);

                if (add > 0) {
                    ccstr tabs = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";
                    if (add >= strlen(tabs)) our_panic("not enough tabs");
                    insert_text("%.*s", add, tabs);
                }
            };

            auto insert_newline = [&](int add_indent = 0) {
                insert_text("\n");
                insert_autoindent(add_indent);
            };

            auto record_position = [&]() {
                curr_postfix->insert_positions.append(cur);
            };

            auto initialize_everything = [&]() {
                operand_text = buf.get_text(ac.operand_start, ac.operand_end);

                raw_move_cursor(ac.operand_start);
                buf.remove(ac.operand_start, ac.operand_end);

                curr_postfix = postfix_stack.append();
                curr_postfix->start();
            };

            bool notfound = false;

            switch (result->postfix_operation) {
            case PFC_ASSIGNAPPEND:
                initialize_everything();
                insert_text("%s = append(%s, ", operand_text, operand_text);
                record_position();
                insert_text(")");
                record_position();
                break;

            case PFC_APPEND:
                initialize_everything();
                insert_text("append(%s, ", operand_text);
                record_position();
                insert_text(")");
                record_position();
                break;

            case PFC_LEN:
                initialize_everything();
                insert_text("len(%s)", operand_text);
                break;

            case PFC_CAP:
                initialize_everything();
                insert_text("cap(%s)", operand_text);
                break;

            case PFC_NIL:
                initialize_everything();
                insert_text("%s == nil", operand_text);
                break;

            case PFC_NOTNIL:
                initialize_everything();
                insert_text("%s != nil", operand_text);
                break;

            case PFC_NOT:
                initialize_everything();
                insert_text("!%s", operand_text);
                break;

            case PFC_EMPTY:
                initialize_everything();
                insert_text("%s == nil || len(%s) == 0", operand_text);
                break;

            case PFC_IF:
                initialize_everything();
                insert_text("if %s {", operand_text);
                save_autoindent();
                insert_newline(1);
                record_position();
                insert_newline(0);
                insert_text("}");
                record_position();
                break;

            case PFC_IFEMPTY:
                initialize_everything();
                insert_text("if %s == nil || %s.len == 0 {", operand_text, operand_text);
                save_autoindent();
                insert_newline(1);
                record_position();
                insert_newline(0);
                insert_text("}");
                record_position();
                break;

            case PFC_IFNOTEMPTY:
                initialize_everything();
                insert_text("if %s != nil && %s.len != 0 {", operand_text, operand_text);
                save_autoindent();
                insert_newline(1);
                record_position();
                insert_newline(0);
                insert_text("}");
                record_position();
                break;

            case PFC_IFNOT:
                initialize_everything();
                insert_text("if !%s {", operand_text);
                save_autoindent();
                insert_newline(1);
                record_position();
                insert_newline(0);
                insert_text("}");
                record_position();
                break;

            case PFC_IFNIL:
                initialize_everything();
                insert_text("if %s == nil {", operand_text);
                save_autoindent();
                insert_newline(1);
                record_position();
                insert_newline(0);
                insert_text("}");
                record_position();
                break;

            case PFC_IFNOTNIL:
                initialize_everything();
                insert_text("if %s != nil {", operand_text);
                save_autoindent();
                insert_newline(1);
                record_position();
                insert_newline(0);
                insert_text("}");
                record_position();
                break;

            case PFC_SWITCH:
                initialize_everything();
                insert_text("switch %s {", operand_text);
                save_autoindent();
                insert_newline(1);
                record_position();
                insert_newline(0);
                insert_text("}");
                record_position();
                break;

            case PFC_DEFSTRUCT:
                initialize_everything();
                insert_text("type %s struct {", operand_text);
                save_autoindent();
                insert_newline(1);
                record_position();
                insert_newline(0);
                insert_text("}");
                record_position();
                break;

            case PFC_DEFINTERFACE:
                initialize_everything();
                insert_text("type %s interface {", operand_text);
                save_autoindent();
                insert_newline(1);
                record_position();
                insert_newline(0);
                insert_text("}");
                record_position();
                break;

            case PFC_FOR:
            case PFC_FORKEY:
            case PFC_FORVALUE:
                {
                    ccstr keyname = "key";
                    ccstr valuename = "val";

                    auto gotype = ac.operand_gotype;
                    if (gotype != NULL)
                        if (gotype->type == GOTYPE_SLICE || gotype->type == GOTYPE_ARRAY) {
                            keyname = "i";
                            valuename = "val";
                        }

                    if (result->postfix_operation == PFC_FORKEY) valuename = "_";
                    if (result->postfix_operation == PFC_FORVALUE) keyname = "_";

                    initialize_everything();
                    insert_text("for %s, %s := range %s {", keyname, valuename, operand_text);
                    save_autoindent();
                    insert_newline(1);
                    record_position();
                    insert_newline(0);
                    insert_text("}");
                    record_position();
                }
                break;

            case PFC_CHECK:
                {
                    if (ac.operand_gotype->type != GOTYPE_MULTI) break;

                    int error_found_at = -1;
                    auto multi_types = ac.operand_gotype->multi_types;

                    for (int i = 0; i < multi_types->len; i++) {
                        auto it = multi_types->at(i);
                        if (it->type == GOTYPE_ID && streq(it->id_name, "error")) {
                            error_found_at = i;
                            break;
                        }
                    }

                    if (error_found_at == -1) break;

                    initialize_everything();

                    int varcount = 0;
                    for (int i = 0; i < multi_types->len; i++) {
                        auto it = multi_types->at(i);
                        if (i == error_found_at) {
                            insert_text("err");
                        } else {
                            if (varcount == 0)
                                insert_text("val");
                            else
                                insert_text("val%d", varcount);
                            varcount++;
                        }

                        if (i + 1 < multi_types->len)
                            insert_text(",");
                        insert_text(" ");
                    }

                    // TODO: make this smarter, like if we're already using err, either
                    // make it a = instead of :=, or use a different name.
                    insert_text(":= %s", operand_text);
                    save_autoindent();
                    insert_newline();
                    insert_text("if err != nil {");
                    insert_newline(1);

                    {
                        bool ok = false;

                        do {
                            // get gotype of current function
                            auto functype = world.indexer.get_closest_function(filepath, cur);
                            if (functype == NULL) break;

                            auto result = functype->func_sig.result;
                            if (result == NULL) break;
                            if (result->len == 0) break;

                            bool error_found = false;
                            auto &ind = world.indexer;

                            auto get_zero_value_of_gotype = [&](Gotype *gotype) -> ccstr {
                                if (gotype == NULL) return NULL;

                                Go_Ctx ctx; ptr0(&ctx);
                                ctx.import_path = ind.filepath_to_import_path(our_dirname(filepath));
                                ctx.filename = our_basename(filepath);

                                auto res = ind.evaluate_type(gotype, &ctx);
                                if (res == NULL) return NULL;

                                auto rres = ind.resolve_type(res->gotype, res->ctx);
                                if (rres == NULL) return NULL;

                                gotype = rres->gotype;

                                // TODO: check for aliases of error
                                if (gotype->type == GOTYPE_ID) {
                                    ccstr int_types[] = {
                                        "byte", "complex128", "complex64", "float32", "float64",
                                        "int", "int16", "int32", "int64", "int8",
                                        "rune", "uint", "uint16", "uint32", "uint64",
                                        "uint8", "uintptr",
                                    };

                                    For (int_types)
                                        if (streq(gotype->id_name, it))
                                            return "0";

                                    if (streq(gotype->id_name, "bool")) return "false";
                                    if (streq(gotype->id_name, "string")) return "\"\"";
                                    if (!error_found && streq(gotype->id_name, "error")) return "err";
                                }

                                return NULL;
                            };

                            insert_text("return ");

                            for (int i = 0; i < result->len; i++) {
                                auto &it = result->at(i);
                                auto val = get_zero_value_of_gotype(it.gotype);

                                if (i > 0)
                                    insert_text(", ");
                                insert_text(val == NULL ? "nil" : val);
                            }

                            ok = true;
                        } while (0);

                        if (!ok) insert_text("// sorry, couldn't deduce return type");
                    }

                    insert_newline(0);
                    insert_text("}");
                    insert_newline(0);
                }
                break;

            default:
                notfound = true;
                break;
            }

            if (notfound) break;

            if (curr_postfix != NULL) {
                if (curr_postfix->insert_positions.len > 1) {
                    trigger_escape(curr_postfix->insert_positions[0]);
                    curr_postfix->current_insert_position++;
                } else {
                    postfix_stack.len--;
                }
            }

            // clear autocomplete
            ptr0(&ac);
        }
        break;

    case ACR_KEYWORD:
    case ACR_DECLARATION:
    case ACR_IMPORT:
        {
            bool is_function = false;

            auto modify_string = [&](ccstr s) -> ccstr {
                switch (result->type) {
                case ACR_IMPORT:
                    return our_sprintf("%s.", s);

                case ACR_KEYWORD:
                    {
                        ccstr builtins_with_space[] = {
                            "package", "import", "const", "var", "func",
                            "type", "struct", "interface", "map", "chan",
                            "fallthrough", "break", "continue", "goto",
                            "go", "defer", "if", "else",
                            "for", "select", "switch",
                        };

                        For (builtins_with_space)
                            if (streq(s, it))
                                return our_sprintf("%s ", s);
                    }
                    break;

                case ACR_DECLARATION:
                    {
                        if (result->declaration_is_struct_literal_field)
                            return our_sprintf("%s: ", s);

                        auto godecl = result->declaration_godecl;
                        if (godecl == NULL) break;

                        if (!world.indexer.ready) break;
                        if (!world.indexer.lock.try_enter()) break;
                        defer { world.indexer.lock.leave(); };

                        Go_Ctx ctx;
                        ctx.import_path = result->declaration_import_path;
                        ctx.filename = result->declaration_filename;

                        auto res = world.indexer.evaluate_type(godecl->gotype, &ctx);
                        if (res == NULL) break;

                        auto rres = world.indexer.resolve_type(res->gotype, res->ctx);
                        if (rres == NULL) break;

                        auto gotype = rres->gotype;
                        if (gotype->type != GOTYPE_FUNC) break;

                        // it's a func, add a '('
                        is_function = true;
                        return our_sprintf("%s(", s);
                    }
                    break;
                }

                return s;
            };

            // this whole section is basically, like, "how do we replace text
            // and make sure nvim/ts are in sync"
            //
            // refactor this out somehow, we're already starting to copy paste

            auto fullstring = modify_string(result->name);

            // grab len & save name
            auto len = strlen(fullstring);
            auto name = alloc_array(uchar, len);
            for (u32 i = 0; i < len; i++)
                name[i] = (uchar)fullstring[i];

            auto ac_start = cur;
            ac_start.x -= strlen(ac.prefix);

            // perform the edit
            buf.remove(ac_start, cur);
            buf.insert(ac_start, name, len);

            // move cursor forward
            raw_move_cursor(new_cur2(ac_start.x + len, ac_start.y));

            // clear out last_closed_autocomplete
            last_closed_autocomplete = new_cur2(-1, -1);

            // clear autocomplete
            ptr0(&ac);

            if (is_function) trigger_parameter_hint();

            auto see_if_we_need_autoimport = [&]() -> ccstr {
                if (result->type == ACR_IMPORT)
                    return result->import_path;
                if (result->type == ACR_DECLARATION)
                    return result->declaration_package;
                return NULL;
            };

            auto import_to_add = see_if_we_need_autoimport();
            if (import_to_add != NULL) {
                auto iter = alloc_object(Parser_It);
                iter->init(&buf);
                auto root = new_ast_node(ts_tree_root_node(buf.tree), iter);

                Ast_Node *imports_node = NULL;

                FOR_NODE_CHILDREN (root) {
                    if (it->type() == TS_IMPORT_DECLARATION) {
                        imports_node = it;
                        break;
                    }
                }

                do {
                    if (imports_node == NULL) break;
                    if (cur <= imports_node->end()) break;

                    auto imports = alloc_list<Go_Import>();
                    world.indexer.import_decl_to_goimports(imports_node, NULL, imports);

                    auto imp = imports->find([&](auto it) { return streq(it->import_path, import_to_add); });
                    if (imp != NULL) break;

                    Text_Renderer rend;
                    rend.init();
                    rend.write("import (\n");
                    rend.write("\"%s\"\n", import_to_add);

                    For (*imports) {
                        switch (it.package_name_type) {
                        case GPN_IMPLICIT:
                            rend.write("\"%s\"", it.import_path);
                            break;
                        case GPN_EXPLICIT:
                            rend.write("%s \"%s\"", it.package_name, it.import_path);
                            break;
                        case GPN_BLANK:
                            rend.write("_ \"%s\"", it.import_path);
                            break;
                        case GPN_DOT:
                            rend.write(". \"%s\"", it.import_path);
                            break;
                        }
                        rend.write("\n");
                    }

                    rend.write(")");

                    GHFmtStart();
                    GHFmtAddLine(rend.finish());
                    GHFmtAddLine("");

                    auto new_contents = GHFmtFinish(GH_FMT_GOIMPORTS);
                    if (new_contents == NULL) break;

                    auto new_contents_len = strlen(new_contents);
                    if (new_contents_len == 0) break;

                    if (new_contents[new_contents_len-1] == '\n') {
                        new_contents[new_contents_len-1] = '\0';
                        new_contents_len--;
                    }

                    {
                        auto start = imports_node->start();
                        auto old_end = imports_node->end();

                        if (old_end.y >= nvim_insert.start.y) break;

                        auto chars = alloc_list<uchar>();
                        auto new_end = start;

                        Cstr_To_Ustr conv;
                        conv.init();

                        for (auto p = new_contents; *p != '\0'; p++) {
                            bool found = false;
                            auto uch = conv.feed(*p, &found);
                            if (found) {
                                chars->append(uch);

                                if (uch == '\n') {
                                    new_end.x = 0;
                                    new_end.y++;
                                } else {
                                    new_end.x++;
                                }
                            }
                        }

                        // perform the edit
                        buf.remove(start, old_end);
                        buf.insert(start, chars->items, chars->len);

                        add_change_in_insert_mode(start, old_end, new_end);
                    }

                    // do something with new_contents
                } while (0);

                if (result->type == ACR_IMPORT)
                    trigger_autocomplete(true, false);
            }
        }
        break;
    }
}

void Editor::add_change_in_insert_mode(cur2 start, cur2 old_end, cur2 new_end) {
    auto &nvi = nvim_insert;

    auto change = nvi.other_changes.append();
    change->start = start;
    change->end = old_end;

    {
        SCOPED_MEM(&nvi.mem);

        change->lines.init(LIST_POOL, new_end.y - start.y + 1);
        for (int i = start.y; i <= new_end.y; i++) {
            auto line = change->lines.append();
            line->init(LIST_POOL, buf.lines[i].len);
            line->len = buf.lines[i].len;
            memcpy(line->items, buf.lines[i].items, sizeof(uchar) * line->len);
        }
    }

    if (old_end.y >= nvim_insert.start.y)
        our_panic("can only add changes in insert mode before current change");

    auto dy = new_end.y - old_end.y;
    nvim_insert.start.y += dy;
    nvim_insert.old_end.y += dy;
    raw_move_cursor(new_cur2(cur.x, cur.y + dy), true);
}

bool Editor::is_current_editor() {
    auto current_editor = world.get_current_editor();
    if (current_editor != NULL)
        if (current_editor->id == id)
            return true;
    return false;
}

void Editor::raw_move_cursor(cur2 c, bool dont_add_to_history) {
    if (c.y == -1) c = buf.offset_to_cur(c.x);
    if (c.y < 0 || c.y >= buf.lines.len) return;
    if (c.x < 0) return;

    auto line_len = buf.lines[c.y].len;
    if (c.x > line_len) {
        if (world.nvim.mode == VI_INSERT)
            c.x = line_len;
        else
            c.x = relu_sub(line_len, 1);
    }

    cur = c;

    auto& line = buf.lines[c.y];

    u32 vx = 0;
    for (u32 i = 0; i < c.x; i++) {
        // TODO: determine vx correctly
        vx += line[i] == '\t' ? options.tabsize : 1;
    }

    if (vx < view.x)
        view.x = vx;
    if (vx >= view.x + view.w)
       view.x = vx - view.w + 1;
    if (relu_sub(cur.y, options.scrolloff) < view.y)
        view.y = relu_sub(cur.y, options.scrolloff);
    if (cur.y + options.scrolloff >= view.y + view.h)
        view.y = cur.y + options.scrolloff - view.h + 1;

    auto is_navigating_here = [&]() -> bool {
        return nvim_data.is_navigating
            && nvim_data.navigating_to_pos == c
            && nvim_data.navigating_to_editor == id;
    };

    bool push_to_history = true;

    if (world.navigating_to) {
        if (world.navigating_to_editor == id) {
            if (world.navigating_to_pos == c)
                push_to_history = false;
            else
                world.navigating_to = false;
        }
    }

    if (push_to_history)
        if (!dont_add_to_history)
            if (world.get_current_editor() == this)
                world.history.push(id, c);
}

void Editor::ensure_cursor_on_screen() {
    if (relu_sub(cur.y, options.scrolloff) < view.y)
        move_cursor(new_cur2((u32)cur.x, (u32)min(view.y + options.scrolloff, view.y + view.h)));
    if (cur.y + options.scrolloff >= view.y + view.h)
        move_cursor(new_cur2((u32)cur.x, (u32)relu_sub(view.y + view.h,  1 + options.scrolloff)));

    // TODO: handle x too
}

void Editor::update_lines(int firstline, int lastline, List<uchar*> *new_lines, List<s32> *line_lengths) {
    if (lastline == -1) lastline = buf.lines.len;

    auto start_cur = new_cur2(0, (i32)firstline);
    auto old_end_cur = new_cur2(0, (i32)lastline);

    if (lastline == buf.lines.len && buf.lines.len > 0) {
        start_cur = buf.dec_cur(start_cur);
        start_cur = buf.dec_cur(start_cur);
        old_end_cur = new_cur2((i32)buf.lines.last()->len, (i32)buf.lines.len - 1);
    }

    buf.internal_start_edit(start_cur, old_end_cur);

    buf.internal_delete_lines(firstline, lastline);
    for (u32 i = 0; i < new_lines->len; i++) {
        auto line = new_lines->at(i);
        auto len = line_lengths->at(i);
        buf.internal_insert_line(firstline + i, line, len);
    }

    if (buf.lines.len == 0)
        buf.internal_append_line(NULL, 0);

    auto new_end_cur = new_cur2(0, firstline + new_lines->len);
    if (firstline + new_lines->len == buf.lines.len) {
        if (buf.lines.len == 0)
            new_end_cur = new_cur2(0, 0);
        else
            new_end_cur = new_cur2((i32)buf.lines.last()->len, (i32)buf.lines.len - 1);
    }

    buf.internal_finish_edit(new_end_cur);
}

void Editor::move_cursor(cur2 c) {
    if (world.nvim.mode == VI_INSERT) {
        nvim_data.waiting_for_move_cursor = true;
        nvim_data.move_cursor_to = c;

        // clear out last_closed_autocomplete
        last_closed_autocomplete = new_cur2(-1, -1);

        trigger_escape();
        return;
    }

    raw_move_cursor(c);

    if (world.use_nvim) {
        auto& nv = world.nvim;
        auto msgid = nv.start_request_message("nvim_win_set_cursor", 2);
        nv.writer.write_int(nvim_data.win_id);
        {
            nv.writer.write_array(2);
            nv.writer.write_int(c.y + 1);
            nv.writer.write_int(buf.idx_cp_to_byte(c.y, c.x));
        }
        nv.end_message();
    }
}

void Editor::reset_state() {
    cur.x = 0;
    cur.y = 0;
    last_closed_autocomplete = new_cur2(-1, -1);
}

bool check_file(File_Mapping *fm) {
    cur2 pos = new_cur2(0, 0);

    for (int i = 0; i < fm->len; i++) {
        if (fm->data[i] == '\n') {
            pos.y++;
            pos.x = 0;
            if (pos.y > 65000) {
                tell_user("Sorry, we're not yet able to open files that have more than 65,000 lines.", "Unable to open file.");
                return false;
            }
        } else {
            pos.x++;
            if (pos.x > CHUNKMAX) {
                tell_user(
                    our_sprintf("Sorry, we're not yet able to open files containing lines with more than %d characters.", CHUNKMAX),
                    "Unable to open file."
                );
                return false;
            }
        }
    }

    return true;
}

// I'm just going to make this a separate function from load_file(), since it is doing mostly a different thing.
void Editor::reload_file(bool because_of_file_watcher) {
    if (because_of_file_watcher && disable_file_watcher_until > current_time_in_nanoseconds())
        return;

    auto fm = map_file_into_memory(filepath);
    if (fm == NULL) {
        // don't error here
        // tell_user(our_sprintf("Unable to open %s for reading: %s", filepath, get_last_error()), "Error opening file");
        return;
    }
    defer { fm->cleanup(); };

    if (is_binary((ccstr)fm->data, fm->len)) {
        auto res = ask_user_yes_no(
            "This file appears to be a binary file. Attempting to open it as text could have adverse results. Do you still want to try?",
            "Binary file encountered",
            "Open", "Don't Open"
        );
        if (res != ASKUSER_YES) return;
    }

    if (!check_file(fm)) return;

    if (buf.initialized)
        buf.cleanup();
    buf.init(&mem, is_go_file);
    buf.read(fm);

    if (world.use_nvim) {
        nvim_data.got_initial_lines = false;

        auto& nv = world.nvim;
        nv.start_request_message("nvim_buf_set_lines", 5);
        nv.writer.write_int(nvim_data.buf_id);
        nv.writer.write_int(0);
        nv.writer.write_int(-1);
        nv.writer.write_bool(false);
        nv.writer.write_array(buf.lines.len);
        For (buf.lines) nv.write_line(&it);
        nv.end_message();
    }
}

bool Editor::load_file(ccstr new_filepath) {
    reset_state();

    if (buf.initialized)
        buf.cleanup();

    is_go_file = (new_filepath != NULL && str_ends_with(new_filepath, ".go"));
    buf.init(&mem, is_go_file);

    FILE* f = NULL;
    if (new_filepath != NULL) {
        auto path = get_normalized_path(new_filepath);
        if (path == NULL)
            return error("unable to normalize filepath"), false;

        strcpy_safe(filepath, _countof(filepath), path);

        auto fm = map_file_into_memory(filepath);
        if (fm == NULL) {
            tell_user(our_sprintf("Unable to open %s for reading: %s", filepath, get_last_error()), "Error opening file");
            return false;
        }
        defer { fm->cleanup(); };

        if (is_binary((ccstr)fm->data, fm->len)) {
            if (ask_user_yes_no("This file appears to be a binary file. Attempting to open it as text could have adverse results. Do you still want to try?", "Binary file encountered", "Open", "Don't Open") != ASKUSER_YES) {
                return false;
            }
        }

        if (!check_file(fm)) return false;

        buf.read(fm);
    } else {
        is_untitled = true;
        uchar tmp = '\0';
        buf.insert(new_cur2(0, 0), &tmp, 0);
        buf.dirty = false;
    }

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

    if (!ed->load_file(NULL)) {
        ed->cleanup();
        editors.len--;
        return NULL;
    }

    ui.recalculate_view_sizes(true);
    return focus_editor_by_index(editors.len - 1);
}

bool Editor::save_file() {
    File f;
    if (f.init(filepath, FILE_MODE_WRITE, FILE_CREATE_NEW) != FILE_RESULT_SUCCESS)
        return error("unable to open %s for writing", filepath), false;
    defer { f.cleanup(); };

    buf.write(&f);
    return true;
}

i32 Editor::cur_to_offset(cur2 c) {
    return buf.cur_to_offset(c);
}

i32 Editor::cur_to_offset() { return cur_to_offset(cur); }

cur2 Editor::offset_to_cur(i32 offset) {
    return buf.offset_to_cur(offset);
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
    set_current_editor(-1);
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
        // TODO: use are_filepaths_equal instead, don't have to access filesystem
        if (are_filepaths_same_file(path, it.filepath))
            return focus_editor_by_index(i, pos);
        i++;
    }

    auto ed = editors.append();
    ed->init();
    if (!ed->load_file(path)) {
        ed->cleanup();
        editors.len--;
        return NULL;
    }

    ui.recalculate_view_sizes(true);
    return focus_editor_by_index(editors.len - 1, pos);
}

Editor *Pane::focus_editor_by_index(u32 idx) {
    return focus_editor_by_index(idx, new_cur2(-1, -1));
}

bool Editor::trigger_escape(cur2 go_here_after) {
    if (go_here_after.x == -1 && go_here_after.y == -1)
        postfix_stack.len = 0;

    bool handled = false;

    if (autocomplete.ac.results != NULL) {
        handled = true;
        ptr0(&autocomplete.ac);

        {
            auto c = cur;
            auto &line = buf.lines[c.y];
            while (c.x > 0 && isident(line[c.x-1])) c.x--;
            if (c.x < cur.x)
                last_closed_autocomplete = c;
        }
    }

    if (parameter_hint.gotype != NULL) {
        handled = true;
        parameter_hint.gotype = NULL;
    }

    if (world.nvim.mode == VI_INSERT) {
        auto &nv = world.nvim;
        auto &writer = nv.writer;

        /*
        auto msgid = nv.start_request_message("nvim_buf_get_changedtick", 1);
        auto req = nv.save_request(NVIM_REQ_POST_INSERT_GETCHANGEDTICK, msgid, id);
        nv.writer.write_int(nvim_data.buf_id);
        nv.end_message();
        */

        {
            // skip next update from nvim
            nvim_insert.skip_changedticks_until = nvim_data.changedtick + nvim_insert.other_changes.len + 1;

            auto start = nvim_insert.start;
            auto old_end = nvim_insert.old_end;

            // set new lines
            nv.start_request_message("nvim_call_atomic", 1);
            {
                writer.write_array(nvim_insert.other_changes.len + 1);

                For (nvim_insert.other_changes) {
                    writer.write_array(2);
                    writer.write_string("nvim_buf_set_lines");
                    {
                        writer.write_array(5);
                        {
                            writer.write_int(nvim_data.buf_id);
                            writer.write_int(it.start.y);
                            writer.write_int(it.end.y + 1);
                            writer.write_bool(false);
                            writer.write_array(it.lines.len);
                            {
                                For (it.lines) nv.write_line(&it);
                            }
                        }
                    }
                }

                writer.write_array(2);
                writer.write_string("nvim_buf_set_lines");
                {
                    writer.write_array(5);
                    {
                        writer.write_int(nvim_data.buf_id);
                        writer.write_int(start.y);
                        writer.write_int(old_end.y + 1);
                        writer.write_bool(false);
                        writer.write_array(cur.y - start.y + 1);
                        {
                            for (u32 y = start.y; y <= cur.y; y++)
                                nv.write_line(&buf.lines[y]);
                        }
                    }
                }
            }
            nv.end_message();

            // reset other_changes and mem
            nvim_insert.other_changes.len = 0;
            nvim_insert.mem.reset();

            // move cursor
            auto old_cur = cur;
            {
                auto c = cur;
                if (c.x > 0) {
                    int gr_idx = buf.idx_cp_to_gr(c.y, c.x);
                    c.x = buf.idx_gr_to_cp(c.y, relu_sub(gr_idx, 1));
                }
                raw_move_cursor(c);
            }

            u32 delete_len = nvim_insert.deleted_graphemes;

            auto msgid = nv.start_request_message("nvim_call_atomic", 1);
            nv.save_request(NVIM_REQ_POST_INSERT_DOTREPEAT_REPLAY, msgid, id);
            {
                writer.write_array(5);

                {
                    writer.write_array(2);
                    writer.write_string("nvim_win_set_cursor");
                    {
                        writer.write_array(2);
                        writer.write_int(nvim_data.win_id);
                        {
                            writer.write_array(2);
                            writer.write_int(cur.y + 1);
                            writer.write_int(buf.idx_cp_to_byte(cur.y, cur.x));
                        }
                    }
                }

                {
                    writer.write_array(2);
                    writer.write_string("nvim_set_current_win");
                    {
                        writer.write_array(1);
                        writer.write_int(nv.dotrepeat_win_id);
                    }
                }

                {
                    writer.write_array(2);
                    writer.write_string("nvim_buf_set_lines");
                    {
                        writer.write_array(5);
                        writer.write_int(nv.dotrepeat_buf_id);
                        writer.write_int(0);
                        writer.write_int(-1);
                        writer.write_bool(false);
                        {
                            writer.write_array(1);
                            writer.write1(MP_OP_STRING);
                            writer.write4(delete_len);
                            for (u32 i = 0; i < delete_len; i++)
                                writer.write1('x');
                        }
                    }
                }

                {
                    writer.write_array(2);
                    writer.write_string("nvim_win_set_cursor");
                    {
                        writer.write_array(2);
                        writer.write_int(nv.dotrepeat_win_id);
                        {
                            writer.write_array(2);
                            writer.write_int(1);
                            writer.write_int(delete_len);
                        }
                    }
                }

                {
                    Text_Renderer r;
                    r.init();
                    for (u32 i = 0; i < delete_len; i++)
                        r.writestr("<BS>");

                    auto it = buf.iter(nvim_insert.start);
                    while (it.pos < old_cur) {
                        // wait, does this take utf-8?
                        auto ch = it.next();
                        if (ch == '<') {
                            r.writestr("<LT>");
                        } else {
                            char buf[4];
                            auto len = uchar_to_cstr(ch, buf);
                            for (int i = 0; i < len; i++)
                                r.writechar(buf[i]);
                        }
                    }

                    writer.write_array(2);
                    writer.write_string("nvim_input");
                    {
                        writer.write_array(1);
                        writer.write_string(r.finish());
                    }
                }
            }

            nv.end_message();
        }

        handled = true;

        go_here_after_escape = go_here_after;
        nv.exiting_insert_mode = true;
    }

    return handled;
}

void Pane::set_current_editor(u32 idx) {
    current_editor = idx;

    auto ed = world.get_current_editor();
    if (ed == NULL) return;

    auto focus_current_editor_in_file_explorer = [&]() {
        auto edpath = get_path_relative_to(ed->filepath, world.current_path);
        auto node = world.find_ft_node(edpath);
        if (node == NULL) return;

        world.file_explorer.selection = node;
        for (auto it = node->parent; it != NULL && it->parent != NULL; it = it->parent)
            it->open = true;
    };

    focus_current_editor_in_file_explorer();
}

Editor *Pane::focus_editor_by_index(u32 idx, cur2 pos) {
    if (current_editor != idx) {
        auto e = world.get_current_editor();
        if (e != NULL) e->trigger_escape();
    }

    set_current_editor(idx);

    auto &editor = editors[idx];

    if (pos.x != -1) {
        if (editor.is_nvim_ready()) {
            editor.move_cursor(pos);
        } else {
            editor.nvim_data.need_initial_pos_set = true;
            editor.nvim_data.initial_pos = pos;
        }
    }

    if (world.nvim.current_win_id != editor.id) {
        if (editor.nvim_data.win_id != 0)
            world.nvim.set_current_window(&editor);
        else
            world.nvim.waiting_focus_window = editor.id;
    }

    return &editor;
}

Editor* Pane::get_current_editor() {
    if (editors.len == 0) return NULL;
    if (current_editor == -1) return NULL;

    return &editors[current_editor];
}

bool Editor::is_nvim_ready() {
    return world.nvim.is_ui_attached
        && nvim_data.is_buf_attached
        && (nvim_data.buf_id != 0)
        && (nvim_data.win_id != 0)
        && nvim_data.got_initial_lines;
}

void Editor::init() {
    ptr0(this);
    id = ++world.next_editor_id;

    mem.init("editor mem");

    {
        SCOPED_MEM(&mem);
        postfix_stack.init();
        nvim_insert.other_changes.init();
    }

    nvim_insert.mem.init("nvim_insert mem");
}

void Editor::cleanup() {
    auto &nv = world.nvim;

    if (nvim_data.win_id != 0) {
        nv.start_request_message("nvim_win_close", 2);
        nv.writer.write_int(nvim_data.win_id);
        nv.writer.write_bool(true);
        nv.end_message();
    }

    if (nvim_data.buf_id != 0) {
        nv.start_request_message("nvim_buf_delete", 2);
        nv.writer.write_int(nvim_data.buf_id);
        {
            nv.writer.write_map(2);
            nv.writer.write_string("force"); nv.writer.write_bool(true);
            nv.writer.write_string("unload"); nv.writer.write_bool(false);
        }
        nv.end_message();
    }

    buf.cleanup();
    mem.cleanup();

    world.history.remove_editor_from_history(id);
}

bool Editor::cur_is_inside_comment_or_string() {
    auto root = new_ast_node(ts_tree_root_node(buf.tree), NULL);
    bool ret = false;

    find_nodes_containing_pos(root, cur, false, [&](auto it) -> Walk_Action {
        switch (it->type()) {
        case TS_RAW_STRING_LITERAL:
        case TS_INTERPRETED_STRING_LITERAL:
        case TS_COMMENT:
            ret = true;
            return WALK_ABORT;
        }
        return WALK_CONTINUE;

    }, true);

    return ret;
}

// basically the rule is, if autocomplete comes up empty ON FIRST OPEN, then keep it closed

void Editor::trigger_autocomplete(bool triggered_by_dot, bool triggered_by_typing_ident, char typed_ident_char) {

    if (cur_is_inside_comment_or_string()) {
        return;
    }

    bool ok = false;
    defer {
        if (!ok) {
            auto c = cur;
            auto &line = buf.lines[c.y];
            while (c.x > 0 && isident(line[c.x-1])) c.x--;
            if (c.x < cur.x)
                last_closed_autocomplete = c;
        }
    };

    if (autocomplete.ac.results != NULL && triggered_by_typing_ident) {
        SCOPED_MEM(&world.autocomplete_mem);
        autocomplete.ac.prefix = our_sprintf("%s%c", autocomplete.ac.prefix, typed_ident_char);
        autocomplete.ac.keyword_end.x++;
    } else {
        auto old_type = autocomplete.ac.type;
        auto old_keyword_start = autocomplete.ac.keyword_start;

        ptr0(&autocomplete);

        SCOPED_MEM(&world.indexer.ui_mem);
        defer { world.indexer.ui_mem.reset(); };

        if (!world.indexer.ready) return; // strictly we can just call try_enter(), but want consistency with UI, which is based on `ready`
        if (!world.indexer.lock.try_enter()) return;
        defer { world.indexer.lock.leave(); };

        Autocomplete ac; ptr0(&ac);

        Timer t;
        t.init();

        if (!world.indexer.autocomplete(filepath, cur, triggered_by_dot, &ac)) return;

        t.log("world.indexer.autocomplete");

        if (old_type != AUTOCOMPLETE_NONE && old_keyword_start != ac.keyword_start)
            if (!triggered_by_dot)
                return;

        last_closed_autocomplete = new_cur2(-1, -1);

        {
            // use autocomplete_mem
            world.autocomplete_mem.reset();
            SCOPED_MEM(&world.autocomplete_mem);

            // copy results
            auto new_results = alloc_list<AC_Result>(ac.results->len);
            For (*ac.results) {
                auto r = new_results->append();
                memcpy(r, it.copy(), sizeof(AC_Result));
            }

            // copy ac over to autocomplete.ac
            memcpy(&autocomplete.ac, &ac, sizeof(Autocomplete));
            autocomplete.filtered_results = alloc_list<int>();
            autocomplete.ac.prefix = our_strcpy(ac.prefix);
            autocomplete.ac.results = new_results;
        }
    }

    {
        auto prefix = autocomplete.ac.prefix;
        auto results = autocomplete.ac.results;

        autocomplete.filtered_results->len = 0;

        Timer t;
        t.init("autocomplete");

        for (int i = 0; i < results->len; i++)
            if (fzy_has_match(prefix, results->at(i).name))
                autocomplete.filtered_results->append(i);

        t.log("matching");

        /*
        if (autocomplete.filtered_results->len == 0) {
            ptr0(&autocomplete);
            return;
        }
        */

        auto scores = alloc_array(double, results->len);
        auto scores_saved = alloc_array(bool, results->len);

        auto get_score = [&](int i) {
            if (!scores_saved[i]) {
                scores[i] = fzy_match(prefix, autocomplete.ac.results->at(i).name);
                scores_saved[i] = true;
            }
            return scores[i];
        };

        autocomplete.filtered_results->sort([&](int *ia, int *ib) -> int {
            auto a = get_score(*ia);
            auto b = get_score(*ib);
            return a < b ? 1 : (a > b ? -1 : 0); // reverse
        });

        t.log("scoring");
    }

    ok = true;
    return;
}

bool is_goident_empty(ccstr name) {
    return (name == NULL || name[0] == '\0' || streq(name, "_"));
}

void Type_Renderer::write_type(Gotype *t, bool parameter_hint_root) {
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
                    write_type(it.gotype);
                    if (i < params->len - 1)
                        write(", ");
                    i++;
                }

                write(")");
            };

            auto &sig = t->func_sig;
            write_params(sig.params, false);

            auto result = sig.result;
            if (result != NULL && result->len > 0) {
                write(" ");
                if (result->len == 1 && is_goident_empty(result->at(0).name))
                    write_type(result->at(0).gotype);
                else
                    write_params(result, true);
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

void Editor::trigger_parameter_hint() {
    ptr0(&parameter_hint);

    {
        SCOPED_MEM(&world.indexer.ui_mem);
        defer { world.indexer.ui_mem.reset(); };

        if (!world.indexer.ready) return; // strictly we can just call try_enter(), but want consistency with UI, which is based on `ready`
        if (!world.indexer.lock.try_enter()) return;
        defer { world.indexer.lock.leave(); };

        auto hint = world.indexer.parameter_hint(filepath, cur);
        if (hint == NULL) return;
        if (hint->gotype->type != GOTYPE_FUNC) return;

        {
            SCOPED_MEM(&world.parameter_hint_mem);
            world.parameter_hint_mem.reset();

            parameter_hint.token_changes.init();
            parameter_hint.gotype = hint->gotype->copy();
            parameter_hint.start = hint->call_args_start;

            Type_Renderer rend;
            rend.init();

            auto add_token_change = [&](int token) {
                auto c = parameter_hint.token_changes.append();
                c->token = token;
                c->index = rend.chars.len;
            };

            {
                auto t = parameter_hint.gotype;
                auto params = t->func_sig.params;
                auto result = t->func_sig.result;

                // write params
                rend.write("(");
                for (u32 i = 0; i < params->len; i++) {
                    auto &it = params->at(i);

                    add_token_change(HINT_NAME);
                    rend.write("%s ", it.name);
                    add_token_change(HINT_NORMAL);
                    rend.write_type(it.gotype);
                    if (i < params->len - 1)
                        rend.write(", ");
                }
                rend.write(")");

                // write result
                if (result != NULL && result->len > 0) {
                    rend.write(" ");
                    if (result->len == 1 && is_goident_empty(result->at(0).name))
                        rend.write_type(result->at(0).gotype);
                    else {
                        rend.write("(");
                        for (u32 i = 0; i < result->len; i++) {
                            if (!is_goident_empty(result->at(i).name))
                                rend.write("%s ", result->at(i).name);
                            rend.write_type(result->at(i).gotype);
                            if (i < result->len - 1)
                                rend.write(", ");
                        }
                        rend.write(")");
                    }
                }
            }

            parameter_hint.help_text = rend.finish();
        }
    }
}

void Editor::type_char(char ch) {
    uchar uch = ch;
    buf.insert(cur, &uch, 1);

    auto old_cur = cur;
    auto new_cur = buf.inc_cur(cur);

    raw_move_cursor(new_cur);
}

void Editor::update_parameter_hint() {
    // reset parameter hint when cursor goes before hint start
    auto& hint = parameter_hint;
    if (hint.gotype == NULL) return;

    auto should_close_hints = [&]() -> bool {
        if (!world.indexer.ready) return true;
        if (!world.indexer.lock.try_enter()) return true;
        defer { world.indexer.lock.leave(); };

        return !world.indexer.check_if_still_in_parameter_hint(filepath, cur, hint.start);
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

/*
// start_change()
curr_change.start_point = cur_to_tspoint(cur);
curr_change.old_end_point = curr_change.start_point;

// end_change()
auto end = cur_to_offset(cur);
if (end < curr_change.start_byte) {
    curr_change.start_byte = end;
    curr_change.start_point = cur_to_tspoint(cur);
} else if (curr_change.start_byte > 0) {
    auto start = tspoint_to_cur(curr_change.start_point);
    start = buf.dec_cur(start);
    curr_change.start_point = cur_to_tspoint(start);
    curr_change.start_byte = cur_to_offset(start);
}
curr_change.new_end_byte = end;
curr_change.new_end_point = cur_to_tspoint(cur);
*/

void Editor::type_char_in_insert_mode(char ch) {
    type_char(ch);

    if (!is_go_file) return;

    // at this point, tree is up to date! we can simply walk, don't need to re-parse :)

    bool did_autocomplete = false;
    bool did_parameter_hint = false;

    if (!isident(ch) && autocomplete.ac.results != NULL)
        ptr0(&autocomplete);

    switch (ch) {
    case '.':
        trigger_autocomplete(true, false);
        did_autocomplete = true;
        break;

    case ',':
    case '(':
        trigger_parameter_hint();
        did_parameter_hint = true;
        break;

    case '}':
    case ')':
    case ']':
        {
            if (!is_go_file) break;

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

            auto root_node = new_ast_node(ts_tree_root_node(buf.tree), &it);

            Ast_Node *rbrace_node = alloc_object(Ast_Node);
            bool rbrace_found = false;

            find_nodes_containing_pos(root_node, rbrace_pos, false, [&](auto it) {
                if (it->type() == brace_type) {
                    memcpy(rbrace_node, it, sizeof(Ast_Node));
                    rbrace_found = true;
                    return WALK_ABORT;
                }
                return WALK_CONTINUE;
            });

            if (!rbrace_found) break;

            auto walk_upwards = [&](auto curr) -> Ast_Node * {
                while (true) {
                    // try to get prev
                    auto prev = curr->prev_all();
                    if (!prev->null) return prev;

                    // unable to? get parent, try again
                    curr = curr->parent();
                    if (curr->null) return curr;
                }
            };

            auto curr = walk_upwards(rbrace_node);
            int depth = 1;

            i32 lbrace_line = -1;

            fn<void(Ast_Node*)> process_node = [&](Ast_Node* node) {
                if (lbrace_line != -1) return;
                if (node->is_missing()) return;

                SCOPED_FRAME();
                auto children = alloc_list<Ast_Node*>(node->all_child_count());
                FOR_ALL_NODE_CHILDREN (node) children->append(it);
                for (; children->len > 0; children->len--)
                    process_node(*children->last());

                if (node->type() == brace_type)
                    depth++;
                if (node->type() == other_brace_type) {
                    depth--;
                    if (depth == 0) {
                        lbrace_line = node->start().y;
                        return;
                    }
                }
            };

            for (; !curr->null && lbrace_line == -1; curr = walk_upwards(curr))
                process_node(curr);

            if (lbrace_line == -1) break;

            auto indentation = alloc_list<uchar>();
            For (buf.lines[lbrace_line]) {
                if (it == '\t' || it == ' ')
                    indentation->append(it);
                else
                    break;
            }

            // backspace to start of line
            backspace_in_insert_mode(0, cur.x);

            // insert indentation
            auto pos = cur;
            buf.insert(pos, indentation->items, indentation->len);
            pos.x += indentation->len;

            // insert the brace
            uchar uch = ch;
            buf.insert(pos, &uch, 1);
            pos.x++;

            // move cursor after everything we typed
            raw_move_cursor(pos);
        }
        break;
    }

    do {
        if (!isident(ch)) break;
        if (autocomplete.ac.results != NULL) break;

        auto c = cur;
        auto &line = buf.lines[c.y];

        while (c.x > 0 && isident(line[c.x-1]))
            c.x--;

        if (c == cur) break; // technically can't happen, i believe

        // c is now start of identifier

        if (last_closed_autocomplete == c) break;

        // don't open autocomplete before 3 chars
        if (cur.x - c.x < 3) break;

        did_autocomplete = true;
        trigger_autocomplete(false, false);
    } while (0);

    if (!did_autocomplete)
        if (autocomplete.ac.results != NULL)
            trigger_autocomplete(false, isident, ch);

    if (!did_parameter_hint) update_parameter_hint();
}

void Editor::update_autocomplete(bool triggered_by_ident) {
    if (autocomplete.ac.results != NULL)
        trigger_autocomplete(false, triggered_by_ident);
}

void Editor::backspace_in_insert_mode(int graphemes_to_erase, int codepoints_to_erase) {
    auto start = cur;

    if (graphemes_to_erase > 0) {
        // we have the current cursor as cp index
        // we want to move it back by one gr index
        // and then convert that back to cp index
        // what the fuck lmao, is backspace going to be this expensive?
        int gr_idx = buf.idx_cp_to_gr(start.y, start.x);
        start.x = buf.idx_gr_to_cp(start.y, relu_sub(gr_idx, graphemes_to_erase));
    } else if (codepoints_to_erase > 0) {
        start.x -= codepoints_to_erase;
    }

    if (start < nvim_insert.start) {
        // this assumes start and nvim_insert.start are on same line
        int gr_end = buf.idx_cp_to_gr(nvim_insert.start.y, nvim_insert.start.x);
        int gr_start = buf.idx_cp_to_gr(start.y, start.x);
        nvim_insert.deleted_graphemes += (gr_end - gr_start);

        nvim_insert.start = start;
    }

    buf.remove(start, cur);
    raw_move_cursor(start);

    last_closed_autocomplete = new_cur2(-1, -1);
}

void Editor::format_on_save(int fmt_type, bool write_to_nvim) {
    auto old_cur = cur;

    Buffer swapbuf; ptr0(&swapbuf);
    bool success = false;

    GHFmtStart();

    for (int i = 0; i < buf.lines.len; i++) {
        SCOPED_FRAME();

        List<char> line;
        line.init();

        For (buf.lines[i]) {
            char tmp[4];
            auto n = uchar_to_cstr(it, tmp);
            for (u32 j = 0; j < n; j++)
                line.append(tmp[j]);
        }

        line.append('\0');
        GHFmtAddLine(line.items);
    }

    auto new_contents = GHFmtFinish(fmt_type);
    if (new_contents == NULL) {
        saving = false;
        return;
    }

    int curr = 0;
    swapbuf.init(MEM, false);
    swapbuf.read([&](char *out) -> bool {
        if (new_contents[curr] != '\0') {
            *out = new_contents[curr++];
            return true;
        }
        return false;
    });

    /// auto was_dirty = buf.dirty;
    buf.copy_from(&swapbuf);
    buf.dirty = true;

    if (write_to_nvim) {
        auto &nv = world.nvim;
        auto &writer = nv.writer;

        // skip next update from nvim
        nvim_insert.skip_changedticks_until = nvim_data.changedtick + 1;

        auto msgid = nv.start_request_message("nvim_buf_set_lines", 5);
        auto req = nv.save_request(NVIM_REQ_POST_SAVE_SETLINES, msgid, id);
        req->post_save_setlines.cur = old_cur;

        writer.write_int(nvim_data.buf_id);
        writer.write_int(0);
        writer.write_int(-1);
        writer.write_bool(false);

        writer.write_array(buf.lines.len);
        For (buf.lines) nv.write_line(&it);
        nv.end_message();
    }
}

void Editor::handle_save(bool about_to_close) {
    saving = true;

    bool untitled = is_untitled;

    if (untitled) {
        Select_File_Opts opts;
        opts.buf = filepath;
        opts.bufsize = _countof(filepath);
        opts.folder = false;
        opts.save = true;
        opts.starting_folder = our_strcpy(world.current_path);
        if (!let_user_select_file(&opts)) return;

        if (!path_contains_in_subtree(world.current_path, filepath)) {
            tell_user("Unable to save file outside workspace.", "Error");
            return;
        }

        is_untitled = false;
        is_go_file = str_ends_with(filepath, ".go");

        if (is_go_file)
            buf.enable_tree();
    }

    format_on_save(GH_FMT_GOIMPORTS, !about_to_close);

    // save to disk
    {
        disable_file_watcher_until = current_time_in_nanoseconds() + (2 * 1000000000);

        File f;
        if (f.init(filepath, FILE_MODE_WRITE, FILE_CREATE_NEW) != FILE_RESULT_SUCCESS) {
            tell_user("Unable to save file.", "Error");
            return;
        }
        defer { f.cleanup(); };

        buf.write(&f);
        buf.dirty = false;
    }

    auto find_node = [&]() -> FT_Node * {
        auto curr = world.file_tree;
        auto subpath = get_path_relative_to(filepath, world.current_path);
        auto parts = make_path(subpath)->parts;

        if (parts->len == 0) return NULL;
        parts->len--; // chop off last, we want dirname

        For (*parts) {
            bool found = false;
            for (auto child = curr->children; child != NULL; child = child->next) {
                if (streq(child->name, it)) {
                    curr = child;
                    found = true;
                    break;
                }
            }
            if (!found) return NULL;
        }
        return curr;
    };

    auto node = find_node();
    if (node != NULL) {
        bool child_exists = false;
        auto filename = our_basename(filepath);

        for (auto child = node->children; child != NULL; child = child->next) {
            if (streq(child->name, filename)) {
                child_exists = true;
                break;
            }
        }

        if (!child_exists) {
            world.add_ft_node(node, [&](auto child) {
                child->is_directory = false;
                child->name = our_strcpy(filename);
            });
        }
    }
}

void go_to_error(int index) {
    auto &b = world.build;
    if (index < 0 || index >= b.errors.len) return;

    auto &error = b.errors[index];

    SCOPED_FRAME();

    auto path = path_join(world.current_path, error.file);
    auto pos = new_cur2(error.col-1, error.row-1);

    auto editor = world.find_editor([&](auto it) {
        return are_filepaths_equal(path, it->filepath);
    });

    // when build finishes, set marks on existing editors
    // when editor opens, get all existing errors and set marks

    if (editor == NULL || error.nvim_extmark == 0) {
        world.focus_editor(path, pos);
        return;
    }

    if (world.focus_editor(path) == NULL) return;

    auto &nv = world.nvim;
    auto msgid = nv.start_request_message("nvim_buf_get_extmark_by_id", 4);
    nv.save_request(NVIM_REQ_GOTO_EXTMARK, msgid, editor->id);
    nv.writer.write_int(editor->nvim_data.buf_id);
    nv.writer.write_int(b.nvim_namespace_id);
    nv.writer.write_int(error.nvim_extmark);
    nv.writer.write_map(0);
    nv.end_message();
}

void go_to_next_error(int direction) {
    auto &b = world.build;

    bool has_valid = false;
    For (b.errors) {
        if (it.valid) {
            has_valid = true;
            break;
        }
    }

    if (!b.ready() || !has_valid) return;

    auto old = b.current_error;
    do {
        b.current_error += direction;
        if (b.current_error < 0)
            b.current_error = b.errors.len - 1;
        if (b.current_error >= b.errors.len)
            b.current_error = 0;
    } while (b.current_error != old && !b.errors[b.current_error].valid);

    go_to_error(b.current_error);
}
