#pragma once

#include "common.hpp"
#include "list.hpp"

struct DString {
    List<uchar> *text;
    int start;
    int end;

    int len() { return end - start; }

    ccstr str() {
        // TODO: utf-8
        List<char> chars;
        chars.init();
        for (int i = 0, n = len(); i < n; i++)
            chars.append((char)get(i));
        chars.append('\0');
        return chars.items;
    }

    DString slice(int a, int b = -1) {
        if (b == -1) b = len();

        DString ret;
        ret.text = text;
        ret.start = start + a;
        ret.end = start + b;
        return ret;
    }

    uchar get(int i) {
        if (i < 0)
            i = len()+i;
        if (start+i >= end) {
            our_panic("out of bounds");
        }
        return text->at(start + i);
    }

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
