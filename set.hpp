#pragma once

#include "uthash.h"
#include "list.hpp"

struct String_Set {
    struct Item {
        ccstr name;
        UT_hash_handle hh;
    };

    struct Item *table;
    s32 len;

    List<ccstr> *items() {
        u32 count = HASH_COUNT(table);
        auto ret = alloc_list<ccstr>(count);
        Item *curr, *tmp;

        HASH_ITER(hh, table, curr, tmp) { ret->append(curr->name); }
        return ret;
    }

    void init() {
        ptr0(this);
        table = NULL;
    }

    void cleanup() {
        HASH_CLEAR(hh, table);
    }

    void add(ccstr s) {
        if (has(s)) return;

        len++;
        auto item = alloc_object(Item);
        item->name = s;
        HASH_ADD_KEYPTR(hh, table, s, strlen(s), item);
    }

    void remove(ccstr s) {
        Item *item = NULL;
        HASH_FIND_STR(table, s, item);
        if (item != NULL) {
            HASH_DEL(table, item);
            len--;
        }
    }

    bool has(ccstr s) {
        Item *item = NULL;
        HASH_FIND_STR(table, s, item);
        return (item != NULL);
    }
};


/*
template <typename T>
struct Set {
};
*/

