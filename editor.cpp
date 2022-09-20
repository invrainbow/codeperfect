#include "editor.hpp"
#include "os.hpp"
#include "world.hpp"
#include "ui.hpp"
#include "go.hpp"
#include "fzy_match.h"
#include "tree_sitter_crap.hpp"
#include "settings.hpp"
#include "set.hpp"
#include "unicode.hpp"
#include "defer.hpp"

bool Editor::is_modifiable() {
    return is_untitled || path_has_descendant(world.current_path, filepath);
}

ccstr Editor::get_autoindent(int for_y) {
    auto y = relu_sub(for_y, 1);

    while (true) {
        auto& line = buf->lines[y];
        for (u32 x = 0; x < line.len; x++)
            if (!isspace(line[x]))
                goto done;
        if (!y) break;
        y--;
    }
done:

    auto& line = buf->lines[y];
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
    if (!len) return;

    SCOPED_FRAME();

    auto text = cstr_to_ustr(s);
    if (!text->len) return;

    buf->insert(cur, text->items, text->len);
    auto c = cur;
    for (u32 i = 0; i < text->len; i++)
        c = buf->inc_cur(c);

    raw_move_cursor(c);
}

void Editor::perform_autocomplete(AC_Result *result) {
    auto& ac = autocomplete.ac;

    switch (result->type) {
    case ACR_POSTFIX: {
        // TODO: this currently only works for insert mode; support normal mode
        // also what if we just forced you to be in insert mode for autocomplete lol

        // remove everything but the operator
        raw_move_cursor(ac.operand_end);
        buf->remove(ac.operand_end, ac.keyword_end);

        // nice little dsl here lol

        ccstr autoindent_chars = NULL;
        ccstr operand_text = NULL;
        Postfix_Info *curr_postfix = NULL;

        auto insert_text = [&](ccstr fmt, ...) {
            SCOPED_FRAME();

            va_list vl;
            va_start(vl, fmt);
            insert_text_in_insert_mode(cp_vsprintf(fmt, vl));
            va_end(vl);
        };

        auto save_autoindent = [&]() {
            auto& line = buf->lines[cur.y];
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

            cp_assert(autoindent_chars);

            insert_text("%s", autoindent_chars);

            if (add) {
                ccstr tabs = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";
                cp_assert(add < strlen(tabs));
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
            operand_text = buf->get_text(ac.operand_start, ac.operand_end);

            raw_move_cursor(ac.operand_start);
            buf->remove(ac.operand_start, ac.operand_end);

            curr_postfix = postfix_stack.append();
            curr_postfix->start();
        };

        bool notfound = false;

        auto is_string = [&]() -> bool {
            auto gotype = ac.operand_gotype;
            if (gotype)
                if (gotype->type == GOTYPE_BUILTIN)
                    if (gotype->builtin_type == GO_BUILTIN_STRING)
                        return true;
            return false;
        };

        auto insert_is_empty = [&](bool invert) {
            if (is_string()) {
                insert_text("%s %s \"\"", operand_text, invert ? "!=" : "==");
            } else {
                insert_text(
                    "%s %s nil %s len(%s) %s 0",
                    operand_text,
                    invert ? "!=" : "==",
                    invert ? "&&" : "||" ,
                    operand_text,
                    invert ? "!=" : "=="
                );
            }
        };

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
            insert_is_empty(false);
            break;

        case PFC_NOTEMPTY:
            initialize_everything();
            insert_is_empty(true);
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
            insert_text("if ");
            insert_is_empty(false);
            insert_text(" {");
            save_autoindent();
            insert_newline(1);
            record_position();
            insert_newline(0);
            insert_text("}");
            record_position();
            break;

        case PFC_IFNOTEMPTY:
            initialize_everything();
            insert_text("if ");
            insert_is_empty(true);
            insert_text(" {");
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
        case PFC_FORVALUE: {
            ccstr keyname = "key";
            ccstr valuename = "val";

            auto gotype = ac.operand_gotype;
            if (gotype && (gotype->type == GOTYPE_SLICE || gotype->type == GOTYPE_ARRAY))
                keyname = "i";

            initialize_everything();

            if (result->postfix_operation == PFC_FORKEY)
                insert_text("for %s := range %s {", keyname, operand_text);
            else if (result->postfix_operation == PFC_FORVALUE)
                insert_text("for _, %s := range %s {", valuename, operand_text);
            else
                insert_text("for %s, %s := range %s {", keyname, valuename, operand_text);

            save_autoindent();
            insert_newline(1);
            record_position();
            insert_newline(0);
            insert_text("}");
            record_position();
            break;
        }

        case PFC_CHECK: {
                auto is_multi = ac.operand_gotype->type == GOTYPE_MULTI;
                if (!(is_multi || ac.operand_is_error_type)) break;

                int error_found_at = -1;
                auto multi_types = ac.operand_gotype->multi_types;

                if (is_multi) {
                    Fori (*multi_types) {
                        if (it->type == GOTYPE_ID && streq(it->id_name, "error")) {
                            error_found_at = i;
                            break;
                        }
                    }

                    if (error_found_at == -1) break;
                }

                initialize_everything();

                if (is_multi) {
                    int varcount = 0;

                    Fori (*multi_types) {
                        if (i == error_found_at) {
                            insert_text("err");
                        } else {
                            if (!varcount)
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
                } else {
                    insert_text("if err := %s; err != nil {", operand_text);
                    save_autoindent();
                    insert_newline(1);
                }

                {
                    bool ok = false;

                    do {
                        // get gotype of current function
                        auto functype = world.indexer.get_closest_function(filepath, cur);
                        if (!functype) break;

                        auto result = functype->func_sig.result;
                        if (!result  || !result->len) {
                            insert_text("return");
                            ok = true;
                            break;
                        }

                        bool error_found = false;
                        auto &ind = world.indexer;

                        auto get_zero_value_of_gotype = [&](Gotype *gotype) -> ccstr {
                            if (!gotype) return NULL;

                            Go_Ctx ctx; ptr0(&ctx);
                            ctx.import_path = ind.filepath_to_import_path(cp_dirname(filepath));
                            ctx.filename = cp_basename(filepath);

                            auto res = ind.evaluate_type(gotype, &ctx);
                            if (!res) return NULL;

                            auto rres = ind.resolve_type(res->gotype, res->ctx);
                            if (!rres) return NULL;

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

                        Fori (*result) {
                            auto val = get_zero_value_of_gotype(it.gotype);
                            if (i)
                                insert_text(", ");
                            insert_text(!val ? "nil" : val);
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

        if (curr_postfix) {
            if (curr_postfix->insert_positions.len > 1) {
                if (world.use_nvim)
                    trigger_escape(curr_postfix->insert_positions[0]);
                else
                    move_cursor(curr_postfix->insert_positions[0]);
                curr_postfix->current_insert_position++;
            } else {
                postfix_stack.len--;
            }
        }

        // clear autocomplete
        ptr0(&ac);
        break;
    }

    case ACR_KEYWORD:
    case ACR_DECLARATION:
    case ACR_IMPORT: {
        bool is_function = false;

        auto modify_string = [&](ccstr s) -> ccstr {
            switch (result->type) {
            case ACR_IMPORT:
                return cp_sprintf("%s.", s);

            case ACR_KEYWORD: {
                ccstr builtins_with_space[] = {
                    "package", "import", "const", "var", "func",
                    "type", "struct", "interface", "map", "chan",
                    "goto", "go", "defer", "if", "else",
                    "for", "select", "switch",
                };

                For (builtins_with_space)
                    if (streq(s, it))
                        return cp_sprintf("%s ", s);
                break;
            }

            case ACR_DECLARATION: {
                if (result->declaration_is_struct_literal_field)
                    return cp_sprintf("%s: ", s);

                auto godecl = result->declaration_godecl;
                if (!godecl) break;

                if (!world.indexer.acquire_lock(IND_READING)) break;
                defer { world.indexer.release_lock(IND_READING); };

                Go_Ctx ctx;
                ctx.import_path = result->declaration_import_path;
                ctx.filename = result->declaration_filename;

                auto res = world.indexer.evaluate_type(godecl->gotype, &ctx);
                if (!res) break;

                auto rres = world.indexer.resolve_type(res->gotype, res->ctx);
                if (!rres) break;

                auto gotype = rres->gotype;
                if (gotype->type != GOTYPE_FUNC) break;

                is_function = true;
                return cp_sprintf("%s(", s); // it's a func, add a '('
                break;
            }
            }

            return s;
        };

        auto see_if_we_need_autoimport = [&]() -> ccstr {
            if (result->type == ACR_IMPORT)
                return result->import_path;
            if (result->type == ACR_DECLARATION)
                return result->declaration_package;
            return NULL;
        };

        buf->hist_batch_mode = true;
        defer { buf->hist_batch_mode = false; };

        auto import_to_add = see_if_we_need_autoimport();
        if (import_to_add) {
            auto iter = alloc_object(Parser_It);
            iter->init(buf);
            auto root = new_ast_node(ts_tree_root_node(buf->tree), iter);

            Ast_Node *package_node = NULL;
            auto import_nodes = alloc_list<Ast_Node*>();

            FOR_NODE_CHILDREN (root) {
                if (it->type() == TS_PACKAGE_CLAUSE) {
                    package_node = it;
                } else if (it->type() == TS_IMPORT_DECLARATION) {
                    import_nodes->append(it);
                }
            }

            do {
                if (!import_nodes->len && !package_node) break;

                auto already_exists = [&]() {
                    For (*import_nodes) {
                        auto imports = alloc_list<Go_Import>();
                        world.indexer.import_decl_to_goimports(it, imports);

                        auto imp = imports->find([&](auto it) { return streq(it->import_path, import_to_add); });
                        if (imp) return true;
                    }
                    return false;
                };

                if (already_exists()) break;

                // we need to import, just insert it into the first import. format on save will fix the multiple imports

                Ast_Node *firstnode = import_nodes->len ? import_nodes->at(0) : NULL;

                Text_Renderer rend; rend.init();
                rend.write("import (\n");
                rend.write("\"%s\"\n", import_to_add);
                {
                    if (firstnode && cur > firstnode->end()) {
                        auto imports = alloc_list<Go_Import>();
                        world.indexer.import_decl_to_goimports(firstnode, imports);

                        For (*imports) {
                            switch (it.package_name_type) {
                            case GPN_IMPLICIT:  rend.write("\"%s\"", it.import_path);                       break;
                            case GPN_EXPLICIT:  rend.write("%s \"%s\"", it.package_name, it.import_path);   break;
                            case GPN_BLANK:     rend.write("_ \"%s\"", it.import_path);                     break;
                            case GPN_DOT:       rend.write(". \"%s\"", it.import_path);                     break;
                            }
                            rend.write("\n");
                        }
                    }
                }
                rend.write(")");

                GHFmtStart();
                GHFmtAddLine(rend.finish());
                GHFmtAddLine("");

                auto new_contents = GHFmtFinish(GH_FMT_GOIMPORTS);
                if (!new_contents) break;
                defer { GHFree(new_contents); };

                auto new_contents_len = strlen(new_contents);
                if (!new_contents_len) break;

                if (new_contents[new_contents_len-1] == '\n') {
                    new_contents[new_contents_len-1] = '\0';
                    new_contents_len--;
                }

                cur2 start, old_end, new_end;
                if (firstnode) {
                    start = firstnode->start();
                    old_end = firstnode->end();
                    if (world.use_nvim)
                        if (old_end.y >= nvim_insert.start.y) break;
                } else {
                    start = package_node->end();
                    old_end = package_node->end();
                }
                new_end = start;

                auto chars = alloc_list<uchar>();
                if (!firstnode) {
                    // add two newlines, it's going after the package decl
                    chars->append('\n');
                    chars->append('\n');
                    new_end.x = 0;
                    new_end.y += 2;
                }

                {
                    auto ustr = cstr_to_ustr(new_contents);
                    For (*ustr) {
                        chars->append(it);
                        if (it == '\n') {
                            new_end.x = 0;
                            new_end.y++;
                        } else {
                            new_end.x++;
                        }
                    }
                }

                // perform the edit
                {
                    if (start != old_end)
                        buf->remove(start, old_end);
                    buf->insert(start, chars->items, chars->len);
                }

                add_change_in_insert_mode(start, old_end, new_end);

                // do something with new_contents
            } while (0);
        }

        // this whole section is basically, like, "how do we replace text
        // and make sure nvim/ts are in sync"
        //
        // refactor this out somehow, we're already starting to copy paste

        auto name = cstr_to_ustr(modify_string(result->name));

        auto ac_start = cur;
        ac_start.x -= strlen(ac.prefix); // what if the prefix contains unicode?

        // perform the edit
        // i don't think we need to create a batch here? as long as it follows the flow of normal text editing
        buf->remove(ac_start, cur);
        buf->insert(ac_start, name->items, name->len);

        // move cursor forward
        raw_move_cursor(new_cur2(ac_start.x + name->len, ac_start.y));

        // clear out last_closed_autocomplete
        last_closed_autocomplete = new_cur2(-1, -1);

        // clear autocomplete
        ptr0(&ac);

        if (is_function) trigger_parameter_hint();

        if (import_to_add)
            if (result->type == ACR_IMPORT)
                trigger_autocomplete(true, false);
        break;
    }
    }
}

void Editor::add_change_in_insert_mode(cur2 start, cur2 old_end, cur2 new_end) {
    if (world.use_nvim) {
        auto change = nvim_insert.other_changes.append();
        change->start = start;
        change->end = old_end;

        {
            SCOPED_MEM(&nvim_insert.mem);

            change->lines.init(LIST_POOL, new_end.y - start.y + 1);
            for (int i = start.y; i <= new_end.y; i++) {
                auto line = change->lines.append();
                line->init(LIST_POOL, buf->lines[i].len);
                line->len = buf->lines[i].len;
                memcpy(line->items, buf->lines[i].items, sizeof(uchar) * line->len);
            }
        }

        if (old_end.y >= nvim_insert.start.y)
            cp_panic("can only add changes in insert mode before current change");
    }

    auto dy = new_end.y - old_end.y;

    if (world.use_nvim) {
        nvim_insert.start.y += dy;
        nvim_insert.old_end.y += dy;
    }

    raw_move_cursor(new_cur2(cur.x, cur.y + dy));
}

bool Editor::is_current_editor() {
    auto current_editor = get_current_editor();
    if (current_editor)
        if (current_editor->id == id)
            return true;
    return false;
}

Move_Cursor_Opts *default_move_cursor_opts() {
    auto ret = alloc_object(Move_Cursor_Opts);

    // set defaults
    ret->dont_add_to_history = false;
    ret->is_user_movement = false;

    return ret;
}

void Editor::raw_move_cursor(cur2 c, Move_Cursor_Opts *opts) {
    if (!is_main_thread) cp_panic("can't call this from outside main thread");

    if (!opts) opts = default_move_cursor_opts();

    if (c.y == -1) c = buf->offset_to_cur(c.x);
    if (c.y < 0 || c.y >= buf->lines.len) return;
    if (c.x < 0) return;

    auto line_len = buf->lines[c.y].len;
    if (c.x > line_len) {
        if (!world.use_nvim || world.nvim.mode == VI_INSERT)
            c.x = line_len;
        else
            c.x = relu_sub(line_len, 1);
    }

    cur = c;

    auto& line = buf->lines[c.y];

    u32 vx = 0;
    u32 i = 0;

    Grapheme_Clusterer gc;
    gc.init();
    gc.feed(line[0]);

    while (i < c.x) {
        if (line[i] == '\t') {
            vx += options.tabsize - (vx % options.tabsize);
            i++;
        } else {
            auto width = cp_wcwidth(line[i]);
            if (width == -1) width = 1;
            vx += width;

            i++;
            while (i < c.x && !gc.feed(line[i]))
                i++;
        }
    }

    savedvx = vx;

    if (vx < view.x)
        view.x = vx;
    if (vx >= view.x + view.w)
       view.x = vx - view.w + 1;
    if (relu_sub(cur.y, options.scrolloff) < view.y)
        view.y = relu_sub(cur.y, options.scrolloff);
    if (cur.y + options.scrolloff >= view.y + view.h)
        view.y = cur.y + options.scrolloff - view.h + 1;

    // If we're not using nvim, then we're using this function to trigger
    // certain "on cursor move" actions
    if (!world.use_nvim && opts->is_user_movement) {
        // clear out autocomplete
        ptr0(&autocomplete.ac);

        // force next change to be a new history entry
        buf->hist_force_push_next_change = true;
    }

    bool push_to_history = true;

    // what the fuck is this stupid pyramid of shit
    auto &nq = world.navigation_queue;
    if (nq.len) {
        auto &top = nq[0];
        if (top.editor_id == id) {
            if (top.pos == c) {
                push_to_history = false;
            } else {
                nq.remove(&nq[0]);
                if (nq.len) {
                    auto top = nq[0];
                    if (top.editor_id == id) {
                        if (top.pos == c)
                            push_to_history = false;
                        else
                            nq.len = 0;
                    }
                }
            }
        }
    }

    if (!opts->dont_add_to_history)
        if (push_to_history)
            if (get_current_editor() == this)
                world.history.push(id, c);
}

void Editor::ensure_cursor_on_screen_by_moving_view(Ensure_Cursor_Mode mode) {
    if (cur.y + options.scrolloff >= view.y + view.h)
        view.y = relu_sub(cur.y + options.scrolloff + 1, view.h);
    if (cur.y - options.scrolloff < view.y)
        view.y = relu_sub(cur.y, options.scrolloff + 1);

    if (mode == ECM_GOTO_DEF) {
        if (cur.y > view.y + view.h * 3 / 4)
            view.y = cur.y - (view.h * 2 / 5);
    }

    // TODO: handle x
}

void Editor::ensure_cursor_on_screen() {
    if (relu_sub(cur.y, options.scrolloff) < view.y)
        move_cursor(new_cur2((u32)cur.x, (u32)min(view.y + options.scrolloff, view.y + view.h)));
    if (cur.y + options.scrolloff >= view.y + view.h)
        move_cursor(new_cur2((u32)cur.x, (u32)relu_sub(view.y + view.h,  1 + options.scrolloff)));

    // TODO: handle x
}

void Editor::update_lines(int firstline, int lastline, List<uchar*> *new_lines, List<s32> *line_lengths) {
    if (lastline == -1) lastline = buf->lines.len;

    auto start_cur = new_cur2(0, (i32)firstline);
    auto old_end_cur = new_cur2(0, (i32)lastline);

    if (lastline >= buf->lines.len && buf->lines.len > 0) {
        // Say firstline = 4 and lastline = 7. It wants to delete everything
        // from line 4 onwards. We need to decrement start_cur so it becomes
        // 3:(last of line 3), so that there isn't an extra empty line 4 at the
        // end.
        start_cur = buf->dec_cur(start_cur);

        old_end_cur = new_cur2((i32)buf->lines.last()->len, (i32)buf->lines.len - 1);
    }

    buf->internal_start_edit(start_cur, old_end_cur);

    buf->internal_delete_lines(firstline, lastline);
    for (u32 i = 0; i < new_lines->len; i++) {
        auto line = new_lines->at(i);
        auto len = line_lengths->at(i);
        buf->internal_insert_line(firstline + i, line, len);
    }

    if (!buf->lines.len)
        buf->internal_append_line(NULL, 0);

    auto new_end_cur = new_cur2(0, firstline + new_lines->len);
    if (firstline + new_lines->len == buf->lines.len) {
        if (!buf->lines.len)
            new_end_cur = new_cur2(0, 0);
        else
            new_end_cur = new_cur2((i32)buf->lines.last()->len, (i32)buf->lines.len - 1);
    }

    buf->internal_finish_edit(new_end_cur);
}

void Editor::move_cursor(cur2 c, Move_Cursor_Opts *opts) {
    if (world.use_nvim && world.nvim.mode == VI_INSERT) {
        nvim_data.waiting_for_move_cursor = true;
        nvim_data.move_cursor_to = c;

        // clear out last_closed_autocomplete
        last_closed_autocomplete = new_cur2(-1, -1);

        trigger_escape();
        return;
    }

    raw_move_cursor(c, opts);

    if (world.use_nvim) {
        auto& nv = world.nvim;
        auto msgid = nv.start_request_message("nvim_win_set_cursor", 2);
        nv.writer.write_int(nvim_data.win_id);
        {
            nv.writer.write_array(2);
            nv.writer.write_int(cur.y + 1);
            nv.writer.write_int(buf->idx_cp_to_byte(cur.y, cur.x));
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
                    cp_sprintf("Sorry, we're not yet able to open files containing lines with more than %d characters.", CHUNKMAX),
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
    if (because_of_file_watcher && disable_file_watcher_until > current_time_nano())
        return;

    auto fm = map_file_into_memory(filepath);
    if (!fm) {
        // don't error here
        // tell_user(cp_sprintf("Unable to open %s for reading: %s", filepath, get_last_error()), "Error opening file");
        return;
    }
    defer { fm->cleanup(); };

    if (!check_file(fm)) return;

    /*
    if (buf->initialized)
        buf->cleanup();
    buf->init(&mem, is_go_file);
    */

    print("=== reloading %s", filepath);
    buf->read(fm, true);

    if (world.use_nvim) {
        nvim_data.got_initial_lines = false;

        auto& nv = world.nvim;
        nv.start_request_message("nvim_buf_set_lines", 5);
        nv.writer.write_int(nvim_data.buf_id);
        nv.writer.write_int(0);
        nv.writer.write_int(-1);
        nv.writer.write_bool(false);
        nv.writer.write_array(buf->lines.len);
        For (buf->lines) nv.write_line(&it);
        nv.end_message();
    }
}

bool Editor::load_file(ccstr new_filepath) {
    reset_state();

    if (buf->initialized)
        buf->cleanup();

    is_go_file = (new_filepath && str_ends_with(new_filepath, ".go"));
    buf->init(&mem, is_go_file, !world.use_nvim);

    FILE* f = NULL;
    if (new_filepath) {
        auto path = get_normalized_path(new_filepath);
        if (!path)
            return error("unable to normalize filepath"), false;

        cp_strcpy_fixed(filepath, path);

        auto fm = map_file_into_memory(filepath);
        if (!fm) {
            tell_user(cp_sprintf("Unable to open %s for reading: %s", filepath, get_last_error()), "Error opening file");
            return false;
        }
        defer { fm->cleanup(); };

        if (is_binary((ccstr)fm->data, fm->len))
            if (ask_user_yes_no("This file appears to be a binary file. Attempting to open it as text could have adverse results. Do you still want to try?", "Binary file encountered", "Open", "Don't Open") != ASKUSER_YES)
                return false;

        if (!check_file(fm)) return false;

        buf->read(fm);
    } else {
        is_untitled = true;
        uchar tmp = '\0';
        buf->insert(new_cur2(0, 0), &tmp, 0);
        buf->dirty = false;
    }

    if (world.use_nvim) {
        auto& nv = world.nvim;
        auto msgid = nv.start_request_message("nvim_create_buf", 2);
        nv.save_request(NVIM_REQ_CREATE_BUF, msgid, id);

        nv.writer.write_bool(false);
        nv.writer.write_bool(true);
        nv.end_message();
    }

    auto &b = world.build;
    if (b.ready()) {
        auto editor_path = get_path_relative_to(filepath, world.current_path);
        For (b.errors) {
            if (is_mark_valid(it.mark)) continue;
            if (!it.valid) continue;
            if (!are_filepaths_equal(editor_path, it.file)) continue;

            // create mark
            auto pos = new_cur2(it.col - 1, it.row - 1);
            buf->mark_tree.insert_mark(MARK_BUILD_ERROR, pos, it.mark);
        }
    }

    // fill in search results
    if (world.searcher.state == SEARCH_SEARCH_DONE) {
        For (world.searcher.search_results) {
            if (!are_filepaths_equal(it.filepath, filepath)) continue;

            For (*it.results) {
                if (!it.mark_start) cp_panic("mark_start was null");
                if (!it.mark_end) cp_panic("mark_end was null");

                if (it.mark_start->valid) cp_panic("this shouldn't be happening");
                if (it.mark_end->valid) cp_panic("this shouldn't be happening");

                buf->mark_tree.insert_mark(MARK_SEARCH_RESULT, it.match_start, it.mark_start);
                buf->mark_tree.insert_mark(MARK_SEARCH_RESULT, it.match_end, it.mark_end);
            }
        }
    }

    return true;
}

bool handle_auth_error() {
    if (!world.auth_error) return false;

    switch (world.auth.state) {
    case AUTH_TRIAL:
        tell_user("Your trial has expired. As a result, opening files is currently disabled.\n\nPlease buy a license key using Help > Buy a License, and enter it using Help > Enter License.", "Trial expired");
        break;
    case AUTH_REGISTERED:
        switch (world.auth_status) {
        case GH_AUTH_BADCREDS:
            tell_user("We were unable to validate your license key, and the grace period has passed. As a result, opening files is currently disabled.\n\nPlease buy a license key using Help > Buy a License, and enter it using Help > Enter License.", "Invalid license key");
            break;
        case GH_AUTH_INTERNETERROR:
            tell_user("We were unable to connect to the internet to validate your license key, and the grace period has passed. As a result, opening files is currently disabled.\n\nPlease buy a license key using Help > Buy a License, and enter it using Help > Enter License.", "No internet connection");
            break;
        }
        break;
    }
    return true;
}

Editor* Pane::open_empty_editor() {
    if (handle_auth_error())
        return NULL;

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

i32 Editor::cur_to_offset(cur2 c) {
    return buf->cur_to_offset(c);
}

i32 Editor::cur_to_offset() { return cur_to_offset(cur); }

cur2 Editor::offset_to_cur(i32 offset) {
    return buf->offset_to_cur(offset);
}

int Editor::get_indent_of_line(int y) {
    auto& line = buf->lines[y];

    int indent = 0;
    for (; indent < line.len; indent++)
        if (line[indent] != '\t')
            break;
    return indent;
}

Buffer_It Editor::iter() { return iter(cur); }
Buffer_It Editor::iter(cur2 _cur) { return buf->iter(_cur); }

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

Editor* Pane::focus_editor(ccstr path, cur2 pos, bool pos_in_byte_format) {
    u32 i = 0;
    For (editors) {
        // TODO: use are_filepaths_equal instead, don't have to access filesystem
        if (are_filepaths_same_file(path, it.filepath))
            return focus_editor_by_index(i, pos, pos_in_byte_format);
        i++;
    }

    if (handle_auth_error())
        return NULL;

    auto ed = editors.append();
    ed->init();
    if (!ed->load_file(path)) {
        ed->cleanup();
        editors.len--;
        return NULL;
    }

    ui.recalculate_view_sizes(true);
    return focus_editor_by_index(editors.len - 1, pos, pos_in_byte_format);
}

Editor *Pane::focus_editor_by_index(u32 idx) {
    return focus_editor_by_index(idx, new_cur2(-1, -1));
}

bool Editor::trigger_escape(cur2 go_here_after) {
    if (go_here_after.x == -1 && go_here_after.y == -1)
        postfix_stack.len = 0;

    bool handled = false;

    if (autocomplete.ac.results) {
        handled = true;
        ptr0(&autocomplete.ac);

        {
            auto c = cur;
            auto &line = buf->lines[c.y];
            while (c.x > 0 && isident(line[c.x-1])) c.x--;
            if (c.x < cur.x)
                last_closed_autocomplete = c;
        }
    }

    if (parameter_hint.gotype) {
        handled = true;
        parameter_hint.gotype = NULL;
    }

    if (world.use_nvim && world.nvim.mode == VI_INSERT) {
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
            skip_next_nvim_update(nvim_insert.other_changes.len + 1);

            auto start = nvim_insert.start;
            auto old_end = nvim_insert.old_end;

            u32 delete_len = nvim_insert.deleted_graphemes;

            post_insert_dotrepeat_time = current_time_nano();

            // set new lines
            auto msgid = nv.start_request_message("nvim_call_atomic", 1);
            nv.save_request(NVIM_REQ_POST_INSERT_DOTREPEAT_REPLAY, msgid, id);
            {
                writer.write_array(nvim_insert.other_changes.len + 8);

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
                                nv.write_line(&buf->lines[y]);
                        }
                    }
                }

                writer.write_array(2);
                writer.write_string("nvim_win_set_cursor");
                {
                    writer.write_array(2);
                    {
                        writer.write_int(nvim_data.win_id);
                        writer.write_array(2);
                        {
                            nv.writer.write_int(cur.y + 1);
                            nv.writer.write_int(buf->idx_cp_to_byte(cur.y, cur.x));
                        }
                    }
                }

                {
                    writer.write_array(2);
                    writer.write_string("nvim_set_option");
                    {
                        writer.write_array(2);
                        writer.write_string("eventignore");
                        writer.write_string("BufWinEnter,BufEnter,BufLeave");
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
                    writer.write_string("nvim_set_option");
                    {
                        writer.write_array(2);
                        writer.write_string("eventignore");
                        writer.write_string("");
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

                    auto it = buf->iter(nvim_insert.start);
                    while (it.pos < cur) {
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

            /*
            // move cursor
            {
                auto c = cur;
                if (c.x) {
                    int gr_idx = buf->idx_cp_to_gr(c.y, c.x);
                    c.x = buf->idx_gr_to_cp(c.y, relu_sub(gr_idx, 1));
                }
                raw_move_cursor(c);
            }
            */

            // reset other_changes and mem
            nvim_insert.other_changes.len = 0;
            nvim_insert.mem.reset();
        }

        handled = true;

        go_here_after_escape = go_here_after;
        nv.exiting_insert_mode = true;
        nv.editor_that_triggered_escape = id;
    } else if (world.use_nvim && world.nvim.mode != VI_NORMAL) {
        send_nvim_keys("<Esc>");
        handled = true;
    }

    return handled;
}

void Pane::set_current_editor(u32 idx) {
    current_editor = idx;

    // focus current editor in file explorer

    if (world.file_tree_busy) return;

    auto ed = get_current_editor();
    if (!ed) return;
    if (ed->is_untitled) return;

    auto edpath = get_path_relative_to(ed->filepath, world.current_path);
    auto node = find_ft_node(edpath);
    if (!node) return;

    world.file_explorer.scroll_to = node;
    world.file_explorer.selection = node;

    for (auto it = node->parent; it && it->parent; it = it->parent)
        it->open = true;
}

Editor *Pane::focus_editor_by_index(u32 idx, cur2 pos, bool pos_in_byte_format) {
    if (current_editor != idx) {
        auto e = get_current_editor();
        if (e) e->trigger_escape();
    }

    set_current_editor(idx);

    auto &editor = editors[idx];

    auto cppos = pos;
    if (pos_in_byte_format)
        cppos.x = editor.buf->idx_byte_to_cp(cppos.y, cppos.x);

    if (pos.x != -1) {
        if (editor.is_nvim_ready()) {
            editor.move_cursor(cppos);
        } else {
            editor.nvim_data.need_initial_pos_set = true;
            editor.nvim_data.initial_pos = cppos;
            editor.raw_move_cursor(cppos);
        }
    }

    if (world.nvim.current_win_id != editor.id) {
        if (editor.nvim_data.win_id)
            world.nvim.set_current_window(&editor);
        else
            world.nvim.waiting_focus_window = editor.id;
    }

    return &editor;
}

Editor* Pane::get_current_editor() {
    if (!editors.len) return NULL;
    if (current_editor == -1) return NULL;

    return &editors[current_editor];
}

bool Editor::is_nvim_ready() {
    // will this fix things?
    if (!world.use_nvim) return true;

    return world.nvim.is_ui_attached
        && nvim_data.is_buf_attached
        && nvim_data.buf_id
        && nvim_data.win_id
        && nvim_data.got_initial_lines
        && nvim_data.got_initial_cur;
}

void Editor::init() {
    ptr0(this);
    id = ++world.next_editor_id;

    mem.init("editor mem");

    {
        SCOPED_MEM(&mem);
        postfix_stack.init();
        nvim_insert.other_changes.init();
        buf = alloc_object(Buffer);
    }

    nvim_insert.mem.init("nvim_insert mem");
}

void Editor::cleanup() {
    auto &nv = world.nvim;

    if (nvim_data.win_id) {
        nv.start_request_message("nvim_win_close", 2);
        nv.writer.write_int(nvim_data.win_id);
        nv.writer.write_bool(true);
        nv.end_message();
    }

    if (nvim_data.buf_id) {
        nv.start_request_message("nvim_buf_delete", 2);
        nv.writer.write_int(nvim_data.buf_id);
        {
            nv.writer.write_map(2);
            nv.writer.write_string("force"); nv.writer.write_bool(true);
            nv.writer.write_string("unload"); nv.writer.write_bool(false);
        }
        nv.end_message();
    }

    buf->cleanup();
    mem.cleanup();

    world.history.remove_invalid_marks();
}

bool Editor::cur_is_inside_comment_or_string() {
    auto root = new_ast_node(ts_tree_root_node(buf->tree), NULL);
    bool ret = false;

    find_nodes_containing_pos(root, cur, false, [&](auto it) {
        switch (it->type()) {
        case TS_RAW_STRING_LITERAL:
        case TS_INTERPRETED_STRING_LITERAL:
            if (cur == it->start() || cur == it->end())
                break;
            ret = true;
            return WALK_ABORT;

        case TS_COMMENT:
            ret = true;
            return WALK_ABORT;
        }
        return WALK_CONTINUE;

    }, true);

    return ret;
}

// basically the rule is, if autocomplete comes up empty ON FIRST OPEN, then keep it closed

void Editor::trigger_autocomplete(bool triggered_by_dot, bool triggered_by_typing_ident, uchar typed_ident_char) {
    if (!is_go_file) return;

    if (cur_is_inside_comment_or_string()) {
        return;
    }

    bool ok = false;
    defer {
        if (!ok) {
            auto c = cur;
            auto &line = buf->lines[c.y];
            while (c.x > 0 && isident(line[c.x-1])) c.x--;
            if (c.x < cur.x)
                last_closed_autocomplete = c;
        }
    };

    auto &ind = world.indexer;

    if (autocomplete.ac.results && triggered_by_typing_ident) {
        SCOPED_MEM(&world.autocomplete_mem);
        autocomplete.ac.prefix = cp_sprintf("%s%c", autocomplete.ac.prefix, typed_ident_char);
        autocomplete.ac.keyword_end.x++;
    } else {
        auto old_type = autocomplete.ac.type;
        auto old_keyword_start = autocomplete.ac.keyword_start;

        ptr0(&autocomplete);

        SCOPED_MEM(&ind.ui_mem);
        defer { ind.ui_mem.reset(); };

        if (!ind.acquire_lock(IND_READING, true)) return;
        defer { ind.release_lock(IND_READING); };

        Autocomplete ac; ptr0(&ac);

        Timer t;
        t.init();

        if (!ind.autocomplete(filepath, cur, triggered_by_dot, &ac)) return;

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
            autocomplete.ac.prefix = cp_strdup(ac.prefix);
            autocomplete.ac.results = new_results;
        }
    }

    ccstr wksp_import_path = NULL;

    if (ind.try_acquire_lock(IND_READING)) {
        defer { ind.release_lock(IND_READING); };

        // only needs to live for duration of function
        wksp_import_path = cp_strdup(ind.index.current_import_path);
    }

    {
        auto prefix = autocomplete.ac.prefix;
        auto results = autocomplete.ac.results;

        bool prefix_is_empty = strlen(prefix) == 0;

        autocomplete.filtered_results->len = 0;

        Timer t;
        t.init("autocomplete");

        Fori (*results)
            if (fzy_has_match(prefix, it.name))
                autocomplete.filtered_results->append(i);

        t.log("matching");

        /*
        if (!autocomplete.filtered_results->len) {
            ptr0(&autocomplete);
            return;
        }
        */

        struct Score {
            double fzy_score;
            AC_Result_Type result_type;
            bool is_struct_literal;
            int str_length;
            int field_order;
            int field_depth;

            bool import_in_workspace;
            bool import_in_file;
        };

        auto compare_scores = [&](Score *a, Score *b) -> int {
            // ACR_DECLARATION comes before ACR_POSTFIX
            if (autocomplete.ac.type == AUTOCOMPLETE_DOT_COMPLETE) {
                auto at = a->result_type;
                auto bt = b->result_type;

                if (at != bt)
                    if (at == ACR_POSTFIX || bt == ACR_POSTFIX)
                        return at == ACR_POSTFIX ? -1 : 1;
            }

            if (a->is_struct_literal != b->is_struct_literal)
                return a->is_struct_literal ? 1 : -1;

            if (a->fzy_score != b->fzy_score && !prefix_is_empty)
                return a->fzy_score > b->fzy_score ? 1 : -1;

            if (a->result_type == ACR_IMPORT && b->result_type == ACR_IMPORT) {
                if (a->import_in_file != b->import_in_file)
                    return a->import_in_file ? 1 : -1;
                if (a->import_in_workspace != b->import_in_workspace)
                    return a->import_in_workspace ? 1 : -1;
            }

            if (a->str_length != b->str_length)
                return b->str_length - a->str_length;

            if (a->result_type == ACR_KEYWORD && b->result_type == ACR_DECLARATION)
                return 1;
            if (a->result_type == ACR_DECLARATION && b->result_type == ACR_KEYWORD)
                return -1;

            if (a->field_depth != -1 && b->field_depth != -1) {
                if (a->field_depth != b->field_depth)
                    return b->field_depth - a->field_depth;
                return b->field_order - a->field_order;
            }

            return 0;
        };

        auto filtered_results = autocomplete.filtered_results;
        auto scores = alloc_array(Score, results->len);

        for (int i = 0; i < filtered_results->len; i++) {
            auto idx = filtered_results->at(i);
            auto &it = results->at(idx);
            auto &score = scores[idx];
            ptr0(&score);

            score.fzy_score = fzy_match(prefix, it.name);
            score.result_type = it.type;
            score.str_length = strlen(it.name);

            if (it.type == ACR_DECLARATION) {
                if (it.declaration_is_struct_literal_field)
                    score.is_struct_literal = true;
                if (it.declaration_godecl->type == GODECL_FIELD) {
                    score.field_depth = it.declaration_godecl->field_depth;
                    score.field_order = it.declaration_godecl->field_order;
                } else {
                    score.field_depth = -1;
                }
            }

            if (it.type == ACR_IMPORT) {
                if (it.import_is_existing)
                    score.import_in_file = true;
                if (wksp_import_path)
                    if (path_has_descendant(wksp_import_path, it.import_path))
                        score.import_in_workspace = true;
            }
        }

        filtered_results->sort([&](int *ia, int *ib) -> int {
            return -compare_scores(&scores[*ia], &scores[*ib]);
        });

        autocomplete.selection = 0;
        autocomplete.view = 0;
        t.log("scoring");
    }

    ok = true;
    return;
}

bool is_goident_empty(ccstr name) {
    return (!name || name[0] == '\0' || streq(name, "_"));
}

void Editor::trigger_parameter_hint() {
    if (!is_go_file) return;

    ptr0(&parameter_hint);

    {
        SCOPED_MEM(&world.indexer.ui_mem);
        defer { world.indexer.ui_mem.reset(); };

        if (!world.indexer.try_acquire_lock(IND_READING)) return;
        defer { world.indexer.release_lock(IND_READING); };

        auto hint = world.indexer.parameter_hint(filepath, cur);
        if (!hint) return;
        if (hint->gotype->type != GOTYPE_FUNC) {
            hint->gotype = NULL;
            return;
        }

        {
            SCOPED_MEM(&world.parameter_hint_mem);
            world.parameter_hint_mem.reset();

            parameter_hint.gotype = hint->gotype->copy();
            parameter_hint.current_param = hint->current_param;
            parameter_hint.start = hint->call_args_start;
        }
    }
}

void Editor::type_char(uchar uch) {
    buf->insert(cur, &uch, 1);

    auto old_cur = cur;
    auto new_cur = buf->inc_cur(cur);

    raw_move_cursor(new_cur);
}

void Editor::update_parameter_hint() {
    // reset parameter hint when cursor goes before hint start
    auto& hint = parameter_hint;
    if (!hint.gotype) return;

    auto should_close_hints = [&]() -> bool {
        if (!world.indexer.try_acquire_lock(IND_READING)) return true;
        defer { world.indexer.release_lock(IND_READING); };

        return !world.indexer.check_if_still_in_parameter_hint(filepath, cur, hint.start);
    };

    if (should_close_hints()) {
        hint.gotype = NULL;
    } else {
        Parser_It it;
        it.init(buf);

        auto tree = ts_tree_copy(buf->tree);
        auto root_node = new_ast_node(ts_tree_root_node(tree), &it);

        Parsed_File pf;
        pf.root = root_node;
        pf.tree = tree;
        pf.it = &it;
        pf.tree_belongs_to_editor = true;
        pf.editor_parser = buf->parser;

        if (!world.indexer.truncate_parsed_file(&pf, cur, "_)}}}}}}}}}}}}}}}}")) return;

        Ast_Node *arglist = NULL;

        find_nodes_containing_pos(pf.root, cur, false, [&](auto it) {
            if (it->type() == TS_ARGUMENT_LIST) {
                if (!arglist)
                    arglist = alloc_object(Ast_Node);
                memcpy(arglist, it, sizeof(Ast_Node));
            }
            return WALK_CONTINUE;
        });

        if (!arglist) return;

        auto commas = alloc_list<cur2>();

        FOR_ALL_NODE_CHILDREN (arglist) {
            if (it->type() == TS_COMMA)
                commas->append(it->start());
        }

        // not even gonna bother binary searching lol

        int current_param = -1;
        Fori (*commas)
            if (cur <= it)
                current_param = i;

        if (current_param == -1)
            current_param = commas->len;

        auto num_params = hint.gotype->func_sig.params->len;
        if (current_param > num_params - 1)
            current_param = num_params - 1;

        hint.current_param = current_param;
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
    start = buf->dec_cur(start);
    curr_change.start_point = cur_to_tspoint(start);
    curr_change.start_byte = cur_to_offset(start);
}
curr_change.new_end_byte = end;
curr_change.new_end_point = cur_to_tspoint(cur);
*/

void Editor::type_char_in_insert_mode(uchar ch) {
    bool already_typed = false;

    // handle typing a dot when an import is selected
    do {
        if (ch != '.') break;

        if (!autocomplete.ac.results) break;
        if (autocomplete.selection >= autocomplete.filtered_results->len) break;

        auto idx = autocomplete.filtered_results->at(autocomplete.selection);
        auto &result = autocomplete.ac.results->at(idx);
        if (result.type != ACR_IMPORT) break;

        // need a full match
        if (!streq(autocomplete.ac.prefix, result.name)) break;

        perform_autocomplete(&result);
        already_typed = true;
    } while (0);

    if (!already_typed) type_char(ch);

    if (!is_go_file) return;

    // at this point, tree is up to date! we can simply walk, don't need to re-parse :)

    bool did_autocomplete = false;
    bool did_parameter_hint = false;

    if (!isident(ch) && autocomplete.ac.results)
        ptr0(&autocomplete);

    switch (ch) {
    case '.':
        trigger_autocomplete(true, false);
        did_autocomplete = true;
        break;

    case '(':
        trigger_parameter_hint();
        did_parameter_hint = true;
        break;

    case ',':
        if (!parameter_hint.gotype) {
            trigger_parameter_hint();
            did_parameter_hint = true;
        }
        break;

    case '}':
    case ')':
    case ']': {
        if (!is_go_file) break;

        if (!cur.x) break;

        auto rbrace_pos = buf->dec_cur(cur);

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

        auto &line = buf->lines[rbrace_pos.y];
        bool starts_with_spaces = true;
        for (u32 x = 0; x < rbrace_pos.x; x++) {
            if (line[x] != ' ' && line[x] != '\t') {
                starts_with_spaces = false;
                break;
            }
        }

        if (!starts_with_spaces) break;

        Parser_It it;
        it.init(buf);

        auto root_node = new_ast_node(ts_tree_root_node(buf->tree), &it);

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
                if (!depth) {
                    lbrace_line = node->start().y;
                    return;
                }
            }
        };

        for (; !curr->null && lbrace_line == -1; curr = walk_upwards(curr))
            process_node(curr);

        if (lbrace_line == -1) break;

        auto indentation = alloc_list<uchar>();
        For (buf->lines[lbrace_line]) {
            if (it == '\t' || it == ' ')
                indentation->append(it);
            else
                break;
        }

        // backspace to start of line
        backspace_in_insert_mode(0, cur.x);

        // insert indentation
        auto pos = cur;
        buf->insert(pos, indentation->items, indentation->len);
        pos.x += indentation->len;

        // insert the brace
        buf->insert(pos, &ch, 1);
        pos.x++;

        // move cursor after everything we typed
        raw_move_cursor(pos);
        break;
    }
    }

    do {
        if (!isident(ch)) break;
        if (autocomplete.ac.results) break;

        auto c = cur;
        auto &line = buf->lines[c.y];

        while (c.x > 0 && isident(line[c.x-1]))
            c.x--;

        if (c == cur) break; // technically can't happen, i believe

        // c is now start of identifier

        if (last_closed_autocomplete == c) break;

        // don't open autocomplete before 2 chars
        if (cur.x - c.x < 2) break;

        did_autocomplete = true;
        trigger_autocomplete(false, false);
    } while (0);

    if (!did_autocomplete)
        if (autocomplete.ac.results)
            trigger_autocomplete(false, isident, ch);

    if (!did_parameter_hint) update_parameter_hint();
}

void Editor::update_autocomplete(bool triggered_by_ident) {
    if (autocomplete.ac.results)
        trigger_autocomplete(false, triggered_by_ident);
}

void Editor::backspace_in_insert_mode(int graphemes_to_erase, int codepoints_to_erase) {
    auto start = cur;
    auto zero = new_cur2(0, 0);

    if (graphemes_to_erase > 0 && codepoints_to_erase > 0)
        cp_panic("backspace_in_insert_mode called with both graphemes and codepoints");

    while ((graphemes_to_erase > 0 || codepoints_to_erase > 0) && start > zero) {
        if (!start.x) {
            start = buf->dec_cur(start);

            if (graphemes_to_erase > 0) graphemes_to_erase--;
            if (codepoints_to_erase > 0) codepoints_to_erase--;

            if (start < nvim_insert.start)
                nvim_insert.deleted_graphemes++;
            continue;
        }

        auto old_start = start;

        if (graphemes_to_erase > 0) {
            // we have current cursor as cp index
            // move it back by 1 gr index, then convert that back to cp index
            int gr_idx = buf->idx_cp_to_gr(start.y, start.x);
            if (gr_idx > graphemes_to_erase) {
                start.x = buf->idx_gr_to_cp(start.y, gr_idx - graphemes_to_erase);
                graphemes_to_erase = 0;
            } else {
                start.x = 0;
                graphemes_to_erase -= gr_idx;
            }
        } else { // codepoints_to_erase > 0
            auto gr_idx = buf->idx_cp_to_gr(start.y, start.x);
            if (codepoints_to_erase > start.x) {
                start.x = 0;
                codepoints_to_erase -= start.x;
            } else {
                start.x -= codepoints_to_erase;
                codepoints_to_erase = 0;
            }
        }

        if (start < nvim_insert.start) {
            if (old_start.y != nvim_insert.start.y)
                cp_panic("this shouldn't happen");

            int lo = buf->idx_cp_to_gr(start.y, start.x);
            int hi = buf->idx_cp_to_gr(start.y, min(old_start.x, nvim_insert.start.x));
            nvim_insert.deleted_graphemes += (hi - lo);
        }
    }

    if (start < nvim_insert.start)
        nvim_insert.start = start;

    buf->remove(start, cur);
    raw_move_cursor(start);

    last_closed_autocomplete = new_cur2(-1, -1);
}

bool Editor::optimize_imports() {
    SCOPED_MEM(&world.indexer.ui_mem);
    defer { world.indexer.ui_mem.reset(); };

    if (!world.indexer.try_acquire_lock(IND_READING)) return false;
    defer { world.indexer.release_lock(IND_READING); };

    auto imports = world.indexer.optimize_imports(filepath);
    if (!imports) return false;
    if (!imports->len) return false;

    // add imports into the file
    do {
        auto iter = alloc_object(Parser_It);
        iter->init(buf);
        auto root = new_ast_node(ts_tree_root_node(buf->tree), iter);

        Ast_Node *package_node = NULL;
        Ast_Node *first_imports_node = NULL;
        Ast_Node *last_imports_node = NULL;

        auto cgo_imports = alloc_list<Ast_Node*>();

        auto is_cgo_import = [&](Ast_Node *it) {
            auto imports = alloc_list<Go_Import>();
            world.indexer.import_decl_to_goimports(it, imports);
            return imports->len == 1 && streq(imports->at(0).import_path, "C");
        };

        FOR_NODE_CHILDREN (root) {
            switch (it->type()) {
            case TS_PACKAGE_CLAUSE:
                package_node = it;
                break;
            case TS_IMPORT_DECLARATION:
                if (!first_imports_node)
                    first_imports_node = it;
                else
                    last_imports_node = it;

                if (is_cgo_import(it)) cgo_imports->append(it);
                break;
            default:
                if (first_imports_node) goto done;
            }
        }
    done:

        if (!first_imports_node && !package_node) break;

        cur2 first_import_start;

        if (first_imports_node) {
            first_import_start = first_imports_node->start();
            if (!last_imports_node)
                last_imports_node = first_imports_node;
        }

        auto cgo_imports_text = alloc_list<char>();

        For (*cgo_imports) {
            auto startnode = it;
            while (true) {
                auto prev = startnode->prev_all(false);
                if (prev->type() != TS_COMMENT) break;
                startnode = prev;
            }

            auto start = startnode->start();
            if (start < first_import_start)
                first_import_start = start;

            cgo_imports_text->append('\n');
            cgo_imports_text->append('\n');

            auto text = buf->get_text(start, it->end());
            for (auto p = text; *p; p++)
                cgo_imports_text->append(*p);
        }

        cgo_imports_text->append('\0');

        Text_Renderer rend;
        rend.init();
        rend.write("import (\n");

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
        rend.write("%s", cgo_imports_text->items);

        GHFmtStart();
        GHFmtAddLine(rend.finish());
        GHFmtAddLine("");

        auto new_contents = GHFmtFinish(GH_FMT_GOIMPORTS);
        if (!new_contents) break;
        defer { GHFree(new_contents); };

        auto new_contents_len = strlen(new_contents);
        if (!new_contents_len) break;

        if (new_contents[new_contents_len-1] == '\n') {
            new_contents[new_contents_len-1] = '\0';
            new_contents_len--;
        }

        cur2 start, old_end;
        if (first_imports_node) {
            start = first_import_start;
            old_end = last_imports_node->end();
        } else {
            start = package_node->end();
            old_end = package_node->end();
        }

        auto chars = alloc_list<uchar>();
        if (!first_imports_node) {
            // add two newlines, it's going after the package decl
            chars->append('\n');
            chars->append('\n');
        }

        {
            auto ustr = cstr_to_ustr(new_contents);
            For (*ustr) chars->append(it);
        }

        buf->hist_force_push_next_change = true;
        if (start != old_end)
            buf->remove(start, old_end);
        buf->insert(start, chars->items, chars->len);

        {
            auto c = cur;
            if (c.y >= buf->lines.len) {
                c.y = buf->lines.len-1;
                c.x = buf->lines[c.y].len;
            }
            if (c.x > buf->lines[c.y].len)
                c.x = buf->lines[cur.y].len;

            move_cursor(c); // use raw_move_cursor()?
        }

        if (world.use_nvim) {
            skip_next_nvim_update();

            auto& nv = world.nvim;
            nv.start_request_message("nvim_buf_set_lines", 5);
            nv.writer.write_int(nvim_data.buf_id);
            nv.writer.write_int(0);
            nv.writer.write_int(-1);
            nv.writer.write_bool(false);
            nv.writer.write_array(buf->lines.len);
            For (buf->lines) nv.write_line(&it);
            nv.end_message();
        }
    } while (0);

    return true;
}

void Editor::format_on_save(int fmt_type, bool write_to_nvim) {
    if (!is_go_file) return; // any more checks needed?

    auto old_cur = cur;

    Buffer swapbuf; ptr0(&swapbuf);
    bool success = false;

    GHFmtStart();

    for (int i = 0; i < buf->lines.len; i++) {
        SCOPED_FRAME();

        List<char> line;
        line.init();

        For (buf->lines[i]) {
            char tmp[4];
            auto n = uchar_to_cstr(it, tmp);
            for (u32 j = 0; j < n; j++)
                line.append(tmp[j]);
        }

        line.append('\0');
        GHFmtAddLine(line.items);
    }

    auto new_contents = GHFmtFinish(fmt_type);
    if (!new_contents) {
        saving = false;
        return;
    }
    defer { GHFree(new_contents); };

    int curr = 0;
    swapbuf.init(MEM, false, false);
    swapbuf.read([&](char *out) -> bool {
        if (new_contents[curr] != '\0') {
            *out = new_contents[curr++];
            return true;
        }
        return false;
    });

    /// auto was_dirty = buf->dirty;
    buf->copy_from(&swapbuf);
    buf->dirty = true;

    // we need to adjust cursor manually
    if (!world.use_nvim) {
        if (cur.y >= buf->lines.len) {
            cur.y = buf->lines.len-1;
            cur.x = buf->lines[cur.y].len;
        }

        if (cur.x > buf->lines[cur.y].len)
            cur.x = buf->lines[cur.y].len;
    }

    if (world.use_nvim && write_to_nvim) {
        auto &nv = world.nvim;
        auto &writer = nv.writer;

        skip_next_nvim_update();

        auto msgid = nv.start_request_message("nvim_buf_set_lines", 5);
        auto req = nv.save_request(NVIM_REQ_POST_SAVE_SETLINES, msgid, id);
        req->post_save_setlines.cur = old_cur;

        writer.write_int(nvim_data.buf_id);
        writer.write_int(0);
        writer.write_int(-1);
        writer.write_bool(false);

        writer.write_array(buf->lines.len);
        For (buf->lines) nv.write_line(&it);
        nv.end_message();
    }
}

void Editor::skip_next_nvim_update(int n) {
    if (nvim_insert.skip_changedticks_until > nvim_data.changedtick) {
        nvim_insert.skip_changedticks_until += n;
    } else {
        nvim_insert.skip_changedticks_until = nvim_data.changedtick + n;
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
        opts.starting_folder = cp_strdup(world.current_path);
        if (!let_user_select_file(&opts)) return;

        if (!path_has_descendant(world.current_path, filepath)) {
            tell_user("Unable to save file outside workspace.", "Error");
            return;
        }

        is_untitled = false;
        is_go_file = str_ends_with(filepath, ".go");

        if (is_go_file)
            buf->enable_tree();
    }

    if (options.format_on_save) {
        bool use_goimports_autoimport = false;
        if (options.organize_imports_on_save)
            if (!optimize_imports())
                use_goimports_autoimport = true;

        format_on_save(use_goimports_autoimport ? GH_FMT_GOIMPORTS_WITH_AUTOIMPORT : GH_FMT_GOIMPORTS, !about_to_close);
    }

    // save to disk
    {
        disable_file_watcher_until = current_time_nano() + (2 * 1000000000);

        File f;
        if (f.init_write(filepath) != FILE_RESULT_OK) {
            tell_user("Unable to save file.", "Error");
            return;
        }
        defer { f.cleanup(); };

        buf->write(&f);
        buf->dirty = false;
    }

    file_was_deleted = false;

    auto find_node = [&]() -> FT_Node * {
        if (world.file_tree_busy) return NULL;

        auto curr = world.file_tree;
        auto subpath = get_path_relative_to(filepath, world.current_path);
        auto parts = make_path(subpath)->parts;

        if (!parts->len) return NULL;
        parts->len--; // chop off last, we want dirname

        For (*parts) {
            bool found = false;
            for (auto child = curr->children; child; child = child->next) {
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
    if (node) {
        bool child_exists = false;
        auto filename = cp_basename(filepath);

        for (auto child = node->children; child; child = child->next) {
            if (streq(child->name, filename)) {
                child_exists = true;
                break;
            }
        }

        if (!child_exists) {
            add_ft_node(node, [&](auto child) {
                child->is_directory = false;
                child->name = cp_strdup(filename);
            });
        }
    }
}

bool Editor::ask_user_about_unsaved_changes() {
    if (!is_unsaved()) return true;

    auto title = "Your changes will be lost if you don't.";
    auto filename  = is_untitled ? "(untitled)" : cp_basename(filepath);
    auto msg = cp_sprintf("Do you want to save your changes to %s?", filename);

    auto result = ask_user_yes_no_cancel(title, msg, "Save", "Don't Save");
    if (result == ASKUSER_CANCEL) return false;

    if (result == ASKUSER_YES)
        handle_save(true);
    return true;
}

void Editor::toggle_comment(int ystart, int yend) {
    print("toggling from [%d, %d]", ystart, yend);

    auto u_isspace = [&](uchar uch) {
        return uch < 128 && isspace(uch);
    };

    auto strip_spaces = [&](auto *line) {
        int x = -1;
        Fori (*line) {
            if (!u_isspace(it)) {
                x = i;
                break;
            }
        }

        auto ret = alloc_list<uchar>();
        if (x != -1)
            for (int i = x; i < line->len; i++)
                ret->append(line->at(i));
        return ret;
    };

    auto is_commented = [&]() {
        for (int y = ystart; y <= yend; y++) {
            auto line = strip_spaces(&buf->lines[y]);

            if (line->len < 2) return false;
            if (line->at(0) != '/') return false;
            if (line->at(1) != '/') return false;
        }
        return true;
    };

    cur2 new_cur = new_cur2(-1, ystart);
    buf->hist_batch_mode = true;

    if (is_commented()) {
        // remove comments
        for (int y = ystart; y <= yend; y++) {
            int x = 0;
            auto line = &buf->lines[y];

            for (; x+1 < line->len; x++) {
                if (u_isspace(line->at(x))) continue;
                if (line->at(x) != '/' || line->at(x+1) != '/') break;

                if (new_cur.x == -1) new_cur.x = x;

                cur2 start = new_cur2(x, y);
                cur2 end = new_cur2(x+2, y);

                if (u_isspace(line->at(x+2))) end.x++;

                buf->remove(start, end);
            }
        }
    } else {
        // add comments
        int smallest_indent = -1;

        for (int y = ystart; y <= yend; y++) {
            auto line = &buf->lines[y];
            auto stripped = strip_spaces(line);
            auto indent = line->len - stripped->len;

            if (smallest_indent == -1 || indent < smallest_indent)
                smallest_indent = indent;
        }

        new_cur.x = smallest_indent;

        for (int y = ystart; y <= yend; y++) {
            auto comments = cstr_to_ustr("// ");
            buf->insert(new_cur2(smallest_indent, y), comments->items, comments->len, false);
        }
    }

    buf->hist_batch_mode = false;
    move_cursor(new_cur);

    if (world.use_nvim) {
        skip_next_nvim_update();

        auto& nv = world.nvim;
        nv.start_request_message("nvim_buf_set_lines", 5);
        nv.writer.write_int(nvim_data.buf_id);
        nv.writer.write_int(ystart);
        nv.writer.write_int(yend+1);
        nv.writer.write_bool(false);
        nv.writer.write_array(yend-ystart+1);
        for (int y = ystart; y <= yend; y++)
            nv.write_line(&buf->lines[y]);
        nv.end_message();

        // remove highlight
        trigger_escape();
    } else {
        selecting = false;
    }
}

void Editor::highlight_snippet(cur2 start, cur2 end) {
    highlight_snippet_state.on = true;
    highlight_snippet_state.time_start_milli = current_time_milli();
    highlight_snippet_state.start = start;
    highlight_snippet_state.end = end;
}

void Editor::delete_selection() {
    if (!selecting) return;

    // REFACTOR: duplicated in handle_backspace()
    auto a = select_start;
    auto b = cur;
    if (a > b) {
        auto tmp = a;
        a = b;
        b = tmp;
    }

    buf->remove(a, b);
    selecting = false;
    move_cursor(a);
}
