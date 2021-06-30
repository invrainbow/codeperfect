#pragma once

#include <pcre.h>

#include "os.hpp"
#include "common.hpp"
#include "list.hpp"

struct Search_Result {
    u32 match_col;
    u32 match_col_in_preview;
    u32 match_len;
    ccstr preview;
    u32 row;
    u32 results_in_row;
    ccstr filename;
};

struct Search_Opts {
    bool case_sensitive;
    bool literal;

};

struct Searcher {
    bool in_progress;

    ccstr query;
    s32 qlen;

    // for literal matching
    s32 *find_skip;
    s32 *alpha_skip;

    // for regex-based matching
    pcre *re;
    pcre_extra *re_extra;

    Search_Opts opts;

    List<Search_Result> search_results;
    Thread_Handle thread;
    List<ccstr> file_queue;
    Lock lock;

    void init(ccstr query, Search_Opts *_opts);
    void search_file(ccstr path); 
    void precompute_crap_for_literal_search();
    void precompute_crap_for_regex_search();
    int boyer_moore_strnstr(ccstr buf, int off, s32 slen);
    int search_buf(ccstr buf, s32 buflen);

    inline bool chars_eq(char a, char b) {
        if (opts.case_sensitive)
            return a == b;
        return tolower(a) == tolower(b);
    }

    bool is_prefix(ccstr s, s32 s_len, s32 pos) {
        for (int i = 0; pos + i < s_len; i++)
            if (!chars_eq(s[i], s[i+pos]))
                return false;
        return true;
    }

    s32 suffix_len(ccstr s, s32 s_len, s32 pos) {
        s32 i = 0;
        for (; i < pos; i++)
            if (!chars_eq(s[pos - i], s[s_len - i - 1]))
                break;
        return i;
    }
};
