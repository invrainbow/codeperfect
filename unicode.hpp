#pragma once

#include "common.hpp"

int cp_wcwidth(uchar c);
int cp_wcswidth(uchar *s, int len);

enum Gr_State {
    GR_ANY,
    GR_CR,
    GR_CONTROLLF,
    GR_L,
    GR_LVV,
    GR_LVTT,
    GR_PREPEND,
    GR_EXTENDEDPICTOGRAPHIC,
    GR_EXTENDEDPICTOGRAPHICZWJ,
    GR_RIODD,
    GR_RIEVEN,
};

struct Grapheme_Clusterer {
    Gr_State state;
    bool start;

    void init();
    bool feed(int codepoint);
};
