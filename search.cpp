#include "search.hpp"
#include "world.hpp"
#include "defer.hpp"
#include "copy.hpp"

#define PREVIEW_LEN 40

bool Searcher::init() {
    ptr0(this);

    thread_mem.init("searcher_thread_mem");
    message_queue.init();

    auto fun = [](void *param) {
        auto s = (Searcher*)param;
        SCOPED_MEM(&s->thread_mem);
        s->search_thread();
    };

    thread = create_thread(fun, this);
    return !!thread;
}

void Searcher::search_thread() {
    Search_Session sess; ptr0(&sess);

    // not a great name - it's basically a mem that lives for a whole "cycle"
    // of search -> replace -> cancel. whenever we cancel or initiate a new
    // search, this gets reset
    Pool cycle_mem; cycle_mem.init("searcher_cycle_mem");

    auto search_result_buffer = new_list(Searcher_Result_File);
    auto search_match_buffer = new_list(Search_Match);
    List<ccstr> *search_file_queue = NULL;
    int search_total_results = 0;

    ccstr replace_with = NULL;
    int replace_current_search_result = 0;

    auto cleanup_previous_search = [&]() {
        if (search_results) {
            For (search_results) {
                if (!it.results) continue;
                For (it.results) {
                    it.mark_start->cleanup();
                    it.mark_end->cleanup();

                    world.mark_fridge.free(it.mark_start);
                    world.mark_fridge.free(it.mark_end);
                }
            }
            search_results = NULL;
        }
        sess.cleanup();
        cycle_mem.cleanup();
    };

    while (true) {
        do {
            if (!message_queue.len()) break;
            auto messages = message_queue.start();
            defer { message_queue.end(); };

            auto msg = messages->last();
            if (!msg) break;

            switch (msg->type) {
            case SM_START_SEARCH: {
                cleanup_previous_search();
                {
                    cycle_mem.init("searcher_cycle_mem");
                    SCOPED_MEM(&cycle_mem);

                    opts = msg->start_search.opts->copy();
                    search_file_queue = copy_string_list(msg->start_search.file_queue);

                    ptr0(&sess);
                    sess.query = cp_strdup(opts->query);
                    sess.qlen = strlen(sess.query);
                    sess.case_sensitive = opts->case_sensitive;
                    sess.literal = opts->literal;
                    if (!sess.init()) break;
                }
                search_total_results = 0;
                search_start_time_milli = current_time_milli();
                search_match_buffer->len = 0;
                search_result_buffer->len = 0;
                state = SEARCH_SEARCH_IN_PROGRESS;
                break;
            }
            case SM_START_REPLACE: {
                if (state != SEARCH_SEARCH_DONE) break;
                {
                    SCOPED_MEM(&cycle_mem);
                    replace_with = cp_strdup(msg->start_replace.replace_with);
                    replace_current_search_result = 0;
                }
                state = SEARCH_REPLACE_IN_PROGRESS;
                break;
            }
            case SM_CANCEL:
                cleanup_previous_search();
                state = SEARCH_NOTHING_HAPPENING;
                break;
            }
        } while (0);

        switch (state) {
        case SEARCH_NOTHING_HAPPENING:
        case SEARCH_SEARCH_DONE:
        case SEARCH_REPLACE_DONE:
            sleep_milli(10);
            break;

        case SEARCH_SEARCH_IN_PROGRESS: {
            // are we done? copy results over to cycle_mem, exit
            if (!search_file_queue->len) {
                sess.cleanup();
                {
                    SCOPED_MEM(&cycle_mem);
                    search_results = copy_list(search_result_buffer);
                }
                state = SEARCH_SEARCH_DONE;
                break;
            }

            // otherwise, do one unit of work - search a single file
            auto filepath = *search_file_queue->last(); // pop();

            auto fm = map_file_into_memory(filepath);
            if (!fm) break;
            defer { fm->cleanup(); };

            auto buf = (char*)fm->data;
            i64 buflen = fm->len;
            if (is_binary(buf, buflen)) break;

            search_match_buffer->len = 0;
            sess.search(buf, buflen, search_match_buffer, 10000);
            if (!search_match_buffer->len) break;

            int curr_match = 0;
            cur2 pos; ptr0(&pos);

            Searcher_Result_Match sr; ptr0(&sr);

            auto results = new_list(Searcher_Result_Match, search_match_buffer->len);

            // convert temp matches into search results
            for (int i = 0; i < buflen; i++) {
                auto it = buf[i];

                auto &next = search_match_buffer->at(curr_match);
                if (i == next.start) {
                    if (search_total_results++ > 1000) break;

                    sr.match_start = pos;
                    sr.match_off = i;

                    if (next.group_starts) {
                        sr.groups = new_list(ccstr, next.group_starts->len);
                        Fori (next.group_starts) {
                            auto start = it;
                            auto end = next.group_ends->at(i);

                            auto group = cp_strncpy(&buf[start], end-start);
                            sr.groups->append(group);
                        }
                    }
                }

                if (i == next.end) {
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
                    for (int k = i; sr.preview_len + to_right < PREVIEW_LEN && buf[k] != '\n' && k < buflen; k++)
                        to_right++;

                    sr.preview_len += to_right;
                    sr.preview_end.x += to_right;
                    sr.preview = cp_strncpy(&buf[prevoff], sr.preview_len);

                    sr.mark_start = world.mark_fridge.alloc();
                    sr.mark_end = world.mark_fridge.alloc();

                    // TODO: do this from main thread.
                    // editor->buf->insert_mark(MARK_SEARCH_RESULT, sr.match_start, sr.mark_start);
                    // editor->buf->insert_mark(MARK_SEARCH_RESULT, sr.match_end, sr.mark_end);

                    results->append(&sr);

                    if (++curr_match >= search_match_buffer->len) break;
                }

                if (it == '\n') {
                    pos.y++;
                    pos.x = 0;
                } else {
                    pos.x++;
                }
            }

            Searcher_Result_File sf;
            sf.filepath = filepath;
            sf.results = results;
            search_result_buffer->append(&sf);
            break;
        }

        case SEARCH_REPLACE_IN_PROGRESS: {
            if (replace_current_search_result >= search_results->len) {
                state = SEARCH_REPLACE_DONE;
                break;
            }

            auto &it = search_results->at(replace_current_search_result++);

            // would these ever happen
            if (!it.filepath) break;
            if (!it.results || !it.results->len) break;

            File_Replacer fr;
            if (!fr.init(it.filepath, "search_and_replace")) break;

            For (it.results) {
                if (fr.done()) break;

                auto newtext = get_replacement_text(&it, replace_with);
                fr.goto_next_replacement(new_cur2(it.match_off, -1));
                fr.do_replacement(new_cur2(it.match_off + it.match_len, -1), newtext);
            }

            fr.finish();
            break;
        }
        }
    }
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

bool Searcher::start_search(Searcher_Opts *opts) {
    if (world.file_tree_busy) {
        tell_user_error("The file list is currently being generated.");
        return false;
    }

    // generate file queue
    auto file_queue = new_list(ccstr);
    auto stack = listof<FT_Node*>(world.file_tree);
    auto stackpaths = listof<ccstr>(world.current_path);
    while (stack->len) {
        auto node = stack->pop();
        auto path = stackpaths->pop();
        auto fullpath = path_join(path, node->name);
        if (!node->is_directory) {
            file_queue->append(fullpath);
            continue;
        }
        for (auto child = node->children; child; child = child->next) {
            stack->append(child);
            stackpaths->append(fullpath);
        }
    }

    message_queue.add([&](auto it) {
        it->type = SM_START_SEARCH;
        it->start_search.opts = opts->copy();
        it->start_search.file_queue = copy_string_list(file_queue);
    });
    return true;
}

void Searcher::start_replace(ccstr replace_with) {
    message_queue.add([&](auto it) {
        it->type = SM_START_REPLACE;
        it->start_replace.replace_with = cp_strdup(replace_with);
    });
}

void Searcher::cancel() {
    message_queue.add([&](auto it) {
        it->type = SM_CANCEL;
    });
}

List<Replace_Part> *parse_search_replacement(ccstr replace_text) {
    int k = 0, rlen = strlen(replace_text);
    auto ret = new_list(Replace_Part);

    while (k < rlen) {
        bool is_dollar = false;
        auto chars = new_list(char);

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
    if (opts->literal) return replace_text;
    if (!sr->groups) return replace_text;

    auto chars = new_list(char);
    auto parts = parse_search_replacement(replace_text);

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

// ========================

void Search_Session::search(ccstr buf, u32 buflen, List<Search_Match> *out, int limit) {
    int startlen = out->len;

    auto add_match = [&](int start, int end) -> Search_Match* {
        if (limit != -1 && out->len - startlen > limit) return NULL;

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
                    m->group_starts = new_list(int, results);
                    m->group_ends = new_list(int, results);
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

bool Search_Session::precompute() {
    if (literal) {
        find_skip = new_array(s32, qlen);

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
