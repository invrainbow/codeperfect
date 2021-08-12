#include "search.hpp"
#include "world.hpp"

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
    if (thread != NULL) {
        kill_thread(thread);
        close_thread_handle(thread);
        thread = NULL;
    }

    if (re != NULL) {
        pcre_free(re);
        re = NULL;
    }

    if (re_extra != NULL) {
        pcre_free(re_extra);
        re_extra = NULL;
    }
}

void Searcher::cleanup() {
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

    struct Temp_Match {
        int start, end;

        List<int> *group_starts;
        List<int> *group_ends;
    };

    auto matches = alloc_list<Temp_Match>();

    while (file_queue.len > 0 && total_results < 1000) {
        auto filepath = *file_queue.last();
        file_queue.len--;

        auto fm = map_file_into_memory(filepath);
        if (fm == NULL) continue;
        defer { fm->cleanup(); };

        auto buf = (char*)fm->data;
        i64 buflen = fm->len;

        if (is_binary(buf, buflen)) continue;

        int bufoff = 0;

        matches->len = 0;

        if (opts.literal) {
            while (bufoff < buflen) {
                bool found = false;

                // boyer moore
                for (int k, i = bufoff + qlen - 1; i < buflen; i += max(alpha_skip[(u8)buf[i]], find_skip[k])) {
                    for (k = qlen - 1; k >= 0 && chars_eq(buf[i], query[k]); k--)
                        i--;

                    if (k < 0) {
                        bufoff = i+1;
                        found = true;
                        break;
                    }
                }

                if (!found) break;
                if (matches->len + total_results > 1000) break;

                auto m = matches->append();
                m->start = bufoff;
                m->end = bufoff + qlen;

                bufoff += qlen;
            }
        } else {
            int offvec[3 * 17]; // full match + 16 possible groups

            while (bufoff < buflen) {
                int results = pcre_exec(re, re_extra, buf, buflen, bufoff, 0, offvec, _countof(offvec));
                if (results <= 0) break;

                if (matches->len + total_results > 1000) break;

                // don't include empty results
                if (offvec[0] != offvec[1]) {
                    auto m = matches->append();
                    m->start = offvec[0];
                    m->end = offvec[1];

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

        if (matches->len == 0) continue;

        int curr_match = 0;
        cur2 pos; ptr0(&pos);

        ccstr final_filepath = NULL;
        {
            SCOPED_MEM(&final_mem);
            final_filepath = our_strcpy(filepath);
        }

        auto sf = search_results.append();
        sf->filepath = final_filepath;
        sf->results = alloc_list<Search_Result>(matches->len);
        sf->results = alloc_list<Search_Result>(matches->len);

        Search_Result *sr = NULL;

        // convert temp matches into search results
        for (int i = 0; i < buflen; i++) {
            auto it = buf[i];

            auto &nextmatch = matches->at(curr_match);
            if (i == nextmatch.start) {
                if (total_results++ > 1000) break;

                sr = sf->results->append();
                sr->match_start = pos;
                sr->match_off = i;

                if (nextmatch.group_starts != NULL) {
                    SCOPED_MEM(&final_mem);

                    sr->groups = alloc_list<ccstr>(nextmatch.group_starts->len);
                    for (int i = 0; i < nextmatch.group_starts->len; i++) {
                        auto start = nextmatch.group_starts->at(i);
                        auto end = nextmatch.group_ends->at(i);

                        auto group = our_strncpy(&buf[start], end-start);
                        sr->groups->append(group);
                    }
                }
            }

            if (i == nextmatch.end) {
                SCOPED_MEM(&final_mem);

                sr->match_len = i - sr->match_off;
                sr->match_end = pos;
                sr->match = our_strncpy(&buf[sr->match_off], sr->match_len);

                sr->preview_start = sr->match_start;
                sr->preview_end = sr->match_end;
                sr->preview_len = sr->match_len;

                sr->match_offset_in_preview = 0;

                auto prevoff = sr->match_off;

                int to_left = min((PREVIEW_LEN - sr->preview_len) / 2, min(sr->preview_start.x, 10));

                prevoff -= to_left;
                sr->preview_start.x -= to_left;
                sr->match_offset_in_preview += to_left;
                sr->preview_len += to_left;

                int to_right = 0;
                for (int k = i; sr->preview_len < PREVIEW_LEN && buf[k] != '\n' && k < buflen; k++)
                    to_right++;

                sr->preview_len += to_right;
                sr->preview_end.x += to_right;
                sr->preview = our_strncpy(&buf[prevoff], sr->preview_len);

                if (++curr_match >= matches->len)
                    break;
            }

            if (it == '\n') {
                pos.y++;
                pos.x = 0;
            } else {
                pos.x++;
            }
        }
    }

    state = SEARCH_SEARCH_DONE;
    close_thread_handle(thread);
    thread = NULL;

    cleanup_search();
}

bool is_binary(ccstr buf, s32 len) {
    s32 suspicious_bytes = 0;
    s32 total_bytes = len > 512 ? 512 : len;

    // empty
    if (len == 0) return false;

    // utf8 bom
    if (len >= 3 && buf[0] == 0xEF && buf[1] == 0xBB && buf[2] == 0xBF) return false;

    // pdf
    if (len >= 5 && strncmp(buf, "%PDF-", 5) == 0) return true;

    for (u32 i = 0; i < total_bytes; i++) {
        if (buf[i] == '\0') return true;

        if ((buf[i] < 7 || buf[i] > 14) && (buf[i] < 32 || buf[i] > 127)) {
            /* UTF-8 detection */
            if (buf[i] > 193 && buf[i] < 224 && i + 1 < total_bytes) {
                i++;
                if (buf[i] > 127 && buf[i] < 192)
                    continue;
            } else if (buf[i] > 223 && buf[i] < 240 && i + 2 < total_bytes) {
                i++;
                if (buf[i] > 127 && buf[i] < 192 && buf[i + 1] > 127 && buf[i + 1] < 192) {
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

bool Searcher::start_search(ccstr _query, Search_Opts *_opts) {
    SCOPED_MEM(&mem);

    state = SEARCH_SEARCH_IN_PROGRESS;
    opts = *_opts;

    query = our_strcpy(_query);
    qlen = strlen(query);

    file_queue.init();

    {
        SCOPED_MEM(&final_mem);
        search_results.init();
    }

    if (opts.include == NULL) include_parts = make_path(opts.include)->parts;
    if (opts.exclude == NULL) exclude_parts = make_path(opts.exclude)->parts;

    // precompute shit
    if (opts.literal) {
        // generate find skip
        {
            find_skip = alloc_array(s32, qlen);

            s32 last_prefix = qlen;
            for (int i = last_prefix; i > 0; i--) {
                if (is_prefix(query, qlen, i))
                    last_prefix = i;
                find_skip[i - 1] = last_prefix + (qlen - i);
            }

            for (int i = 0; i < qlen; i++) {
                auto slen = suffix_len(query, qlen, i);
                if (query[i - slen] != query[qlen - 1 - slen])
                    find_skip[qlen - 1 - slen] = qlen - 1 - i + slen;
            }
        }

        // generate alpha skip
        {
            alpha_skip = alloc_array(s32, 256);
            for (int i = 0; i < 256; i++)
                alpha_skip[i] = qlen;

            int len = qlen-1;
            for (int i = 0; i < len; i++) {
                if (opts.case_sensitive) {
                    alpha_skip[(u8)query[i]] = len - i;
                } else {
                    alpha_skip[(u8)tolower(query[i])] = len - i;
                    alpha_skip[(u8)toupper(query[i])] = len - i;
                }
            }
        }
    } else {
        int pcre_opts = PCRE_MULTILINE;
        if (!opts.case_sensitive)
            pcre_opts |= PCRE_CASELESS;

        const char *pcre_err = NULL;
        int pcre_err_offset = 0;

        re = pcre_compile(query, pcre_opts, &pcre_err, &pcre_err_offset, NULL);
        if (re == NULL) return false;
        // TODO: die("Bad regex! pcre_compile() failed at position %i: %s", pcre_err_offset, pcre_err);

        // jit enabled?
        int has_jit = 0;
        pcre_config(PCRE_CONFIG_JIT, &has_jit);
        re_extra = pcre_study(re, has_jit ? PCRE_STUDY_JIT_COMPILE : 0, &pcre_err);
    }

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
                for (auto child = node->children; child != NULL; child = child->next) {
                    stack.append(child);
                    stackpaths.append(fullpath);
                }
            } else {
                // if (include_parts == NULL || pattern_matches(include_parts, fullpath))
                //     if (exclude_parts == NULL || !pattern_matches(exclude_parts, fullpath))
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
    return (thread != NULL);
}

bool Searcher::start_replace(ccstr _replace_with) {
    SCOPED_MEM(&mem);
    replace_with = our_strcpy(_replace_with);

    auto fun = [](void *param) {
        auto s = (Searcher*)param;
        SCOPED_MEM(&s->mem);
        s->replace_worker();
    };

    thread = create_thread(fun, this);
    return (thread != NULL);
}

ccstr Searcher::get_replacement_text(Search_Result *sr, ccstr replace_text) {
    if (opts.literal) return replace_text;

    auto chars = alloc_list<char>();
    auto rlen = strlen(replace_text);

    int k = 0;
    while (k < rlen) {
        bool is_dollar_replacement = false;

        do {
            if (sr->groups == NULL) break;
            if (replace_text[k] != '$') break;
            if (k+1 >= rlen) break;
            if (!isdigit(replace_text[k+1])) break;

            Frame frame;

            auto k2 = k + 1;
            auto tmp = alloc_list<char>();
            for(; k2 < rlen && isdigit(replace_text[k2]); k2++)
                tmp->append(replace_text[k2]);
            tmp->append('\0');

            ccstr groupstr = NULL;

            int group = atoi(tmp->items);
            if (group == 0) {
                groupstr = sr->match;
            } else if (group-1 < sr->groups->len) {
                groupstr = sr->groups->at(group-1);
            } else {
                frame.restore();
                break;
            }

            for (auto p = groupstr; *p != '\0'; p++)
                chars->append(*p);

            k = k2;
            is_dollar_replacement = true;
        } while (0);

        if (!is_dollar_replacement) {
            chars->append(replace_text[k]);
            k++;
        }
    }

    chars->append('\0');
    return chars->items;
}

void Searcher::replace_worker() {
    int off = 0;

    For (search_results) {
        // would these ever happen
        if (it.filepath == NULL) continue;
        if (it.results == NULL || it.results->len == 0) continue;

#if 0
        if (!streq(it.filepath, "/Users/brandon/dev/hugo/CONTRIBUTING.md")) continue;
#endif

        File_Mapping_Opts optsr; ptr0(&optsr);
        optsr.write = false;

        auto fmr = map_file_into_memory(it.filepath, &optsr);
        if (fmr == NULL) continue;
        defer { if (fmr != NULL) fmr->cleanup(); };

        File_Mapping_Opts optsw; ptr0(&optsw);
        optsw.write = true;
        optsw.initial_size = 1024;

        auto fmw = map_file_into_memory(".search_and_replace.tmp", &optsw);
        if (fmw == NULL) continue;
        defer { if (fmw != NULL) fmw->cleanup(); };

        int write_pointer = 0;

        auto write = [&](char ch) -> bool {
            if (write_pointer >= fmw->len)
                if (!fmw->resize(fmw->len * 2))
                    return false;

            fmw->data[write_pointer++] = ch;
            return true;
        };

        int index = 0;
        int result_index = 0;
        Search_Result *nextmatch = &it.results->at(result_index);

        while (index < fmr->len) {
            if (nextmatch != NULL && index == nextmatch->match_off) {
                auto newtext = get_replacement_text(nextmatch, replace_with);
                for (auto p = newtext; *p != '\0'; p++)
                    write(*p);

                result_index++;
                index += nextmatch->match_len;
                nextmatch = result_index < it.results->len ? &it.results->at(result_index) : NULL;
            } else {
                write(fmr->data[index]);
                index++;
            }
        }

        fmw->finish_writing(write_pointer);
        fmw->cleanup();
        fmw = NULL;

        fmr->cleanup();
        fmr = NULL;

        move_file_atomically(".search_and_replace.tmp", it.filepath);
    }

    state = SEARCH_REPLACE_DONE;
    close_thread_handle(thread);
    thread = NULL;
}
