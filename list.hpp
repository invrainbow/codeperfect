#pragma once

#include "common.hpp"
#include <stdlib.h>

enum ListMode {
    LIST_UNINITIALIZED = 0,
    LIST_MALLOC,
    LIST_FIXED,
    LIST_CHUNK,
    LIST_STACK,
};

uchar* alloc_chunk_stub(s32 needed, s32* new_size);
void free_chunk_stub(uchar* buf, s32 cap);
Stack* get_current_mem_stub();

template <typename T>
struct List {
    T* items;
    s32 len;
    s32 cap;
    ListMode mode;
    Stack* stack;    // for LIST_STACK

    void init_with_stack(s32 _cap) {
        stack = get_current_mem_stub();
        T* _items = (T*)stack->alloc(sizeof(T) * _cap);
        init(LIST_STACK, _cap, _items);
    }

    void init(ListMode _mode, s32 _cap, T* _items = NULL) {
        ptr0(this);

        mode = _mode;
        switch (mode) {
            case LIST_MALLOC:
                cap = _cap;
                items = (T*)our_malloc(sizeof(T) * cap);
                if (items == NULL)
                    throw new Oom_Error("unable to our_malloc for array");
                mem0(items, sizeof(T) * cap);
                break;
            case LIST_CHUNK:
                items = (T*)alloc_chunk_stub(_cap, &cap);
                if (items == NULL)
                    throw new Oom_Error("unable to alloc chunk for array");
                break;
            case LIST_FIXED:
                items = _items;
                cap = _cap;
                break;
            case LIST_STACK:
                items = _items;
                cap = _cap;
                break;
        }
    }

    void cleanup() {
        switch (mode) {
            case LIST_MALLOC:
                our_free(items);
                break;
            case LIST_CHUNK:
                free_chunk_stub((uchar*)items, cap);
                break;
            case LIST_FIXED:
                break;
            case LIST_STACK:
                break;
        }

        items = NULL;
        cap = 0;
        len = 0;
    }

    bool append(T* t) {
        auto newitem = append();
        if (newitem != NULL) {
            memcpy(newitem, t, sizeof(T));
            return true;
        }
        return false;
    }

    bool append(T t) { return append(&t); }

    T* append() {
        if (!ensure_cap(len + 1))
            return NULL;
        return items + (len++);
    }

    bool ensure_cap(s32 new_cap) {
        if (cap >= new_cap)
            return true;

        switch (mode) {
            case LIST_MALLOC:
                while (cap < new_cap)
                    cap *= 2;
                items = (T*)realloc(items, sizeof(T) * cap);
                if (items == NULL)
                    return false;
                mem0(items + len, sizeof(T) * (cap - len));
                break;

            case LIST_STACK:
                {
                    while (cap < new_cap)
                        cap *= 2;
                    auto new_items = (T*)stack->alloc(sizeof(T) * cap);
                    if (new_items == NULL)
                        return false;

                    mem0(new_items, sizeof(T) * cap);
                    memcpy(new_items, items, sizeof(T) * len);
                    items = new_items;
                }
                break;

            case LIST_FIXED:
                return false;

            case LIST_CHUNK:
                {
                    s32 chunksize;
                    auto chunk = (T*)alloc_chunk_stub(new_cap, &chunksize);

                    memcpy(chunk, items, sizeof(T) * len);
                    free_chunk_stub((uchar*)items, cap);

                    items = chunk;
                    cap = chunksize;
                }
                break;
        }

        return true;
    }

    void remove(u32 i) {
        if (i >= len)
            return;
        memmove(items + i, items + i + 1, sizeof(T) * (len - i - 1));
        len--;
    }

    typedef fn<bool(T* it)> find_pred;

    i32 find(find_pred f) {
        for (int i = 0; i < len; i++)
            if (f(&items[i]))
                return i;
        return -1;
    }

    T* find_or_append(find_pred f) {
        auto idx = find(f);
        if (idx == -1)
            return append();
        return &items[idx];
    }

    typedef fn<int(T *a, T *b)> cmp_func;

    T *bfind(T *key, cmp_func cmp) {
        return (T*)xplat_binary_search(key, items, len, sizeof(T), [&](const void *a, const void *b) -> int {
            return cmp((T*)a, (T*)b);
        });
    }

    void sort(cmp_func cmp) {
        xplat_quicksort(items, len, sizeof(T), [&](const void *a, const void *b) -> int {
            return cmp((T*)a, (T*)b);
        });
    }

    bool remove(find_pred f) {
        auto idx = find(f);
        if (idx == -1)
            return false;

        remove(idx);
        return true;
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

    iter begin() { return iter(items); }
    iter end() { return iter(items + len); }
    T& operator[](s32 index) { return at(index); }

    // used when we have a pointer to a list and can't do [].
    T& at(s32 index) { return items[index]; }
};
