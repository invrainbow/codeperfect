#include "ui.hpp"
#include "common.hpp"
#include "world.hpp"

UI ui;

vec3f rgb_hex(ccstr s) {
    if (s[0] == '#') s++;

    char r[] = { s[0], s[1], '\0' };
    char g[] = { s[2], s[3], '\0' };
    char b[] = { s[4], s[5], '\0' };

    return {
        strtol(r, NULL, 16) / 255.0f,
        strtol(g, NULL, 16) / 255.0f,
        strtol(b, NULL, 16) / 255.0f,
    };
}

vec4f rgba(vec3f color, float alpha) {
    vec4f ret;
    ret.rgb = color;
    ret.a = alpha;
    return ret;
}

const vec3f COLOR_WHITE = rgb_hex("#ffffff");
const vec3f COLOR_DARK_RED = rgb_hex("#880000");
const vec3f COLOR_DARK_YELLOW = rgb_hex("#6b6d0a");
const vec3f COLOR_BLACK = rgb_hex("#000000");
const vec3f COLOR_LIGHT_GREY = rgb_hex("#eeeeee");
const vec3f COLOR_DARK_GREY = rgb_hex("#333333");
const vec3f COLOR_MEDIUM_DARK_GREY = rgb_hex("#585858");
const vec3f COLOR_MEDIUM_GREY = rgb_hex("#888888");
const vec3f COLOR_LIME = rgb_hex("#22ff22");
const vec3f COLOR_THEME_1 = rgb_hex("fd3f5c");
const vec3f COLOR_THEME_2 = rgb_hex("fbd19b");
const vec3f COLOR_THEME_3 = rgb_hex("edb891");
const vec3f COLOR_THEME_4 = rgb_hex("eca895");
const vec3f COLOR_THEME_5 = rgb_hex("e8918c");

void UI::init() {
    ptr0(this);
    font = &world.font;
}

void UI::flush_verts() {
    if (verts.len == 0) return;

    glBufferData(GL_ARRAY_BUFFER, sizeof(Vert) * verts.len, verts.items, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, verts.len);
    verts.len = 0;
}

void UI::draw_quad(boxf b, boxf uv, vec4f color, bool solid) {
    if (verts.len + 6 >= verts.cap)
        flush_verts();

    b.x *= world.display_scale.x;
    b.w *= world.display_scale.x;
    b.y *= world.display_scale.y;
    b.h *= world.display_scale.y;

    verts.append({ b.x, b.y + b.h, uv.x, uv.y + uv.h, color, solid });
    verts.append({ b.x, b.y, uv.x, uv.y, color, solid });
    verts.append({ b.x + b.w, b.y, uv.x + uv.w, uv.y, color, solid });
    verts.append({ b.x, b.y + b.h, uv.x, uv.y + uv.h, color, solid });
    verts.append({ b.x + b.w, b.y, uv.x + uv.w, uv.y, color, solid });
    verts.append({ b.x + b.w, b.y + b.h, uv.x + uv.w, uv.y + uv.h, color, solid });
}

void UI::draw_rect(boxf b, vec4f color) {
    draw_quad(b, { 0, 0, 1, 1 }, color, true);
}

void UI::draw_bordered_rect_outer(boxf b, vec4f color, vec4f border_color, int border_width) {
    auto b2 = b;
    b2.x -= border_width;
    b2.y -= border_width;
    b2.h += border_width * 2;
    b2.w += border_width * 2;

    draw_rect(b2, border_color);
    draw_rect(b, color);
}

// advances pos forward
void UI::draw_char(vec2f* pos, char ch, vec4f color) {
    stbtt_aligned_quad q;
    stbtt_GetPackedQuad(font->char_info, font->tex_size, font->tex_size, ch - ' ', &pos->x, &pos->y, &q, 0);
    if (q.x1 > q.x0) {
        boxf box = { q.x0, q.y0, q.x1 - q.x0, q.y1 - q.y0 };
        boxf uv = { q.s0, q.t0, q.s1 - q.s0, q.t1 - q.t0 };
        draw_quad(box, uv, color, false);
    }
}

void UI::draw_string(vec2f pos, ccstr s, vec4f color) {
    pos.y += font->offset_y;
    for (u32 i = 0, len = strlen(s); i < len; i++)
        draw_char(&pos, s[i], color);
}

float UI::get_text_width(ccstr s) {
    float x = 0, y = 0;
    stbtt_aligned_quad q;
    for (u32 i = 0, len = strlen(s); i < len; i++)
        stbtt_GetPackedQuad(font->char_info, font->tex_size, font->tex_size, s[i] - ' ', &x, &y, &q, 0);
    return x;
}

boxf UI::get_sidebar_area() {
    boxf sidebar_area;
    ptr0(&sidebar_area);

    if (world.sidebar.view != SIDEBAR_CLOSED) {
        sidebar_area.h = world.window_size.y;
        sidebar_area.w = world.sidebar.width;
    }

    return sidebar_area;
}

boxf UI::get_panes_area() {
    boxf panes_area;
    panes_area.pos = { 0, 0 };
    panes_area.size = world.window_size;

    boxf sidebar_area = get_sidebar_area();
    panes_area.x += sidebar_area.w;
    panes_area.w -= sidebar_area.w;

    return panes_area;
}

u32 advance_subtree_in_file_explorer(u32 i) {
    auto &it = world.file_tree[i++];
    if (it.num_children != -1)
        for (u32 j = 0; j < it.num_children; j++)
            i = advance_subtree_in_file_explorer(i);
    return i;
}

void UI::draw_everything(GLuint vao, GLuint vbo, GLuint program) {
    {
        // prepare opengl for drawing shit
        glViewport(0, 0, world.display_size.x, world.display_size.y);
        glUseProgram(program);
        glActiveTexture(GL_TEXTURE0); // activate texture
        glBindTexture(GL_TEXTURE_2D, font->texid);
        glBindVertexArray(vao); // bind my vertex array & buffers
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        verts.init(LIST_FIXED, 6 * 128, alloc_array(Vert, 6 * 128));
    }

    auto& wksp = world.wksp;

    boxf panes_area = get_panes_area();
    boxf sidebar_area = get_sidebar_area();

    boxf error_list_area;
    error_list_area.x = 0;
    error_list_area.y = world.window_size.y;
    error_list_area.w = world.window_size.x;
    error_list_area.h = 0;

    if (world.error_list.show) {
        panes_area.h -= world.error_list.height;
        error_list_area.y -= world.error_list.height;
        error_list_area.h += world.error_list.height;
    }

    boxf pane_area;
    pane_area.pos = panes_area.pos;

    if (world.sidebar.view != SIDEBAR_CLOSED) {
        draw_rect(sidebar_area, rgba(COLOR_BLACK));

        switch (world.sidebar.view) {
            case SIDEBAR_FILE_EXPLORER:
                {
                    vec2f pos = sidebar_area.pos;
                    pos.y -= world.file_explorer.scroll_offset;

                    u32 depth = 0;

                    for (u32 i = 0; i < world.file_tree.len;) {
                        auto it = &world.file_tree[i];

                        if (pos.y >= sidebar_area.y) {
                            SCOPED_FRAME();
                            auto s = (cstr)our_sprintf(
                                "%*s%s%s",
                                it->depth * 2,
                                "",
                                it->name,
                                it->num_children == -1 ? "" : "/"
                            );
                            auto available_width = (int)(sidebar_area.w / font->width);
                            if (available_width < strlen(s))
                                s[available_width] = '\0';

                            boxf line_area;
                            line_area.pos = pos;
                            line_area.w = sidebar_area.w;
                            line_area.h = font->height;

                            if (line_area.contains(world.ui.mouse_pos))
                                draw_rect(line_area, rgba(COLOR_DARK_GREY));
                            draw_string(pos, s, rgba(COLOR_WHITE));
                        }

                        pos.y += font->height;
                        if (pos.y > sidebar_area.h) break;

                        if (it->num_children != -1 && !it->state.open)
                            i = advance_subtree_in_file_explorer(i);
                        else
                            i++;
                    }
                }
                break;
            case SIDEBAR_SEARCH_RESULTS:
                {
                    vec2f pos;
                    pos.x = 0;
                    pos.y = sidebar_area.y - world.search_results.scroll_offset;
                    pos.y += font->offset_y;

                    For (world.search_results.results) {
                        if (pos.y >= sidebar_area.y) {
                            SCOPED_FRAME();

                            pos.x = 0;

                            boxf line_area;
                            line_area.pos = pos;
                            line_area.y -= font->offset_y;
                            line_area.w = sidebar_area.w;
                            line_area.h = font->height;

                            if (line_area.contains(world.ui.mouse_pos))
                                draw_rect(line_area, rgba(COLOR_DARK_GREY));

                            auto str = our_sprintf("%s:%d:%d ", it->filename, it->row, it->match_col);
                            auto len = strlen(str);

                            for (u32 i = 0; i < len && pos.x < sidebar_area.w; i++)
                                draw_char(&pos, str[i], rgba(COLOR_WHITE));

                            len = strlen(it->preview);
                            for (u32 i = 0; i < len && pos.x < sidebar_area.w; i++) {
                                auto color = rgba(COLOR_WHITE);
                                if (it->match_col_in_preview <= i && i < it->match_col_in_preview + it->match_len)
                                    color = rgba(COLOR_LIME);
                                draw_char(&pos, it->preview[i], color);
                            }
                        }

                        pos.y += font->height;
                        if (pos.y > sidebar_area.h) break;
                    }
                }
                break;
        }
    }

    // draw panes
    u32 current_pane = 0;
    for (auto && pane : wksp.panes) {
        auto is_pane_selected = (current_pane == wksp.current_pane);

        pane_area.w = pane.width;
        pane_area.h = panes_area.h;

        boxf tabs_area, editor_area;
        get_tabs_and_editor_area(&pane_area, &tabs_area, &editor_area);

        draw_rect(tabs_area, rgba(is_pane_selected ? COLOR_MEDIUM_GREY : COLOR_DARK_GREY));
        draw_rect(editor_area, rgba(COLOR_BLACK));

        vec2 tab_padding = { 15, 5 };

        boxf tab;
        tab.pos = tabs_area.pos + new_vec2(5, tabs_area.h - tab_padding.y * 2 - font->height);

        // draw tabs
        u32 tab_id = 0;
        for (auto&& editor : pane.editors) {
            bool is_selected = (tab_id == pane.current_editor);

            ccstr label;
            if (editor.is_untitled) {
                label = "<untitled>";
            } else {
                auto filepath = editor.filepath;
                label = filepath;
                auto root_len = strlen(wksp.path);
                if (wksp.path[root_len - 1] != '/')
                    root_len++;
                label += root_len;
            }

            auto text_width = get_text_width(label);

            tab.w = text_width + tab_padding.x * 2;
            tab.h = font->height + tab_padding.y * 2;

            draw_rect(tab, rgba(is_selected ? COLOR_BLACK : COLOR_MEDIUM_DARK_GREY));
            draw_string(tab.pos + tab_padding, label, rgba(is_selected ? COLOR_WHITE : COLOR_LIGHT_GREY));

            tab.pos.x += tab.w + 5;
            tab_id++;
        }

        // draw editor
        if (pane.editors.len > 0) {
            vec2f cur_pos = editor_area.pos + new_vec2f(EDITOR_MARGIN_X, EDITOR_MARGIN_Y);
            cur_pos.y += font->offset_y;

            auto editor = pane.get_current_editor();

            if (world.nvim_data.waiting_focus_window == editor->id) {
                // TODO
            } else {
                auto &buf = editor->buf;
                auto &view = editor->view;

                vec2f actual_cursor_position = { -1, -1 };

                auto draw_background = [&](vec3f color) {
                    boxf b = { cur_pos.x, cur_pos.y - font->offset_y, (float)font->width, (float)font->height };
                    draw_rect(b, rgba(color));
                };

                auto draw_cursor = [&]() {
                    actual_cursor_position = cur_pos;    // save position where cursor is drawn for later use
                    draw_background(COLOR_LIME);
                };

                List<Client_Breakpoint> breakpoints_for_this_editor;

                {
                    u32 len = 0;
                    For (world.dbg.breakpoints)
                        if (streq(it.file, editor->filepath))
                            len++;

                    alloc_list(&breakpoints_for_this_editor, len);
                    For (world.dbg.breakpoints) {
                        if (streq(it.file, editor->filepath)) {
                            auto p = breakpoints_for_this_editor.append();
                            memcpy(p, &it, sizeof(it));
                        }
                    }
                }

                if (buf.lines.len == 0) {
                    draw_cursor();
                }

                auto relative_y = 0;
                for (u32 y = view.y; y < view.y + view.h; y++, relative_y++) {
                    if (y >= buf.lines.len) break;

                    auto line = &buf.lines[y];
                    auto highlights = &editor->highlights.rows->at(relative_y);
                    auto highlight_idx = 0;

                    auto is_stopped_at_this_line = [&]() -> bool {
                        if (world.dbg.state_flag == DBGSTATE_PAUSED)
                            if (streq(world.dbg.state.file_stopped_at, editor->filepath))
                                if (world.dbg.state.line_stopped_at == y + 1)
                                    return true;
                        return false;
                    };

                    boxf line_box = {
                        cur_pos.x,
                        cur_pos.y - font->offset_y,
                        (float)editor_area.w,
                        (float)font->height - 1,
                    };

                    if (is_stopped_at_this_line()) {
                        draw_rect(line_box, rgba(COLOR_DARK_YELLOW));
                    } else {
                        For (breakpoints_for_this_editor) {
                            if (it.line == y + 1) {
                                bool inactive = (it.pending || world.dbg.state_flag == DBGSTATE_INACTIVE);
                                draw_rect(line_box, rgba(COLOR_DARK_RED, inactive ? 0.5 : 1.0));
                                break;
                            }
                        }
                    }

                    auto is_cursor_match = [&](cur2 cur, i32 x, i32 y) -> bool {
                        if (cur.y != y) return false;
                    };

                    u32 actual_x = 0;
                    for (u32 x = view.x; x < view.x + view.w; x++) {
                        if (x >= line->len) break;

                        /*
                        while (highlight_idx < highlights->len) {
                            auto hl = highlights->at(highlight_idx);
                            if (actual_x > hl.col + hl.len) {
                                highlight_idx++;
                            } else {
                                break;
                            }
                        }
                        */

                        auto text_color = COLOR_WHITE;
                        if (editor->cur == new_cur2(x, y)) {
                            draw_cursor();
                            text_color = COLOR_BLACK;
                        } else {
                            switch (highlights->at(actual_x).type) {
                                case HL_COMMENT:     text_color = COLOR_THEME_1; break;
                                case HL_STATEMENT: text_color = COLOR_THEME_2; break;
                                case HL_TYPE:            text_color = COLOR_THEME_3; break;
                                case HL_CONSTANT:    text_color = COLOR_THEME_4; break;
                                case HL_VISUAL:
                                    draw_background(COLOR_MEDIUM_GREY);
                                    text_color = COLOR_BLACK;
                                    break;
                            }
                        }

                        uchar uch = line->at(x);
                        if (uch == '\t')
                            cur_pos.x += font->width * TAB_SIZE;
                        else
                            draw_char(&cur_pos, (char)uch, rgba(text_color));

                        actual_x += (uch == '\t' ? TAB_SIZE : 1);
                    }

                    if (editor->cur == new_cur2(line->len, y))
                        draw_cursor();

                    cur_pos.x = editor_area.x + EDITOR_MARGIN_X;
                    cur_pos.y += font->height;
                }

                do {
                    // we can't draw the autocomplete if we don't know where to draw it
                    // when would this ever happen though?
                    if (actual_cursor_position.x == -1) break;

                    auto &ac = editor->autocomplete;

                    if (ac.ac.results == NULL) break;

                    s32 max_len = 0;
                    s32 num_items = min(ac.filtered_results->len, AUTOCOMPLETE_WINDOW_ITEMS);

                    auto format_name = [&](int i, ccstr name) -> ccstr {
                        return our_sprintf("%d) %s", i + 1, name);
                    };

                    {
                        s32 idx = 0;
                        For(*ac.filtered_results) {
                            s32 len;
                            {
                                SCOPED_FRAME();
                                len = strlen(format_name(idx, ac.ac.results->at(it).name));
                            }
                            if (len > max_len)
                                max_len = len;
                            idx++;
                        }
                    }

                    if (num_items > 0) {
                        boxf menu;
                        menu.w = font->width * max_len;
                        menu.h = font->height * num_items;
                        menu.x = min(actual_cursor_position.x - strlen(ac.prefix) * font->width, world.window_size.x - menu.w);
                        menu.y = min(actual_cursor_position.y - font->offset_y + font->height, world.window_size.y - menu.h);

                        draw_bordered_rect_outer(menu, rgba(COLOR_BLACK), rgba(COLOR_LIGHT_GREY), 1);

                        auto menu_pos = menu.pos;

                        for (int i = ac.view; i < ac.view + num_items; i++) {
                            auto idx = ac.filtered_results->at(i);

                            vec3f color = new_vec3f(1.0, 1.0, 1.0);

                            if (i == ac.selection) {
                                boxf b;
                                b.pos = menu_pos;
                                b.h = font->height;
                                b.w = menu.w;
                                draw_rect(b, rgba(COLOR_WHITE));
                                color = new_vec3f(0.0, 0.0, 0.0);
                            }

                            {
                                SCOPED_FRAME();
                                auto str = format_name(i, ac.ac.results->at(idx).name);
                                draw_string(menu_pos, str, rgba(color));
                            }

                            menu_pos.y += font->height;
                        }
                    }
                } while (0);

                do {
                    if (actual_cursor_position.x == -1) break;

                    auto &hint = editor->parameter_hint;
                    if (hint.params == NULL) break;

                    u32 str_len = 0;
                    if (hint.params->len == 0)
                        str_len = 2;
                    else
                        str_len = (2 * (hint.params->len - 1)) + 2;
                    For(*hint.params) str_len += strlen(it);

                    boxf bg;
                    bg.w = font->width * str_len;
                    bg.h = font->height;
                    bg.x = min(actual_cursor_position.x, world.window_size.x - bg.w);
                    bg.y = min(actual_cursor_position.y - font->offset_y - font->height, world.window_size.y - bg.h);

                    draw_bordered_rect_outer(bg, rgba(COLOR_DARK_GREY), rgba(COLOR_WHITE), 1);

                    Text_Renderer rend;
                    rend.init();
                    rend.write("(");
                    for (int i = 0; i < hint.params->len; i++) {
                        rend.write("%s", hint.params->at(i));
                        if (i < hint.params->len - 1)
                            rend.write(", ");
                    }
                    rend.write(")");
                    draw_string(bg.pos, rend.finish(), rgba(COLOR_WHITE));
                } while (0);

                if (world.use_nvim) {
                    ccstr mode_str = NULL;

                    switch (editor->nvim_data.vimode) {
                        case VIMODE_NORMAL: mode_str = "NORMAL"; break;
                        case VIMODE_VISUAL: mode_str = "VISUAL"; break;
                        case VIMODE_INSERT: mode_str = "INSERT"; break;
                        case VIMODE_REPLACE: mode_str = "REPLACE"; break;
                    }

                    if (mode_str != NULL) {
                        boxf b;
                        b.x = editor_area.x;
                        b.y = editor_area.y + editor_area.h;
                        b.x += 10;
                        b.y -= 10;
                        b.w = font->width * strlen(mode_str);
                        b.h = font->height;
                        b.y -= b.h;

                        draw_rect(b, rgba(COLOR_WHITE));
                        draw_string(b.pos, mode_str, rgba(COLOR_BLACK));
                    }
                }
            }
        }

        current_pane++;
        pane_area.x += pane_area.w;
    }

    // draw pane resizers

    {
        auto num_areas = world.wksp.panes.len - 1;
        auto resize_areas = alloc_array(boxf, num_areas);
        ui.get_pane_resize_areas(resize_areas, num_areas);

        for (u32 i = 0; i < num_areas; i++) {
            auto& b = resize_areas[i];
            if (b.contains(world.ui.mouse_pos))
                draw_rect(b, rgba(COLOR_WHITE));
            else
                draw_rect(b, rgba(COLOR_WHITE));
        }
    }

    auto get_debugger_state_string = [&]() -> ccstr {
        switch (world.dbg.state_flag) {
            case DBGSTATE_PAUSED: return "PAUSED";
            case DBGSTATE_STARTING: return "STARTING";
            case DBGSTATE_RUNNING: return "RUNNING";
        }
        return NULL;
    };

    auto state_str = get_debugger_state_string();
    if (state_str != NULL) {
        boxf b;
        b.w = (font->width * strlen(state_str));
        b.h = font->height;
        b.x = world.display_size.x - 10 - b.w;
        b.y = world.display_size.y - 10 - b.h;

        draw_rect(b, rgba(COLOR_DARK_RED));
        draw_string(b.pos, state_str, rgba(COLOR_WHITE));
    }

    if (world.error_list.show) {
        draw_rect(error_list_area, rgba(COLOR_DARK_GREY));

        vec2f pos = error_list_area.pos;

        For (world.build_errors.errors) {
            SCOPED_FRAME();
            auto s = our_sprintf("%s:%d:%d: %s", it->file, it->row, it->col, it->message);
            draw_string(pos, s, rgba(COLOR_WHITE));
        }
    }

    // TODO: draw 'search anywhere' window
    flush_verts();
}

void UI::get_tabs_and_editor_area(boxf* pane_area, boxf* ptabs_area, boxf* peditor_area) {
    boxf tabs_area, editor_area;

    tabs_area.pos = pane_area->pos;
    tabs_area.w = pane_area->w;
    tabs_area.h = 30; // ???

    editor_area.pos = pane_area->pos;
    editor_area.y += tabs_area.h;
    editor_area.w = pane_area->w;
    editor_area.h = pane_area->h - tabs_area.h;

    if (ptabs_area != NULL)
        memcpy(ptabs_area, &tabs_area, sizeof(boxf));
    if (peditor_area != NULL)
        memcpy(peditor_area, &editor_area, sizeof(boxf));
}

void UI::recalculate_view_sizes() {
    resize_panes_proportionally();

    boxf panes_area = get_panes_area();

    boxf pane_area;
    pane_area.x = 0;
    pane_area.y = 0;
    pane_area.w = panes_area.w / world.wksp.panes.len;
    pane_area.h = panes_area.h;

    for (auto&& pane : world.wksp.panes) {
        pane_area.w = pane.width;

        boxf editor_area;
        get_tabs_and_editor_area(&pane_area, NULL, &editor_area);

        for (auto&& editor : pane.editors) {
            vec2 new_size;
            new_size.x = (i32)((editor_area.w - EDITOR_MARGIN_X) / world.font.width);
            new_size.y = (i32)((editor_area.h - EDITOR_MARGIN_Y) / world.font.height);

            auto &hls = editor.highlights;

            auto newmem = hls.mem_toggle ? &hls.mem_a : &hls.mem_b;
            auto oldmem = !hls.mem_toggle ? &hls.mem_a : &hls.mem_b;
            hls.mem_toggle = !hls.mem_toggle;

            SCOPED_MEM(newmem);
            newmem->reset();

            auto buf = alloc_array(Hl_Token, new_size.x * new_size.y);
            auto rows = alloc_list<List<Hl_Token>>(new_size.y);

            {
                SCOPED_LOCK(&hls.lock);
                for (u32 row = 0; row < new_size.y; row++) {
                    auto hlrow = rows->append();
                    hlrow->init(LIST_FIXED, new_size.x, &buf[row * new_size.x]);

                    List<Hl_Token>* old_row = NULL;
                    if (hls.rows != NULL && row < hls.rows->len)
                        old_row = &hls.rows->at(row);

                    for (u32 col = 0; col < new_size.x; col++) {
                        auto p = hlrow->append();
                        if (old_row != NULL && col < old_row->len)
                            p->type = old_row->at(col).type;
                        else
                            p->type = HL_NONE;
                    }
                }
                hls.buf = buf;
                hls.rows = rows;
            }

            editor.view.size = new_size;
            if (!editor.nvim_data.is_resizing)
                if (!world.nvim.resize_editor(&editor))
                    editor.nvim_data.need_initial_resize = true;
        }

        pane_area.x += pane_area.w;
    }
}

void UI::get_pane_resize_areas(boxf *out, s32 count) {
    auto panes_area = get_panes_area();
    float offset = panes_area.x;

    for (u32 i = 0; i < world.wksp.panes.len - 1 && i < count; i++) {
        offset += world.wksp.panes[i].width;

        auto box = &out[i];
        box->w = 2;
        box->h = panes_area.h;
        box->x = offset - 1;
        box->y = 0;
    }
}

i32 UI::get_current_resize_area(boxf* out) {
    SCOPED_FRAME();

    auto num_areas = world.wksp.panes.len - 1;
    auto resize_areas = alloc_array(boxf, num_areas);

    get_pane_resize_areas(resize_areas, num_areas);

    for (u32 i = 0; i < num_areas; i++)
        if (resize_areas[i].contains(world.ui.mouse_pos))
            return memcpy(out, resize_areas+i, sizeof(boxf)), i;
    return -1;
}

void UI::resize_panes_proportionally() {
    resize_panes_proportionally(get_panes_area().w);
}

void UI::resize_panes_proportionally(float new_width) {
    auto& panes = world.wksp.panes;
    float total = 0;
    auto widths = alloc_array(float, panes.cap);

    {
        u32 i = 0;
        For (panes) {
            total += it.width;
            widths[i++] = it.width;
        }
    }

    for (u32 i = 0; i < panes.len; i++)
        panes[i].width = widths[i] / total * new_width;
}

