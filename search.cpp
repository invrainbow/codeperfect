#include "search.hpp"
#include "world.hpp"
#include "defer.hpp"

#define PREVIEW_LEN 40

void Searcher::init() {
    ptr0(this);

    mem.init();
    final_mem.init();

    mem_active = true;
    final_mem_active = true;
}

// two cases to cleanup:
// 1) user clears (whether the thread is running or not)
//     - clear everything
// 2) thread completes
//     - clear everything but final

void Searcher::cleanup_search() {
    if (thread) {
        kill_thread(thread);
        close_thread_handle(thread);
        thread = NULL;
    }

    sess.cleanup();
}

void Searcher::cleanup() {
    For (&search_results) {
        if (!it.results) continue;
        For (it.results) {
            it.mark_start->cleanup();
            it.mark_end->cleanup();

            world.mark_fridge.free(it.mark_start);
            world.mark_fridge.free(it.mark_end);
        }
    }
    search_results.len = 0;

    cleanup_search();

    if (mem_active) {
        mem.cleanup();
        mem_active = false;
    }

    if (final_mem_active) {
        final_mem.cleanup();
        final_mem_active = false;
    }

    state = SEARCH_NOTHING_HAPPENING;
}

void Searcher::search_worker() {
    int total_results = 0;

    auto matches = alloc_list<Search_Match>();

    while (file_queue.len > 0 && total_results < 1000) {
        auto filepath = *file_queue.last();
        file_queue.len--;

        auto fm = map_file_into_memory(filepath);
        if (!fm) continue;
        defer { fm->cleanup(); };

        auto buf = (char*)fm->data;
        i64 buflen = fm->len;

        if (is_binary(buf, buflen)) continue;

        matches->len = 0;
        sess.search(buf, buflen, matches, 1000);
        if (!matches->len) continue;

        int curr_match = 0;
        cur2 pos; ptr0(&pos);

        ccstr final_filepath = NULL;
        {
            SCOPED_MEM(&final_mem);
            final_filepath = cp_strdup(filepath);
        }

        auto editor = find_editor_by_filepath(final_filepath);

        Searcher_Result_Match sr; ptr0(&sr);
        auto results = alloc_list<Searcher_Result_Match>(matches->len);

        // convert temp matches into search results
        for (int i = 0; i < buflen; i++) {
            auto it = buf[i];

            auto &nextmatch = matches->at(curr_match);
            if (i == nextmatch.start) {
                if (total_results++ > 1000) break;

                sr.match_start = pos;
                sr.match_off = i;

                if (nextmatch.group_starts) {
                    SCOPED_MEM(&final_mem);

                    sr.groups = alloc_list<ccstr>(nextmatch.group_starts->len);
                    Fori (nextmatch.group_starts) {
                        auto start = it;
                        auto end = nextmatch.group_ends->at(i);

                        auto group = cp_strncpy(&buf[start], end-start);
                        sr.groups->append(group);
                    }
                }
            }

            if (i == nextmatch.end) {
                SCOPED_MEM(&final_mem);

                sr.match_len = i - sr.match_off;
                sr.match_end = pos;
                sr.match = cp_strncpy(&buf[sr.match_off], sr.match_len);

                sr.preview_start = sr.match_start;
                sr.preview_end = sr.match_end;
                sr.preview_len = sr.match_len;

                sr.match_offset_in_preview = 0;

                auto prevoff = sr.match_off;

                int to_left = min((PREVIEW_LEN - sr.preview_len) / 2, min(sr.preview_start.x, 10));

                prevoff -= to_left;
                sr.preview_start.x -= to_left;
                sr.match_offset_in_preview += to_left;
                sr.preview_len += to_left;

                int to_right = 0;
                for (int k = i; sr.preview_len < PREVIEW_LEN && buf[k] != '\n' && k < buflen; k++)
                    to_right++;

                sr.preview_len += to_right;
                sr.preview_end.x += to_right;
                sr.preview = cp_strncpy(&buf[prevoff], sr.preview_len);

                sr.mark_start = world.mark_fridge.alloc();
                sr.mark_end = world.mark_fridge.alloc();

                if (editor) {
                    // what do we do here?
                    editor->buf->mark_tree.insert_mark(MARK_SEARCH_RESULT, sr.match_start, sr.mark_start);
                    editor->buf->mark_tree.insert_mark(MARK_SEARCH_RESULT, sr.match_end, sr.mark_end);
                }

                results->append(&sr);

                if (++curr_match >= matches->len) break;
            }

            if (it == '\n') {
                pos.y++;
                pos.x = 0;
            } else {
                pos.x++;
            }
        }

        Searcher_Result_File sf;
        sf.filepath = final_filepath;
        sf.results = results;
        search_results.append(&sf);
    }

    state = SEARCH_SEARCH_DONE;
    close_thread_handle(thread);
    thread = NULL;

    cleanup_search();
}

bool is_binary(ccstr buf, s32 len) {
    s32 suspicious_bytes = 0;
    s32 total_bytes = len > 512 ? 512 : len;

    auto ubuf = (u8*)buf;

    // empty
    if (!len) return false;

    // utf8 bom
    if (len >= 3 && ubuf[0] == 0xEF && ubuf[1] == 0xBB && ubuf[2] == 0xBF) return false;

    // pdf
    if (len >= 5 && !strncmp(buf, "%PDF-", 5)) return true;

    for (u32 i = 0; i < total_bytes; i++) {
        if (buf[i] == '\0') return true;

        if ((ubuf[i] < 7 || ubuf[i] > 14) && (ubuf[i] < 32 || ubuf[i] > 127)) {
            /* UTF-8 detection */
            if (ubuf[i] > 193 && ubuf[i] < 224 && i + 1 < total_bytes) {
                i++;
                if (ubuf[i] > 127 && ubuf[i] < 192)
                    continue;
            } else if (ubuf[i] > 223 && ubuf[i] < 240 && i + 2 < total_bytes) {
                i++;
                if (ubuf[i] > 127 && ubuf[i] < 192 && ubuf[i + 1] > 127 && ubuf[i + 1] < 192) {
                    i++;
                    continue;
                }
            }
            suspicious_bytes++;
            /* Disk IO is so slow that it's worthwhile to do this calculation after every suspicious byte. */
            /* This is true even on a 1.6Ghz Atom with an Intel 320 SSD. */
            /* Read at least 32 bytes before making a decision */
            if (i >= 32 && (suspicious_bytes * 100) / total_bytes > 10)
                return true;
        }
    }
    return ((suspicious_bytes * 100) / total_bytes > 10);
}

bool Searcher::start_search(ccstr _query, Searcher_Opts *_opts) {
    SCOPED_MEM(&mem);

    bool ok = false;
    state = SEARCH_SEARCH_IN_PROGRESS;
    defer { if (!ok) state = SEARCH_NOTHING_HAPPENING; };

    opts = *_opts;

    ptr0(&sess);
    sess.query = cp_strdup(_query);
    sess.qlen = strlen(sess.query);
    sess.case_sensitive = opts.case_sensitive;
    sess.literal = opts.literal;
    // sess.match_limit = 1000;
    if (!sess.start()) return false;

    if (world.file_tree_busy) {
        tell_user_error("The file list is currently being generated.");
        return false;
    }

    file_queue.init();

    {
        SCOPED_MEM(&final_mem);
        search_results.init();
    }

    if (opts.include) include_parts = make_path(opts.include)->parts;
    if (opts.exclude) exclude_parts = make_path(opts.exclude)->parts;

    // generate file queue
    {
        List<FT_Node*> stack;
        List<ccstr> stackpaths;

        stack.init();
        stackpaths.init();

        stack.append(world.file_tree);
        stackpaths.append(world.current_path);

        while (stack.len > 0) {
            auto node = *stack.last();
            auto path = *stackpaths.last();

            stack.len--;
            stackpaths.len--;

            auto fullpath = path_join(path, node->name);
            if (node->is_directory) {
                for (auto child = node->children; child; child = child->next) {
                    stack.append(child);
                    stackpaths.append(fullpath);
                }
            } else {
                // if (!include_parts || pattern_matches(include_parts, fullpath))
                //     if (!exclude_parts || !pattern_matches(exclude_parts, fullpath))
                file_queue.append(fullpath);
            }
        }
    }

    auto fun = [](void *param) {
        auto s = (Searcher*)param;
        SCOPED_MEM(&s->mem);
        s->search_worker();
    };

    thread = create_thread(fun, this);
    if (thread == NULL) return false;

    ok = true;
    return true;
}

bool Searcher::start_replace(ccstr _replace_with) {
    SCOPED_MEM(&mem);
    replace_with = cp_strdup(_replace_with);

    auto fun = [](void *param) {
        auto s = (Searcher*)param;
        SCOPED_MEM(&s->mem);
        s->replace_worker();
    };

    thread = create_thread(fun, this);
    return thread != NULL;
}

List<Replace_Part> *Search_Session::parse_replacement(ccstr replace_text) {
    int k = 0, rlen = strlen(replace_text);
    auto ret = alloc_list<Replace_Part>();

    while (k < rlen) {
        bool is_dollar = false;
        auto chars = alloc_list<char>();

        Replace_Part rp; ptr0(&rp);

        if (replace_text[k] == '$')
            if (k+1 < rlen)
                if (isdigit(replace_text[k+1]))
                    rp.dollar = true;

        if (rp.dollar) {
            chars->append('$');
            for (k++; k < rlen && isdigit(replace_text[k]); k++)
                chars->append(replace_text[k]);
        } else {
            for (; k < rlen && replace_text[k] != '$'; k++)
                chars->append(replace_text[k]);
        }
        chars->append('\0');

        rp.string = chars->items;
        if (rp.dollar)
            rp.group = atoi(chars->items+1);
        ret->append(&rp);
    }

    return ret;
}

ccstr Searcher::get_replacement_text(Searcher_Result_Match *sr, ccstr replace_text) {
    if (opts.literal) return replace_text;
    if (!sr->groups) return replace_text;

    auto chars = alloc_list<char>();
    auto parts = sess.parse_replacement(replace_text);

    For (parts) {
        ccstr newstr = NULL;
        if (it.dollar) {
            if (!it.group)
                newstr = sr->match;
            else if (it.group-1 < sr->groups->len)
                newstr = sr->groups->at(it.group-1);
            else
                newstr = it.string;
        } else {
            newstr = it.string;
        }

        for (auto p = newstr; *p; p++)
            chars->append(*p);
    }

    chars->append('\0');
    return chars->items;
}

void Searcher::replace_worker() {
    int off = 0;

    For (&search_results) {
        // would these ever happen
        if (!it.filepath) continue;
        if (!it.results || !it.results->len) continue;

        File_Replacer fr;
        if (!fr.init(it.filepath, "search_and_replace")) continue;

        For (it.results) {
            if (fr.done()) break;

            auto newtext = get_replacement_text(&it, replace_with);
            fr.goto_next_replacement(new_cur2(it.match_off, -1));
            fr.do_replacement(new_cur2(it.match_off + it.match_len, -1), newtext);
        }

        fr.finish();
    }

    state = SEARCH_REPLACE_DONE;
    close_thread_handle(thread);
    thread = NULL;
}

// ========================

void Search_Session::search(char *buf, u32 buflen, List<Search_Match> *out, int limit) {
    int startlen = out->len;

    auto add_match = [&](int start, int end) -> Search_Match* {
        if (out->len - startlen > limit) return NULL;

        auto m = out->append();
        m->start = start;
        m->end = end;
        return m;
    };

    int bufoff = limit_to_range ? limit_start : 0;
    int bufend = limit_to_range ? min(limit_end, buflen) : buflen;

    if (literal) {
        while (bufoff < bufend) {
            bool found = false;

            // boyer moore
            for (int k, i = bufoff + qlen - 1; i < bufend; i += max(alpha_skip[(u8)buf[i]], find_skip[k])) {
                for (k = qlen - 1; k >= 0 && chars_eq(buf[i], query[k]); k--)
                    i--;

                if (k < 0) {
                    bufoff = i+1;
                    found = true;
                    break;
                }
            }

            if (!found) break;
            if (!add_match(bufoff, bufoff + qlen)) break;

            bufoff += qlen;
        }
    } else {
        int offvec[3 * 17]; // full match + 16 possible groups

        while (bufoff < bufend) {
            int results = pcre_exec(re, re_extra, buf, bufend, bufoff, 0, offvec, _countof(offvec));
            if (results <= 0) break;

            // don't include empty results
            if (offvec[0] != offvec[1]) {
                auto m = add_match(offvec[0], offvec[1]);
                if (!m) break;

                if (results > 1) {
                    m->group_starts = alloc_list<int>(results);
                    m->group_ends = alloc_list<int>(results);
                    for (int i = 1; i < results; i++) {
                        m->group_starts->append(offvec[2*i + 0]);
                        m->group_ends->append(offvec[2*i + 1]);
                    }
                }
            }

            bufoff = offvec[1];
            if (offvec[0] == offvec[1]) bufoff++;
        }
    }
}

void Search_Session::cleanup() {
    if (!literal) {
        if (re) {
            pcre_free(re);
            re = NULL;
        }
        if (re_extra) {
            pcre_free(re_extra);
            re_extra = NULL;
        }
    }
}

bool Search_Session::start() {
    if (literal) {
        find_skip = alloc_array(s32, qlen);
        alpha_skip = alloc_array(s32, 256);

        // generate find skip
        for (int last_prefix = qlen, i = qlen; i > 0; i--) {
            if (is_prefix(query, qlen, i))
                last_prefix = i;
            find_skip[i - 1] = last_prefix + (qlen - i);
        }

        for (int i = 0; i < qlen; i++) {
            auto slen = suffix_len(query, qlen, i);
            if (query[i - slen] != query[qlen - 1 - slen])
                find_skip[qlen - 1 - slen] = qlen - 1 - i + slen;
        }

        // generate alpha skip
        for (int i = 0; i < 256; i++)
            alpha_skip[i] = qlen;

        for (int i = 0, len = qlen-1; i < len; i++) {
            if (case_sensitive) {
                alpha_skip[(u8)query[i]] = len - i;
            } else {
                alpha_skip[(u8)tolower(query[i])] = len - i;
                alpha_skip[(u8)toupper(query[i])] = len - i;
            }
        }
    } else {
        int pcre_opts = PCRE_MULTILINE;
        if (!case_sensitive) pcre_opts |= PCRE_CASELESS;

        const char *pcre_err = NULL;
        int pcre_err_offset = 0;

        re = pcre_compile(query, pcre_opts, &pcre_err, &pcre_err_offset, NULL);
        if (!re) {
            error("pcre error at position %d: %s", pcre_err_offset, pcre_err);
            return false;
        }

        // jit enabled?
        int jit = 0;
        pcre_config(PCRE_CONFIG_JIT, &jit);
        re_extra = pcre_study(re, jit ? PCRE_STUDY_JIT_COMPILE : 0, &pcre_err);
        if (!re_extra) {
            error("pcre error at position %d: %s", pcre_err_offset, pcre_err);
            return false;
        }
    }
    return true;
}

// ========================

bool File_Replacer::init(ccstr _filepath, ccstr unique_id) {
    ptr0(this);
    filepath = _filepath;
    tmpfile = cp_sprintf(".file_replacer_%s.tmp", unique_id);

    bool success = false;
    defer {
        // if init fails, it needs to cleanup everything
        if (!success) {
            if (fmr) {
                fmr->cleanup();
                fmr = NULL;
            }
            if (fmw) {
                fmw->cleanup();
                fmw = NULL;
            }
        }
    };

    File_Mapping_Opts optsr; ptr0(&optsr);
    optsr.write = false;

    fmr = map_file_into_memory(filepath, &optsr);
    if (!fmr) return false;

    File_Mapping_Opts optsw; ptr0(&optsw);
    optsw.write = true;
    optsw.initial_size = 1024;

    fmw = map_file_into_memory(tmpfile, &optsw);
    if (!fmw) return false;

    success = true;
    return true;
}

bool File_Replacer::write(char ch) {
    if (write_pointer >= fmw->len)
        if (!fmw->resize(fmw->len * 2))
            return false;

    fmw->data[write_pointer++] = ch;
    return true;
}

char File_Replacer::advance_read_pointer() {
    auto ch = fmr->data[read_pointer++];
    if (ch == '\n') {
        read_cur.y++;
        read_cur.x = 0;
    } else {
        read_cur.x++;
    }
    return ch;
}

bool File_Replacer::goto_next_replacement(cur2 pos) {
    while (read_pointer < fmr->len) {
        if (pos.y != -1 ? (read_cur == pos) : (read_pointer == pos.x))
            return true;
        write(advance_read_pointer());
    }
    return false;
}

void File_Replacer::do_replacement(cur2 skipuntil, ccstr newtext) {
    for (auto p = newtext; *p != '\0'; p++)
        write(*p);

    while (read_pointer < fmr->len) {
        if (skipuntil.y != -1) {
            if (read_cur == skipuntil)
                break;
        } else {
            if (read_pointer == skipuntil.x)
                break;
        }
        advance_read_pointer();
    }
}

bool File_Replacer::done() { return read_pointer >= fmr->len; }

void File_Replacer::finish() {
    while (read_pointer < fmr->len) {
        write(fmr->data[read_pointer]);
        read_pointer++;
    }

    fmw->finish_writing(write_pointer);
    fmw->cleanup();
    fmw = NULL;

    fmr->cleanup();
    fmr = NULL;

    move_file_atomically(tmpfile, filepath);
}
