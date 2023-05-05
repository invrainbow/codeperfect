#include "editor.hpp"
#include "enums.hpp"
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
    if (is_untitled) return true;
    if (path_has_descendant(world.current_path, filepath)) return true;
    if (!world.workspace) return false;
    if (world.workspace->find_module_containing_resolved(filepath)) return true;
    return false;
}

// returned ranges are ordered
Selection *Editor::get_selection(Selection_Type sel_type) {
    if (sel_type == SEL_NONE)
        sel_type = vim.visual_type;

    auto ret = new_object(Selection);
    ret->ranges = new_list(Selection_Range);

    auto add_range = [&](cur2 start, cur2 end) {;
        Selection_Range sr; ptr0(&sr);
        sr.start = start;
        sr.end = end;
        ret->ranges->append(&sr);
    };

    if (world.vim.on) {
        if (world.vim_mode() != VI_VISUAL) return NULL;

        ret->type = sel_type;
        switch (sel_type) {
        case SEL_CHAR: {
            auto a = vim.visual_start;
            auto b = cur;
            ORDER(a, b);
            b = buf->inc_gr(b);

            add_range(a, b);
            return ret;
        }
        case SEL_LINE: {
            auto a = vim.visual_start.y;
            auto b = cur.y;
            ORDER(a, b);

            auto &lines = buf->lines;

            auto start = new_cur2(0, a);
            auto end = new_cur2(lines[b].len, b);
            add_range(start, end);
            return ret;
        }
        case SEL_BLOCK: {
            auto a = vim.visual_start;
            auto b = cur;

            auto avx = buf->idx_cp_to_vcp(a.y, a.x);
            auto ay = a.y;

            // Originally we had max(vim.hidden_vx, ...). Turns out neovim
            // doesn't do this so we didn't either. But maybe it's superior
            // experience.
            auto bvx = buf->idx_cp_to_vcp(b.y, b.x);
            auto by = b.y;

            ORDER(avx, bvx);
            ORDER(ay, by);

            for (int y = ay; y <= by; y++) {
                int len = buf->lines[y].len;
                if (!len) continue;

                auto ax = buf->idx_vcp_to_cp(y, avx);
                if (ax > len) continue;

                auto bx = min(buf->idx_vcp_to_cp(y, bvx), len-1);
                add_range(new_cur2(ax, y), buf->inc_gr(new_cur2(bx, y)));
            }
            return ret;
        }
        }
        return NULL;
    }

    if (selecting) {
        ret->type = SEL_CHAR;
        auto a = select_start;
        auto b = cur;
        ORDER(a, b);
        add_range(a, b);
        return ret;
    }

    return NULL;
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
    auto ret = new_list(char);
    for (u32 x = 0; x < copy_spaces_until; x++)
        ret->append((char)line[x]); // has to be ' ' or '\t'

    if (lang == LANG_GO) {
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

void Editor::flash_cursor_error() {
    flash_cursor_error_start_time = current_time_milli();
}

void Editor::insert_text_in_insert_mode(ccstr s) {
    auto len = strlen(s);
    if (!len) return;

    SCOPED_FRAME();

    auto text = cstr_to_ustr(s);
    if (!text->len) return;

    auto newcur = buf->insert(cur, text->items, text->len);
    move_cursor(newcur);
}

void Editor::perform_autocomplete(AC_Result *result) {
    auto& ac = autocomplete.ac;

    switch (result->type) {
    case ACR_POSTFIX: {
        // TODO: this currently only works for insert mode; support normal mode
        // also what if we just forced you to be in insert mode for autocomplete lol

        // remove everything but the operator
        move_cursor(ac.operand_end);
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

            auto ret = new_list(char, copy_spaces_until + 1);

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

            move_cursor(ac.operand_start);
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
                Fori (multi_types) {
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

                Fori (multi_types) {
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
                    if (!result || !result->len) {
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

                        Goresult *rres = NULL;
                        {
                            auto old = ind.dont_resolve_builtin;
                            ind.dont_resolve_builtin = true;
                            defer { ind.dont_resolve_builtin = old; };

                            rres = ind.resolve_type(res->gotype, res->ctx);
                        }
                        if (!rres) return NULL;

                        gotype = rres->gotype;

                        if (gotype->type == GOTYPE_BUILTIN) {
                            switch (gotype->builtin_type) {
                            case GO_BUILTIN_COMPLEXTYPE:
                            case GO_BUILTIN_FLOATTYPE:
                            case GO_BUILTIN_INTEGERTYPE:
                            case GO_BUILTIN_BYTE:
                            case GO_BUILTIN_COMPLEX128:
                            case GO_BUILTIN_COMPLEX64:
                            case GO_BUILTIN_FLOAT32:
                            case GO_BUILTIN_FLOAT64:
                            case GO_BUILTIN_INT:
                            case GO_BUILTIN_INT16:
                            case GO_BUILTIN_INT32:
                            case GO_BUILTIN_INT64:
                            case GO_BUILTIN_INT8:
                            case GO_BUILTIN_UINT:
                            case GO_BUILTIN_UINT16:
                            case GO_BUILTIN_UINT32:
                            case GO_BUILTIN_UINT64:
                            case GO_BUILTIN_UINT8:
                            case GO_BUILTIN_UINTPTR:
                                return "0";
                            case GO_BUILTIN_STRING:
                                return "\"\"";
                            case GO_BUILTIN_BOOL:
                                return "false";
                            case GO_BUILTIN_ANY:
                                return "nil";
                            case GO_BUILTIN_ERROR:
                                return "err";
                            }
                        }
                        return NULL;
                    };

                    insert_text("return ");

                    Fori (result) {
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
            break;
        }

        default:
            notfound = true;
            break;
        }

        if (notfound) break;

        if (curr_postfix) {
            if (curr_postfix->insert_positions.len > 1) {
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

                For (&builtins_with_space)
                    if (streq(s, it))
                        return cp_sprintf("%s ", s);
                break;
            }

            case ACR_DECLARATION: {
                if (result->declaration_is_struct_literal_field)
                    return cp_sprintf("%s: ", s);

                // check if it's a func type and add "("

                if (!options.autocomplete_func_add_paren) break;

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

        ccstr import_to_add = NULL;

        {
            SCOPED_BATCH_CHANGE(buf);

            import_to_add = see_if_we_need_autoimport();
            if (import_to_add) {
                auto iter = new_object(Parser_It);
                iter->init(buf);
                auto root = new_ast_node(ts_tree_root_node(buf->tree), iter);

                Ast_Node *package_node = NULL;
                auto import_nodes = new_list(Ast_Node*);

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
                        For (import_nodes) {
                            auto imports = new_list(Go_Import);
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
                            auto imports = new_list(Go_Import);
                            world.indexer.import_decl_to_goimports(firstnode, imports);

                            For (imports) {
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

                    auto new_contents = gh_fmt_finish(false);
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
                    } else {
                        start = package_node->end();
                        old_end = package_node->end();
                    }
                    new_end = start;

                    auto chars = new_list(uchar);
                    if (!firstnode) {
                        // add two newlines, it's going after the package decl
                        chars->append('\n');
                        chars->append('\n');
                        new_end.x = 0;
                        new_end.y += 2;
                    }

                    {
                        auto ustr = cstr_to_ustr(new_contents);
                        For (ustr) {
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
                    buf->apply_edit(start, old_end, chars->items, chars->len);

                    move_cursor(new_cur2(cur.x, cur.y + new_end.y - old_end.y));
                } while (0);
            }

            auto name = cstr_to_ustr(modify_string(result->name));

            auto ac_start = cur;
            ac_start.x -= strlen(ac.prefix); // what if the prefix contains unicode?

            // perform the edit & move cursor
            auto newcur = buf->apply_edit(ac_start, cur, name->items, name->len);
            move_cursor(newcur);
        }

        // clear out last_closed_autocomplete
        last_closed_autocomplete = NULL_CUR;

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

bool Editor::is_current_editor() {
    auto current_editor = get_current_editor();
    if (current_editor)
        if (current_editor->id == id)
            return true;
    return false;
}

Move_Cursor_Opts *default_move_cursor_opts() {
    auto ret = new_object(Move_Cursor_Opts);

    // set defaults
    ret->is_user_movement = false;

    return ret;
}

void Editor::move_cursor(cur2 c, Move_Cursor_Opts *opts) {
    if (!is_main_thread) cp_panic("can't call this from outside main thread");

    if (!opts) opts = default_move_cursor_opts();

    if (c.y == -1) c = buf->offset_to_cur(c.x);
    if (c.y < 0 || c.y >= buf->lines.len) return;
    if (c.x < 0) return;

    auto& line = buf->lines[c.y];

    if (c.x > line.len)
        c.x = line.len;

    cur = c;

    u32 vx = 0;
    u32 i = 0;

    if (line.len) {
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

    // we're using this function to trigger
    // certain "on cursor move" actions
    if (opts->is_user_movement) {
        // clear out autocomplete
        ptr0(&autocomplete.ac);

        // force next change to be a new history entry
        buf->hist_force_push_next_change = true;
    }

    if (!world.dont_push_history)
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
        move_cursor(new_cur2(cur.x, min(view.y + options.scrolloff, view.y + view.h)));
    if (cur.y + options.scrolloff >= view.y + view.h)
        move_cursor(new_cur2(cur.x, relu_sub(view.y + view.h,  1 + options.scrolloff)));
    // TODO: handle x
}

void Editor::reset_state() {
    cur.x = 0;
    cur.y = 0;
    last_closed_autocomplete = NULL_CUR;
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
    auto now = current_time_nano();
    if (because_of_file_watcher && disable_file_watcher_until > now)
        return;

    auto fm = map_file_into_memory(filepath);
    if (!fm) return;
    defer { fm->cleanup(); };

    if (!check_file(fm)) return;

    buf->read(fm);
    buf->dirty = false;
}

Parse_Lang determine_lang(ccstr filepath) {
    if (filepath) {
        if (str_ends_with(filepath, ".go")) return LANG_GO;

        auto base = cp_basename(filepath);
        if (streq(base, "go.mod")) return LANG_GOMOD;
        if (streq(base, "go.work")) return LANG_GOWORK;
    }
    return LANG_NONE;
}

bool Editor::load_file(ccstr new_filepath) {
    reset_state();

    if (buf->initialized)
        buf->cleanup();

    lang = determine_lang(new_filepath);
    buf->init(&mem, lang, true, true);
    buf->editable_from_main_thread_only = true;

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
    }

    buf->dirty = false;

    {
        // reset buf history so user can't undo initial file contents
        for (int i = buf->hist_start; i < buf->hist_top; i++)
            buf->hist_free(i);
        buf->hist_curr = buf->hist_start;
        buf->hist_top = buf->hist_start;
    }

    auto &b = world.build;
    if (b.ready()) {
        auto editor_path = get_path_relative_to(filepath, world.current_path);
        For (&b.errors) {
            if (is_mark_valid(it.mark)) continue;
            if (!it.valid) continue;
            if (!are_filepaths_equal(editor_path, it.file)) continue;

            // create mark
            auto pos = new_cur2(it.col - 1, it.row - 1);
            buf->insert_mark(MARK_BUILD_ERROR, pos, it.mark);
        }
    }

    // fill in search results
    if (world.searcher.state == SEARCH_SEARCH_DONE) {
        For (&world.searcher.search_results) {
            if (!are_filepaths_equal(it.filepath, filepath)) continue;

            For (it.results) {
                if (!it.mark_start) cp_panic("mark_start was null");
                if (!it.mark_end) cp_panic("mark_end was null");

                if (it.mark_start->valid) cp_panic("this shouldn't be happening");
                if (it.mark_end->valid) cp_panic("this shouldn't be happening");

                buf->insert_mark(MARK_SEARCH_RESULT, it.match_start, it.mark_start);
                buf->insert_mark(MARK_SEARCH_RESULT, it.match_end, it.mark_end);
            }
        }
    }

    // clean out world.last_closed
    {
        auto new_last_closed = new_list(Last_Closed);
        For (world.last_closed)
            if (!streq(filepath, it.filepath))
                new_last_closed->append(it);
        world.last_closed->len = 0;
        world.last_closed->concat(new_last_closed);
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

    auto editor = editors.append();
    editor->init();

    if (!editor->load_file(NULL)) {
        editor->cleanup();
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
    For (&editors) {
        it.cleanup();
    }
    editors.cleanup();
}

Editor* Pane::focus_editor(ccstr path) {
    return focus_editor(path, NULL_CUR);
}

Editor* Pane::focus_editor(ccstr path, cur2 pos, bool pos_in_byte_format) {
    u32 i = 0;
    For (&editors) {
        // TODO: use are_filepaths_equal instead, don't have to access filesystem
        if (are_filepaths_same_file(path, it.filepath))
            return focus_editor_by_index(i, pos, pos_in_byte_format);
        i++;
    }

    if (handle_auth_error())
        return NULL;

    auto editor = editors.append();
    editor->init();
    if (!editor->load_file(path)) {
        editor->cleanup();
        editors.len--;
        return NULL;
    }

    ui.recalculate_view_sizes(true);
    return focus_editor_by_index(editors.len - 1, pos, pos_in_byte_format);
}

Editor *Pane::focus_editor_by_index(u32 idx) {
    return focus_editor_by_index(idx, NULL_CUR);
}

// It feels like this is very related to but sufficiently different from w/e/b
// handling in vim (too many different edge cases) that we can just duplicate
// it here.
cur2 Editor::handle_alt_move(bool back, bool backspace) {
    auto it = iter();

    if (back) {
        if (it.bof()) return it.pos;
        it.prev();
    }

    auto done = [&]() { return back ? it.bof() : it.eof(); };
    auto advance = [&]() { back ? it.prev() : it.next(); };

    enum { TYPE_SPACE, TYPE_IDENT, TYPE_OTHER };

    auto get_char_type = [](char ch) -> int {
        if (isspace(ch)) return TYPE_SPACE;
        if (isident(ch)) return TYPE_IDENT;
        return TYPE_OTHER;
    };

    int start_type = get_char_type(it.peek());
    int chars_moved = 0;

    for (; !done(); advance(), chars_moved++) {
        if (get_char_type(it.peek()) != start_type) {
            if (start_type == TYPE_SPACE && chars_moved == 1) {
                // If we only found one space, start over with the next space type.
                start_type = get_char_type(it.peek());
                chars_moved = 0;
                continue;
            }

            if (back) it.next();
            break;
        }
    }

    return it.pos;
}

bool Editor::handle_escape() {
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

    return handled;
}

void Pane::set_current_editor(u32 idx) {
    current_editor = idx;

    // focus current editor in file explorer

    if (world.file_tree_busy) return;

    auto editor = get_current_editor();
    if (!editor) return;
    if (editor->is_untitled) return;

    auto edpath = get_path_relative_to(editor->filepath, world.current_path);
    auto node = find_ft_node(edpath);
    if (!node) return;

    world.file_explorer.scroll_to = node;
    world.file_explorer.selection = node;

    for (auto it = node->parent; it && it->parent; it = it->parent)
        it->open = true;
}

Editor *Pane::focus_editor_by_index(u32 idx, cur2 pos, bool pos_in_byte_format) {
    if (current_editor != idx)
        reset_everything_when_switching_editors(get_current_editor());

    set_current_editor(idx);

    auto &editor = editors[idx];

    auto cppos = pos;

    if (cppos.y >= editor.buf->lines.len)
        cppos.y = editor.buf->lines.len - 1;

    if (pos_in_byte_format) {
        int bc = editor.buf->bctree.get(cppos.y);
        if (cppos.x >= bc) cppos.x = bc-1;
        cppos.x = editor.buf->idx_byte_to_cp(cppos.y, cppos.x);
    } else {
        if (cppos.x > editor.buf->lines[cppos.y].len)
            cppos.x = editor.buf->lines[cppos.y].len;
    }

    if (pos.x != -1) {
        editor.move_cursor(cppos);
    } else {
        world.history.push(editor.id, editor.cur);
    }

    return &editor;
}

Editor* Pane::get_current_editor() {
    if (!editors.len) return NULL;
    if (current_editor == -1) return NULL;

    return &editors[current_editor];
}

void Editor::init() {
    ptr0(this);
    id = ++world.next_editor_id;

    mem.init("editor mem");

    if (world.vim.on) {
        vim.mem.init();
        SCOPED_MEM(&vim.mem);
        // TODO: set a limit on command len?
        vim.command_buffer = new_list(Vim_Command_Input);
        vim.insert_command.init();
        vim.insert_visual_block_other_starts.init();
        vim.replace_old_chars.init();
    }

    {
        SCOPED_MEM(&mem);
        postfix_stack.init();
        buf = new_object(Buffer);
    }

    ast_navigation.mem.init("ast_navigation mem");
}

void Editor::update_selected_ast_node(Ast_Node *node) {
    auto &nav = ast_navigation;

    auto prev_siblings = new_list(Ast_Node*);
    for (auto curr = node->prev(); curr; curr = curr->prev())
        prev_siblings->append(curr);

    {
        SCOPED_MEM(&nav.mem);

        nav.node = node->dup();
        nav.siblings->len = 0;

        for (int i = prev_siblings->len-1; i >= 0; i--)
            nav.siblings->append(prev_siblings->at(i)->dup());
        for (auto curr = node->next(); curr; curr = curr->next())
            nav.siblings->append(curr);
    }
}

void Editor::update_ast_navigate(fn<Ast_Node*(Ast_Node*)> cb) {
    auto &nav = ast_navigation;
    if (!nav.on) return;

    if (nav.tree_version != buf->tree_version) return;

    auto node = nav.node;
    if (!node) return;

    node = cb(node);
    if (!node) return;
    if (!node) return;

    update_selected_ast_node(node);
    move_cursor(node->start());

    auto end = node->end().y;
    if (end >= view.y + view.h)
        view.y = relu_sub(node->start().y, 3);
}

void Editor::ast_navigate_in() {
    update_ast_navigate([&](auto node) -> Ast_Node* {
        auto child = node->child();
        if (!child) return NULL;

        // skip the children that have no siblings
        while (child && !child->prev() && !child->next()) // no siblings
            child = child->child();

        // no children with siblings, just grab the innermost child
        if (!child) {
            child = node->child();
            while (true) {
                auto next = child->child();
                if (!next) break;
                child = next;
            }

            // if the innermost child is indistinguishable from current node, return null
            if (child->start() == node->start() && child->end() == node->end())
                return NULL;
        }

        return child;
    });
}

void Editor::ast_navigate_out() {
    update_ast_navigate([&](auto node) -> Ast_Node* {
        auto parent = node->parent();
        if (!parent) return NULL;
        if (parent->type() == TS_SOURCE_FILE) return NULL;

        // skip the children that have no siblings
        while (parent && !parent->prev() && !parent->next()) // no siblings
            parent = parent->parent();

        if (!parent) {
            parent = node->parent();
            while (true) {
                auto next = parent->parent();
                if (!next) break;
                if (next->type() == TS_SOURCE_FILE) break;
                parent = next;
            }
        }

        return parent;
    });
}

void Editor::ast_navigate_prev() {
    update_ast_navigate([&](auto node) {
        return node->prev();
    });
}

void Editor::ast_navigate_next() {
    update_ast_navigate([&](auto node) {
        return node->next();
    });
}

void Editor::cleanup() {
    // before exiting, if we're in normal mode, get out
    if (world.vim.on && world.vim_mode() != VI_NORMAL)
        vim_return_to_normal_mode();

    buf->cleanup();
    mem.cleanup();
    if (world.vim.on) {
        For (&vim.local_marks) {
            if (it) {
                it->cleanup();
                world.mark_fridge.free(it);
            }
        }
        For (&vim.global_marks) {
            if (it) {
                it->cleanup();
                world.mark_fridge.free(it);
            }
        }

        world.vim.dotrepeat.mem_finished.cleanup();
        world.vim.dotrepeat.mem_working.cleanup();

        for (auto &&macro : world.vim.macros)
            if (macro.active)
                macro.mem.cleanup();

        vim.mem.cleanup();
    }
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
    if (lang != LANG_GO) return;

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
        t.init(NULL, false);

        if (!ind.autocomplete(filepath, cur, triggered_by_dot, &ac)) return;

        t.log("world.indexer.autocomplete");

        if (old_type != AUTOCOMPLETE_NONE && old_keyword_start != ac.keyword_start)
            if (!triggered_by_dot)
                return;

        last_closed_autocomplete = NULL_CUR;

        {
            // use autocomplete_mem
            world.autocomplete_mem.reset();
            SCOPED_MEM(&world.autocomplete_mem);

            // copy results
            auto new_results = new_list(AC_Result, ac.results->len);
            For (ac.results) {
                auto r = new_results->append();
                memcpy(r, it.copy(), sizeof(AC_Result));
            }

            // copy ac over to autocomplete.ac
            memcpy(&autocomplete.ac, &ac, sizeof(Autocomplete));
            autocomplete.filtered_results = new_list(int);
            autocomplete.ac.prefix = cp_strdup(ac.prefix);
            autocomplete.ac.results = new_results;
        }
    }

    {
        auto prefix = autocomplete.ac.prefix;
        auto prefix_len = strlen(prefix);
        auto results = autocomplete.ac.results;

        autocomplete.filtered_results->len = 0;

        Timer t;
        t.init("autocomplete", false);

        Fori (results)
            if (fzy_has_match(prefix, it.name))
                autocomplete.filtered_results->append(i);

        t.log("matching");

        /*
        if (!autocomplete.filtered_results->len) {
            ptr0(&autocomplete);
            return;
        }
        */

        struct Cached_Stuff {
            bool filled;
            double score;
            int str_length;
        };

        auto cache = new_array(Cached_Stuff, results->len);
        auto filtered_results = autocomplete.filtered_results;

        // lazy load expensive operations
        auto fill_cache = [&](int i) {
            auto &it = cache[i];
            if (it.filled) return;

            auto name = results->at(i).name;
            it.score = fzy_match(prefix, name);
            it.str_length = strlen(name);
            it.filled = true;
        };

        filtered_results->sort([&](int *pa, int *pb) -> int {
            int ia = *pa;
            int ib = *pb;
            auto &a = results->at(ia);
            auto &b = results->at(ib);

            int ret = 0;

            auto compare = [&](fn<bool(AC_Result*)> test, bool good) -> bool {
                auto isa = test(&a);
                auto isb = test(&b);
                if (isa == isb) return false;

                // I once changed this to `return (sia == good ? ...` because
                // it looked like that was what I wanted to do, and that I was
                // erroneously just returning true. That is not the case. This
                // function returns WHETHER OR NOT A AND B ARE DIFFERENT. `ret`
                // then stores the result of that comparison.
                ret = (isa == good ? -1 : 1);
                return true;
            };

#define reward(expr) if (compare([&](auto it) { return expr; }, true)) { return ret; }
#define penalize(expr) if (compare([&](auto it) { return expr; }, false)) { return ret; }

            // penalize ACR_POSTFIX
            penalize(it->type == ACR_POSTFIX);

            // struct literals come first
            reward(it->declaration_is_struct_literal_field);

            // if prefix is less than 2 characters and it's a lone identifier dotcomplete, then, in order:
            //  - reward scopeops (locally defined vars)
            //  - reward same-file decls
            //  - penalize external imports
            //  - penalize imports
            if (prefix_len < 2 && autocomplete.ac.type == AUTOCOMPLETE_IDENTIFIER) {
                reward(it->type == ACR_DECLARATION && it->declaration_is_scopeop);
                reward(it->type == ACR_DECLARATION && it->declaration_is_own_file);
                penalize(it->type == ACR_IMPORT && !it->import_is_existing);
                penalize(it->type == ACR_IMPORT);
            }

            // if they're both scope ops, inner ones first
            {
                auto isa = a.type == ACR_DECLARATION && a.declaration_is_scopeop;
                auto isb = b.type == ACR_DECLARATION && b.declaration_is_scopeop;
                if (isa && isb) return b.declaration_scopeop_depth - a.declaration_scopeop_depth;
            }

            // now we need stuff inside the cache
            fill_cache(ia);
            fill_cache(ib);

            // compare based on score
            if (cache[ia].score != cache[ib].score && prefix_len)
                return cache[ia].score < cache[ib].score ? 1 : -1;

            if (a.type == ACR_IMPORT && b.type == ACR_IMPORT) {
                // check if import in file
                reward(it->import_is_existing);

                // check if import in workspace
                if (world.workspace)
                    reward(world.workspace->find_module_containing(it->import_path) != NULL);
            }

            if (cache[ia].str_length != cache[ib].str_length)
                return cache[ia].str_length - cache[ib].str_length;

            if (a.type == ACR_KEYWORD     && b.type == ACR_DECLARATION) return -1;
            if (a.type == ACR_DECLARATION && b.type == ACR_KEYWORD)     return 1;

            auto is_field = [&](auto it) {
                if (it->type == ACR_DECLARATION)
                    if (it->declaration_godecl)
                        if (it->declaration_godecl->type == GODECL_FIELD)
                            return true;
                return false;
            };

            if (is_field(&a) && is_field(&b)) {
                int deptha = a.declaration_godecl->field_depth;
                int depthb = b.declaration_godecl->field_depth;
                if (deptha != depthb) return deptha - depthb;

                int ordera = a.declaration_godecl->field_order;
                int orderb = b.declaration_godecl->field_order;
                return deptha - depthb;
            }

            return 0;
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
    if (lang != LANG_GO) return;

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

        find_nodes_containing_pos(pf.root, cur, true, [&](auto it) {
            if (it->type() == TS_ARGUMENT_LIST) {
                if (!arglist)
                    arglist = new_object(Ast_Node);
                memcpy(arglist, it, sizeof(Ast_Node));
            }
            return WALK_CONTINUE;
        });

        if (!arglist) return;

        auto commas = new_list(cur2);

        FOR_ALL_NODE_CHILDREN (arglist) {
            if (it->type() == TS_COMMA)
                commas->append(it->start());
        }

        // not even gonna bother binary searching lol

        int current_param = -1;
        Fori (commas)
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

void Editor::backspace_in_replace_mode() {
    auto old_cur = cur;

    auto it = iter();
    if (it.bof()) return;
    it.gr_prev();
    auto start = it.pos;

    if (cur <= vim.replace_start) {
        // move cursor but don't delete
        move_cursor(start);
        vim.replace_start = start;
        vim.edit_backspaced_graphemes++;
        return;
    }

    // delete the char, then insert old character if we're in range
    buf->remove(start, cur);
    move_cursor(start);

    if (old_cur > vim.replace_end) return;

    cp_assert(vim.replace_old_chars.len);

    auto rev = new_list(uchar);
    while (vim.replace_old_chars.len) {
        uchar ch = *vim.replace_old_chars.last();
        vim.replace_old_chars.len--;
        if (ch == 0) break;
        rev->append(ch);
    }

    auto codepoints = new_list(uchar);
    for (int i = rev->len; i >= 0; i--)
        codepoints->append(rev->at(i));

    buf->insert(start, codepoints->items, codepoints->len);

    vim.replace_end = start;
}

void Editor::type_char(uchar ch, Type_Char_Opts *opts) {
    if (!opts) opts = new_object(Type_Char_Opts);

    // ...wait, why did i comment this out
    // i think it was because it was completing when i didn't want it to
    // handle typing a dot when an import is selected
    /*
    bool already_typed = false;
    do {
        // commented out now, but if we bring it back, don't run it when opts->replace_mode
        if (opts->replace_mode) break;
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
    */

    if (opts->replace_mode) {
        bool replacing = (cur.x < buf->lines[cur.y].len);
        if (replacing) {
            auto it = iter();
            auto gr = it.gr_next();

            vim.replace_old_chars.append((uchar)0);
            vim.replace_old_chars.concat(gr);

            buf->remove(cur, it.pos);
        }
        move_cursor(buf->insert(cur, ch));
        if (replacing)
            vim.replace_end = cur;
    } else {
        move_cursor(buf->insert(cur, ch));
    }

    if (lang != LANG_GO) return;

    // at this point, tree is up to date! we can simply walk, don't need to re-parse :)

    bool did_autocomplete = false;
    bool did_parameter_hint = false;

    if (!isident(ch) && autocomplete.ac.results)
        ptr0(&autocomplete);

    // replace mode still has bracket auto dedent, annoyingly enough
    // will it continue to work?
    switch (ch) {
    case '}':
    case ')':
    case ']': {
        cp_assert(cur.x > 0); // we just typed
        auto rbrace_pos = buf->dec_cur(cur);

        auto &line = buf->lines[rbrace_pos.y];
        bool starts_with_spaces = true;
        for (u32 x = 0; x < rbrace_pos.x; x++) {
            if (line[x] != ' ' && line[x] != '\t') {
                starts_with_spaces = false;
                break;
            }
        }

        if (!starts_with_spaces) break;

        auto pos = find_matching_brace((uchar)ch, rbrace_pos);
        if (pos == NULL_CUR) break;

        auto indentation = new_list(uchar);
        For (&buf->lines[pos.y]) {
            if (it == '\t' || it == ' ')
                indentation->append(it);
            else
                break;
        }

        // ======================================
        // code that actually performs the dedent
        // ======================================

        // start at cur
        pos = cur;

        // remove the indentation without calling backspace (don't set
        // insert_start or replace_start or update edit_backspaced_graphemes)
        auto start = new_cur2(0, pos.y);
        pos = buf->apply_edit(start, pos, indentation->items, indentation->len);

        // set this as the new insert_start/replace_start, don't call edit_backspaced_graphemes
        // honestly this is a huge hack and i hate it
        if (world.vim.on) {
            switch (world.vim_mode()) {
            case VI_INSERT:
                if (pos < vim.insert_start)
                    vim.insert_start = pos;
                break;
            case VI_REPLACE:
                if (pos < vim.replace_start)
                    vim.replace_start = pos;
                break;
            }
        }

        // now insert the brace
        pos = buf->insert(pos, ch);

        // move cursor after everything we typed
        move_cursor(pos);
        break;
    }
    }

    if (opts->replace_mode || opts->automated) return;
    if (world.vim.macro_state == MACRO_RUNNING) return;

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
            trigger_autocomplete(false, isident(ch), ch);

    if (!did_parameter_hint) update_parameter_hint();
}

void Editor::vim_save_inserted_indent(cur2 start, cur2 end) {
    if (!world.vim.on) return;

    vim.inserted_indent.inserted = true;
    vim.inserted_indent.buf_version = buf->buf_version;
    vim.inserted_indent.start = start;
    vim.inserted_indent.end = end;
}

void Editor::update_autocomplete(bool triggered_by_ident) {
    if (autocomplete.ac.results)
        trigger_autocomplete(false, triggered_by_ident);
}

void Editor::backspace_in_insert_mode() {
    auto it = iter();
    if (it.bof()) return;
    it.gr_prev();

    buf->remove(it.pos, cur);
    move_cursor(it.pos);

    if (world.vim.on) {
        if (it.pos < vim.insert_start) {
            vim.insert_start = it.pos;
            vim.edit_backspaced_graphemes++;
        }
    }

    last_closed_autocomplete = NULL_CUR;
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
        auto iter = new_object(Parser_It);
        iter->init(buf);
        auto root = new_ast_node(ts_tree_root_node(buf->tree), iter);

        Ast_Node *package_node = NULL;
        Ast_Node *first_imports_node = NULL;
        Ast_Node *last_imports_node = NULL;

        auto cgo_imports = new_list(Ast_Node*);

        auto is_cgo_import = [&](Ast_Node *it) {
            auto imports = new_list(Go_Import);
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

        auto cgo_imports_text = new_list(char);

        For (cgo_imports) {
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

        For (imports) {
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

        auto new_contents = gh_fmt_finish(false);
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

        auto chars = new_list(uchar);
        if (!first_imports_node) {
            // add two newlines, it's going after the package decl
            chars->append('\n');
            chars->append('\n');
        }

        chars->concat(cstr_to_ustr(new_contents));

        buf->apply_edit(start, old_end, chars->items, chars->len);

        {
            auto c = cur;
            if (c.y >= buf->lines.len)
                c = buf->end_pos();
            if (c.x > buf->lines[c.y].len)
                c.x = buf->lines[cur.y].len;

            move_cursor(c);
        }
    } while (0);

    return true;
}

void Editor::format_on_save() {
    if (lang != LANG_GO) return; // any more checks needed?

    auto old_cur = cur;

    GHFmtStart();

    for (int i = 0; i < buf->lines.len; i++) {
        SCOPED_FRAME();

        List<char> line;
        line.init();

        For (&buf->lines[i]) {
            char tmp[4];
            auto n = uchar_to_cstr(it, tmp);
            for (u32 j = 0; j < n; j++)
                line.append(tmp[j]);
        }

        line.append('\0');
        GHFmtAddLine(line.items);
    }

    auto new_contents = gh_fmt_finish();
    if (!new_contents) {
        saving = false;
        return;
    }
    defer { GHFree(new_contents); };

    auto uchars = cstr_to_ustr(new_contents);
    cp_assert(uchars);

    buf->apply_edit(new_cur2(0, 0), buf->end_pos(), uchars->items, uchars->len);

    // we need to adjust cursor manually
    if (cur.y >= buf->lines.len)
        cur = buf->end_pos();
    if (cur.x > buf->lines[cur.y].len)
        cur.x = buf->lines[cur.y].len;
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
        lang = determine_lang(filepath);
        if (lang != LANG_NONE)
            buf->enable_tree(lang);
    }

    if (options.format_on_save && !file_was_deleted) {
        if (options.organize_imports_on_save)
            optimize_imports();
        format_on_save();
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

        For (parts) {
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

bool Editor::is_unsaved() {
    if (!is_modifiable())
        return false;
    if (file_was_deleted)
        return true;
    if (buf->dirty) {
        // even if it's dirty, don't count it as unsaved if it's an untitled empty file
        if (is_untitled)
            if (buf->lines.len == 1 && !buf->lines[0].len)
                return false;
        return true;
    }
    return false;
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
        Fori (line) {
            if (!u_isspace(it)) {
                x = i;
                break;
            }
        }

        auto ret = new_list(uchar);
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
    {
        SCOPED_BATCH_CHANGE(buf);

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
    }

    move_cursor(new_cur);
    selecting = false;
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
    ORDER(a, b);

    buf->remove(a, b);
    selecting = false;
    move_cursor(a);
}

int Editor::first_nonspace_cp(int y) {
    auto it = iter(new_cur2(0, y));
    while (!it.eol() && !it.eof() && isspace(it.peek()))
        it.next();
    return it.pos.x;
}

cur2 Editor::vim_handle_J(Vim_Command *cmd, bool add_spaces) {
    int y, count;

    switch (world.vim_mode()) {
    case VI_NORMAL: {
        int o_count = cmd->o_count == 0 ? 1 : cmd->o_count;
        y = cur.y;
        count = max(o_count - 1, 1);
        break;
    }
    case VI_VISUAL: {
        auto sel = get_selection();

        int y1 = sel->ranges->at(0).start.y;
        int y2 = sel->ranges->last()->end.y;
        ORDER(y1, y2);

        y = y1;
        count = y2-y1;
        break;
    }
    default:
        return NULL_CUR;
    }

    // do the joining
    cur2 pos = NULL_CUR;
    {
        SCOPED_BATCH_CHANGE(buf);

        auto &lines = buf->lines;
        cur2 last_start = NULL_CUR;

        for (int i = 0; i < count; i++) {
            if (y == lines.len-1) break;

            auto start = new_cur2(lines[y].len, y);
            auto x = first_nonspace_cp(y+1);
            auto end = new_cur2(x, y+1);

            auto new_text = new_list(uchar);
            if (add_spaces && x < lines[y+1].len && lines[y+1][x] != ')')
                new_text->append(' ');

            buf->apply_edit(start, end, new_text->items, new_text->len);
            last_start = start;
        }

        pos = last_start;
    }

    if (world.vim_mode() == VI_VISUAL)
        vim_return_to_normal_mode();
    return pos;
}

// y1 to y2 inclusive
void Editor::indent_block(int y1, int y2, int indents) {
    SCOPED_BATCH_CHANGE(buf);

    for (int y = y1; y <= y2; y++) {
        auto x = first_nonspace_cp(y);
        int vx = buf->idx_cp_to_vcp(y, x);

        int new_vx = vx + max(options.tabsize * indents, 0);

        auto chars = new_list(uchar);
        for (int i = 0; i < new_vx / options.tabsize; i++)
            chars->append('\t');
        for (int i = 0; i < new_vx % options.tabsize; i++)
            chars->append(' ');

        buf->apply_edit(new_cur2(0, y), new_cur2(x, y), chars->items, chars->len);
    }
}

Vim_Parse_Status Editor::vim_parse_command(Vim_Command *out) {
    int ptr = 0;
    auto bof = [&]() { return ptr == 0; };
    auto eof = [&]() { return ptr == vim.command_buffer->len; };
    auto peek = [&]() { return vim.command_buffer->at(ptr); };
    auto mode = world.vim_mode();

    auto peek_char = [&]() -> char {
        if (eof()) return 0;

        auto it = peek();
        if (it.is_key) return 0;
        return it.ch;
    };

    auto peek_digit = [&]() -> char {
        if (eof()) return 0;

        auto it = peek();
        if (!it.is_key && it.ch < 127 && isdigit(it.ch))
            return (char)it.ch;
        return 0;
    };

    auto read_number = [&]() -> int {
        auto ret = new_list(char);
        char digit;
        while ((digit = peek_digit()) != 0) {
            ret->append(digit);
            ptr++;
        }
        ret->append('\0');
        return strtol(ret->items, NULL, 10);
    };

    auto read_count = [&]() -> int {
        auto digit = peek_digit();
        if (digit && digit != '0')
            return read_number();
        return 0;
    };

    auto char_input = [&](char ch) -> Vim_Command_Input {
        Vim_Command_Input ret; ptr0(&ret);
        ret.is_key = false;
        ret.ch = ch;
        return ret;
    };

    if (eof()) return VIM_PARSE_WAIT;

    out->o_count = read_count();

    if (eof()) return VIM_PARSE_WAIT;

    bool skip_motion = false;
    auto it = peek();

    if (it.is_key) {
        switch (it.mods) {
        case CP_MOD_NONE:
            switch (it.key) {
            case CP_KEY_TAB:
                out->op->append(it);
                skip_motion = true;
                break;
            }
            break;

        case CP_MOD_CTRL:
            switch (it.key) {
            case CP_KEY_R:
            case CP_KEY_O:
            case CP_KEY_I:
            case CP_KEY_E:
            case CP_KEY_Y:
            case CP_KEY_D:
            case CP_KEY_U:
            case CP_KEY_B:
            case CP_KEY_F:
                out->op->append(it);
                skip_motion = true;
                break;
            case CP_KEY_V:
                switch (mode) {
                case VI_NORMAL:
                case VI_VISUAL:
                    out->op->append(it);
                    skip_motion = true;
                    break;
                }
                break;
            }
            break;
        }
    } else {
        switch (it.ch) {
        case 'i':
        case 'I':
        case 'a':
        case 'A':
            if (mode == VI_VISUAL)
                break;
            skip_motion = true;
            out->op->append(it);
            ptr++;
            break;

        case 'U':
            if (mode != VI_VISUAL)
                break;
            skip_motion = true;
            out->op->append(it);
            ptr++;
            break;

        case '/':
        case 'u':
        case '~':
        case 'J':
        case 'R':
        case 'o':
        case 'O':
        case 's':
        case 'S':
        case 'x':
        case 'v':
        case 'V':
        case 'D':
        case 'Y':
        case 'C':
        case 'p':
        case 'P':
        case '.':
        case 'Q':
            skip_motion = true;
            out->op->append(it);
            ptr++;
            break;

        case 'q':
            if (world.vim.macro_state == MACRO_RECORDING) {
                skip_motion = true;
                out->op->append(it);
                ptr++;
            } else {
                ptr++;
                if (eof()) return VIM_PARSE_WAIT;

                auto ch = peek_char();
                if (ch < 127 && (isalnum(ch) && islower(ch)) || ch == '"') {
                    ptr++;
                    out->op->append(it);
                    out->op->append(char_input(ch));
                    skip_motion = true;
                    break;
                }

                ptr--;
            }
            break;

        case '@': {
            ptr++;
            if (eof()) return VIM_PARSE_WAIT;

            auto ch = peek_char();
            if (ch < 127 && ((isalnum(ch) && islower(ch)) || ch == '"' || ch == '@')) {
                ptr++;
                out->op->append(it);
                out->op->append(char_input(ch));
                skip_motion = true;
                break;
            }

            ptr--;
            break;
        }

        case 'm':
        case '`': { // in neovim ` is a motion, but fuck that, it's too hard
            ptr++;
            if (eof()) return VIM_PARSE_WAIT;

            auto ch = peek_char();
            if (ch < 127 && isalnum(ch)) {
                ptr++;
                out->op->append(it);
                out->op->append(char_input(ch));
                skip_motion = true;
                break;
            }

            ptr--;
            break;
        }

        case '[': {
            ptr++;
            if (eof()) return VIM_PARSE_WAIT;

            auto ch = peek_char();
            switch (ch) {
            case 'p':
            case 'q':
                ptr++;
                out->op->append(it);
                out->op->append(char_input(ch));
                skip_motion = true;
                break;
            }

            ptr--;
            break;
        }

        case ']': {
            ptr++;
            if (eof()) return VIM_PARSE_WAIT;

            auto ch = peek_char();
            switch (ch) {
            case 'p':
            case 'q':
                ptr++;
                out->op->append(it);
                out->op->append(char_input(ch));
                skip_motion = true;
                break;
            }

            ptr--;
            break;
        }

        case 'c':
        case 'd':
        case '>':
        case '<':
        case 'y':
        case '=':
            if (mode == VI_VISUAL)
                skip_motion = true;
            out->op->append(it);
            ptr++;
            break;

        case 'r': {
            ptr++;
            if (eof()) return VIM_PARSE_WAIT;

            auto ch = peek_char();
            if (ch) {
                ptr++;
                out->op->append(it);
                out->op->append(char_input(ch));
                skip_motion = true;
                break;
            }

            ptr--;
            break;
        }

        case 'g': {
            ptr++;
            if (eof()) return VIM_PARSE_WAIT;

            auto ch = peek_char();
            switch (ch) {
            case 'q':
            case 'w':
            case '@':
                ptr++;
                out->op->append(it);
                out->op->append(char_input(ch));
                goto done;
            case '~':
            case 'u':
            case 'U':
            case '?':
                if (mode == VI_VISUAL)
                    skip_motion = true;
                ptr++;
                out->op->append(it);
                out->op->append(char_input(ch));
                goto done;
            case 'd':
            case 'I':
            case 'J':
                ptr++;
                out->op->append(it);
                out->op->append(char_input(ch));
                skip_motion = true;
                goto done;
            }

            ptr--;
            break;
        }
        case 'z': {
            ptr++;
            if (eof()) return VIM_PARSE_WAIT;

            auto it2 = peek();
            bool is_zscroll = false;

            if (it2.is_key) {
                is_zscroll = (it2.key == CP_KEY_ENTER && it2.mods == 0);
            } else {
                switch (it2.ch) {
                case 't':
                case '.':
                case 'z':
                case '-':
                case 'b':
                    is_zscroll = true;
                    break;
                }
            }

            if (is_zscroll) {
                ptr++;
                out->op->append(it);
                out->op->append(it2);
                skip_motion = true;
                break;
            }

            ptr--;
            break;
        }
        }
    }
done:

    // skip_motion implies o->op->len > 0
    if (skip_motion) return VIM_PARSE_DONE;

    if (eof()) return VIM_PARSE_WAIT;

    out->m_count = 0;
    if (out->o_count && !out->op->len) {
        out->m_count = out->o_count;
        out->o_count = 0;
    } else {
        out->m_count = read_count();
    }

    if (eof()) return VIM_PARSE_WAIT;

    it = peek();
    if (it.is_key) {
        if (it.key == CP_KEY_ENTER) {
            ptr++;
            out->motion->append(it);
            return VIM_PARSE_DONE;
        }

        switch (it.mods) {
        case CP_MOD_CTRL:
            switch (it.key) {
            case CP_KEY_M:
                ptr++;
                out->motion->append(it);
                return VIM_PARSE_DONE;
            }
            break;

        case CP_MOD_NONE:
            switch (it.key) {
            case CP_KEY_UP:
            case CP_KEY_LEFT:
            case CP_KEY_RIGHT:
            case CP_KEY_DOWN:
                ptr++;
                out->motion->append(it);
                return VIM_PARSE_DONE;
            }
            break;
        }

        return VIM_PARSE_DISCARD;
    }

    switch (it.ch) {
    case 'h':
    case 'j':
    case 'k':
    case 'l':
    case '-':
    case '+':
    case '_':
    case 'H':
    case 'L':
    case 'M':
    case '0':
    case '^':
    case '$':
    case '%':
    case 'w':
    case 'W':
    case 'e':
    case 'E':
    case 'b':
    case 'B':
    case 'G':
    case '}':
    case '{':
    case 'n':
    case 'N':
    case '*':
    case '#':
        ptr++;
        out->motion->append(it);
        return VIM_PARSE_DONE;
    case '[':
    case ']': {
        ptr++;
        if (eof()) return VIM_PARSE_WAIT;

        auto ch = peek_char();
        switch (ch) {
        case '[':
        case ']':
            ptr++;
            out->motion->append(it);
            out->motion->append(char_input(ch));
            return VIM_PARSE_DONE;
        }
        ptr--;
        break;
    }
    case 'g': {
        ptr++;
        if (eof()) return VIM_PARSE_WAIT;

        auto ch = peek_char();
        switch (ch) {
        case 'g':
        case 'e':
        case 'E':
        case 'o':
        case 'm':
        case 'M':
        case '^':
        case '$':
            ptr++;
            out->motion->append(it);
            out->motion->append(char_input(ch));
            return VIM_PARSE_DONE;
        }
        ptr--;
        break;
    }
    case 'f':
    case 'F':
    case 't':
    case 'T': {
        ptr++;
        if (eof()) return VIM_PARSE_WAIT;

        auto ch = peek_char();
        if (ch) {
            out->motion->append(it);
            out->motion->append(char_input(ch));
            return VIM_PARSE_DONE;
        }
        break;
    }

    case 'i':
    case 'a': {
        // text objects require visual mode or an operator
        if (mode != VI_VISUAL && !out->op->len)
            break;

        ptr++;
        if (eof()) return VIM_PARSE_WAIT;

        auto ch = peek_char();
        switch (ch) {
        case 'w':
        case 'W':
        case 's':
        case 'p':
        case '[':
        case ']':
        case '(':
        case ')':
        case 'b':
        case '<':
        case '>':
        case 'B':
        case '{':
        case '}':
        case 't':
        case '\'':
        case '"':
        case '`':
        case 'a':
        case 'f': // function
            out->motion->append(it);
            out->motion->append(char_input(ch));
            return VIM_PARSE_DONE;
        }
        break;
    }
    default: {
        if (out->op->len != 1) break;

        auto inp = out->op->at(0);
        if (inp.is_key) break;
        if (inp.ch != it.ch) break;

        switch (inp.ch) {
        case 'd':
        case 'y':
        case 'c':
        case '>':
        case '<':
            out->motion->append(inp);
            return VIM_PARSE_DONE;
        }
        break;
    }
    }

    return VIM_PARSE_DISCARD;
}

ccstr render_key(int key, int mods) {
    Text_Renderer rend; rend.init();

    auto parts = new_list(ccstr);
    if (mods & CP_MOD_CMD) parts->append("cmd");
    if (mods & CP_MOD_CTRL) parts->append("ctrl");
    if (mods & CP_MOD_ALT) parts->append("option");
    if (mods & CP_MOD_SHIFT) parts->append("shift");
    parts->append(key_str((Key)key) + strlen("CP_KEY_"));
    return join_array(parts, "+");
}

ccstr render_command_buffer(List<Vim_Command_Input> *arr) {
    Text_Renderer rend; rend.init();
    For (arr) {
        if (it.is_key)
            rend.write("<%s>", render_key(it.key, it.mods));
        else
            rend.write("%c", (char)it.ch);
    }
    return rend.finish();
}

ccstr render_command(Vim_Command *cmd) {
    Text_Renderer rend; rend.init();

    rend.write("operator: %d ", cmd->o_count);
    if (cmd->op->len)
        rend.write("%s", render_command_buffer(cmd->op));
    else
        rend.write("<none>", render_command_buffer(cmd->op));

    rend.write(" | motion: %d ", cmd->m_count);
    if (cmd->motion->len)
        rend.write("%s", render_command_buffer(cmd->motion));
    else
        rend.write("<none>", render_command_buffer(cmd->motion));

    return rend.finish();
}

bool gr_isspace(Grapheme gr) {
    if (!gr) return false;

    if (gr->len == 1) {
        auto uch = gr->at(0);
        if (uch < 127 && isspace(uch))
            return true;
    }
    return false;
}

bool gr_isident(Grapheme gr) {
    if (!gr) return false;

    if (gr->len == 1) {
        auto uch = gr->at(0);
        if (isident(uch))
            return true;
    }
    return false;
}

ccstr Editor::get_selection_text(Selection *selection) {
    switch (selection->type) {
    case SEL_CHAR: {
        auto range = selection->ranges->at(0);
        return buf->get_text(range.start, range.end);
    }
    case SEL_LINE: {
        auto range = selection->ranges->at(0);
        auto text = buf->get_text(range.start, range.end);
        return cp_strcat(text, "\n");
    }
    case SEL_BLOCK: {
        auto lines = new_list(ccstr);
        For (selection->ranges)
            lines->append(buf->get_text(it.start, it.end));
        return join_array(lines, "\n");
    }
    }
    cp_panic("invalid selection type");
}

Gr_Type gr_type(Grapheme gr) {
    if (!gr) return GR_NULL;
    if (gr_isspace(gr)) return GR_SPACE;
    if (gr_isident(gr)) return GR_IDENT;
    return GR_OTHER;
}

Motion_Result* Editor::vim_eval_motion(Vim_Command *cmd) {
    auto motion = cmd->motion;
    cp_assert(motion->len);

    auto op = cmd->op;

    auto c = cur;
    auto &lines = buf->lines;
    auto ret = new_object(Motion_Result);

    int o_count = (cmd->o_count == 0 ? 1 : cmd->o_count);
    int m_count = (cmd->m_count == 0 ? 1 : cmd->m_count);
    int count = o_count * m_count;

    // TODO: does this break anything?
    c = buf->fix_cur(c);
    cp_assert(buf->is_valid(c));

    auto get_second_char_arg = [&]() -> uchar {
        if (motion->len < 2) return 0;
        auto &inp = motion->at(1);
        if (inp.is_key) return 0;
        return inp.ch;
    };

    auto return_line_first_nonspace = [&](int y) {
        ret->dest = new_cur2(first_nonspace_cp(y), y);
        ret->type = MOTION_LINE;
    };

#define CLAMP_UPPER(val, hi) do { if (val > hi) { val = hi; ret->interrupted = true; } } while (0)
#define CLAMP_LINE_Y(val) CLAMP_UPPER(val, lines.len-1)

    auto handle_plus_command = [&]() {
        int y = cur.y + count;
        CLAMP_LINE_Y(y);
        return_line_first_nonspace(y);
    };

    auto handle_basic_up_down = [&](bool down) -> bool {
        int newy = c.y + count * (down ? 1 : -1);
        if (newy < 0)          { ret->interrupted = true; newy = 0; }
        if (newy >= lines.len) { ret->interrupted = true; newy = lines.len-1; }
        if (newy == c.y)       { ret->interrupted = true; return false; }

        auto vx = buf->idx_cp_to_vcp(c.y, c.x);
        auto newvx = max(vx, vim.hidden_vx);

        auto newx = buf->idx_vcp_to_cp(newy, newvx);
        if (newx >= lines[newy].len && lines[newy].len > 0)
            newx = lines[newy].len-1;

        ret->dest = new_cur2(newx, newy);
        ret->type = MOTION_LINE;
        return true;
    };

    auto handle_basic_left = [&]() {
        auto it = iter();
        for (int i = 0; i < count && !it.bol(); i++) {
            it.gr_prev();
            if (it.bol()) {
                ret->interrupted = true;
                break;
            }
        }
        ret->dest = it.pos;
        ret->type = MOTION_CHAR_EXCL;
    };

    auto handle_basic_right = [&]() {
        auto it = iter();
        for (int i = 0; i < count && !it.eol(); i++) {
            it.gr_next();
            if (it.eol()) {
                ret->interrupted = true;
                break;
            }
        }
        ret->dest = it.pos;
        ret->type = MOTION_CHAR_EXCL;
    };

    auto inp = motion->at(0);
    if (inp.is_key) {
        switch (inp.key) {
        case CP_KEY_ENTER:
            handle_plus_command();
            return ret;
        }

        switch (inp.mods) {
        case CP_MOD_CTRL:
            switch (inp.key) {
            case CP_KEY_M:
                handle_plus_command();
                return ret;
            }
            break;
        case CP_MOD_NONE:
            switch (inp.key) {
            case CP_KEY_DOWN:
            case CP_KEY_UP:
                if (!handle_basic_up_down(inp.key == CP_KEY_DOWN))
                    break;
                return ret;
            case CP_KEY_RIGHT:
                handle_basic_right();
                return ret;
            case CP_KEY_LEFT:
                handle_basic_left();
                return ret;
            }
            break;
        }
    } else {
        switch (inp.ch) {
        case 'g': {
            auto second = get_second_char_arg();
            if (!second) break;

            switch (second) {
            case 'e':
            case 'E': {
                /*
                test string: alksdjahf  $$$$asdjalf  asjfh

                if we're on a space:
                    find first nonspace
                else
                    find first non-type
                    if next is a space, find first nonspace

                more simply...

                if nonspace:
                    goto nontype
                if space:
                    goto nonspace

                note that those are not exclusive if statements
                */

                auto it = iter();

                for (int i = 0; i < count && !it.bof(); i++) {
                    auto type = gr_type(it.gr_peek());
                    if (type != GR_SPACE) {
                        while (!it.bof()) {
                            auto gr = it.gr_prev();
                            if (second == 'e' && gr_type(gr) != type) break;
                            if (second == 'E' && gr_isspace(gr)) break;
                        }
                    }

                    if (it.bof()) {
                        ret->interrupted = true;
                        break;
                    }

                    if (gr_isspace(it.gr_peek())) {
                        while (!it.bof()) {
                            if (!gr_isspace(it.gr_prev()))
                                break;
                            if (it.bof())
                                ret->interrupted = true;
                        }
                    }
                }

                ret->dest = it.pos;
                ret->type = MOTION_CHAR_INCL;
                return ret;
            }

            case 'g':
                if (!cmd->o_count && !cmd->m_count) {
                    return_line_first_nonspace(0);
                } else {
                    int y = count-1;
                    CLAMP_LINE_Y(y);
                    return_line_first_nonspace(y);
                }
                return ret;
            case 'o': {
                ret->dest = buf->offset_to_cur(count - 1, &ret->interrupted);
                ret->type = MOTION_CHAR_EXCL;
                return ret;
            }
            }
            break;
        }

        case '{':
        case '}': {
            auto isblank = [&](int y) { return !lines[y].len; };
            auto isok = [&](int y) { return 0 <= y && y < lines.len; };

            cur2 pos = c;

            for (int i = 0; i < count; i++) {
                int y = pos.y;
                if (isblank(y))
                    while (isok(y) && isblank(y))
                        y += (inp.ch == '{' ? -1 : 1);

                while (isok(y) && !isblank(y))
                    y += (inp.ch == '{' ? -1 : 1);

                if (y == -1) {
                    pos = new_cur2(0, 0);
                    ret->interrupted = true;
                    break;
                }

                if (y >= lines.len) {
                    y = lines.len-1;
                    pos = new_cur2(relu_sub(lines[y].len, 1), y);
                    ret->interrupted = true;
                    break;
                }

                pos = new_cur2(0, y);
            }

            ret->dest = pos;
            ret->type = MOTION_CHAR_EXCL;
            return ret;
        }

        case '[':
        case ']': {
            auto arg = get_second_char_arg();
            switch (arg) {
            case '[':
            case ']': {
                bool forward = (inp.ch == ']');
                bool find_close = (arg != inp.ch);

                auto try_using_ast = [&]() -> bool {
                    if (lang != LANG_GO) return false;
                    auto root = new_ast_node(ts_tree_root_node(buf->tree), NULL);
                    if (root->type() != TS_SOURCE_FILE) return false;

                    Ast_Node *target = NULL;
                    Ast_Node *last = NULL;

                    FOR_NODE_CHILDREN (root) {
                        if (it->end() >= c) {
                            target = it;
                            break;
                        }
                        last = it;
                    }

                    if (!target) {
                        if (forward) return false;
                        target = last;
                    }

                    cur2 dest = c;
                    for (int i = 0; i < count; i++) {
                        if (forward) {
                            if (dest >= target->start())
                                if (!find_close || buf->inc_gr(dest) == target->end())
                                    if (!(target = target->next())) {
                                        ret->interrupted = true;
                                        break;
                                    }
                        } else {
                            if (dest < target->end())
                                if (find_close || dest <= target->start())
                                    if (!(target = target->prev())) {
                                        ret->interrupted = true;
                                        break;
                                    }
                        }
                        dest = (find_close ? buf->dec_gr(target->end()) : target->start());
                    }

                    ret->dest = dest;
                    return true;
                };

                if (!try_using_ast()) {
                    int y = cur.y;

                    for (int i = 0; i < count; i++) {
                        uchar target = (find_close ? '}' : '{');
                        if (forward) {
                            for (; y < lines.len-1; y++)
                                if (lines[y].len && lines[y][0] == target) {
                                    ret->interrupted = true;
                                    break;
                                }
                        } else {
                            for (; y > 0; y--)
                                if (lines[y].len && lines[y][0] == target) {
                                    ret->interrupted = true;
                                    break;
                                }
                        }
                    }
                    ret->dest = new_cur2(0, y);
                }

                ret->type = MOTION_CHAR_EXCL;
                return ret;
            }
            }
            break;
        }

        case 'f':
        case 't': {
            auto arg = get_second_char_arg();
            if (!arg) break;

            auto it = iter();
            cur2 last;

            for (int i = 0; i < count && !it.eol(); i++) {
                // ignore the first character
                last = it.pos;
                it.gr_peek();
                it.gr_next();

                while (!it.eol()) {
                    auto gr = it.gr_peek();
                    if (gr->len == 1)
                        if (arg == gr->at(0))
                            break;
                    last = it.pos;
                    it.gr_next();
                }
            }

            if (it.eol()) {
                ret->interrupted = true;
                break;
            }

            ret->dest = inp.ch == 't' ? last : it.pos;
            ret->type = MOTION_CHAR_INCL;
            return ret;
        }

        case 'i':
        case 'a': {
            ret->type = (inp.ch == 'i' ? MOTION_OBJ_INNER : MOTION_OBJ);

            auto arg = get_second_char_arg();
            if (!arg) break;

            switch (arg) {
            case 'w':
            case 'W': {
                // look at char under start
                // walk backward and eat all chars with same type
                //
                // for iw,
                //
                //     1. look to right to find current word
                //     2. if whole word highlighted, look rightward for next word
                //
                // for aw,
                //
                //     if char under start is a space, grab all spaces + next word
                //     otherwise, grab next word + all spaces
                //     if whole range highlighted, repeat this process rightward

                bool already_had_visual = false;
                cur2 start, end;
                if (world.vim_mode() == VI_VISUAL) {
                    if (vim.visual_start != c)
                        already_had_visual = true;
                    start = vim.visual_start;
                    end = c;
                } else {
                    start = c;
                    end = c;
                }

                // auto start = (world.vim_mode() == VI_VISUAL ? vim.visual_start : c);
                // auto end = c;
                bool forward = start <= end;
                // SWAP(start, end);

                auto is_edge = [&](Grapheme gr, Gr_Type starting_type) {
                    if (arg == 'w')
                        return (gr_type(gr) != starting_type);
                    return (gr_isspace(gr) != (starting_type == GR_SPACE));
                };

                auto it = iter();

                auto advance = [&]() { forward ? it.gr_next () : it.gr_prev(); };

                auto literally_eat_forward = [&](fn<bool(Grapheme)> check) {
                    while (!it.eof()) {
                        auto old = it.pos;
                        it.gr_next();
                        if (it.eof() || check(it.gr_peek())) {
                            it.pos = old;
                            break;
                        }
                    }
                };

                auto literally_eat_backward = [&](fn<bool(Grapheme)> check) {
                    while (!it.bof()) {
                        auto old = it.pos;
                        if (check(it.gr_prev())) {
                            it.pos = old;
                            break;
                        }
                    }
                };

                auto eat_forward = [&](auto check) {
                    if (forward)
                        literally_eat_forward(check);
                    else
                        literally_eat_backward(check);
                };

                auto eat_backward = [&](auto check) {
                    if (forward)
                        literally_eat_backward(check);
                    else
                        literally_eat_forward(check);
                };

                Grapheme starting_gr = 0;

                {
                    it.pos = start;
                    starting_gr = it.gr_peek();

                    if (!already_had_visual) {
                        // walk backward to find start of word
                        auto type = gr_type(starting_gr);
                        eat_backward([&](auto x) { return is_edge(x, type); });
                        start = it.pos;
                    }
                }

                auto find_next_word = [&]() {
                    auto eat_spaces = [&]() {
                        eat_forward([&](auto x) { return !gr_isspace(x); });
                    };

                    auto eat_word = [&]() {
                        auto type = gr_type(it.gr_peek());
                        eat_forward([&](auto x) { return is_edge(x, type); });
                    };

                    if (ret->type == MOTION_OBJ_INNER) {
                        eat_word();
                        return;
                    }

                    if (starting_gr != NULL && gr_isspace(starting_gr)) {
                        eat_spaces();
                        advance();
                        eat_word();
                    } else {
                        eat_word();

                        auto old = it.pos;
                        advance();
                        if (gr_isspace(it.gr_peek())) {
                            eat_spaces();
                        } else {
                            it.pos = old;
                        }
                    }
                };

                for (int i = 0; i < count; i++) {
                    it.pos = end;

                    find_next_word();
                    // if the word we found is already captured by our range, find the next word
                    if (forward ? (it.pos <= end) : (it.pos >= end)) {
                        advance();
                        if (!(forward ? it.eof() : it.bof()))
                            find_next_word();
                    }

                    end = it.pos;

                    if (forward ? it.eof() : it.bof()) {
                        ret->interrupted = true;
                        break;
                    }
                }

                if (already_had_visual)
                    ret->type = MOTION_CHAR_INCL;
                else
                    ret->object_start = start;
                ret->dest = end;
                return ret;
            }

            case '[':
            case ']':
            case '(':
            case ')':
            case 'b':
            case 'B':
            case '{':
            case '}': {
                char open, close;
                switch (arg) {
                case '[': open = '['; close = ']'; break;
                case ']': open = '['; close = ']'; break;
                case '(': open = '('; close = ')'; break;
                case ')': open = '('; close = ')'; break;
                case 'b': open = '('; close = ')'; break;
                case 'B': open = '{'; close = '}'; break;
                case '{': open = '{'; close = '}'; break;
                case '}': open = '{'; close = '}'; break;
                case '<': open = '<'; close = '>'; break;
                case '>': open = '<'; close = '>'; break;
                }

                // note for iw/aw: when visual select is one char long it will
                // select whole word, but if select is multiple chars, it will
                // only select rightward

                // ( asdf ( asdf asdf ( asdf asdf ) ) asdf )

                // 1. walk left from start of range to find the first unmatched `open` brace
                // 2. call find_matching_brace to find the `close` brace
                // 3. if we're "already covering" open:close then repeat the process, this time starting on open?
                //    a. if ret->type == MOTION_OBJ, we have to be *covering* it
                //       - i don't actually think this can happen, since if we were covering it, step 1 would
                //         already move to the next outer brace
                //    b. if ret->type == MOTION_OBJ_INNER, we must be covering [open+1, close-1]
                // 4. note: i guess it's the same for iw/aw, except it's rightward?

                auto find_next_pair_with_range = [&](cur2 *start, cur2 *end) -> bool {
                    // find the open brace
                    auto it = iter(*start);
                    int depth = 1;
                    while (!it.bof()) {
                        auto ch = it.prev();
                        if (ch == close) {
                            depth++;
                        }
                        if (ch == open) {
                            if (--depth == 0)
                                break;
                        }
                    }
                    if (depth) return false;

                    // find the close brace
                    auto closing = find_matching_brace(open, it.pos);
                    if (closing == NULL_CUR) return false;

                    *start = it.pos;
                    *end = closing;
                    return true;
                };

                // define our current range
                auto start = (world.vim_mode() == VI_VISUAL ? vim.visual_start : c);
                auto end = c;

                bool error = false;

                for (int i = 0; i < count; i++) {
                    auto newstart = start;
                    auto newend = end;
                    if (!find_next_pair_with_range(&newstart, &newend)) {
                        error = true;
                        break;
                    }

                    // determine if we need to repeat the previous process
                    // i.e. if we're already "covering" the given range
                    auto is_already_covering = [&]() {
                        if (start == end) return false;

                        auto a = newstart;
                        auto b = newend;
                        if (ret->type == MOTION_OBJ_INNER) {
                            a = buf->inc_cur(a);
                            b = buf->dec_cur(b);
                        }
                        return (start <= a && end >= b);
                    };

                    if (is_already_covering()) {
                        if (!find_next_pair_with_range(&newstart, &newend)) {
                            error = true;
                            break;
                        }
                    }

                    start = newstart;
                    end = newend;
                }

                if (error) {
                    ret->interrupted = true;
                    break;
                }

                if (ret->type == MOTION_OBJ_INNER) {
                    start = buf->inc_cur(start);
                    end = buf->dec_cur(end);
                }

                ret->object_start = start;
                ret->dest = end;
                return ret;
            }

            case '"':
            case '\'':
            case '`': {
                if (count > 1) break;

                Ts_Ast_Type node_type;
                switch (arg) {
                case '"': node_type = TS_INTERPRETED_STRING_LITERAL; break;
                case '`': node_type = TS_RAW_STRING_LITERAL; break;
                case '\'': node_type = TS_RUNE_LITERAL; break;
                }

                cur2 start = NULL_CUR;
                cur2 end = NULL_CUR;

                // first try with ast
                if (lang == LANG_GO) {
                    auto root = new_ast_node(ts_tree_root_node(buf->tree), NULL);
                    find_nodes_containing_pos(root, cur, false, [&](auto it) {
                        if (it->type() != node_type) return WALK_CONTINUE;

                        start = it->start();
                        end = buf->dec_cur(it->end());
                        return WALK_ABORT;
                    });
                }

                // then try with just text
                if (start == NULL_CUR || end == NULL_CUR) {
                    auto its = iter(c);
                    auto ite = iter(c);

                    while (true) {
                        if (its.peek() == arg) break;
                        if (arg == '`') {
                            if (its.bof()) break;
                        } else {
                            if (its.bol()) break;
                        }
                        its.prev();
                    }

                    if (its.peek() != arg) break;

                    ite.next();
                    while (true) {
                        if (ite.peek() == arg) break;
                        if (arg == '`') {
                            if (ite.eof()) break;
                        } else {
                            if (ite.eol()) break;
                        }
                        ite.next();
                    }

                    if (ite.peek() != arg) break;

                    start = its.pos;
                    end = ite.pos;
                }

                cp_assert(start != NULL_CUR);
                cp_assert(end != NULL_CUR);

                if (ret->type == MOTION_OBJ_INNER) {
                    start = buf->inc_cur(start);
                    end = buf->dec_cur(end);

                    // if we're already covering the inner area, grab the quotes back
                    if (world.vim_mode() == VI_VISUAL) {
                        auto curr_start = vim.visual_start;
                        auto curr_end = c;
                        if (curr_start <= start && curr_end >= end) {
                            start = buf->dec_cur(start);
                            end = buf->inc_cur(end);
                        }
                    }
                }

                ret->object_start = start;
                ret->dest = end;
                return ret;
            }

            // toplevel
            case 't': {
                if (count > 1) break;
                if (lang != LANG_GO) break;

                cur2 start = NULL_CUR, end = NULL_CUR;

                auto root = new_ast_node(ts_tree_root_node(buf->tree), NULL);
                find_nodes_containing_pos(root, cur, false, [&](auto it) {
                    auto parent = it->parent();
                    if (!parent) return WALK_CONTINUE;

                    if (parent->type() == TS_SOURCE_FILE) {
                        start = it->start();
                        end = it->end();
                    }

                    return WALK_ABORT;
                });

                if (start == NULL_CUR || end == NULL_CUR) break;

                ret->object_start = start;
                ret->dest = buf->dec_gr(end);
                return ret;
            }

            case 'a': break; // argument?
            case 'f': break; // function?
                             // think about supporting ast based text objects (it is literally our strength)

            // these are low priority, i just don't believe anyone uses them whilst coding
            case 's': break;
            case 'p': break;
            }
            break;
        }
        case 'F':
        case 'T': {
            auto arg = get_second_char_arg();
            if (!arg) break;

            auto it = iter();
            int start_y = it.pos.y;
            cur2 last;

            auto do_shit = [&]() {
                for (int i = 0; i < count; i++) {
                    // we can *end* on bol, but we can't be at bol mid-process
                    if (it.bol()) return false;

                    last = it.pos;
                    auto gr = it.gr_prev();

                    while (true) {
                        // gr guaranteed to be valid, since it.x != 0
                        cp_assert(gr);

                        // if it's a match, break
                        if (gr->len == 1 && arg == gr->at(0))
                            break;

                        // try to move back, checking for bol so the cp_assert(gr)
                        // above remains consistent
                        last = it.pos;
                        if (it.bol()) return false;
                        gr = it.gr_prev();
                    }
                }
                return true;
            };

            if (!do_shit()) {
                ret->interrupted = true;
                break;
            }

            ret->dest = inp.ch == 'T' ? last : it.pos;
            ret->type = MOTION_CHAR_INCL;
            return ret;
        }

        case 'G':
            if (!cmd->o_count && !cmd->m_count) {
                return_line_first_nonspace(lines.len-1);
            } else {
                int y = count-1;
                CLAMP_LINE_Y(y);
                return_line_first_nonspace(y);
            }
            return ret;

        case '0':
            ret->dest = new_cur2(0, c.y);
            ret->type = MOTION_CHAR_EXCL;
            return ret;

        case '^': {
            int x = first_nonspace_cp(c.y);
            ret->dest = new_cur2(x, c.y);
            ret->type = MOTION_CHAR_EXCL;
            return ret;
        }

        case '$': {
            int y = c.y + count-1;
            CLAMP_LINE_Y(y);
            ret->dest = new_cur2(lines[y].len, y);
            ret->type = MOTION_CHAR_INCL;
            return ret;
        }
        case 'c':
        case 'y':
        case '<':
        case '>':
        case 'd': {
            if (op->len != 1) break;

            auto it = op->at(0);
            if (it.is_key || it.ch != inp.ch) break;

            int y = c.y + count-1;
            CLAMP_LINE_Y(y);

            ret->dest = new_cur2(0, y);
            ret->type = MOTION_LINE;
            return ret;
        }

        case '%': {
            if (cmd->o_count == 0 && cmd->m_count == 0) {
                auto it = iter();
                uchar ch = 0;

                for (; !it.eof(); it.next()) {
                    switch (it.peek()) {
                    case '{':
                    case '[':
                    case '(':
                    case '}':
                    case ']':
                    case ')':
                        ch = it.peek();
                        break;
                    }
                    if (ch) break;
                }

                if (!ch) {
                    ret->interrupted = true;
                    break;
                }

                auto res = find_matching_brace(ch, it.pos);
                if (res == NULL_CUR) {
                    ret->interrupted = true;
                    break;
                }

                ret->dest = res;
                ret->type = MOTION_CHAR_INCL;
                return ret;
            } else {
                if (count > 100) {
                    ret->interrupted = true;
                    break;
                }

                // don't need to clamp, any input 1-100 is valid
                int y = min(lines.len * count / 100, lines.len-1);
                ret->dest = new_cur2(first_nonspace_cp(y), y);
                ret->type = MOTION_CHAR_EXCL;
                return ret;
            }
        }

        case '*':
        case '#':
        case 'n':
        case 'N': {
            if (inp.ch == '*' || inp.ch == '#') {
                auto it = iter();
                while (!it.eof() && !gr_isident(it.gr_peek()))
                    it.gr_next();

                if (it.eof()) {
                    ret->interrupted = true;
                    break;
                }

                while (!it.bof()) {
                    auto old = it.pos;
                    auto gr = it.gr_prev();
                    if (!gr_isident(gr)) {
                        it.pos = old;
                        break;
                    }
                }

                auto chars = new_list(char);
                while (!it.eof()) {
                    auto gr = it.gr_next();
                    if (!gr_isident(gr)) break;
                    For (gr) {
                        char buf[4];
                        auto count = uchar_to_cstr(it, buf);
                        for (int i = 0; i < count; i++)
                            chars->append(buf[i]);
                    }
                }

                auto &wnd = world.wnd_local_search;

                if (chars->len + 2 > _countof(wnd.query)) {
                    ret->interrupted = true;
                    break;
                }

                auto query = cp_sprintf("\\b%.*s\\b", chars->len, chars->items);

                cp_strcpy_fixed(wnd.permanent_query, query);
                cp_strcpy_fixed(wnd.query, query);
                wnd.replace = false;
                wnd.case_sensitive = false;
                wnd.use_regex = true;

                trigger_file_search();
            }

            // TODO: move new search result
            auto idx = move_file_search_result(inp.ch == 'n' || inp.ch == '*', count);
            if (idx == -1) {
                ret->interrupted = true;
                break;
            }

            auto node = buf->search_tree->get_node(idx);
            if (!node) {
                ret->interrupted = true;
                break;
            }

            file_search.current_idx = idx;
            ret->dest = node->pos;
            ret->type = MOTION_CHAR_EXCL;
            return ret;
        }

        case 'j':
        case 'k':
            if (!handle_basic_up_down(inp.ch == 'j'))
                break;
            return ret;
        case 'h':
            handle_basic_left();
            return ret;
        case 'l':
            handle_basic_right();
            return ret;
        case '-': {
            int y = cur.y - count;
            if (y < 0) {
                ret->interrupted = true;
                y = 0;
            }
            return_line_first_nonspace(y);
            return ret;
        }
        case '+': {
            handle_plus_command();
            return ret;
        }
        case '_': {
            int y = cur.y + count-1;
            CLAMP_LINE_Y(y);
            return_line_first_nonspace(y);
            return ret;
        }
        case 'H':
        case 'M':
        case 'L': {
            u32 y = 0;
            if (inp.ch == 'H') y = view.y + min(view.h - 1, max(count - 1, options.scrolloff));
            if (inp.ch == 'M') y = view.y + (view.h / 2);
            if (inp.ch == 'L') y = view.y + relu_sub(view.h, 1 + max(count - 1, options.scrolloff));

            // this needs to be refactored out
            if (y >= lines.len) {
                // does this count as an interruption?
                y = lines.len ? lines.len-1 : 0;
            }

            return_line_first_nonspace(y);
            return ret;
        }
        case 'w':
        case 'W': {
            auto it = iter();

            for (int i = 0; i < count; i++) {
                auto gr = it.gr_peek();
                it.gr_next();

                auto type = gr_type(gr);
                while (!it.eof()) {
                    gr = it.gr_peek();
                    if (inp.ch == 'w') {
                        if (gr_type(gr) != type)
                            break;
                    } else {
                        if (gr_isspace(gr) != (type == GR_SPACE))
                            break;
                    }
                    it.gr_next();
                }

                if (type != GR_SPACE) {
                    while (!it.eof()) {
                        auto gr = it.gr_peek();
                        if (!gr_isspace(gr)) break;
                        it.gr_next();
                    }
                }
                if (it.eof()) {
                    ret->interrupted = true;
                    break;
                }
            }

            ret->dest = it.pos;
            ret->type = MOTION_CHAR_EXCL;
            return ret;
        }
        case 'b':
        case 'B': {
            auto it = iter();

            for (int i = 0; i < count; i++) {
                auto is_start = [&](Grapheme prev_graph, Gr_Type current_type) {
                    if (inp.ch == 'b')
                        return gr_type(prev_graph) != current_type;
                    return gr_isspace(prev_graph);
                };

                auto type = gr_type(it.gr_peek());

                auto old = it.pos;
                auto gr = it.gr_prev();
                it.pos = old;

                if (type == GR_SPACE || is_start(gr, type)) {
                    while (!it.bof()) {
                        auto old = it.pos;
                        if (!gr_isspace(it.gr_prev())) {
                            it.pos = old;
                            break;
                        }
                    }
                }

                if (it.bof()) {
                    ret->interrupted = true;
                    break;
                }

                // now go to the front of the previous word
                type = gr_type(it.gr_prev());
                while (!it.bof()) {
                    auto old = it.pos;
                    if (is_start(it.gr_prev(), type)) {
                        it.pos = old;
                        break;
                    }
                }

                if (it.bof()) {
                    ret->interrupted = true;
                    break;
                }
            }

            ret->dest = it.pos;
            ret->type = MOTION_CHAR_EXCL;
            return ret;
        }
        case 'e':
        case 'E': {
            auto it = iter();

            for (int i = 0; i < count; i++) {
                auto is_end = [&](Grapheme next_graph, Gr_Type current_type) {
                    if (inp.ch == 'e')
                        return gr_type(next_graph) != current_type;
                    return gr_isspace(next_graph);
                };

                auto type = gr_type(it.gr_peek());
                it.gr_next();

                if (!it.eof() && (type == GR_SPACE || is_end(it.gr_peek(), type)))
                    while (!it.eof() && gr_isspace(it.gr_peek()))
                        it.gr_next();

                if (it.eof()) {
                    ret->interrupted = true;
                    break;
                }

                // now go to the end of the next word
                type = gr_type(it.gr_peek());
                while (!it.eof()) {
                    auto old = it.pos;
                    it.gr_next();
                    if (is_end(it.gr_peek(), type)) {
                        it.pos = old;
                        break;
                    }
                }

                if (it.eof()) {
                    ret->interrupted = true;
                    break;
                }
            }

            ret->dest = it.pos;
            ret->type = MOTION_CHAR_INCL;
            return ret;
        }

        }
    }

#undef CLAMP_UPPER
#undef CLAMP_LINE_Y

    return NULL;
}

void Editor::vim_enter_visual_mode(Selection_Type type) {
    world.vim_set_mode(VI_VISUAL);
    vim.visual_start = cur;
    vim.visual_type = type;
}

void Editor::vim_handle_visual_mode_key(Selection_Type type) {
    if (world.vim_mode() == VI_VISUAL) {
        if (vim.visual_type == type)
            vim_return_to_normal_mode();
        else
            vim.visual_type = type;
    } else {
        vim_enter_visual_mode(type);
    }
}

// mirrors buf->remove_lines, y1-y2 are inclusive
void Editor::vim_delete_lines(int y1, int y2) {
    Selection_Range range; ptr0(&range);
    range.start = new_cur2(0, y1);
    range.end = new_cur2(buf->lines[y2].len, y2);

    Selection sel; ptr0(&sel);
    sel.type = SEL_LINE;
    sel.ranges = new_list(Selection_Range);
    sel.ranges->append(&range);

    // this handles SEL_LINE correctly
    vim_delete_selection(&sel);
}

void Editor::vim_yank_text(ccstr text) {
    if (options.vim_use_clipboard) {
        world.window->set_clipboard_string(text);
        return;
    }

    auto &reg = world.vim.yank_register;
    reg.len = 0;
    int len = strlen(text);
    reg.ensure_cap(len + 1);
    for (int i = 0; i < len; i++)
        reg.append(text[i]);
    reg.append('\0');
    world.vim.yank_register_filled = true;
}

ccstr Editor::vim_paste_text() {
    if (options.vim_use_clipboard)
        return world.window->get_clipboard_string();
    if (!world.vim.yank_register_filled)
        return NULL;
    return world.vim.yank_register.items;
}

cur2 Editor::vim_delete_range(cur2 start, cur2 end) {
    Selection_Range range; ptr0(&range);
    range.start = start;
    range.end = end;

    Selection sel; ptr0(&sel);
    sel.type = SEL_CHAR;
    sel.ranges = new_list(Selection_Range);
    sel.ranges->append(&range);
    return vim_delete_selection(&sel);
}

cur2 Editor::delete_selection(Selection *sel) {
    switch (sel->type) {
    case SEL_CHAR:
    case SEL_BLOCK: {
        // it should be safe to go forwards since the only time we have
        // multiple ranges is SEL_BLOCK, and then all the blocks are on
        // separate lines and don't touch newlines.
        //
        // if we go forwards undo lands in right spot but redo is wrong
        // vice versa if we go backwards
        // what we really need is a way to group a bunch of edits together in history
        // so that they share a starting cursor point
        For (sel->ranges)
            buf->remove(it.start, it.end);
        break;
    }
    case SEL_LINE: {
        auto &range = sel->ranges->at(0);
        buf->remove_lines(range.start.y, range.end.y);
        break;
    }
    }

    return sel->ranges->at(0).start;
}

cur2 Editor::vim_delete_selection(Selection *sel) {
    auto text = get_selection_text(sel);
    vim_yank_text(text);
    auto ret = delete_selection(sel);
    if (ret.y >= buf->lines.len)
        ret = new_cur2(0, buf->lines.len-1);
    return ret;
}

// copies *into* dest inline, instead of making a complete copy
// expects dest to be initialized
void vim_copy_command(Vim_Command *dest, Vim_Command *src) {
    dest->m_count = src->m_count;
    dest->o_count = src->o_count;

    dest->op->len = 0;
    For (src->op)
        dest->op->append(it);

    dest->motion->len = 0;
    For (src->motion)
        dest->motion->append(it);
}

void Editor::vim_enter_insert_mode(Vim_Command *cmd, fn<void()> prep) {
    buf->hist_batch_start(); // end in vim_return_to_normal_mode()

    // honestly it feels pretty ugly/brittle to handle this here
    Selection *visual_block_sel = NULL;
    do {
        if (world.vim_mode() != VI_VISUAL) break;
        if (vim.visual_type != SEL_BLOCK) break;

        auto sel = get_selection();
        if (sel->type != SEL_BLOCK) break;

        visual_block_sel = sel;
    } while (0);

    prep(); // call this after starting history batch

    vim.insert_start = cur;
    vim.edit_backspaced_graphemes = 0;
    vim_copy_command(&vim.insert_command, cmd); // why are we doing this?
    vim.insert_visual_block_other_starts.len = 0;

    if (visual_block_sel) {
        For (visual_block_sel->ranges) {
            auto pos = it.start;

            if (pos.y == cur.y) continue;
            if (pos.y >= buf->lines.len) continue;
            if (pos.x > buf->lines[pos.y].len) continue;

            vim.insert_visual_block_other_starts.append(pos);
        }
    }

    world.vim_set_mode(VI_INSERT);
}

void Editor::vim_enter_replace_mode() {
    buf->hist_batch_start(); // end in vim_return_to_normal_mode()

    vim.replace_start = cur;
    vim.replace_end = cur;
    vim.replace_old_chars.len = 0;
    vim.edit_backspaced_graphemes = 0;

    world.vim_set_mode(VI_REPLACE);
}

// like vim_return_to_normal_mode but triggered by user
void Editor::vim_return_to_normal_mode_user_input() {
    /*
    // if we're directly escaping out of visual mode, wipe dotrepeat (we didn't perform any operations)
    if (world.vim_mode() == VI_VISUAL) {
        vim.dotrepeat.input_working.filled = false;
    }
    */
    vim_return_to_normal_mode();
}

void Editor::vim_return_to_normal_mode(bool from_dotrepeat) {
    auto mode = world.vim_mode();
    switch (mode) {
    case VI_INSERT:
    case VI_REPLACE: {
        auto &ref = vim.inserted_indent;
        // TODO: so far we're only handling indent for insert mode. Need to
        // support for replace mode too. But that comes with handling Enter.
        if (mode == VI_INSERT && ref.inserted && ref.buf_version == buf->buf_version) {
            // honestly should be an assert, but i dunno when this might fail
            // can't just buf->remove() and move_cursor(), need to backspace,
            // because we need the backspace logic
            if (cur == ref.end)
                while (cur > ref.start)
                    backspace_in_insert_mode();
        } else {
            auto gr = buf->idx_cp_to_gr(cur.y, cur.x);
            if (gr) {
                auto x = buf->idx_gr_to_cp(cur.y, gr-1);
                move_cursor(new_cur2(x, cur.y));
            }
        }

        auto inserted_text = new_list(uchar);
        {
            auto a = mode == VI_INSERT ? vim.insert_start : vim.replace_start;
            auto b = buf->inc_cur(cur);
            ORDER(a, b);

            for (auto it = iter(a); it.pos != b; it.next())
                inserted_text->append(it.peek());
        }

        if (vim.insert_visual_block_other_starts.len)
            if (vim.insert_start.y == cur.y)
                For (&vim.insert_visual_block_other_starts)
                    buf->insert(it, inserted_text->items, inserted_text->len);

        buf->hist_batch_end(); // started when entering vi_insert and vi_replace

        do {
            if (from_dotrepeat) break;

            auto &vdr = world.vim.dotrepeat;

            auto &iw = vdr.input_working;
            if (!iw.filled) break;

            Vim_Dotrepeat_Command *out = NULL;
            {
                SCOPED_MEM(&vdr.mem_working);
                out = iw.commands->append();
                out->type = VDC_INSERT_TEXT;
                out->insert_text.backspaced_graphemes = vim.edit_backspaced_graphemes;
                out->insert_text.chars = new_list(uchar);
                out->insert_text.chars->concat(inserted_text);
            }

            vim_dotrepeat_commit();
        } while (0);

        vim.insert_start = NULL_CUR;
        break;
    }
    // anything else? other modes?
    }

    world.vim_set_mode(VI_NORMAL, /* calling_from_vim_return_to_normal_mode = */ true);
}

Motion_Range* Editor::vim_process_motion(Motion_Result *motion_result) {
    auto ret = new_object(Motion_Range);

    switch (motion_result->type) {
    // is there even any difference between OBJ and OBJ_INNER outside of vim_eval_motion?
    case MOTION_OBJ:
    case MOTION_OBJ_INNER:
    case MOTION_CHAR_INCL:
    case MOTION_CHAR_EXCL: {
        cur2 a = cur;
        cur2 b = motion_result->dest;
        if (motion_result->type == MOTION_OBJ || motion_result->type == MOTION_OBJ_INNER)
            a = motion_result->object_start;

        ORDER(a, b);

        if (motion_result->type != MOTION_CHAR_EXCL) {
            auto it = iter(b);
            it.gr_peek();
            it.gr_next();
            b = it.pos;
        }

        ret->start = a;
        ret->end = b;
        ret->is_line = false;
        break;
    }
    case MOTION_LINE: {
        int y1 = cur.y;
        int y2 = motion_result->dest.y;

        if (y1 > y2) {
            int tmp = y1;
            y1 = y2;
            y2 = tmp;
        }

        cur2 start = new_cur2(0, y1);
        cur2 end = new_cur2(0, y2+1);

        bool at_end = (y2 == buf->lines.len-1);
        if (at_end) {
            start = buf->dec_cur(start);
            end = buf->end_pos();
        }

        ret->start = start;
        ret->end = end;
        ret->is_line = true;
        ret->line_info.at_end = at_end;
        ret->line_info.y1 = y1;
        ret->line_info.y2 = y2;
    }
    }
    return ret;
}

void Editor::vim_transform_text(uchar command, cur2 a, cur2 b) {
    auto transform_char = [&](uchar it) -> uchar {
        switch (command) {
        case '?': // g?
            if ('a' <= it && it <= 'z')
                return 'a' + ((it - 'a' + 13) % 26);
            else if ('A' <= it && it <= 'A')
                return 'A' + ((it - 'A' + 13) % 26);
            break;
        case '~':
            if ('a' <= it && it <= 'z')
                return toupper(it);
            if ('A' <= it && it <= 'Z')
                return tolower(it);
            break;
        case 'u':
            if ('A' <= it && it <= 'Z')
                return tolower(it);
            break;
        case 'U':
            if ('a' <= it && it <= 'z')
                return toupper(it);
            break;
        }
        return it;
    };

    auto new_chars = new_list(uchar);
    auto it = iter(a);
    while (!it.eof() && it.pos != b)
        new_chars->append(transform_char(it.next()));
    buf->apply_edit(a, b, new_chars->items, new_chars->len);
}

cur2 Editor::vim_handle_text_transform_command(char command, Motion_Result *motion_result) {
    SCOPED_BATCH_CHANGE(buf);

    switch (world.vim_mode()) {
    case VI_VISUAL: {
        auto sel = get_selection();
        For (sel->ranges)
            vim_transform_text(command, it.start, it.end);
        vim_return_to_normal_mode();
        return sel->ranges->at(0).start;
    }
    case VI_NORMAL: {
        auto res = vim_process_motion(motion_result);
        vim_transform_text(command, res->start, res->end);
        return res->start;
    }
    }
    return NULL_CUR;
}

bool Editor::vim_exec_command(Vim_Command *cmd, bool *can_dotrepeat) {
    *can_dotrepeat = false;

    auto mode = world.vim_mode();

    vim.last_command_interrupted = false;

    Motion_Result *motion_result = NULL;
    if (cmd->motion->len) {
        motion_result = vim_eval_motion(cmd);
        if (!motion_result || motion_result->interrupted)
            vim.last_command_interrupted = true;
        if (cmd->motion->len && !motion_result)
            return false;
    }

    auto &c = cur;
    auto &lines = buf->lines;

#define CLAMP_UPPER(val, hi) do { if (val > hi) { val = hi; vim.last_command_interrupted = true; } } while (0)
#define CLAMP_LINE_Y(val) CLAMP_UPPER(val, lines.len-1)

    if (mode == VI_NORMAL)
        buf->hist_force_push_next_change = true;

    int o_count = cmd->o_count == 0 ? 1 : cmd->o_count;

    // Move cursor in normal mode.
    auto move_cursor_normal = [&](cur2 pos) {
        auto len = lines[pos.y].len;
        if (pos.x >= len)
            pos.x = len ? len-1 : 0;

        bool update_hidden_vx = false;

        // man this is getting too "magical" and complicated
        if (motion_result)
            if (motion_result->type == MOTION_CHAR_INCL || motion_result->type == MOTION_CHAR_EXCL)
                update_hidden_vx = true;

        // is is part of "moving" back to normal mode
        if (mode != VI_NORMAL && world.vim_mode() == VI_NORMAL)
            update_hidden_vx = true;

        if (update_hidden_vx)
            vim.hidden_vx = buf->idx_cp_to_vcp(pos.y, pos.x);

        move_cursor(pos);
    };

    auto enter_insert_mode = [&](auto f) {
        vim_enter_insert_mode(cmd, f);
    };

    auto delete_selection_enter_insert_mode = [&](Selection *selection) {
        cp_assert(selection);

        enter_insert_mode([&]() {
            if (selection->type == SEL_LINE) {
                vim_delete_selection(selection);
                // can't use the return value from
                // vim_delete_selection, because if it's at the end
                // it'll return the line above, and we want to open the
                // newline at the line the deletion was made
                auto range = selection->ranges->at(0);
                move_cursor(open_newline(range.start.y));
            } else {
                auto start = vim_delete_selection(selection);
                move_cursor(start);
            }
        });
    };

    auto op = cmd->op;
    if (!op->len) {
        auto mr = motion_result;
        cp_assert(mr);

        auto end = mr->dest;
        if (world.vim_mode() != VI_VISUAL) {
            if (end.y >= lines.len)
                end = buf->end_pos();
            int len = lines[end.y].len;
            if (end.x >= len && len > 0) end.x = len-1;
        } else if (mr->type == MOTION_OBJ || mr->type == MOTION_OBJ_INNER) {
            vim.visual_start = mr->object_start;
        }

        move_cursor_normal(end);
        return true;
    }

    auto get_second_char_arg = [&]() -> uchar {
        if (op->len < 2) return 0;
        auto &inp = op->at(1);
        if (inp.is_key) return 0;
        return inp.ch;
    };

    auto inp = op->at(0);
    if (inp.is_key) {
        switch (inp.mods) {
        case CP_MOD_NONE:
            switch (inp.key) {
            case CP_KEY_TAB: {
                if (!world.history.go_forward(o_count))
                    vim.last_command_interrupted = true;
                return true;
            }
            }
            break;
        case CP_MOD_CTRL:
            switch (inp.key) {
            case CP_KEY_E:
            case CP_KEY_Y: {
                int y = 0;
                if (inp.key == CP_KEY_E) {
                    y = view.y + o_count;
                    CLAMP_LINE_Y(y);
                } else {
                    y = view.y - o_count;
                    if (y < 0) {
                        y = 0;
                        vim.last_command_interrupted = true;
                    }
                }
                int old = view.y;
                view.y = y;
                if (old != view.y) ensure_cursor_on_screen();
                return true;
            }

            case CP_KEY_D:
            case CP_KEY_U:
            case CP_KEY_B:
            case CP_KEY_F: {
                bool forward = false;
                int jumpsize = 0;

                switch (inp.key) {
                case CP_KEY_D: forward = true; jumpsize = view.h/2; break;
                case CP_KEY_U: forward = false; jumpsize = view.h/2; break;
                case CP_KEY_B: forward = false; jumpsize = view.h; break;
                case CP_KEY_F: forward = true; jumpsize = view.h; break;
                }

                jumpsize *= o_count;
                if (forward) {
                    int maxy = lines.len-1;
                    view.y = min(maxy, view.y + jumpsize);
                    move_cursor(new_cur2((int)cur.x, (int)min(maxy, cur.y + jumpsize)));
                } else {
                    view.y = relu_sub(view.y, jumpsize);
                    move_cursor(new_cur2((int)cur.x, (int)relu_sub(cur.y, jumpsize)));
                }
                ensure_cursor_on_screen();
                break;
            }

            case CP_KEY_R: {
                cur2 pos;
                for (int i = 0; i < o_count; i++) {
                    pos = buf->hist_redo();
                    if (pos == NULL_CUR) return true;
                }
                move_cursor_normal(pos);
                return true;
            }
            case CP_KEY_TAB:
            case CP_KEY_I:
                if (!world.history.go_forward(o_count))
                    vim.last_command_interrupted = true;
                return true;
            case CP_KEY_O:
                if (!world.history.go_backward(o_count))
                    vim.last_command_interrupted = true;
                return true;
            case CP_KEY_V:
                vim_handle_visual_mode_key(SEL_BLOCK);
                *can_dotrepeat = true;
                return true;
            }
            break;
        }
    } else {
        switch (inp.ch) {
        case '.': {
            if (world.vim_mode() != VI_NORMAL) break;

            auto &vdi = world.vim.dotrepeat.input_finished;
            if (!vdi.filled) break;

            auto run_commands = [&]() {
                For (vdi.commands) {
                    switch (it.type) {
                    case VDC_COMMAND: {
                        Vim_Command tmp; tmp.init();
                        vim_copy_command(&tmp, &it.command);
                        if (cmd->o_count != 0) {
                            tmp.o_count = cmd->o_count;
                            tmp.m_count = 0;
                        }

                        bool junk;
                        if (!vim_exec_command(&tmp, &junk))
                            return false;
                    }

                    case VDC_VISUAL_MOVE: {
                        auto newcur = new_cur2(cur.x + it.visual_move_distance.x, cur.y + it.visual_move_distance.y);
                        if (newcur.y >= lines.len)
                            newcur.y = lines.len-1;
                        if (newcur.x >= lines[newcur.y].len)
                            newcur.x = lines[newcur.y].len-1;
                        move_cursor(newcur);
                        break;
                    }

                    case VDC_INSERT_TEXT: {
                        auto mode = world.vim_mode();
                        for (int i = 0; i < it.insert_text.backspaced_graphemes; i++) {
                            switch (mode) {
                            case VI_INSERT:
                                backspace_in_insert_mode();
                                break;
                            case VI_REPLACE:
                                backspace_in_replace_mode();
                                break;
                            }
                        }

                        For (it.insert_text.chars) {
                            Type_Char_Opts opts; ptr0(&opts);
                            opts.replace_mode = (mode == VI_REPLACE);
                            opts.automated = true;
                            type_char(it, &opts);
                        }

                        vim_return_to_normal_mode(true); // from_dotrepeat
                        break;
                    }
                    }
                }
                return true;
            };

            if (!run_commands()) break;
            return true;
        }

        case ']':
        case '[': {
            auto arg = get_second_char_arg();
            switch (arg) {
            case 'q':
                move_search_result(inp.ch == ']', o_count);
                return true;
            }
            break;
        }

        case 'm': {
            auto arg = get_second_char_arg();
            if (!arg) break;
            if (!isalnum(arg)) break;

            auto mark = world.mark_fridge.alloc();
            buf->insert_mark(MARK_VIM_MARK, cur, mark);

            auto clear_marks = [&](Mark** marks, int idx) {
                if (marks[idx]) {
                    marks[idx]->cleanup();
                    world.mark_fridge.free(marks[idx]);
                    marks[idx] = NULL;
                }
            };

            if (isdigit(arg) || isupper(arg)) {
                int idx = (isdigit(arg) ? arg-'0'+26 : arg-'A');
                For (get_all_editors())
                    clear_marks(it->vim.global_marks, idx);
                vim.global_marks[idx] = mark;
            } else {
                clear_marks(vim.local_marks, arg - 'a');
                vim.local_marks[arg - 'a'] = mark;
            }
            return true;
        }

        case '`': {
            auto arg = get_second_char_arg();
            if (!arg) break;
            if (!isalnum(arg)) break;

            Mark *mark = NULL;
            Editor *editor = NULL;

            auto try_editor = [&](Editor *it, Mark **marks, int idx) {
                if (marks[idx]) {
                    mark = marks[idx];
                    editor = it;
                    return true;
                }
                return false;
            };

            auto find_mark = [&]() -> bool {
                if (isupper(arg) || isdigit(arg)) {
                    int idx = (isdigit(arg) ? arg-'0'+26 : arg-'A');
                    For (get_all_editors())
                        if (try_editor(it, it->vim.global_marks, idx))
                            return true;
                    return false;
                }
                return try_editor(this, vim.local_marks, arg - 'a');
            };

            if (!find_mark()) return false;

            focus_editor_by_id(editor->id, mark->pos());
            return true;
        }

        case 'q': {
            if (world.vim.macro_state == MACRO_RECORDING) {
                auto macro = vim_get_macro(world.vim.macro_record.macro);
                if (macro && macro->inputs->len) {
                    auto input = macro->inputs->last();
                    if (!input->is_key && input->ch == 'q')
                        macro->inputs->len--;
                }
                world.vim.macro_state = MACRO_IDLE;
                world.vim.macro_record.macro = 0;
                return true;
            }

            if (world.vim.macro_state != MACRO_IDLE) break;

            auto arg = get_second_char_arg();
            if (!arg) break;

            auto macro = vim_get_macro(arg);
            if (!macro) break;

            if (macro->active)
                macro->mem.reset();
            else
                macro->mem.init();
            macro->active = true;
            {
                SCOPED_MEM(&macro->mem);
                macro->inputs = new_list(Vim_Command_Input);
            }

            world.vim.macro_state = MACRO_RECORDING;
            world.vim.macro_record.macro = (char)arg;
            world.vim.macro_record.last = (char)arg;
            return true;
        }

        case 'Q':
        case '@': {
            if (world.vim.macro_state != MACRO_IDLE) break;

            char arg = 0;

            if (inp.ch == 'Q') {
                arg = world.vim.macro_record.last;
            } else {
                arg = get_second_char_arg();
                if (arg == '@')
                    arg = world.vim.macro_run.last;
            }

            if (!arg) break;
            if (!(arg < 127 && (isalnum(arg) && islower(arg)) || arg == '"')) break;

            int idx = isalpha(arg) ? arg-'a' : (isdigit(arg) ? arg-'0' + 26 : 26+10);
            auto &macro = world.vim.macros[idx];
            if (!macro.active) break;

            world.vim.macro_state = MACRO_RUNNING;
            world.vim.macro_run.macro = arg;
            world.vim.macro_run.runs = o_count;
            world.vim.macro_run.run_idx = 0;
            world.vim.macro_run.input_idx = 0;
            world.vim.macro_run.last = arg; // do we set this before or after the run?
            return true;
        }

        case 'p':
        case 'P': {
            SCOPED_BATCH_CHANGE(buf);

            auto raw_text = vim_paste_text();
            if (!raw_text) return false;
            if (!raw_text[0]) return false;

            auto text = cstr_to_ustr(raw_text);
            bool as_line = (*text->last() == '\n');

            auto paste_and_move_cursor = [&](cur2 pos) {
                auto full_text = new_list(uchar);
                for (int i = 0; i < o_count; i++)
                    full_text->concat(text);

                cur2 newpos = buf->insert(pos, full_text->items, full_text->len);
                if (as_line)
                    newpos = new_cur2(first_nonspace_cp(pos.y), pos.y);
                else
                    newpos = buf->dec_gr(newpos);
                move_cursor_normal(newpos);
            };

            switch (world.vim_mode()) {
            case VI_VISUAL: {
                auto sel = get_selection();

                if (as_line) {
                    switch (sel->type) {
                    case SEL_CHAR:
                    case SEL_LINE: {
                        vim_delete_selection(sel);

                        auto &range = sel->ranges->at(0);
                        int y = range.start.y;

                        if (sel->type == SEL_CHAR) {
                            buf->insert(range.start, '\n');
                            y++;
                        }
                        paste_and_move_cursor(new_cur2(0, y));
                        break;
                    }
                    case SEL_BLOCK:
                        // TODO
                        break;
                    }
                } else {
                    switch (sel->type) {
                    case SEL_CHAR:
                    case SEL_LINE: {
                        auto start = vim_delete_selection(sel);
                        paste_and_move_cursor(start);
                        break;
                    }
                    case SEL_BLOCK:
                        // TODO
                        break;
                    }
                }
                vim_return_to_normal_mode();
                *can_dotrepeat = true;
                return true;
            }
            case VI_NORMAL:
                if (as_line) {
                    int y = 0;
                    if (inp.ch == 'p') {
                        if (c.y == lines.len-1) {
                            buf->insert(buf->end_pos(), '\n');
                            text->len--; // ignore the last newline
                        }
                        y = c.y+1;
                    } else {
                        y = c.y;
                    }
                    paste_and_move_cursor(new_cur2(0, y));
                } else {
                    cur2 pos = c;
                    if (inp.ch == 'p')
                        if (lines[pos.y].len)
                            pos = buf->inc_gr(pos);
                    paste_and_move_cursor(pos);
                }
                *can_dotrepeat = true;
                return true;
            }
            break;
        }
        case 'v':
            vim_handle_visual_mode_key(SEL_CHAR);
            *can_dotrepeat = true;
            return true;
        case 'V':
            vim_handle_visual_mode_key(SEL_LINE);
            *can_dotrepeat = true;
            return true;
        case 'i':
            if (mode != VI_NORMAL) return false;
            enter_insert_mode([]() { return; });
            *can_dotrepeat = true;
            return true;
        case 'a': {
            auto gr = buf->idx_cp_to_gr(c.y, c.x);
            auto x = buf->idx_gr_to_cp(c.y, gr+1);
            move_cursor(new_cur2(x, c.y));
            enter_insert_mode([]() { return; });
            *can_dotrepeat = true;
            return true;
        }
        case 'A': {
            move_cursor(new_cur2(lines[c.y].len, c.y));
            enter_insert_mode([]() { return; });
            *can_dotrepeat = true;
            return true;
        }
        case 'R': {
            switch (world.vim_mode()) {
            case VI_NORMAL:
                vim_enter_replace_mode();
                *can_dotrepeat = true;
                return true;
            case VI_VISUAL: {
                delete_selection_enter_insert_mode(get_selection(SEL_LINE));
                *can_dotrepeat = true;
                return true;
            }
            }
        }
        case 'I': {
            if (world.vim_mode() == VI_NORMAL) {
                move_cursor(new_cur2(first_nonspace_cp(c.y), c.y));
                enter_insert_mode([]() { return; });
                *can_dotrepeat = true;
                return true;
            }
            break;
        }
        case 'o':
        case 'O':
            switch (world.vim_mode()) {
            case VI_NORMAL: {
                int y = inp.ch == 'o' ? c.y + 1 : c.y;
                enter_insert_mode([&]() {
                    move_cursor(open_newline(y));
                });
                *can_dotrepeat = true;
                return true;
            }
            case VI_VISUAL:
                *can_dotrepeat = true;
                return true;
            }
            break;

        case 's':
            switch (world.vim_mode()) {
            case VI_NORMAL:
                enter_insert_mode([&]() {
                    auto it = iter(c);
                    it.gr_next();
                    vim_delete_range(c, it.pos);
                    // move_cursor(c); // redundant
                });
                *can_dotrepeat = true;
                return true;
            case VI_VISUAL: {
                delete_selection_enter_insert_mode(get_selection());
                *can_dotrepeat = true;
                return true;
            }
            }
            break;

        case 'S':
            switch (world.vim_mode()) {
            case VI_NORMAL:
                enter_insert_mode([&]() {
                    int y2 = c.y + o_count - 1;
                    CLAMP_LINE_Y(y2);
                    vim_delete_lines(c.y, y2);
                    move_cursor(open_newline(c.y));
                });
                *can_dotrepeat = true;
                return true;
            case VI_VISUAL: {
                delete_selection_enter_insert_mode(get_selection(SEL_LINE));
                *can_dotrepeat = true;
                return true;
            }
            }
            break;

        case 'Y': {
            switch (world.vim_mode()) {
            case VI_NORMAL: {
                int yend = c.y + o_count - 1;
                CLAMP_LINE_Y(yend);
                auto end = new_cur2(lines[yend].len, yend);
                vim_yank_text(buf->get_text(c, end));
                return true;
            }
            case VI_VISUAL:
                auto selection = get_selection(SEL_LINE);
                vim_yank_text(get_selection_text(selection));

                vim_return_to_normal_mode();
                move_cursor_normal(selection->ranges->at(0).start);
                return true;
            }
            break;
        }

        case 'D':
        case 'C':
            switch (world.vim_mode()) {
            case VI_NORMAL: {
                auto start = cur;
                int y = cur.y + o_count - 1;
                CLAMP_LINE_Y(y);
                auto end = new_cur2(lines[y].len, y);

                if (inp.ch == 'D') {
                    vim_delete_range(start, end);
                    move_cursor_normal(start);
                } else {
                    enter_insert_mode([&]() {
                        vim_delete_range(start, end);
                        move_cursor(start);
                    });
                }
                *can_dotrepeat = true;
                return true;
            }
            case VI_VISUAL: {
                auto selection = get_selection(SEL_LINE);

                if (inp.ch == 'D') {
                    auto start = vim_delete_selection(selection);
                    vim_return_to_normal_mode();
                    move_cursor_normal(new_cur2(first_nonspace_cp(start.y), start.y));
                } else {
                    delete_selection_enter_insert_mode(selection);
                }
                *can_dotrepeat = true;
                return true;
            }
            }
            break;

        case 'U':
            if (mode == VI_VISUAL) {
                auto pos = vim_handle_text_transform_command(inp.ch, NULL);
                if (pos != NULL_CUR) {
                    move_cursor_normal(pos);
                    *can_dotrepeat = true;
                    return true;
                }
            }

        case '/':
            open_current_file_search(false, true);
            break;

        case '~':
            switch (mode) {
            case VI_NORMAL: {
                auto start = c;
                auto it = iter(start);
                for (int i = 0; i < o_count && !it.eol(); i++)
                    it.next();
                auto end = it.pos;

                vim_transform_text('~', start, end);
                move_cursor_normal(end);
                *can_dotrepeat = true;
                return true;
            }
            case VI_VISUAL: {
                auto pos = vim_handle_text_transform_command(inp.ch, NULL);
                if (pos != NULL_CUR) {
                    move_cursor_normal(pos);
                    *can_dotrepeat = true;
                    return true;
                }
            }
            }
            break;

        case 'u': {
            switch (mode) {
            case VI_NORMAL: {
                cur2 pos = NULL_CUR;
                for (int i = 0; i < o_count; i++) {
                    auto nextpos = buf->hist_undo();
                    if (nextpos == NULL_CUR) {
                        vim.last_command_interrupted = true;
                        break;
                    }
                    pos = nextpos;
                }
                if (pos != NULL_CUR)
                    move_cursor_normal(pos);
                return true;
            }
            case VI_VISUAL: {
                auto pos = vim_handle_text_transform_command(inp.ch, NULL);
                if (pos != NULL_CUR) {
                    move_cursor_normal(pos);
                    *can_dotrepeat = true;
                    return true;
                }
                break;
            }
            }
            break;
        }
        case 'J': {
            auto pos = vim_handle_J(cmd, true);
            if (pos == NULL_CUR) break;

            move_cursor_normal(pos);
            *can_dotrepeat = true;
            return true;
        }
        case 'x': {
            SCOPED_BATCH_CHANGE(buf);

            switch (world.vim_mode()) {
            case VI_NORMAL: {
                auto it = iter();
                for (int i = 0; i < o_count && !it.eol(); i++) {
                    it.gr_peek();
                    it.gr_next();
                }

                if (c != it.pos) {
                    vim_delete_range(c, it.pos);
                    int len = lines[c.y].len;
                    if (c.x >= len && len > 0) {
                        move_cursor_normal(new_cur2(len-1, c.y));
                    }
                }
                *can_dotrepeat = true;
                return true;
            }
            case VI_VISUAL: {
                auto start = vim_delete_selection(get_selection());
                move_cursor_normal(start);
                vim_return_to_normal_mode();
                *can_dotrepeat = true;
                return true;
            }
            }
            break;
        }
        case '<':
        case '>': {
            switch (mode) {
            case VI_VISUAL: {
                auto sel = get_selection();
                int y1 = sel->ranges->at(0).start.y;
                int y2 = sel->ranges->last()->end.y;
                ORDER(y1, y2);

                indent_block(y1, y2, o_count * (inp.key == '<' ? -1 : 1));
                vim_return_to_normal_mode();
                move_cursor_normal(c); // enforce bounds on current pos
                *can_dotrepeat = true;
                return true;
            }
            case VI_NORMAL: {
                cp_assert(motion_result);

                int y1 = c.y;
                int y2 = motion_result->dest.y;
                ORDER(y1, y2);

                indent_block(y1, y2, inp.key == '<' ? -1 : 1);
                move_cursor_normal(new_cur2(first_nonspace_cp(y1), y1));
                *can_dotrepeat = true;
                return true;
            }
            }
            break;
        }

        case 'c': {
            SCOPED_BATCH_CHANGE(buf);
            switch (mode) {
            case VI_VISUAL: {
                delete_selection_enter_insert_mode(get_selection());
                *can_dotrepeat = true;
                return true;
            }
            case VI_NORMAL: {
                auto res = vim_process_motion(motion_result);
                enter_insert_mode([&]() {
                    vim_delete_range(res->start, res->end);
                    move_cursor(res->is_line ? open_newline(res->line_info.y1) : res->start);
                });
                *can_dotrepeat = true;
                return true;
            }
            }
            break;
        }

        case 'd': {
            SCOPED_BATCH_CHANGE(buf);
            switch (mode) {
            case VI_VISUAL: {
                auto sel = get_selection();
                auto start = vim_delete_selection(sel);
                vim_return_to_normal_mode();
                move_cursor_normal(start);
                *can_dotrepeat = true;
                return true;
            }
            case VI_NORMAL: {
                auto res = vim_process_motion(motion_result);
                if (res->is_line) {
                    vim_delete_range(res->start, res->end);
                    int y = res->line_info.y1;
                    CLAMP_LINE_Y(y);
                    move_cursor_normal(new_cur2(first_nonspace_cp(y), y));
                } else {
                    vim_delete_range(res->start, res->end);
                    move_cursor_normal(res->start);
                }
                *can_dotrepeat = true;
                return true;
            }
            }
            break;
        }

        case 'y': {
            switch (mode) {
            case VI_VISUAL: {
                auto sel = get_selection();
                vim_yank_text(get_selection_text(sel));
                vim_return_to_normal_mode();
                move_cursor_normal(sel->ranges->at(0).start);
                *can_dotrepeat = true;
                return true;
            }
            case VI_NORMAL: {
                auto res = vim_process_motion(motion_result);
                if (res->is_line) {
                    auto real_start = res->start;
                    if (res->line_info.at_end)
                        real_start = buf->inc_cur(real_start);

                    auto text = buf->get_text(real_start, res->end);
                    if (res->line_info.at_end)
                        text = cp_strcat(text, "\n");

                    vim_yank_text(text);
                } else {
                    vim_yank_text(buf->get_text(res->start, res->end));

                    switch (motion_result->type) {
                    case MOTION_OBJ:
                    case MOTION_OBJ_INNER:
                        move_cursor_normal(res->start);
                        break;
                    }
                }
                *can_dotrepeat = true;
                return true;
            }
            }
            break;
        }
        case 'r': {
            auto arg = get_second_char_arg();
            if (!arg) break;

            switch (mode) {
            case VI_VISUAL: {
                cp_assert(!motion_result);
                auto sel = get_selection();

                SCOPED_BATCH_CHANGE(buf);

                For (sel->ranges) {
                    auto &range = it;

                    cur2 start, end;
                    if (sel->type == SEL_LINE) {
                        start = new_cur2(0, range.start.y);
                        end = new_cur2(lines[range.end.y].len, range.end.y);
                    } else {
                        start = range.start;
                        end = range.end;
                    }
                    if (start == end) continue;
                    cp_assert(start < end);

                    for (int y = start.y; y <= end.y; y++) {
                        int x0 = (y == start.y ? start.x : 0);
                        int x1 = (y == end.y ? end.x : lines[y].len);

                        // from (x0,y) to (x1,y), fill everything with the new character

                        auto chars = new_list(uchar);
                        for (int i = 0, count = x1 - x0; i < count; i++)
                            chars->append(arg);

                        buf->apply_edit(new_cur2(x0, y), new_cur2(x1, y), chars->items, chars->len);
                    }

                    move_cursor_normal(start);
                    vim_return_to_normal_mode();
                }

                *can_dotrepeat = true;
                return true;
            }
            case VI_NORMAL: {
                auto it = iter();
                auto start = it.pos;
                auto new_text = new_list(uchar);

                for (int i = 0; i < o_count; i++) {
                    if (it.eol()) return true;
                    it.gr_next();
                    new_text->append(arg);
                }

                auto end = it.pos;

                buf->apply_edit(start, end, new_text->items, new_text->len);

                move_cursor_normal(new_cur2(start.x + new_text->len - 1, start.y));
                *can_dotrepeat = true;
                return true;
            }
            }
            break;
        }
        case '!':
            break;
        case '=':
            break;
        case 'g': {
            auto arg = get_second_char_arg();
            switch (arg) {
            case 'q':
            case 'w':
            case '@':
                break;

            case '~':
            case 'u':
            case 'U':
            case '?': {
                auto pos = vim_handle_text_transform_command(arg, motion_result);
                if (pos != NULL_CUR) {
                    move_cursor_normal(pos);
                    *can_dotrepeat = true;
                    return true;
                }
                break;
            }

            case 'I':
                switch (world.vim_mode()) {
                case VI_NORMAL:
                    move_cursor(new_cur2(0, c.y));
                    enter_insert_mode([]() { return; });
                    *can_dotrepeat = true;
                    return true;
                case VI_VISUAL:
                    move_cursor(new_cur2(0, c.y));
                    return true;
                }
                break;
            case 'd':
                if (world.vim_mode() != VI_NORMAL)
                    vim_return_to_normal_mode();
                handle_goto_definition();
                return true;
            case 'J': {
                auto pos = vim_handle_J(cmd, false);
                if (pos == NULL_CUR) break;

                move_cursor_normal(pos);
                *can_dotrepeat = true;
                return true;
            }
            }
            break;
        }
        case 'z': {
            uchar arg = 0;
            auto &inp2 = cmd->op->at(1);
            if (inp2.is_key) {
                // if this happens enough, maybe just turn all non-mod enter
                // key events into winev_char with ch = '\n'
                if (inp2.mods == CP_MOD_NONE && inp2.key == CP_KEY_ENTER)
                    arg = '\n';
                else
                    break;
            } else {
                arg = inp2.ch;
            }

            switch (arg) {
            case '\n':
            case 't':
            case '.':
            case 'z':
            case '-':
            case 'b': {
                enum Screen_Pos { SCREEN_TOP, SCREEN_MIDDLE, SCREEN_BOTTOM };

                Screen_Pos screen_pos;
                bool reset_cursor;

                switch (arg) {
                case '\n':
                case 't':
                    view.y = relu_sub(c.y, options.scrolloff);
                    reset_cursor = (arg == '\n');
                    break;
                case '.':
                case 'z':
                    view.y = relu_sub(c.y, view.h / 2);
                    reset_cursor = (arg == '.');
                    break;
                case '-':
                case 'b':
                    view.y = relu_sub(c.y + options.scrolloff + 1, view.h);
                    reset_cursor = (arg == '-');
                    break;
                }

                if (reset_cursor)
                    move_cursor(new_cur2(first_nonspace_cp(c.y), c.y));
                break;
            }
            }
            break;
        }
        }
    }

#undef CLAMP_UPPER
#undef CLAMP_LINE_Y

    return false;
}

cur2 Editor::open_newline(int y) {
    if (y == 0) {
        // if it's the first line, start at 0, 0 and insert
        // indent + \n, or rather, just \n
        buf->insert(new_cur2(0, 0), '\n');
        return new_cur2(0, 0);
    }

    // otherwise, start at the line above, and add \n + indent
    auto yp = y-1;

    auto indent_chars = get_autoindent(y);
    int indent_len = strlen(indent_chars);

    auto text = new_list(uchar);
    text->append((uchar)'\n');
    for (int i = 0; i < indent_len; i++)
        text->append((uchar)indent_chars[i]);
    buf->insert(new_cur2(buf->lines[yp].len, yp), text->items, text->len);

    auto start = new_cur2(0, y);
    auto end = new_cur2(indent_len, y);
    vim_save_inserted_indent(start, end);
    return end;
}

Find_Matching_Brace_Result Editor::find_matching_brace_with_ast(uchar ch, cur2 pos, cur2 *out) {
    cp_assert(lang == LANG_GO);

    Ts_Ast_Type brace_type = TS_ERROR;
    Ts_Ast_Type other_brace_type = TS_ERROR;
    bool forward = false;

    switch (ch) {
    case '}': brace_type = TS_RBRACE; other_brace_type = TS_LBRACE; forward = false; break;
    case ')': brace_type = TS_RPAREN; other_brace_type = TS_LPAREN; forward = false; break;
    case ']': brace_type = TS_RBRACK; other_brace_type = TS_LBRACK; forward = false; break;
    case '{': brace_type = TS_LBRACE; other_brace_type = TS_RBRACE; forward = true; break;
    case '(': brace_type = TS_LPAREN; other_brace_type = TS_RPAREN; forward = true; break;
    case '[': brace_type = TS_LBRACK; other_brace_type = TS_RBRACK; forward = true; break;
    default:
        cp_panic("find_matching_brace_with_ast called with invalid ch");
    }

    Parser_It it;
    it.init(buf);
    auto root_node = new_ast_node(ts_tree_root_node(buf->tree), &it);

    Ast_Node *brace_node = NULL;

    find_nodes_containing_pos(root_node, pos, false, [&](auto it) {
        if (it->type() != brace_type)
            return WALK_CONTINUE;

        brace_node = new_object(Ast_Node);
        memcpy(brace_node, it, sizeof(Ast_Node));
        return WALK_ABORT;
    });

    if (!brace_node) return FMB_AST_NOT_FOUND;
    if (brace_node->is_missing()) return FMB_AST_NOT_FOUND;

    auto walk_upwards = [&](auto curr) -> Ast_Node * {
        while (true) {
            // try to get sibling
            auto sib = forward ? curr->next_all() : curr->prev_all();
            if (sib) return sib;

            // unable to? get parent, try again
            curr = curr->parent();
            if (!curr) return NULL;
        }
    };

    auto curr = walk_upwards(brace_node);
    int depth = 1;
    cur2 ret = NULL_CUR;

    fn<void(Ast_Node*)> process_node = [&](Ast_Node* node) {
        if (ret != NULL_CUR) return;
        if (node->is_missing()) return;

        SCOPED_FRAME();

        auto children = new_list(Ast_Node*, node->all_child_count());
        FOR_ALL_NODE_CHILDREN (node) children->append(it);

        if (forward) {
            For (children)
                process_node(it);
        } else {
            for (; children->len > 0; children->len--)
                process_node(*children->last());
        }

        if (node->type() == brace_type)
            depth++;
        if (node->type() == other_brace_type) {
            if (!(--depth)) {
                ret = node->start();
                return;
            }
        }
    };

    for (; curr && ret == NULL_CUR; curr = walk_upwards(curr))
        process_node(curr);

    if (ret == NULL_CUR)
        return FMB_MATCH_NOT_FOUND;

    *out = ret;
    return FMB_OK;
}

cur2 Editor::find_matching_brace_with_text(uchar ch, cur2 pos) {
    bool forward;
    uchar other;

    switch (ch) {
    case '{': forward = true; other = '}'; break;
    case '}': forward = false; other = '{'; break;
    case '(': forward = true; other = ')'; break;
    case ')': forward = false; other = '('; break;
    case '[': forward = true; other = ']'; break;
    case ']': forward = false; other = '['; break;
    case '<': forward = false; other = '>'; break;
    case '>': forward = false; other = '<'; break;
    default:
        cp_panic("find_matching_brace_with_text called with invalid char");
        break;
    }

    auto it = iter(pos);
    int depth = 0;

    while (!(forward ? it.eof() : it.bof())) {
        if (it.peek() == ch) {
            depth++;
        } else if (it.peek() == other) {
            if (--depth == 0) {
                return it.pos;
            }
        }
        forward ? it.next() : it.prev();
    }

    return NULL_CUR;
}

cur2 Editor::find_matching_brace(uchar ch, cur2 pos) {
    if (lang == LANG_GO) {
        cur2 ret;
        int result = find_matching_brace_with_ast(ch, pos, &ret);
        if (result == FMB_OK) return ret;
        if (result == FMB_AST_NOT_FOUND) {
            // the brace didn't belong to an ast node, this means it's part of
            // a string or comment, proceed with text-based match
            return find_matching_brace_with_text(ch, pos);
        }
        return NULL_CUR;
    }
    return find_matching_brace_with_text(ch, pos);
}

Vim_Macro *Editor::vim_get_macro(char arg) {
    if (arg < 127) {
        if (isalpha(arg) && islower(arg)) return &world.vim.macros[arg-'a'];
        if (isdigit(arg))                 return &world.vim.macros[arg-'0' + 26];
        if (arg == '"')                   return &world.vim.macros[26+10];
    }
    return NULL;
}

void Editor::vim_dotrepeat_commit() {
    auto &vdr = world.vim.dotrepeat;

    vdr.mem_finished.reset();
    {
        SCOPED_MEM(&vdr.mem_finished);
        memcpy(&vdr.input_finished, vdr.input_working.copy(), sizeof(Vim_Dotrepeat_Input));
    }

    vdr.input_working.filled = false;
}

// oh fuck
// how do macros work with autocomplete?

void Editor::handle_type_enter() {
    // handle replace mode, it seems to insert a newline
    // actually i wonder if this will just work as written
    delete_selection();
    type_char('\n');

    auto indent_chars = get_autoindent(cur.y);
    auto start = cur;

    // remove any existing indent, this happens if you have "foo  bar" and
    // press enter right after "foo", the next line will contain "  bar"
    buf->remove(start, new_cur2(first_nonspace_cp(start.y), start.y));

    // insert the new indent
    insert_text_in_insert_mode(indent_chars);

    // if we're at the end of the line, save this indent so it can be
    // deleted if the user escapes out without further edits
    if (cur.x == buf->lines[cur.y].len)
        vim_save_inserted_indent(start, cur);
}

void Editor::handle_type_tab(int mods) {
    if (mods == CP_MOD_NONE) {
        auto& ac = autocomplete;
        if (ac.ac.results && ac.filtered_results->len) {
            auto idx = ac.filtered_results->at(ac.selection);
            auto& result = ac.ac.results->at(idx);
            perform_autocomplete(&result);
            return;
        }
    }
    type_char('\t');
}

void Editor::handle_type_backspace(int mods) {
    if (selecting) {
        delete_selection();
    } else {
        bool is_replace = world.vim.on && world.vim_mode() == VI_REPLACE;

        auto go_back_one_the_right_way = [&]() {
            if (is_replace)
                backspace_in_replace_mode();
            else
                backspace_in_insert_mode();
        };

        if (mods & CP_MOD_TEXT) {
            auto new_cur = handle_alt_move(true, true);
            while (cur > new_cur)
                go_back_one_the_right_way();
        } else {
            go_back_one_the_right_way();
        }
    }

    update_autocomplete(false);
    update_parameter_hint();
}

bool Editor::vim_handle_input(Vim_Command_Input *input) {
    do {
        if (world.vim.macro_state != MACRO_RECORDING) break;
        if (!world.vim.macro_record.macro) break;
        auto macro = vim_get_macro(world.vim.macro_record.macro);
        if (!macro) break;

        // when user presses q to end recording, the handler for q will erase
        // it from macro->inputs. not sure if that's the greatest idea
        macro->inputs->append(input);
    } while (0);

    if (input->is_key) {
        if (input->key == CP_KEY_ESCAPE) {
            handle_escape();
            if (world.vim_mode() != VI_NORMAL) {
                // NOTE: was `handled` supposed to just refer to whether it affected
                // autocomplete/parameter? will setting it to true in the vim handler
                // here break anything?
                vim_return_to_normal_mode_user_input();
            }
            vim.command_buffer->len = 0;
            return true;
        }
        switch (input->mods) {
        case CP_MOD_CTRL:
            switch (input->key) {
            case CP_KEY_C:
                if (world.vim_mode() != VI_NORMAL)
                    vim_return_to_normal_mode_user_input();
                vim.command_buffer->len = 0;
                return true;
            }
        }
    }

    switch (world.vim_mode()) {
    case VI_REPLACE:
    case VI_INSERT: {
        if (input->is_key) {
            switch (input->key) {
            case CP_KEY_ENTER:
                handle_type_enter();
                return true;
            case CP_KEY_BACKSPACE:
                handle_type_backspace(input->mods);
                return true;
            case CP_KEY_TAB:
                handle_type_tab(input->mods);
                return true;
            }
        } else {
            if (!is_modifiable()) return false;

            Type_Char_Opts opts; ptr0(&opts);
            opts.replace_mode = world.vim_mode() == VI_REPLACE;
            type_char(input->ch, &opts);
            return true;
        }
        break;
    }
    case VI_VISUAL:
    case VI_NORMAL: {
        vim.command_buffer->append(input);

#ifdef DEBUG_BUILD
        print("[%s | %zu] %s", cur.str(), buf->lines.len, render_command_buffer(vim.command_buffer));
#endif

        Vim_Command cmd;
        cmd.init();

        auto status = vim_parse_command(&cmd);
        if (status == VIM_PARSE_WAIT) return true; // this means it was processed

        vim.command_buffer->len = 0;
        if (status == VIM_PARSE_DISCARD) {
            print("command discarded");
            break;
        }

        auto old_mode = world.vim_mode();
        // bool was_normal = world.vim_mode() == VI_NORMAL;
        // bool was_visual = world.vim_mode() == VI_VISUAL;

        cur2 visual_select_distance = NULL_CUR;
        if (old_mode == VI_VISUAL)
            visual_select_distance = new_cur2(cur.x - vim.visual_start.x, cur.y - vim.visual_start.y);

        bool can_dotrepeat;
        if (vim_exec_command(&cmd, &can_dotrepeat)) {
            // several kinds of dotrepeatable changes:
            //
            //  - normal -> oper
            //  - normal -> insert/replace -> input
            //  - normal -> visual -> oper
            //  - normal -> visual -> insert -> input
            //
            //  so commit when we end in normal (that's "oper") and when we leave insert/replace

            auto &vdr = world.vim.dotrepeat;
            auto mode = world.vim_mode();

            auto should_save = [&]() {
                if (can_dotrepeat) {
                    // if we started in visual and ended in normal, that's fine
                    // if we ended in (i.e. entered) visual, that's fine
                    // if we started and ended in normal, that's fine
                    // just not start and end in visual
                    if (old_mode != VI_VISUAL || mode != VI_VISUAL)
                        return true;
                }
                return false;
            };

            if (should_save()) {
                // if we're starting from normal, begin new input
                if (old_mode == VI_NORMAL) {
                    vdr.mem_working.reset();
                    SCOPED_MEM(&vdr.mem_working);
                    ptr0(&vdr.input_working);
                    vdr.input_working.filled = true;
                    vdr.input_working.commands = new_list(Vim_Dotrepeat_Command);
                } else if (old_mode == VI_VISUAL) {
                    SCOPED_MEM(&vdr.mem_working);
                    auto out = vdr.input_working.commands->append();
                    out->type = VDC_VISUAL_MOVE;
                    out->visual_move_distance = visual_select_distance;
                }

                {
                    SCOPED_MEM(&vdr.mem_working);
                    auto out = vdr.input_working.commands->append();
                    out->type = VDC_COMMAND;
                    out->command.init();
                    vim_copy_command(&out->command, &cmd);
                }

                // if we're *ending* in normal, copy working -> finished
                if (mode == VI_NORMAL)
                    vim_dotrepeat_commit();
            }
        }
        return true;
    }
    }
    return false;
}

bool Editor::vim_handle_char(u32 ch) {
    Vim_Command_Input input; ptr0(&input);
    input.is_key = false;
    input.ch = ch;
    return vim_handle_input(&input);
}

bool Editor::vim_handle_key(int key, int mods) {
    Vim_Command_Input input; ptr0(&input);
    input.is_key = true;
    input.key = key;
    input.mods = mods;
    return vim_handle_input(&input);
}

void Editor::vim_execute_macro_little_bit(u64 deadline) {
    if (world.vim.macro_state != MACRO_RUNNING) return;

    auto &mr = world.vim.macro_run;

    auto macro = vim_get_macro(mr.macro);
    if (!macro) {
        world.vim.macro_state = MACRO_IDLE;
        return;
    }

    while (current_time_nano() < deadline) {
        // within each iteration of this loop, execute one input

        // check if we're at end of current run
        if (mr.input_idx == macro->inputs->len) {
            mr.input_idx = 0;
            mr.run_idx++;
        }

        // check if we finished runs
        if (mr.run_idx >= mr.runs) {
            world.vim.macro_state = MACRO_IDLE;
            return;
        }

        // grab the next input & increment
        auto input = &macro->inputs->at(mr.input_idx++);

        // if input failed, skip rest of run
        if (!vim_handle_input(input))
            mr.input_idx = macro->inputs->len;

        // cut the whole ting short if a command was interrupted
        if (vim.last_command_interrupted) {
            world.vim.macro_state = MACRO_IDLE;
            if (world.vim_mode() == VI_INSERT || world.vim_mode() == VI_REPLACE)
                vim_handle_key(CP_KEY_ESCAPE, 0);
            return;
        }
    }
}

void Editor::trigger_escape() {
    if (world.vim.on)
        vim_handle_key(CP_KEY_ESCAPE, 0);
    else
        handle_escape();
}

void Editor::trigger_file_search(int limit_start, int limit_end) {
    auto editors = get_all_editors();
    if (isempty(editors)) return;

    // clear out results in all editors
    For (editors) {
        auto editor = it;
        auto buf = editor->buf;
        auto tree = buf->search_tree;
        cp_assert(tree);

        tree->cleanup();
        tree->init();
        buf->search_mem.reset();
    }

    auto &wnd = world.wnd_local_search;
    if (!wnd.query[0]) return;

    Search_Session sess; ptr0(&sess);
    sess.case_sensitive = wnd.case_sensitive;
    sess.literal = !wnd.use_regex;
    sess.query = wnd.query;
    sess.qlen = strlen(wnd.query);
    if (!sess.init()) return;

    For (editors) {
        auto editor = it;
        auto buf = editor->buf;

        // this is so stupid, why doesn't pcre take a custom iterator?
        int len;
        auto text = buf->get_text(new_cur2(0, 0), buf->end_pos(), &len);
        auto matches = new_list(Search_Match);
        sess.search((char*)text, len, matches, -1);

        auto tree = editor->buf->search_tree;

        For (matches) {
            auto start = editor->offset_to_cur(it.start);
            auto end = editor->offset_to_cur(it.end);

            auto convert_groups = [&](List<int> *arr) -> List<cur2> * {
                if (!arr) return NULL;

                List<cur2> *ret;
                {
                    SCOPED_MEM(&buf->search_mem);
                    ret = new_list(cur2);
                }
                For (arr) ret->append(editor->offset_to_cur(it));
                return ret;
            };

            auto node = tree->insert_node(start);
            node->search_result.end = end;
            node->search_result.group_starts = convert_groups(it.group_starts);
            node->search_result.group_ends = convert_groups(it.group_ends);

            tree->check_tree_integrity();
        }
    }
}

int Editor::move_file_search_result(bool forward, int count) {
    auto tree = buf->search_tree;

    auto total = tree->get_size();
    if (!total) return -1;

    bool in_match = false;
    auto idx = find_current_or_next_match(cur, &in_match);

    if (forward) {
        if (!in_match) count--;
        idx += count;
    } else {
        idx += total - (count % total);
    }
    return idx % total;
}

int Editor::find_current_or_next_match(cur2 pos, bool *in_match) {
    auto tree = buf->search_tree;

    auto total = tree->get_size();
    if (!total) return -1;

    int idx = 0;
    auto curr = tree->root;
    Avl_Node *prev = NULL;

    while (curr) {
        if (curr->pos <= pos && pos < curr->search_result.end) {
            *in_match = true;
            return idx + tree->get_size(curr->left);
        }
        prev = curr;
        if (pos < curr->pos) {
            curr = curr->left;
        } else {
            idx += tree->get_size(curr->left) + 1;
            curr = curr->right;
        }
    }

    *in_match = false;
    int ret = idx;
    if (prev->search_result.end <= pos)
        ret = (ret + 1) % total;
    return ret;
}

char* gh_fmt_finish() {
    return gh_fmt_finish(options.format_with_gofumpt);
}

char* gh_fmt_finish(bool use_gofumpt) {
    auto ret = GHFmtFinish(use_gofumpt);
#ifdef DEBUG_BUILD
    if (!ret) {
        #
        auto err = GHGetLastError();
        if (err) {
            print("%s", err);
            GHFree(err);
        } else {
            print("unable to format and also there was no last error");
        }
    }
#endif
    return ret;
}
