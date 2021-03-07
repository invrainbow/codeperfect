#pragma once

#include "common.hpp"
#include "os.hpp"
#include "mem.hpp"
#include "uthash.h"

ccstr our_format_json(ccstr s);
ccstr our_strcpy(ccstr s);
ccstr our_dirname(ccstr path);
ccstr our_basename(ccstr path);
ccstr our_sprintf(ccstr fmt, ...);
ccstr our_strcat(ccstr a, ccstr b);

bool strcpy_safe(cstr buf, s32 count, ccstr src);

struct Text_Renderer {
    List<char> chars;

    void init() {
        ptr0(this);
        chars.init();
    }

    void write(ccstr fmt, ...) {
        va_list args, args2;
        va_start(args, fmt);
        va_copy(args2, args);

        auto n = vsnprintf(NULL, 0, fmt, args);
        chars.ensure_cap(chars.len + n + 1);

        auto buf = chars.items + chars.len;
        vsnprintf((char*)buf, n + 1, fmt, args2);
        chars.len += n;

        va_end(args);
        va_end(args2);
    }

    void writestr(ccstr s, s32 len = -1) { write("%s", s); }
    void writechar(char ch) { chars.append(ch); }
    void erasechar() { chars.len--; }
    cstr finish() { return writechar('\0'), chars.items; }
};

struct Json_Renderer : public Text_Renderer {
    void prim(ccstr s) {
        writechar('"');
        for (ccstr p = s; *p != '\0'; p++) {
            switch (*p) {
                case '\n': write("\\n"); break;
                case '\r': write("\\r"); break;
                case '\t': write("\\t"); break;
                case '"': write("\\\""); break;
                case '\\': write("\\\\"); break;
                default: writechar(*p); break;
            }
        }
        writechar('"');
    }

    void prim(int n) { write("%d", n); }
    void prim(float n) { write("%f", n); }
    void prim(bool b) { write("%s", b ? "true" : "false"); }

    void prim(void* v) {
        if (v != NULL)
            print("warning: prim(void*) is only for sending 'NULL'");
        write("null");
    }

    void field(ccstr key, int value) { field(key, [&]() { prim(value); }); }
    void field(ccstr key, float value) { field(key, [&]() { prim(value); }); }
    void field(ccstr key, ccstr value) { field(key, [&]() { prim(value); }); }
    void field(ccstr key, bool value) { field(key, [&]() { prim(value); }); }
    void field(ccstr key, void* value) { field(key, [&]() { prim(value); }); }

    void field(ccstr key, lambda value) {
        prim(key);
        writechar(':');
        value();
        sep();
    }

    void obj(lambda f) {
        writechar('{');
        f();
        if (chars[chars.len-1] == ',')
            erasechar();
        writechar('}');
    }

    void arr(lambda f) {
        writechar('[');
        f();
        if (chars[chars.len-1] == ',')
            erasechar();
        writechar(']');
    }

    void sep() { writechar(','); }
};

const s32 DEFAULT_QUEUE_SIZE = 128;

template <typename T>
struct In_Memory_Queue {
    T *arr;
    s32 queue_base;
    s32 queue_pos;
    s32 cap;
    Lock lock;

    void init(s32 n = DEFAULT_QUEUE_SIZE) {
        ptr0(this);

        arr = (T*)alloc_memory(sizeof(T) * n);
        cap = n;
        lock.init();
    }

    void cleanup() {
        lock.cleanup();
    }

    s32 increment_index(s32 i) {
        return i + 1 == cap ? 0 : i + 1;
    }

    bool push(T *t) {
        SCOPED_LOCK(&lock);

        auto new_pos = increment_index(queue_pos);
        if (new_pos == queue_base) // we're full!
            return false;

        memcpy(&arr[queue_pos], t, sizeof(T));
        queue_pos = new_pos;
        return true;
    }

    bool pop(T *t) {
        SCOPED_LOCK(&lock);

        if (queue_base == queue_pos) return false;

        memcpy(t, &arr[queue_base], sizeof(T));
        queue_base = increment_index(queue_base);
        return true;
    }
};

struct Path {
    List<ccstr> *parts;

    void init(List<ccstr> *parts);
    bool contains(Path *path);
    ccstr str();

    bool goto_parent();
    void goto_child(ccstr child);
};

Path* make_path(ccstr s);
List<ccstr> *split_string(ccstr str, fn<bool(char)> pred);
List<ccstr> *split_string(ccstr str, char sep);
bool path_contains_in_subtree(ccstr base_path, ccstr full_path);

template <typename T>
struct Scoped_Table {
    struct Old_Value {
        ccstr name;
        T value;
    };

    struct Table_Entry {
        ccstr name;
        T value;
        UT_hash_handle hh;
    };

    List<Old_Value> old_values;
    List<int> frames;
    Table_Entry *lookup = NULL;
    Pool *mem;

    List<Table_Entry*> *entries() {
        auto ret = alloc_list<Table_Entry*>(HASH_COUNT(lookup));
        Table_Entry *curr = NULL, *tmp = NULL;
        HASH_ITER(hh, lookup, curr, tmp) ret->append(curr);
        return ret;
    }

    void init() {
        mem = MEM;
        SCOPED_MEM(mem);

        old_values.init();
        frames.init();
    }

    void cleanup() {
        SCOPED_MEM(mem);

        HASH_CLEAR(hh, lookup);
    }

    void push_scope() {
        SCOPED_MEM(mem);

        frames.append(old_values.len);
    }

    void pop_scope() {
        SCOPED_MEM(mem);

        if (frames.len == 0) return;
        for (auto pos = *frames.last(); old_values.len > pos; old_values.len--) {
            auto old = old_values.last();

            // there was no previous value, delete it
            if (old->value == NULL) {
                Table_Entry *item = NULL;
                HASH_FIND_STR(lookup, old->name, item);
                if (item != NULL)
                    HASH_DEL(lookup, item);
            } else {
                _set_value(old->name, old->value);
            }
        }
        frames.len--;
    }

    void set(ccstr name, T value) {
        SCOPED_MEM(mem);

        auto old = old_values.append();
        old->name = name;
        old->value = get(old->name);

        _set_value(old->name, value);
    }

    T get(ccstr name, bool *found = NULL) {
        SCOPED_MEM(mem);

        Table_Entry *item = NULL;
        HASH_FIND_STR(lookup, name, item);

        if (item == NULL) {
            T zero = {0};
            if (found != NULL) *found = false;
            return zero;
        }

        if (found != NULL) *found = true;
        return item->value;
    }

    void _set_value(ccstr name, T value) {
        SCOPED_MEM(mem);

        Table_Entry *item = NULL;
        HASH_FIND_STR(lookup, name, item);
        if (item == NULL) {
            item = alloc_object(Table_Entry);
            item->name = name;
            HASH_ADD_KEYPTR(hh, lookup, name, strlen(name), item);
        }
        item->value = value;
    }
};
