#include "search.hpp"
#include "world.hpp"

int Searcher::boyer_moore_strnstr(ccstr buf, int off, s32 slen) {
    int i;
    s32 pos = qlen - 1;

    auto s = &buf[off];
    slen -= off;

    while (pos < slen) {
        for (i = qlen - 1; i >= 0 && chars_eq(s[pos], query[i]); i--)
            pos--;
        if (i < 0)
            return off + pos + 1;
        pos += max(alpha_skip[(u8)s[pos]], find_skip[i]);
    }

    return -1;
}

void Searcher::search_buf(ccstr buf, s32 buflen, ccstr filename) {
    s32 buf_offset = 0;

    struct Temp_Match {
        int start;
        int end;
    };

    auto matches = alloc_list<Temp_Match>();

    if (opts.literal) {
        while (buf_offset < buflen) {
            buf_offset = boyer_moore_strnstr(buf, buf_offset, buflen - buf_offset);
            if (buf_offset == -1) break;

            auto m = matches->append();
            m->start = buf_offset;
            m->end = buf_offset + qlen;

            buf_offset += qlen;
        }
    } else {
        int offset_vector[3];
        while (buf_offset < buflen && pcre_exec(re, re_extra, buf, buflen, buf_offset, 0, offset_vector, 3) >= 0) {
            buf_offset = offset_vector[1];
            if (offset_vector[0] == offset_vector[1])
                buf_offset++;

            auto m = matches->append();
            m->start = offset_vector[0];
            m->end = offset_vector[1];
        }
    }

    if (matches->len == 0) return;

    int curr_match = 0;
    int line = 0;
    int col = 0;

    Search_Result *sr = NULL;
    int matchoff = 0;

    SCOPED_LOCK(&lock);

    // convert temp matches into search results
    for (int i = 0; i < buflen; i++) {
        auto it = buf[i];

        auto &nextmatch = matches->at(curr_match);
        if (i == nextmatch.start) {
            matchoff = i;

            sr = search_results.append();
            sr->filename = filename;
            sr->match_start = new_cur2(col, line);
        }

        if (i == nextmatch.end) {
            sr->match_len = i - matchoff;
            sr->match_end = new_cur2(col, line);
            sr->match = our_strncpy(&buf[matchoff], sr->match_len);

            sr->preview_start = sr->match_start;
            sr->preview_end = sr->match_end;
            sr->preview_len = sr->match_len;

            auto prevoff = matchoff;

            int to_left = min((80 - sr->preview_len) / 2, min(sr->preview_start.x, 10));

            prevoff -= to_left;
            sr->preview_start.x -= to_left;
            sr->preview_len += to_left;

            int to_right = 0;
            for (int k = i; sr->preview_len < 80 && buf[k] != '\n' && k < buflen; k++)
                to_right++;

            sr->preview_len += to_right;
            sr->preview_end.x += to_right;
            sr->preview = our_strncpy(&buf[prevoff], sr->preview_len);

            if (++curr_match >= matches->len)
                break;
        }

        if (it == '\n') {
            line++;
            col = 0;
        } else {
            col++;
        }
    }
}

void Searcher::precompute_crap_for_literal_search() {
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
}

void Searcher::precompute_crap_for_regex_search() {
    int pcre_opts = PCRE_MULTILINE;
    if (!opts.case_sensitive)
        pcre_opts |= PCRE_CASELESS;

    const char *pcre_err = NULL;
    int pcre_err_offset = 0;

    re = pcre_compile(query, pcre_opts, &pcre_err, &pcre_err_offset, NULL);

    // TODO: surface error
    /*
    die("Bad regex! pcre_compile() failed at position %i: %s",
        pcre_err_offset,
        pcre_err);
    */
    if (re == NULL) return;

    // is jit enabled?
    int has_jit = 0;
    pcre_config(PCRE_CONFIG_JIT, &has_jit);

    re_extra = pcre_study(re, has_jit ? PCRE_STUDY_JIT_COMPILE : 0, &pcre_err);
}

void Searcher::init(ccstr _query, Search_Opts *_opts) {
    ptr0(this);

    SCOPED_MEM(&world.search_mem);
    world.search_mem.reset(); // cleanup and re-init?

    query = our_strcpy(_query);
    qlen = strlen(query);
    memcpy(&opts, _opts, sizeof(_opts));

    if (opts.literal)
        precompute_crap_for_literal_search();
    else
        precompute_crap_for_regex_search();

    // actually start the search

    search_results.init();
    in_progress = true;

    // unroll into a while loop with our own stack?
    fn<void(ccstr, File_Tree_Node *)> process_ftnode = [&](auto path, auto it) {
        auto fullpath = path_join(path, it->name);
        if (it->is_directory)
            process_ftnode(fullpath, it);
        else
            file_queue.append(fullpath);
    };

    process_ftnode(world.current_path, world.file_tree);

    auto worker = [](void *param) {
        auto s = (Searcher*)param;

        auto pop_next_file = [](Searcher *s) -> ccstr {
            SCOPED_LOCK(&s->lock);
            if (s->file_queue.len == 0) return NULL;

            auto ret = *s->file_queue.last();
            s->file_queue.len--;
            return ret;
        };

        ccstr filepath = pop_next_file(s);
        if (filepath == NULL) return;

        // TODO
    };

    thread = create_thread(worker, this);
    if (thread == NULL) {
        // TODO: handle error
        in_progress = false;
    }
}
