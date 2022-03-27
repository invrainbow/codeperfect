#pragma once

#include "mem.hpp"

struct Str {
    ccstr s;
    u32 len;

    void _copy_string(ccstr source, int len) {
        auto ret = alloc_array(char, len+1);
        memcpy(ret, source, len);
        return ret;
    }

    Str(ccstr known_s, u32 known_len) {
        len = known_len;
        s = _copy_string(source, len);
    }

    Str(ccstr source) {
        len = strlen(s);
        s = _copy_string(source, len);
    }

    Str copy() {
        Str ret = {0};
        ret.len = len;
        ret.s = _copy_string(s, len);
        return ret;
    }

    Str concat(Str b) {
        Str ret = {0};
        ret.len = len + b.len;
        ret.s = alloc_array(char, ret.len + 1);
        memcpy(ret.s, s, len);
        memcpy(ret.s + len, b.s, b.len);
        return ret;
    }

    // Everything below blindly stolen from internet.
    // Makes "for (auto &&it : array)" and "array[i]" work.

    struct iter {
        iter(T* ptr) : ptr_(ptr) {}

        iter operator++() {
            iter i = *this;
            ptr_++;
            return i;
        }

        iter operator++(int junk) {
            ptr_++;
            return *this;
        }

        T& operator*() { return *ptr_; }
        T* operator->() { return ptr_; }
        bool operator==(const iter& rhs) { return ptr_ == rhs.ptr_; }
        bool operator!=(const iter& rhs) { return ptr_ != rhs.ptr_; }

        T* ptr_;
    };

    iter begin() { return iter(s); }
    iter end() { return iter(s + len); }
    T& operator[](s32 index) { return at(index); }

    // used when we have a pointer to a list and can't do [].
    T& at(s32 index) { return s[index]; }
};
