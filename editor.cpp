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
        if (world.vim.mode != VI_VISUAL) return NULL;

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
                auto ax = buf->idx_vcp_to_cp(y, avx);
                int len = buf->lines[y].len;
                if (ax > len) continue;

                auto bx = min(buf->idx_vcp_to_cp(y, bvx), len);
                add_range(new_cur2(ax, y), new_cur2(bx, y));
            }
            return ret;
        }
        }
        return NULL;
    }

    if (!selecting) return NULL;

    ret->type = SEL_CHAR;
    auto a = select_start;
    auto b = cur;
    ORDER(a, b);
    add_range(a, b);
    return ret;
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

    buf->insert(cur, text->items, text->len);
    auto c = cur;
    for (u32 i = 0; i < text->len; i++)
        c = buf->inc_cur(c);

    move_cursor(c);
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

        buf->hist_batch_mode = true;
        defer { buf->hist_batch_mode = false; };

        auto import_to_add = see_if_we_need_autoimport();
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

                auto new_contents = GHFmtFinish(false);
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
        move_cursor(new_cur2(ac_start.x + name->len, ac_start.y));

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

void Editor::add_change_in_insert_mode(cur2 start, cur2 old_end, cur2 new_end) {
    auto dy = new_end.y - old_end.y;
    move_cursor(new_cur2(cur.x, cur.y + dy));
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

    auto line_len = buf->lines[c.y].len;
    if (c.x > line_len) {
        c.x = line_len;
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
    buf->init(&mem, lang == LANG_GO);
    */

    print("=== reloading %s", filepath);
    buf->read(fm, true);
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
    buf->init(&mem, lang, true);
    buf->editable_from_main_thread_only = true;

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

    auto &b = world.build;
    if (b.ready()) {
        auto editor_path = get_path_relative_to(filepath, world.current_path);
        For (&b.errors) {
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
        For (&world.searcher.search_results) {
            if (!are_filepaths_equal(it.filepath, filepath)) continue;

            For (it.results) {
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

bool Editor::trigger_escape(cur2 go_here_after) {
    if (go_here_after == NULL_CUR)
        postfix_stack.len = 0;

    bool handled = false;

    if (world.vim.on) {
        // NOTE: was `handled` supposed to just refer to whether it affected
        // autocomplete/parameter? will setting it to true in the vim handler
        // here break anything?

        switch (world.vim.mode) {
        case VI_INSERT: {
            handled = true;

            auto &ref = vim.inserted_indent;
            if (ref.inserted && ref.buf_tree_version == buf->tree_version) {
                buf->remove(ref.start, ref.end);

                // It's expected that ref.start.x == 0 and the line is now empty,
                // so we don't need to subtract 1.
                move_cursor(ref.start);
            } else {
                auto gr = buf->idx_cp_to_gr(cur.y, cur.x);
                if (gr) {
                    auto x = buf->idx_gr_to_cp(cur.y, gr-1);
                    move_cursor(new_cur2(x, cur.y));
                }
            }
            world.vim.mode = VI_NORMAL;
            break;
        }
        case VI_VISUAL:
            handled = true;
            world.vim.mode = VI_NORMAL;
            break;
        }
    }

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

    if (cppos.y >= editor.buf->lines.len)
        cppos.y = editor.buf->lines.len - 1;

    if (pos_in_byte_format) {
        if (cppos.x >= editor.buf->bytecounts[cppos.y])
            cppos.x = editor.buf->bytecounts[cppos.y] - 1;
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
        // TODO: set a limit?
        SCOPED_MEM(&vim.mem);
        vim.command_buffer = new_list(Vim_Command_Input);
        vim.insert_command.op.init();
        vim.insert_command.motion.init();
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
    for (auto curr = node->prev(); !isastnull(curr); curr = curr->prev())
        prev_siblings->append(curr);

    {
        SCOPED_MEM(&nav.mem);

        nav.node = node->dup();
        nav.siblings->len = 0;

        for (int i = prev_siblings->len-1; i >= 0; i--)
            nav.siblings->append(prev_siblings->at(i)->dup());
        for (auto curr = node->next(); !isastnull(curr); curr = curr->next())
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
    if (isastnull(node)) return;

    update_selected_ast_node(node);
    move_cursor(node->start());

    auto end = node->end().y;
    if (end >= view.y + view.h)
        view.y = relu_sub(node->start().y, 3);
}

void Editor::ast_navigate_in() {
    update_ast_navigate([&](auto node) -> Ast_Node* {
        auto child = node->child();
        if (isastnull(child)) return NULL;

        // skip the children that have no siblings
        while (!isastnull(child) && isastnull(child->prev()) && isastnull(child->next())) // no siblings
            child = child->child();

        // no children with siblings, just grab the innermost child
        if (isastnull(child)) {
            child = node->child();
            while (true) {
                auto next = child->child();
                if (isastnull(next)) break;
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
        if (isastnull(parent)) return NULL;
        if (parent->type() == TS_SOURCE_FILE) return NULL;

        // skip the children that have no siblings
        while (!isastnull(parent) && isastnull(parent->prev()) && isastnull(parent->next())) // no siblings
            parent = parent->parent();

        if (isastnull(parent)) {
            parent = node->parent();
            while (true) {
                auto next = parent->parent();
                if (isastnull(next)) break;
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
    buf->cleanup();
    mem.cleanup();
    if (world.vim.on)
        vim.mem.cleanup();
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

void Editor::type_char(uchar uch) {
    buf->insert(cur, &uch, 1);

    auto old_cur = cur;
    auto new_cur = buf->inc_cur(cur);

    move_cursor(new_cur);
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

void Editor::type_char_in_insert_mode(uchar ch) {
    bool already_typed = false;

    /*
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
    */

    if (!already_typed) type_char(ch);

    if (lang != LANG_GO) return;

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
        if (lang != LANG_GO) break;

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

        Ast_Node *rbrace_node = new_object(Ast_Node);
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
                if (!isastnull(prev)) return prev;

                // unable to? get parent, try again
                curr = curr->parent();
                if (isastnull(curr)) return curr;
            }
        };

        auto curr = walk_upwards(rbrace_node);
        int depth = 1;

        i32 lbrace_line = -1;

        fn<void(Ast_Node*)> process_node = [&](Ast_Node* node) {
            if (lbrace_line != -1) return;
            if (node->is_missing()) return;

            SCOPED_FRAME();
            auto children = new_list(Ast_Node*, node->all_child_count());
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

        for (; !isastnull(curr) && lbrace_line == -1; curr = walk_upwards(curr))
            process_node(curr);

        if (lbrace_line == -1) break;

        auto indentation = new_list(uchar);
        For (&buf->lines[lbrace_line]) {
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
        move_cursor(pos);
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
            trigger_autocomplete(false, isident(ch), ch);

    if (!did_parameter_hint) update_parameter_hint();
}

void Editor::vim_save_inserted_indent(cur2 start, cur2 end) {
    if (!world.vim.on) return;

    vim.inserted_indent.inserted = true;
    vim.inserted_indent.buf_tree_version = buf->tree_version;
    vim.inserted_indent.start = start;
    vim.inserted_indent.end = end;
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
            continue;
        }

        auto old_start = start.x;

        // if we're not backspace past the beginning of the line,
        // from here on, we only backspace within the current line

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
    }

    buf->remove(start, cur);
    move_cursor(start);

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

        auto new_contents = GHFmtFinish(false);
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

            move_cursor(c);
        }
    } while (0);

    return true;
}

void Editor::format_on_save(bool fix_imports) {
    if (lang != LANG_GO) return; // any more checks needed?

    auto old_cur = cur;

    Buffer swapbuf; ptr0(&swapbuf);
    bool success = false;

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

    auto new_contents = GHFmtFinish(fix_imports);
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
    if (cur.y >= buf->lines.len) {
        cur.y = buf->lines.len-1;
        cur.x = buf->lines[cur.y].len;
    }

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
        bool use_goimports_autoimport = false;
        if (options.organize_imports_on_save)
            if (!optimize_imports())
                use_goimports_autoimport = true;
        format_on_save(use_goimports_autoimport);
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

int Editor::find_first_nonspace_cp(int y) {
    auto it = iter(new_cur2(0, y));
    while (!it.eol() && !it.eof() && isspace(it.peek()))
        it.next();
    return it.pos.x;
}

Vim_Parse_Status Editor::vim_parse_command(Vim_Command *out) {
    int ptr = 0;
    auto bof = [&]() { return ptr == 0; };
    auto eof = [&]() { return ptr == vim.command_buffer->len; };
    auto peek = [&]() { return vim.command_buffer->at(ptr); };

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
                out->op.append(it);
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
                out->op.append(it);
                skip_motion = true;
                break;
            case CP_KEY_V:
                switch (world.vim.mode) {
                case VI_NORMAL:
                case VI_VISUAL:
                    out->op.append(it);
                    skip_motion = true;
                    break;
                }
                break;
            }
            break;
        }
    } else {
        switch (it.ch) {
        case 'u':
        case 'J':
        case 'i':
        case 'I':
        case 'a':
        case 'A':
        case 'o':
        case 'O':
        case 's':
        case 'S':
        case 'x':
        case 'v':
        case 'V':
        case 'D':
        case 'C':
            skip_motion = true;
            out->op.append(it);
            ptr++;
            break;

        case 'c':
        case 'd':
            if (world.vim.mode == VI_VISUAL)
                skip_motion = true;
            out->op.append(it);
            ptr++;
            break;

        case 'y':
        case '~':
        case '!':
        case '=':
        case '<':
        case '>':
            out->op.append(it);
            ptr++;
            break;

        case 'g': {
            ptr++;

            auto ch = peek_char();
            switch (ch) {
            case 'q':
            case 'w':
            case '?':
            case '@':
            case '~':
            case 'u':
            case 'U':
                ptr++;
                out->op.append(char_input('g'));
                out->op.append(char_input(ch));
                goto done;
            case 'd':
                ptr++;
                out->op.append(char_input('g'));
                out->op.append(char_input(ch));
                skip_motion = true;
                goto done;
            }

            ptr--;
            break;
        }
        case 'z': {
            ptr++;
            auto ch = peek_char();
            switch (ch) {
            case 'f':
                ptr++;
                out->op.append(char_input('z'));
                out->op.append(char_input(ch));
                goto done;
            }
            ptr--;
            break;
        }
        }
    }
done:

    // skip_motion implies o->op.len > 0
    if (skip_motion) return VIM_PARSE_DONE;

    if (eof()) return VIM_PARSE_WAIT;

    out->m_count = 0;
    if (out->o_count && !out->op.len) {
        out->m_count = out->o_count;
        out->o_count = 0;
    } else {
        out->m_count = read_count();
    }

    if (eof()) return VIM_PARSE_WAIT;

    it = peek();
    if (it.is_key) {
        /*
        switch (it.mods) {
        case CP_MOD_CTRL:
            switch (it.key) {
            }
            break;
        }
        */
        return VIM_PARSE_DISCARD;
    }

    switch (it.ch) {
    case 'h':
    case 'j':
    case 'k':
    case 'l':
    case 'H':
    case 'L':
    case 'M':
    case '0':
    case '^':
    case '$':
    case 'w':
    case 'W':
    case 'e':
    case 'E':
    case 'b':
    case 'B':
    case 'G':
    case '}':
    case '{':
        ptr++;
        out->motion.append(it);
        return VIM_PARSE_DONE;

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
            out->motion.append(char_input('g'));
            out->motion.append(char_input(ch));
            return VIM_PARSE_DONE;
        }

        break;
    }
    case 'f':
    case 'F':
    case 't':
    case 'T': {
        ptr++;
        if (eof()) return VIM_PARSE_WAIT;

        char motion = it.ch;

        auto ch = peek_char();
        if (ch) {
            out->motion.append(char_input(motion));
            out->motion.append(char_input(ch));
            return VIM_PARSE_DONE;
        }
        break;
    }
    default: {
        if (out->op.len != 1) break;

        auto inp = out->op[0];
        if (inp.is_key) break;
        if (inp.ch != it.ch) break;

        switch (inp.ch) {
        case 'd':
        case 'y':
        case 'c':
            out->motion.append(inp);
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
    if (cmd->op.len)
        rend.write("%s", render_command_buffer(&cmd->op));
    else
        rend.write("<none>", render_command_buffer(&cmd->op));

    rend.write(" | motion: %d ", cmd->m_count);
    if (cmd->motion.len)
        rend.write("%s", render_command_buffer(&cmd->motion));
    else
        rend.write("<none>", render_command_buffer(&cmd->motion));

    return rend.finish();
}

bool gr_isspace(Grapheme gr) {
    if (gr->len == 1) {
        auto uch = gr->at(0);
        if (uch < 127 && isspace(uch))
            return true;
    }
    return false;
}

bool gr_isident(Grapheme gr) {
    if (gr->len == 1) {
        auto uch = gr->at(0);
        if (isident(uch))
            return true;
    }
    return false;
}

ccstr Editor::get_selection_text(Selection *selection) {
    switch (selection->type) {
    case SEL_CHAR:
    case SEL_LINE: {
        auto range = selection->ranges->at(0);
        return buf->get_text(range.start, range.end);
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

enum Gr_Type {
    GR_SPACE,
    GR_IDENT,
    GR_OTHER,
};

Gr_Type gr_type(Grapheme gr) {
    if (gr_isspace(gr))
        return GR_SPACE;
    if (gr_isident(gr))
        return GR_IDENT;
    return GR_OTHER;
}

Eval_Motion_Result* Editor::vim_eval_motion(Vim_Command *cmd) {
    auto &motion = cmd->motion;
    if (!motion.len) return NULL;

    auto &op = cmd->op;

    auto is_op = [&](ccstr s) {
        int len = strlen(s);
        if (len != op.len) return false;

        for (int i = 0; i < len; i++)
            if (op[i].is_key || op[i].ch != s[i])
                return false;
        return true;
    };

    auto &c = cur;
    auto &lines = buf->lines;
    auto ret = new_object(Eval_Motion_Result);

    int o_count = (cmd->o_count == 0 ? 1 : cmd->o_count);
    int m_count = (cmd->m_count == 0 ? 1 : cmd->m_count);
    int count = o_count * m_count;

    // TODO: does this break anything?
    c = buf->fix_cur(c);
    cp_assert(buf->is_valid(c));

    auto goto_line_first_nonspace = [&](int y) {
        ret->new_dest = new_cur2(find_first_nonspace_cp(y), y);
        ret->type = MOTION_LINE;
    };

    auto inp = motion[0];
    if (inp.is_key) {
    } else {
        switch (inp.ch) {
        case 'g': {
            if (motion.len < 2) break;

            auto inp2 = motion[1];
            if (inp2.is_key) break;

            switch (inp2.ch) {
            case 'g':
                if (!cmd->o_count && !cmd->m_count)
                    goto_line_first_nonspace(0);
                else
                    goto_line_first_nonspace(count-1);
                return ret;
            case 'o':
                ret->new_dest = buf->offset_to_cur(count - 1);
                ret->type = MOTION_CHAR_EXCL;
                return ret;
            }
            break;
        }

        case '{':
        case '}': {
            auto isempty = [&](int y) {
                return !lines[y].len;
            };

            auto isok = [&](int y) {
                return 0 <= y && y < lines.len;
            };

            cur2 pos = c;

            for (int i = 0; i < count; i++) {
                int y = pos.y;
                if (isempty(y))
                    while (isok(y) && isempty(y))
                        y += (inp.ch == '{' ? -1 : 1);

                while (isok(y) && !isempty(y))
                    y += (inp.ch == '{' ? -1 : 1);

                if (y == -1) {
                    pos = new_cur2(0, 0);
                    break;
                }

                if (y >= lines.len) {
                    y = lines.len-1;
                    pos = new_cur2(relu_sub(lines[y].len, 1), y);
                    break;
                }

                pos = new_cur2(0, y);
            }

            ret->new_dest = pos;
            ret->type = MOTION_CHAR_EXCL;
            return ret;
        }

        case 'f':
        case 't': {
            if (motion.len < 2) break;

            auto inp2 = motion[1];
            if (inp2.is_key) break;

            auto it = iter();

            cur2 last;

            for (int i = 0; i < count && !it.eol(); i++) {
                // ignore the first character
                if (!it.eol()) {
                    it.gr_peek();
                    last = it.pos;
                    it.gr_next();
                }

                while (!it.eol()) {
                    auto gr = it.gr_peek();
                    if (gr->len == 1)
                        if (inp2.ch == gr->at(0))
                            break;
                    last = it.pos;
                    it.gr_next();
                }
            }

            if (it.eol()) break;

            ret->new_dest = inp.ch == 't' ? last : it.pos;
            ret->type = MOTION_CHAR_INCL;
            return ret;
        }

        case 'F':
        case 'T': {
            if (motion.len < 2) break;

            auto inp2 = motion[1];
            if (inp2.is_key) break;

            break;
        }

        case 'G':
            if (!cmd->o_count && !cmd->m_count)
                goto_line_first_nonspace(lines.len-1);
            else
                goto_line_first_nonspace(count-1);
            return ret;

        case '0':
            ret->new_dest = new_cur2(0, c.y);
            ret->type = MOTION_CHAR_EXCL;
            return ret;

        case '^': {
            int x = find_first_nonspace_cp(c.y);
            ret->new_dest = new_cur2(x, c.y);
            ret->type = MOTION_CHAR_EXCL;
            return ret;
        }

        case '$': {
            int newy = min(lines.len-1, c.y + count-1);
            ret->new_dest = new_cur2(lines[newy].len, newy);
            ret->type = MOTION_CHAR_INCL;
            return ret;
        }
        case 'c':
        case 'y':
        case 'd': {
            if (!is_op(cp_sprintf("%c", (char)inp.ch))) break;

            int y = min(c.y + count-1, lines.len-1);
            ret->new_dest = new_cur2(0, y);
            ret->type = MOTION_LINE;
            return ret;
        }

        case 'j':
        case 'k': {
            int newy = c.y + count * (inp.ch == 'j' ? 1 : -1);
            if (newy < 0) newy = 0;
            if (newy >= lines.len) newy = lines.len-1;
            if (newy == c.y) break;

            auto vx = buf->idx_cp_to_vcp(c.y, c.x);
            auto newvx = max(vx, vim.hidden_vx);

            auto newx = buf->idx_vcp_to_cp(newy, newvx);
            if (newx >= lines[newy].len && lines[newy].len > 0)
                newx = lines[newy].len-1;

            ret->new_dest = new_cur2(newx, newy);
            ret->type = MOTION_LINE;
            return ret;
        }
        case 'h':
        case 'l': {
            auto grx = buf->idx_cp_to_gr(c.y, c.x);

            if (inp.ch == 'h') {
                if (grx) grx--;
            } else {
                grx++;
            }

            auto x = buf->idx_gr_to_cp(c.y, grx);
            if (x == lines[c.y].len && lines[c.y].len > 0)
                x = lines[c.y].len-1;

            // vim.hidden_vx = buf->idx_cp_to_vcp(c.y, x);
            ret->new_dest = new_cur2(x, c.y);
            ret->type = MOTION_CHAR_EXCL;
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
            if (y >= lines.len)
                y = lines.len ? lines.len-1 : 0;

            goto_line_first_nonspace(y);
            return ret;
        }
        case 'w':
        case 'W': {
            auto it = iter();

            for (int i = 0; i < count && !it.eof(); i++) {
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
            }

            ret->new_dest = it.pos;
            ret->type = MOTION_CHAR_EXCL;
            return ret;
        }
        case 'b':
        case 'B': {
            auto it = iter();

            for (int i = 0; i < count && !it.bof(); i++) {
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
                        gr = it.gr_prev();
                        if (!gr_isspace(gr)) {
                            it.pos = old;
                            break;
                        }
                    }
                }

                if (it.bof()) break;

                // now go to the front of the previous word
                type = gr_type(it.gr_prev());
                while (!it.bof()) {
                    auto old = it.pos;
                    if (is_start(it.gr_prev(), type)) {
                        it.pos = old;
                        break;
                    }
                }
            }

            ret->new_dest = it.pos;
            ret->type = MOTION_CHAR_EXCL;
            return ret;
        }
        case 'e':
        case 'E': {
            auto it = iter();

            for (int i = 0; i < count && !it.eof(); i++) {
                auto is_end = [&](Grapheme next_graph, Gr_Type current_type) {
                    if (inp.ch == 'e')
                        return gr_type(next_graph) != current_type;
                    return gr_isspace(next_graph);
                };

                auto type = gr_type(it.gr_peek());
                it.gr_next();

                if (type == GR_SPACE || is_end(it.gr_peek(), type))
                    while (!it.eof() && gr_isspace(it.gr_peek()))
                        it.gr_next();

                if (it.eof()) break;

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
            }

            ret->new_dest = it.pos;
            ret->type = MOTION_CHAR_INCL;
            return ret;
        }

        }
    }

    return NULL;
}

void Editor::vim_handle_visual_mode_key(Selection_Type type) {
    if (world.vim.mode == VI_VISUAL) {
        if (vim.visual_type == type)
            world.vim.mode = VI_NORMAL;
        else
            vim.visual_type = type;
    } else {
        world.vim.mode = VI_VISUAL;
        vim.visual_start = cur;
        vim.visual_type = type;
    }
}

// mirrors buf->remove_lines, y1-y2 are inclusive
void Editor::vim_delete_lines(int y1, int y2) {
    // TODO: pick up here, this ties into clipboard support
    buf->remove_lines(y1, y2);
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

cur2 Editor::vim_delete_selection(Selection *selection) {
    auto text = get_selection_text(selection);
    world.window->set_clipboard_string(text);

    int len = selection->ranges->len;
    for (int i = len-1; i >= 0; i--) {
        auto &it = selection->ranges->at(i);
        buf->remove(it.start, it.end);
    }

    return selection->ranges->at(0).start;
}

bool Editor::vim_exec_command(Vim_Command *cmd) {
    auto mode = world.vim.mode;
    auto motion_result = vim_eval_motion(cmd);

    auto &c = cur;
    auto &lines = buf->lines;

    int o_count = cmd->o_count == 0 ? 1 : cmd->o_count;

    // Move cursor in normal mode.
    auto move_cursor_normal = [&](cur2 pos) {
        auto len = lines[pos.y].len;
        if (pos.x >= len)
            pos.x = len ? len-1 : 0;

        // man this is getting too "magical" and complicated
        if (motion_result)
            if (motion_result->type == MOTION_CHAR_INCL || motion_result->type == MOTION_CHAR_EXCL)
                vim.hidden_vx = buf->idx_cp_to_vcp(pos.y, pos.x);

        move_cursor(pos);
    };

    auto enter_insert_mode = [&]() {
        vim.insert_start = cur;
        world.vim.mode = VI_INSERT;

        // copy the command over
        vim.insert_command.m_count = cmd->m_count;
        vim.insert_command.o_count = cmd->o_count;
        For (&cmd->op) vim.insert_command.op.append(it);
        For (&cmd->motion) vim.insert_command.motion.append(it);
    };

    auto &op = cmd->op;
    if (!op.len) {
        if (!motion_result) return false;

        auto pos = motion_result->new_dest;
        if (pos.y >= lines.len) {
            pos.y = lines.len-1;
            pos.x = lines[pos.y].len-1;
        }

        int len = lines[pos.y].len;
        if (pos.x >= len && len > 0) pos.x = len-1;

        move_cursor_normal(pos);
        return true;
    }

    auto inp = op[0];
    if (inp.is_key) {
        switch (inp.mods) {
        case CP_MOD_NONE:
            switch (inp.key) {
            case CP_KEY_TAB: {
                for (int i = 0; i < o_count; i++)
                    if (!world.history.go_forward())
                        break;
                return true;
            }
            }
            break;
        case CP_MOD_CTRL:
            switch (inp.key) {
            case CP_KEY_E:
            case CP_KEY_Y: {
                int old = view.y;
                if (inp.key == CP_KEY_E)
                    view.y = min(lines.len-1, view.y + o_count);
                else
                    view.y = relu_sub(view.y, o_count);
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
                for (int i = 0; i < o_count; i++)
                    pos = buf->hist_redo();
                move_cursor_normal(pos);
                return true;
            }
            case CP_KEY_TAB:
            case CP_KEY_I: {
                for (int i = 0; i < o_count; i++)
                    if (!world.history.go_forward())
                        break;
                return true;
            }
            case CP_KEY_O: {
                for (int i = 0; i < o_count; i++)
                    if (!world.history.go_backward())
                        break;
                return true;
            }
            case CP_KEY_V:
                vim_handle_visual_mode_key(SEL_BLOCK);
                return true;
            }
            break;
        }
    } else {
        switch (inp.ch) {
        case 'v':
            vim_handle_visual_mode_key(SEL_CHAR);
            return true;
        case 'V':
            vim_handle_visual_mode_key(SEL_LINE);
            return true;
        case 'i':
            enter_insert_mode();
            return true;
        case 'a': {
            auto gr = buf->idx_cp_to_gr(c.y, c.x);
            auto x = buf->idx_gr_to_cp(c.y, gr+1);
            move_cursor(new_cur2(x, c.y));
            enter_insert_mode();
            return true;
        }
        case 'A': {
            move_cursor(new_cur2(lines[c.y].len, c.y));
            enter_insert_mode();
            return true;
        }
        case 'I': {
            move_cursor(new_cur2(find_first_nonspace_cp(c.y), c.y));
            enter_insert_mode();
            return true;
        }
        case 'o':
        case 'O':
            switch (world.vim.mode) {
            case VI_NORMAL: {
                int y = inp.ch == 'o' ? c.y + 1 : c.y;
                move_cursor(open_newline(y));
                enter_insert_mode();
                return true;
            }
            case VI_VISUAL:
                return true;
            }
            break;

        case 's':
        case 'S':
            switch (world.vim.mode) {
            case VI_NORMAL: {
                cur2 start, end;
                if (inp.ch == 's') {
                    start = c;
                    auto gr = buf->idx_cp_to_gr(c.y, c.x);
                    auto cp = buf->idx_gr_to_cp(c.y, gr+1);
                    end = new_cur2(cp, c.y);
                } else {
                    start = new_cur2(find_first_nonspace_cp(c.y), c.y);
                    end = new_cur2(lines[c.y].len, c.y);
                }

                vim_delete_range(start, end);
                move_cursor(start);
                enter_insert_mode();
                return true;
            }
            case VI_VISUAL: {
                cur2 start;
                if (inp.ch == 's') {
                    auto selection = get_selection();
                    start = vim_delete_selection(selection);
                } else {
                    auto selection = get_selection(SEL_LINE);
                    auto range = selection->ranges->at(0);
                    vim_delete_lines(range.start.y, range.end.y);
                    start = open_newline(range.start.y);
                }
                move_cursor(start);
                enter_insert_mode();
                return true;
            }
            }
            break;

        case 'D':
        case 'C':
            switch (world.vim.mode) {
            case VI_NORMAL: {
                auto start = cur;
                int y = cur.y + o_count - 1;
                auto end = new_cur2(lines[y].len, y);
                vim_delete_range(start, end);
                if (inp.ch == 'D') {
                    move_cursor_normal(start);
                } else {
                    move_cursor(start);
                    enter_insert_mode();
                }
                return true;
            }
            case VI_VISUAL: {
                auto selection = get_selection(SEL_LINE);
                auto range = selection->ranges->at(0);
                vim_delete_lines(range.start.y, range.end.y);

                if (inp.ch == 'D') {
                    world.vim.mode = VI_NORMAL;
                    auto x = find_first_nonspace_cp(range.start.y);
                    move_cursor_normal(new_cur2(x, range.start.y));
                } else {
                    auto newcur = open_newline(range.start.y);
                    move_cursor(newcur);
                    enter_insert_mode();
                }
                return true;
            }
            }
            break;

        case 'u': {
            cur2 pos;
            for (int i = 0; i < o_count; i++)
                pos = buf->hist_undo();
            move_cursor_normal(pos);
            return true;
        }
        case 'J':
            for (int i = 0; i < o_count; i++) {
                int y = c.y;
                if (y == lines.len-1) break;

                auto start = new_cur2(lines[y].len, y);
                auto x = find_first_nonspace_cp(y+1);
                auto end = new_cur2(x, y+1);
                bool add_space = (x < lines[y+1].len && lines[y+1][x] != ')');

                buf->remove(start, end);
                if (add_space) {
                    uchar space = ' ';
                    buf->insert(start, &space, 1);
                }
            }
            return true;
        case 'x': {
            switch (world.vim.mode) {
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
                return true;
            }
            case VI_VISUAL: {
                auto start = vim_delete_selection(get_selection());
                move_cursor_normal(start);
                world.vim.mode = VI_NORMAL;
                return true;
            }
            }
            break;
        }
        case 'c':
        case 'd': {
            switch (mode) {
            case VI_VISUAL: {
                cp_assert(!motion_result);
                auto start = vim_delete_selection(get_selection());
                if (inp.ch == 'c') {
                    move_cursor(start);
                    enter_insert_mode();
                } else {
                    world.vim.mode = VI_NORMAL;
                    move_cursor_normal(start);
                }
                return true;
            }
            case VI_NORMAL: {
                cp_assert(motion_result);

                switch (motion_result->type) {
                case MOTION_CHAR_INCL:
                case MOTION_CHAR_EXCL: {
                    cur2 a = c, b = motion_result->new_dest;
                    ORDER(a, b);

                    if (motion_result->type == MOTION_CHAR_INCL) {
                        auto it = iter(b);
                        it.gr_peek();
                        it.gr_next();
                        b = it.pos;
                    }

                    vim_delete_range(a, b);

                    if (inp.ch == 'c') {
                        move_cursor(a);
                        enter_insert_mode();
                    } else {
                        move_cursor_normal(a);
                    }
                    break;
                }
                case MOTION_LINE: {
                    int y1 = c.y;
                    int y2 = motion_result->new_dest.y;

                    if (y1 > y2) {
                        int tmp = y1;
                        y1 = y2;
                        y2 = tmp;
                    }

                    cur2 start = new_cur2(0, y1);
                    cur2 end = new_cur2(0, y2+1);

                    if (y2 == lines.len-1) {
                        start = buf->dec_cur(start);
                        end = new_cur2(lines[y2].len, y2);
                    }

                    vim_delete_range(start, end);

                    if (inp.ch == 'c') {
                        move_cursor(open_newline(y1));
                        enter_insert_mode();
                    } else {
                        auto to = new_cur2(0, y1);
                        if (y1 >= lines.len) {
                            // TODO: find first non whitespace
                            to = new_cur2(0, lines.len-1);
                        }
                        move_cursor_normal(to);
                    }
                }
                }
                return true;
            }
            }
            break;
        }
        case 'y':
            break;
        case '~':
            break;
        case '!':
            break;
        case '=':
            break;
        case '<':
            break;
        case '>':
            break;
        case 'g': {
            auto it2 = op[1];
            if (!it2.is_key) {
                switch (it2.ch) {
                case 'q':
                case 'w':
                case '?':
                case '@':
                case '~':
                case 'u':
                case 'U':
                    break;
                case 'd': {
                    handle_goto_definition();
                    break;
                }
                }
            }
            break;
        }
        case 'z': {
            auto it2 = op[1];
            if (!it2.is_key) {
                switch (it2.ch) {
                case 'f': {
                    break;
                }
                }
            }
            break;
        }
        }
    }
    return false;
}

cur2 Editor::open_newline(int y) {
    if (y == 0) {
        // if it's the first line, start at 0, 0 and insert
        // indent + \n, or rather, just \n
        uchar newline = '\n';
        buf->insert(new_cur2(0, 0), &newline, 1);
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

bool Editor::vim_handle_input(Vim_Command_Input *input) {
    switch (world.vim.mode) {
    case VI_VISUAL:
    case VI_NORMAL: {
        vim.command_buffer->append(input);

        print("%s", render_command_buffer(vim.command_buffer));

        Vim_Command cmd; ptr0(&cmd);
        cmd.op.init();
        cmd.motion.init();

        auto status = vim_parse_command(&cmd);
        if (status == VIM_PARSE_WAIT) break;

        vim.command_buffer->len = 0;
        if (status == VIM_PARSE_DISCARD) {
            print("command discarded");
            break;
        }

        vim_exec_command(&cmd);
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
