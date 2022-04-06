#pragma once

int cp_wcwidth(int c);

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

enum Uni_Property {
    PR_ANY,
    PR_PREPREND,
    PR_CR,
    PR_LF,
    PR_CONTROL,
    PR_EXTEND,
    PR_REGIONALINDICATOR,
    PR_SPACINGMARK,
    PR_L,
    PR_V,
    PR_T,
    PR_LV,
    PR_LVT,
    PR_ZWJ,
    PR_EXTENDEDPICTOGRAPHIC,
};

struct Property_Range {
    int start;
    int end;
    Uni_Property prop;
};

struct Grapheme_Clusterer {
    Gr_State state;
    bool start;

    void init();
    bool feed(int codepoint);
};
