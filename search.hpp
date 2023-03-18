#pragma once

#include <pcre.h>

#include "os.hpp"
#include "common.hpp"
#include "list.hpp"
#include "mem.hpp"
#include "buffer.hpp"

struct Search_Match {
    int start;
    int end;
    List<int> *group_starts;
    List<int> *group_ends;
};

struct Replace_Part {
    bool dollar;
    int group;
    ccstr string;
};

struct Search_Session {
    ccstr query;
    u32 qlen;
    bool case_sensitive;
    bool literal;

    bool limit_to_range;
    int limit_start;
    int limit_end;

    union {
        struct {
            s32 *find_skip;
            s32 alpha_skip[256];
        };
        struct {
            pcre *re;
            pcre_extra *re_extra;
        };
    };

    bool init() { return precompute(); }
    bool precompute();
    void cleanup();
    void search(char *buf, u32 buflen, List<Search_Match> *out, int limit);

    inline bool chars_eq(char a, char b) {
        if (!case_sensitive) {
            a = tolower(a);
            b = tolower(b);
        }
        return a == b;
    }

    inline bool is_prefix(ccstr s, s32 s_len, s32 pos) {
        for (int i = 0; pos + i < s_len; i++)
            if (!chars_eq(s[i], s[i+pos]))
                return false;
        return true;
    }

    inline s32 suffix_len(ccstr s, s32 s_len, s32 pos) {
        s32 i = 0;
        for (; i < pos; i++)
            if (!chars_eq(s[pos - i], s[s_len - i - 1]))
                break;
        return i;
    }
};

List<Replace_Part> *parse_search_replacement(ccstr replace_text);

struct Searcher_Opts {
    bool case_sensitive;
    bool literal;
    ccstr include;
    ccstr exclude;
};

enum Searcher_State {
    SEARCH_NOTHING_HAPPENING = 0,
    SEARCH_SEARCH_IN_PROGRESS,
    SEARCH_SEARCH_DONE,
    SEARCH_REPLACE_IN_PROGRESS,
    SEARCH_REPLACE_DONE,
};

struct Searcher_Result_Match {
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

    Mark *mark_start;
    Mark *mark_end;

    List<ccstr> *groups;
};

struct Searcher_Result_File {
    ccstr filepath;
    List<Searcher_Result_Match> *results;
};

struct Searcher {
    Pool mem; // orchestration
    Pool final_mem; // results

    bool mem_active;
    bool final_mem_active;

    Search_Session sess;
    Searcher_State state;
    Searcher_Opts opts;
    ccstr replace_with;

    List<ccstr> *include_parts;
    List<ccstr> *exclude_parts;

    // search
    // s32 *find_skip;
    // s32 *alpha_skip;
    // pcre *re;
    // pcre_extra *re_extra;

    List<ccstr> file_queue;
    List<Searcher_Result_File> search_results;

    Thread_Handle thread;

    bool cleaned_up; // don't clean up twice

    void init();
    void cleanup();
    void cleanup_search();

    bool start_search(ccstr _query, Searcher_Opts *_opts);
    void search_worker();

    ccstr get_replacement_text(Searcher_Result_Match *sr, ccstr replace_text);
    bool start_replace(ccstr _replace_with);
    void replace_worker();
};

bool is_binary(ccstr buf, s32 len);

struct File_Replacer {
    File_Mapping *fmr;
    File_Mapping *fmw;
    int write_pointer;
    int read_pointer;
    cur2 read_cur;
    ccstr tmpfile;
    ccstr filepath;

    bool init(ccstr filepath, ccstr unique_id);
    void finish();
    char advance_read_pointer();
    bool goto_next_replacement(cur2 pos);
    void do_replacement(cur2 skipuntil, ccstr newtext);
    bool write(char ch);
    bool done();
};
