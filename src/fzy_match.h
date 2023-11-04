#ifndef MATCH_H
#define MATCH_H MATCH_H

#include <math.h>

typedef double score_t;
#define SCORE_MAX INFINITY
#define SCORE_MIN -INFINITY

#define MATCH_MAX_LEN 1024

void fzy_init();
int fzy_has_match(const char *needle, const char *haystack);
score_t fzy_match(const char *needle, const char *haystack);

#define SCORE_GAP_LEADING -0.005
#define SCORE_GAP_TRAILING -0.005
#define SCORE_GAP_INNER -0.01
#define SCORE_MATCH_CONSECUTIVE 1.0
#define SCORE_MATCH_SLASH 0.9
#define SCORE_MATCH_WORD 0.8
#define SCORE_MATCH_CAPITAL 0.7
#define SCORE_MATCH_DOT 0.6

#define COMPUTE_BONUS(last_ch, ch) (bonus_states[bonus_index[(unsigned char)(ch)]][(unsigned char)(last_ch)])

#endif
