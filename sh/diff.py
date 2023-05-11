# this script lets us test our diff implementation, it's the python source that
# we ported to c

#!/usr/bin/python3

DIFF_DELETE = -1
DIFF_INSERT = 1
DIFF_EQUAL = 0


def format_string(s):
    return s.replace("\n", "\\n")


class Diff:
    def diff_main(self, a, b):
        print('diff_main')

        # Check for null inputs.
        if a == None or b == None:
            raise ValueError("Null inputs. (diff_main)")

        # Check for equality (speedup).
        if a == b:
            if a:
                return [(DIFF_EQUAL, a)]
            return []

        # Trim off common prefix (speedup).
        commonlength = self.diff_commonPrefix(a, b)
        commonprefix = a[:commonlength]
        a = a[commonlength:]
        b = b[commonlength:]

        print(f'common prefix {format_string(commonprefix)}')

        # Trim off common suffix (speedup).
        commonlength = self.diff_commonSuffix(a, b)
        if commonlength == 0:
            commonsuffix = ""
        else:
            commonsuffix = a[-commonlength:]
            a = a[:-commonlength]
            b = b[:-commonlength]

        print(f'common suffix {format_string(commonsuffix)}')

        # Compute the diff on the middle block.
        diffs = self.diff_compute(a, b)

        # Restore the prefix and suffix.
        if commonprefix:
            diffs[:0] = [(DIFF_EQUAL, commonprefix)]
        if commonsuffix:
            diffs.append((DIFF_EQUAL, commonsuffix))
        return diffs

    def diff_compute(self, a, b):
        if not a:
            print('insert b')
            return [(DIFF_INSERT, b)]

        if not b:
            print('delete a')
            return [(DIFF_DELETE, a)]

        if len(a) > len(b):
            print('biga')
            (big, small) = (a, b)
        else:
            (small, big) = (a, b)

        i = big.find(small)
        print(f'i = {i}')
        if i != -1:
            diffs = [
                (DIFF_INSERT, big[:i]),
                (DIFF_EQUAL, small),
                (DIFF_INSERT, big[i + len(small) :]),
            ]
            if len(a) > len(b):
                diffs[0] = (DIFF_DELETE, diffs[0][1])
                diffs[2] = (DIFF_DELETE, diffs[2][1])
            return diffs

        if len(small) == 1:
            print('small.len == 1');
            return [(DIFF_DELETE, a), (DIFF_INSERT, b)]

        hm = self.diff_half_match(a, b)
        print('diff half match: %d' % (hm is None))

        if hm:
            (text1_a, text1_b, text2_a, text2_b, mid_common) = hm
            print('halfmatch %d %d %d %d %d' % (len(text1_a), len(text1_b), len(text2_a), len(text2_b), len(mid_common)))

            diffs_a = self.diff_main(text1_a, text2_a)
            diffs_b = self.diff_main(text1_b, text2_b)
            return diffs_a + [(DIFF_EQUAL, mid_common)] + diffs_b

        print('bisecting')
        return self.diff_bisect(a, b)

    def diff_bisect(self, a, b):
        # Cache the text lengths to prevent multiple calls.
        alen = len(a)
        blen = len(b)
        max_d = (alen + blen + 1) // 2

        print('alen = %d, blen = %d, max_d = %d' % (alen, blen, max_d))

        v_offset = max_d
        v_length = 2 * max_d

        v1 = [-1] * v_length
        v1[v_offset + 1] = 0
        v2 = v1[:]

        delta = alen - blen
        front = delta % 2 != 0

        k1start = 0
        k1end = 0
        k2start = 0
        k2end = 0

        for d in range(max_d):
            for k1 in range(-d + k1start, d + 1 - k1end, 2):
                k1_offset = v_offset + k1
                if k1 == -d or (k1 != d and v1[k1_offset - 1] < v1[k1_offset + 1]):
                    x1 = v1[k1_offset + 1]
                else:
                    x1 = v1[k1_offset - 1] + 1
                y1 = x1 - k1
                while (
                    x1 < alen and y1 < blen and a[x1] == b[y1]
                ):
                    x1 += 1
                    y1 += 1
                v1[k1_offset] = x1
                if x1 > alen:
                    k1end += 2
                elif y1 > blen:
                    k1start += 2
                elif front:
                    k2_offset = v_offset + delta - k1
                    if k2_offset >= 0 and k2_offset < v_length and v2[k2_offset] != -1:
                        x2 = alen - v2[k2_offset]
                        if x1 >= x2:
                            return self.diff_bisectSplit(a, b, x1, y1)

            # Walk the reverse path one step.
            for k2 in range(-d + k2start, d + 1 - k2end, 2):
                k2_offset = v_offset + k2
                if k2 == -d or (k2 != d and v2[k2_offset - 1] < v2[k2_offset + 1]):
                    x2 = v2[k2_offset + 1]
                else:
                    x2 = v2[k2_offset - 1] + 1
                y2 = x2 - k2
                while (x2 < alen and y2 < blen and a[-x2 - 1] == b[-y2 - 1]):
                    x2 += 1
                    y2 += 1
                v2[k2_offset] = x2
                if x2 > alen:
                    k2end += 2
                elif y2 > blen:
                    k2start += 2
                elif not front:
                    k1_offset = v_offset + delta - k2
                    if k1_offset >= 0 and k1_offset < v_length and v1[k1_offset] != -1:
                        x1 = v1[k1_offset]
                        y1 = v_offset + x1 - k1_offset
                        x2 = alen - x2
                        if x1 >= x2:
                            return self.diff_bisectSplit(a, b, x1, y1)

        return [(DIFF_DELETE, a), (DIFF_INSERT, b)]

    def diff_bisectSplit(self, a, b, x, y):
        text1a = a[:x]
        text2a = b[:y]
        text1b = a[x:]
        text2b = b[y:]

        diffs = self.diff_main(text1a, text2a)
        diffsb = self.diff_main(text1b, text2b)
        return diffs + diffsb

    def diff_commonPrefix(self, a, b):
        """Determine the common prefix of two strings.

        Args:
          a: First string.
          b: Second string.

        Returns:
          The number of characters common to the start of each string.
        """
        # Quick check for common null cases.
        if not a or not b or a[0] != b[0]:
            return 0
        # Binary search.
        # Performance analysis: https://neil.fraser.name/news/2007/10/09/
        pointermin = 0
        pointermax = min(len(a), len(b))
        pointermid = pointermax
        pointerstart = 0
        while pointermin < pointermid:
            if a[pointerstart:pointermid] == b[pointerstart:pointermid]:
                pointermin = pointermid
                pointerstart = pointermin
            else:
                pointermax = pointermid
            pointermid = (pointermax - pointermin) // 2 + pointermin
        return pointermid

    def diff_commonSuffix(self, a, b):
        """Determine the common suffix of two strings.

        Args:
          a: First string.
          b: Second string.

        Returns:
          The number of characters common to the end of each string.
        """
        # Quick check for common null cases.
        if not a or not b or a[-1] != b[-1]:
            return 0
        # Binary search.
        # Performance analysis: https://neil.fraser.name/news/2007/10/09/
        pointermin = 0
        pointermax = min(len(a), len(b))
        pointermid = pointermax
        pointerend = 0
        while pointermin < pointermid:
            if (
                a[-pointermid : len(a) - pointerend]
                == b[-pointermid : len(b) - pointerend]
            ):
                pointermin = pointermid
                pointerend = pointermin
            else:
                pointermax = pointermid
            pointermid = (pointermax - pointermin) // 2 + pointermin
        return pointermid

    def diff_half_match(self, a, b):
        """Do the two texts share a substring which is at least half the length of
        the longer text?
        This speedup can produce non-minimal diffs.

        Args:
          a: First string.
          b: Second string.

        Returns:
          Five element Array, containing the prefix of a, the suffix of a,
          the prefix of b, the suffix of b and the common middle.  Or None
          if there was no match.
        """
        if len(a) > len(b):
            (big, small) = (a, b)
        else:
            (small, big) = (a, b)

        if len(big) < 4 or len(small) * 2 < len(big):
            return None  # Pointless.

        def diff_half_match_i(big, small, i):
            seed = big[i : i + len(big) // 4]
            print(f'seed = {format_string(seed)}')

            best_common = ""
            j = small.find(seed)
            while j != -1:
                plen = self.diff_commonPrefix(big[i:], small[j:])
                slen = self.diff_commonSuffix(big[:i], small[:j])
                print(f"plen = {plen}, slen = {slen}")

                if len(best_common) < slen + plen:
                    best_common = (small[j-slen:j] + small[j:j+plen])
                    best_big_a = big[:i-slen]
                    best_big_b = big[i+plen:]
                    best_small_a = small[:j-slen]
                    best_small_b = small[j+plen:]

                j = small.find(seed, j + 1)

            if len(best_common) * 2 < len(big):
                return None
            return best_big_a, best_big_b, best_small_a, best_small_b, best_common

        hm1 = diff_half_match_i(big, small, (len(big) + 3) // 4)
        hm2 = diff_half_match_i(big, small, (len(big) + 1) // 2)

        if not hm1 and not hm2:
            print("not hm1 and not hm2")
            return None

        if not hm2:
            print("hm = hm1")
            hm = hm1
        elif not hm1:
            print("hm = hm2")
            hm = hm2
        else:
            # Both matched.  Select the longest.
            if len(hm1[4]) > len(hm2[4]):
                print("hm = hm1")
                hm = hm1
            else:
                print("hm = hm2")
                hm = hm2

        # A half-match was found, sort out the return data.
        if len(a) > len(b):
            (text1_a, text1_b, text2_a, text2_b, mid_common) = hm
        else:
            (text2_a, text2_b, text1_a, text1_b, mid_common) = hm
        return (text1_a, text1_b, text2_a, text2_b, mid_common)



if __name__ == '__main__':
    diff = Diff()

    old = open("old.go").read()
    new = open("new.go").read()

    diff.diff_main(old, new)
    '''
        if diff[0] == DIFF_INSERT:
            print(" - [insert] |%s|" % format_string(diff[1]))
        if diff[0] == DIFF_DELETE:
            print(" - [delete] |%s|" % format_string(diff[1]))
        if diff[0] == DIFF_EQUAL:
            print(" - [same] |%s|" % format_string(diff[1]))
    '''
