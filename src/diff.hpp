#pragma once

#include "common.hpp"
#include "list.hpp"
#include "mem.hpp"

struct DString {
    List<uchar> *text;
    int start;
    int end;

    int len() { return end - start; }

    List<uchar> *uchars() {
        auto ret = new_list(uchar);
        ret->concat(&text->items[start], len());
        return ret;
    }

    ccstr str();

    DString slice(int a, int b = -1) {
        if (b == -1) b = len();

        DString ret;
        ret.text = text;
        ret.start = start + a;
        ret.end = start + b;
        return ret;
    }

    uchar get(int i);

    bool equals(DString other) {
        if (len() != other.len()) return false;
        for (int i = 0, n = len(); i < n; i++)
            if (get(i) != other.get(i))
                return false;
        return true;
    }
};

enum Diff_Type {
    DIFF_INSERT,
    DIFF_DELETE,
    DIFF_SAME,
};

struct Diff {
    Diff_Type type;
    DString s;
};

struct Half_Match {
    DString big_prefix;
    DString big_suffix;
    DString small_prefix;
    DString small_suffix;
    DString middle;
};

int div_ceil(int x, int y);
Half_Match *diff_half_match(DString a, DString b);
DString diff_common_suffix(DString a, DString b);
DString diff_common_prefix(DString a, DString b);
List<Diff> *diff_bisect_split(DString a, DString b, int x, int y);
List<Diff> *diff_bisect(DString a, DString b);
List<Diff> *diff_main(DString a, DString b);
List<Diff> *diff_compute(DString a, DString b);
Diff *add_diff(List<Diff> *arr, Diff_Type type, DString s);
DString new_dstr(List<uchar> *text);
void diff_print(Diff *diff);
