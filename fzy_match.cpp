// ripped out of match.c in fzy
// https://github.com/jhawthorn/fzy/blob/master/src/match.c

#include <ctype.h>
#include <string.h>
// #include <strings.h>
#include <stdio.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>

#include "fzy_match.h"

score_t bonus_states[3][256] = {0};
size_t bonus_index[256] = {0};

// initialize bonus tables
void fzy_init() {
    for (char c = 'A'; c < 'Z'; c++) bonus_index[c] = 2;
    for (char c = 'a'; c < 'z'; c++) bonus_index[c] = 1;
    for (char c = '0'; c < '9'; c++) bonus_index[c] = 1;

    bonus_states[1]['/'] = SCORE_MATCH_SLASH;
    bonus_states[1]['-'] = SCORE_MATCH_WORD;
    bonus_states[1]['_'] = SCORE_MATCH_WORD;
    bonus_states[1][' '] = SCORE_MATCH_WORD;
    bonus_states[1]['.'] = SCORE_MATCH_DOT;

    bonus_states[2]['/'] = SCORE_MATCH_SLASH;
    bonus_states[2]['-'] = SCORE_MATCH_WORD;
    bonus_states[2]['_'] = SCORE_MATCH_WORD;
    bonus_states[2][' '] = SCORE_MATCH_WORD;
    bonus_states[2]['.'] = SCORE_MATCH_DOT;

    for (char c = 'a'; c < 'z'; c++) bonus_states[2][c] = SCORE_MATCH_CAPITAL;
}

int fzy_has_match(const char *needle, const char *haystack) {
    while (*needle) {
        char nch = *needle++;
        const char accept[3] = {nch, toupper(nch), 0};
        if (!(haystack = strpbrk(haystack, accept))) {
            return 0;
        }
        haystack++;
    }
    return 1;
}

#define SWAP(x, y, T) do { T SWAP = x; x = y; y = SWAP; } while (0)

#define max(a, b) (((a) > (b)) ? (a) : (b))

struct match_struct {
    int needle_len;
    int haystack_len;

    char lower_needle[MATCH_MAX_LEN];
    char lower_haystack[MATCH_MAX_LEN];

    score_t match_bonus[MATCH_MAX_LEN];
};

static void precompute_bonus(const char *haystack, score_t *match_bonus) {
    /* Which positions are beginning of words */
    char last_ch = '/';
    for (int i = 0; haystack[i]; i++) {
        char ch = haystack[i];
        match_bonus[i] = COMPUTE_BONUS(last_ch, ch);
        last_ch = ch;
    }
}

static void setup_match_struct(struct match_struct *match, const char *needle, const char *haystack) {
    match->needle_len = strlen(needle);
    match->haystack_len = strlen(haystack);

    if (match->haystack_len > MATCH_MAX_LEN || match->needle_len > match->haystack_len) {
        return;
    }

    for (int i = 0; i < match->needle_len; i++)
        match->lower_needle[i] = tolower(needle[i]);

    for (int i = 0; i < match->haystack_len; i++)
        match->lower_haystack[i] = tolower(haystack[i]);

    precompute_bonus(haystack, match->match_bonus);
}

static inline void match_row(const struct match_struct *match, int row, score_t *curr_D, score_t *curr_M, const score_t *last_D, const score_t *last_M) {
    int n = match->needle_len;
    int m = match->haystack_len;
    int i = row;

    const char *lower_needle = match->lower_needle;
    const char *lower_haystack = match->lower_haystack;
    const score_t *match_bonus = match->match_bonus;

    score_t prev_score = SCORE_MIN;
    score_t gap_score = i == n - 1 ? SCORE_GAP_TRAILING : SCORE_GAP_INNER;

    for (int j = 0; j < m; j++) {
        if (lower_needle[i] == lower_haystack[j]) {
            score_t score = SCORE_MIN;
            if (!i) {
                score = (j * SCORE_GAP_LEADING) + match_bonus[j];
            } else if (j) { /* i > 0 && j > 0*/
                score = max(
                        last_M[j - 1] + match_bonus[j],

                        /* consecutive match, doesn't stack with match_bonus */
                        last_D[j - 1] + SCORE_MATCH_CONSECUTIVE);
            }
            curr_D[j] = score;
            curr_M[j] = prev_score = max(score, prev_score + gap_score);
        } else {
            curr_D[j] = SCORE_MIN;
            curr_M[j] = prev_score = prev_score + gap_score;
        }
    }
}

score_t fzy_match(const char *needle, const char *haystack) {
    if (!*needle)
        return SCORE_MIN;

    struct match_struct match;
    setup_match_struct(&match, needle, haystack);

    int n = match.needle_len;
    int m = match.haystack_len;

    if (m > MATCH_MAX_LEN || n > m) {
        /*
         * Unreasonably large candidate: return no score
         * If it is a valid match it will still be returned, it will
         * just be ranked below any reasonably sized candidates
         */
        return SCORE_MIN;
    } else if (n == m) {
        /* Since this method can only be called with a haystack which
         * matches needle. If the lengths of the strings are equal the
         * strings themselves must also be equal (ignoring case).
         */
        return SCORE_MAX;
    }

    /*
     * D[][] Stores the best score for this position ending with a match.
     * M[][] Stores the best possible score at this position.
     */
    score_t D[2][MATCH_MAX_LEN], M[2][MATCH_MAX_LEN];

    score_t *last_D, *last_M;
    score_t *curr_D, *curr_M;

    last_D = D[0];
    last_M = M[0];
    curr_D = D[1];
    curr_M = M[1];

    for (int i = 0; i < n; i++) {
        match_row(&match, i, curr_D, curr_M, last_D, last_M);

        SWAP(curr_D, last_D, score_t *);
        SWAP(curr_M, last_M, score_t *);
    }

    return last_M[m - 1];
}
