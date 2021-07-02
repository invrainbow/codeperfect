#pragma once

#include <pcre.h>

#include "os.hpp"
#include "common.hpp"
#include "list.hpp"
#include "mem.hpp"

struct Search_Result {
    ccstr match;
    s32 match_len;
    cur2 match_start;
    cur2 match_end;
    s32 match_off;

    ccstr preview;
    s32 preview_len;
    cur2 preview_start;
    cur2 preview_end;

    int match_offset_in_preview;

    List<ccstr> *groups;
};

struct Search_File {
    ccstr filepath;
    List<Search_Result> *results;
};

struct Search_Opts {
    bool case_sensitive;
    bool literal;
};

enum Searcher_State {
    SEARCH_NOTHING_HAPPENING = 0,
    SEARCH_SEARCH_IN_PROGRESS,
    SEARCH_REPLACE_IN_PROGRESS,
    SEARCH_DONE,
};

struct Searcher {
    Pool mem; // orchestration
    Pool final_mem; // results

    bool mem_active;
    bool final_mem_active;

    Searcher_State state;

    Search_Opts opts;
    ccstr query;
    s32 qlen;

    // for literal matching
    s32 *find_skip;
    s32 *alpha_skip;

    // for regex-based matching
    pcre *re;
    pcre_extra *re_extra;

    List<ccstr> file_queue;
    List<Search_File> search_results;
    Thread_Handle thread;

    bool cleaned_up; // don't clean up twice

    void init();
    void cleanup(bool keep_final);
    void worker();
    ccstr boyer_moore_strnstr(ccstr s, s32 slen);
    bool start_search(ccstr query, Search_Opts *_opts);
    bool perform_replace(ccstr replace_with);

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

bool is_binary(ccstr buf, s32 len);
