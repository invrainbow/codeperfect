#include "diff.hpp"
#include "mem.hpp"
#include "os.hpp"
#include "buffer.hpp"

DString new_dstr(List<uchar> *text) {
    DString i;
    i.text = text;
    i.start = 0;
    i.end = text->len;
    return i;
}

ccstr format_string(ccstr s) {
    auto ret = new_list(char);
    for (auto *p = s; *p; p++) {
        if (*p == '\n') {
            ret->append('\\');
            ret->append('n');
        } else {
            ret->append(*p);
        }
    }
    ret->append('\0');
    return ret->items;
}

void diff_print(Diff *diff) {
    switch (diff->type) {
    case DIFF_INSERT: print(" - [insert] |%s|", format_string(diff->s.str())); break;
    case DIFF_DELETE: print(" - [delete] |%s|", format_string(diff->s.str())); break;
    case DIFF_SAME: print(" - [same] |%s|", format_string(diff->s.str())); break;
    }
}

Diff *add_diff(List<Diff> *arr, Diff_Type type, DString s) {
    auto diff = arr->append();
    diff->type = type;
    diff->s = s;
    return diff;
}

uchar DString::get(int i) {
    if (i < 0)
        i = len()+i;
    if (start+i >= end)
        cp_panic("out of bounds");
    return text->at(start + i);
}

ccstr DString::str() {
    return ustr_to_cstr(uchars())->items;
}

int index_of(DString big, DString small, int start = 0) {
    for (int i = start, len = big.len(); i < len; i++) {
        if (i + small.len() > len)
            break;
        bool found = true;
        for (int j = 0; j < small.len(); j++) {
            if (big.get(i+j) != small.get(j)) {
                found = false;
                break;
            }
        }
        if (found) return i;
    }
    return -1;
}

List<Diff> *diff_compute(DString a, DString b) {
    if (!a.len()) {
        auto ret = new_list(Diff);
        add_diff(ret, DIFF_INSERT, b);
        return ret;
    }

    if (!b.len()) {
        auto ret = new_list(Diff);
        add_diff(ret, DIFF_DELETE, a);
        return ret;
    }

    bool abig = a.len() > b.len();

    DString big = abig ? a : b;
    DString small = abig ? b : a;

    int i = index_of(big, small);
    if (i != -1) {
        auto ret = new_list(Diff);
        add_diff(ret, abig ? DIFF_DELETE : DIFF_INSERT, big.slice(0, i));
        if (abig)
            add_diff(ret, DIFF_SAME, big.slice(i, i+small.len()));
        else
            add_diff(ret, DIFF_SAME, small);
        add_diff(ret, abig ? DIFF_DELETE : DIFF_INSERT, big.slice(i + small.len()));
        return ret;
    }

    if (small.len() == 1) {
        auto ret = new_list(Diff);
        add_diff(ret, DIFF_DELETE, a);
        add_diff(ret, DIFF_INSERT, b);
        return ret;
    }

    auto hm = diff_half_match(a, b);
    if (hm) {
        // A half-match was found, sort out the return data.
        auto text1_a = hm->big_prefix;
        auto text1_b = hm->big_suffix;
        auto text2_a = hm->small_prefix;
        auto text2_b = hm->small_suffix;
        auto mid_common = hm->middle;

        // Send both pairs off for separate processing.
        auto diffs_a = diff_main(text1_a, text2_a);
        auto diffs_b = diff_main(text1_b, text2_b);

        auto ret = new_list(Diff);
        For (diffs_a) ret->append(&it);
        add_diff(ret, DIFF_SAME, mid_common);
        For (diffs_b) ret->append(&it);
        return ret;
    }

    return diff_bisect(a, b);
}

List<Diff> *diff_main(DString a, DString b) {
    if (a.equals(b)) {
        auto ret = new_list(Diff);
        if (a.len() > 0)
            add_diff(ret, DIFF_SAME, a);
        return ret;
    }

    auto prefix = diff_common_prefix(a, b);
    a.start += prefix.len();
    b.start += prefix.len();

    auto suffix = diff_common_suffix(a, b);
    a.end -= suffix.len();
    b.end -= suffix.len();

    auto diffs = diff_compute(a, b);

    auto ret = new_list(Diff);
    if (prefix.len() > 0) add_diff(ret, DIFF_SAME, prefix);
    For (diffs) ret->append(&it);
    if (suffix.len() > 0) add_diff(ret, DIFF_SAME, suffix);
    return ret;
}

List<Diff> *diff_bisect(DString a, DString b) {
    auto alen = a.len();
    auto blen = b.len();
    auto max_d = (alen + blen + 1) / 2;

    auto v_off = max_d;
    auto vlen = max_d*2;

    auto v1 = new_array(int, vlen);
    auto v2 = new_array(int, vlen);
    for (int i = 0; i < vlen; i++) {
        v1[i] = -1;
        v2[i] = -1;
    }

    v1[v_off+1] = 0;
    v2[v_off+1] = 0;

    auto delta = alen-blen;
    bool front = (delta % 2 != 0);
    int k1start = 0, k1end = 0, k2start = 0, k2end = 0;

    for (int d = 0; d < max_d; d++) {
        auto range_start = -d + k1start;
        auto range_end = d + 1 - k1end;

        for (int k1 = range_start; k1 < range_end; k1 += 2) {
            int k1_offset = v_off + k1;
            int x1;
            if (k1 == -d || (k1 != d && v1[k1_offset - 1] < v1[k1_offset + 1]))
                x1 = v1[k1_offset + 1];
            else
                x1 = v1[k1_offset - 1] + 1;
            int y1 = x1 - k1;
            while (x1 < alen && y1 < blen && a.get(x1) == b.get(y1)) {
                x1++;
                y1++;
            }
            v1[k1_offset] = x1;
            if (x1 > alen) {
                k1end += 2;
            } else if (y1 > blen) {
                k1start += 2;
            } else if (front) {
                int k2_offset = v_off + delta - k1;
                if (k2_offset >= 0 && k2_offset < vlen && v2[k2_offset] != -1) {
                    int x2 = alen - v2[k2_offset];
                    if (x1 >= x2) {
                        return diff_bisect_split(a, b, x1, y1);
                    }
                }
            }
        }

        range_start = -d + k2start;
        range_end = d + 1 - k2end;

        for (int k2 = range_start; k2 < range_end; k2 += 2) {
            int k2_offset = v_off + k2;
            int x2;
            if (k2 == -d || (k2 != d && v2[k2_offset - 1] < v2[k2_offset + 1]))
                x2 = v2[k2_offset + 1];
            else
                x2 = v2[k2_offset - 1] + 1;
            int y2 = x2 - k2;
            while (x2 < alen && y2 < blen && a.get(alen-x2-1) == b.get(blen-y2-1)) {
                x2++;
                y2++;
            }
            v2[k2_offset] = x2;
            if (x2 > alen) {
                k2end += 2;
            } else if (y2 > blen) {
                k2start += 2;
            } else if (!front) {
                int k1_offset = v_off + delta - k2;
                if (k1_offset >= 0 && k1_offset < vlen && v1[k1_offset] != -1) {
                    int x1 = v1[k1_offset];
                    int y1 = v_off + x1 - k1_offset;
                    x2 = alen - x2;
                    if (x1 >= x2)
                        return diff_bisect_split(a, b, x1, y1);
                }
            }
        }
    }

    auto ret = new_list(Diff);
    add_diff(ret, DIFF_DELETE, a);
    add_diff(ret, DIFF_INSERT, b);
    return ret;
}

List<Diff> *diff_bisect_split(DString a, DString b, int x, int y) {
    auto diffs = diff_main(a.slice(0, x), b.slice(0, y));
    auto diffsb = diff_main(a.slice(x), b.slice(y));
    diffs->concat(diffsb);
    return diffs;
}

DString diff_common_prefix(DString a, DString b) {
  auto alen = a.len();
  auto blen = b.len();

  if (!alen || !blen || a.get(0) != b.get(0))
      return a.slice(0, 0);

  for (int i = 0; i < alen && i < blen; i++)
      if (a.get(i) != b.get(i))
          return a.slice(0, i);
  return alen < blen ? a : b;
}

DString diff_common_suffix(DString a, DString b) {
    auto alen = a.len();
    auto blen = b.len();

    if (!alen || !blen || a.get(-1) != b.get(-1))
        return a.slice(0, 0);

    for (int i = 0; i < alen && i < blen; i++)
        if (a.get(-i-1) != b.get(-i-1))
            return a.slice(a.len() - i);
    return alen < blen ? a : b;
}

Half_Match *diff_half_match(DString a, DString b) {
    DString big = a.len() > b.len() ? a : b;
    DString small = a.len() > b.len() ? b : a;

    if (big.len() < 4 || small.len() * 2 < big.len())
        return NULL;

    auto diff_half_match_i = [](DString big, DString small, int i) -> Half_Match* {
        auto seed = big.slice(i, i + big.len() / 4);

        int j = -1;

        DString best_common = {0};
        DString best_big_a, best_big_b, best_small_a, best_small_b;

        while ((j = index_of(small, seed, j + 1)) != -1) {
            auto plen = diff_common_prefix(big.slice(i), small.slice(j)).len();
            auto slen = diff_common_suffix(big.slice(0, i), small.slice(0, j)).len();

            if (best_common.len() < slen + plen) {
                best_common = small.slice(j - slen, j + plen);
                best_big_a = big.slice(0, i - slen);
                best_big_b = big.slice(i + plen);
                best_small_a = small.slice(0, j - slen);
                best_small_b = small.slice(j + plen);
            }
        }

        if (best_common.len() * 2 < big.len()) return NULL;

        auto ret = new_object(Half_Match);
        ret->big_prefix = best_big_a;
        ret->big_suffix = best_big_b;
        ret->small_prefix = best_small_a;
        ret->small_suffix = best_small_b;
        ret->middle = best_common;
        return ret;
    };

    auto hm1 = diff_half_match_i(big, small, (big.len()+3) / 4);
    auto hm2 = diff_half_match_i(big, small, (big.len()+1) / 2);

    if (!hm1 && !hm2) return NULL;

    Half_Match *hm = NULL;
    if (!hm2)
        hm = hm1;
    else if (!hm1)
        hm = hm2;
    else
        hm = hm1->middle.len() > hm2->middle.len() ? hm1 : hm2;

    if (a.len() <= b.len()) {
        auto tmp = hm->big_prefix;
        hm->big_prefix = hm->small_prefix;
        hm->small_prefix = tmp;

        tmp = hm->big_suffix;
        hm->big_suffix = hm->small_suffix;
        hm->small_suffix = tmp;
    }

    return hm;
}
