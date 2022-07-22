#pragma once

#include "common.hpp"
#include "os.hpp"
#include "mem.hpp"
#include "hash.hpp"
#include <stb/stb_sprintf.h>

ccstr cp_format_json(ccstr s);
ccstr cp_strdup(ccstr s);
ccstr cp_strncpy(ccstr s, int n);

ccstr cp_vsprintf(ccstr fmt, va_list args);
ccstr cp_sprintf(ccstr fmt, ...);
ccstr cp_strcat(ccstr a, ccstr b);

bool cp_strcpy(cstr buf, s32 count, ccstr src);
ccstr str_replace(ccstr s, ccstr find, ccstr replace);

#define cp_strcpy_fixed(x, y) cp_strcpy(x, _countof(x), y)

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

        auto n = stbsp_vsnprintf(NULL, 0, fmt, args);
        chars.ensure_cap(chars.len + n + 1);

        auto buf = chars.items + chars.len;
        stbsp_vsnprintf((char*)buf, n + 1, fmt, args2);
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
        if (v)
            print("warning: prim(void*) is only for sending 'NULL'");
        write("null");
    }

    void field(ccstr key, int value) { field(key, (lambda)([&]() { prim(value); })); }
    void field(ccstr key, float value) { field(key, (lambda)([&]() { prim(value); })); }
    void field(ccstr key, ccstr value) { field(key, (lambda)([&]() { prim(value); })); }
    void field(ccstr key, bool value) { field(key, (lambda)([&]() { prim(value); })); }
    void field(ccstr key, void* value) { field(key, (lambda)([&]() { prim(value); })); }

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

struct Path {
    List<ccstr> *parts;

    void init(List<ccstr> *parts);
    bool contains(Path *path);
    ccstr str(char sep = 0);

    bool goto_parent();
    void goto_child(ccstr child);
};

Path* make_path(ccstr s);
List<ccstr> *split_string(ccstr str, fn<bool(char)> pred);
List<ccstr> *split_string(ccstr str, char sep);
ccstr join_array(List<ccstr> *arr, ccstr glue);
ccstr join_array(List<ccstr> *arr, char glue);
bool path_has_descendant(ccstr base_path, ccstr full_path);

template <typename T>
struct Table {
    struct Table_Entry {
        ccstr name;
        T value;
        UT_hash_handle hh;
    };

    Table_Entry *lookup = NULL;
    Pool *mem;

    void clear() {
        List<ccstr> keys; keys.init();
        auto ents = entries();
        For (*ents) keys.append(it->name);
        For (keys) remove(it);
    }

    List<Table_Entry*> *entries() {
        auto ret = alloc_list<Table_Entry*>(HASH_COUNT(lookup));
        Table_Entry *curr = NULL, *tmp = NULL;
        HASH_ITER(hh, lookup, curr, tmp) ret->append(curr);
        return ret;
    }

    void init() { mem = MEM; }

    void cleanup() {
        SCOPED_MEM(mem);
        HASH_CLEAR(hh, lookup);
    }

    void set(ccstr name, T value) {
        SCOPED_MEM(mem);
        _set_value(name, value);
    }

    void remove(ccstr name) {
        SCOPED_MEM(mem);

        Table_Entry *item = NULL;
        HASH_FIND_STR(lookup, name, item);
        if (!item) return;

        HASH_DEL(lookup, item);
    }

    T get(ccstr name, bool *found = NULL) {
        SCOPED_MEM(mem);

        Table_Entry *item = NULL;
        HASH_FIND_STR(lookup, name, item);

        if (!item) {
            T zero = {0};
            if (found) *found = false;
            return zero;
        }

        if (found) *found = true;
        return item->value;
    }

    void _set_value(ccstr name, T value) {
        SCOPED_MEM(mem);

        Table_Entry *item = NULL;
        HASH_FIND_STR(lookup, name, item);
        if (!item) {
            item = alloc_object(Table_Entry);
            item->name = name;
            HASH_ADD_KEYPTR(hh, lookup, name, strlen(name), item);
        }
        item->value = value;
    }
};

template <typename T>
struct Scoped_Table {
    struct Old_Value {
        ccstr name;
        T value;
        bool dne;
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

    int count() {
        return HASH_COUNT(lookup);
    }

    void clear() {
        List<ccstr> keys; keys.init();
        auto ents = entries();
        For (*ents) keys.append(it->name);
        For (keys) remove(it);
    }

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

        if (!frames.len) return;
        for (auto pos = *frames.last(); old_values.len > pos; old_values.len--) {
            auto old = old_values.last();

            // there was no previous value, delete it
            if (old->dne) {
                Table_Entry *item = NULL;
                HASH_FIND_STR(lookup, old->name, item);
                if (item)
                    HASH_DEL(lookup, item);
            } else {
                _set_value(old->name, old->value);
            }
        }
        frames.len--;
    }

    void set(ccstr name, T value) {
        SCOPED_MEM(mem);

        bool exists = false;
        auto val = get(name, &exists);

        auto old = old_values.append();
        old->name = name;
        old->dne = !exists;
        if (!old->dne)
            old->value = val;

        _set_value(old->name, value);
    }

    void remove(ccstr name) {
        SCOPED_MEM(mem);

        Table_Entry *item = NULL;
        HASH_FIND_STR(lookup, name, item);
        if (!item) return;

        auto old = old_values.append();
        old->name = name;
        old->value = item->value;

        HASH_DEL(lookup, item);
    }

    T get(ccstr name, bool *found = NULL) {
        SCOPED_MEM(mem);

        Table_Entry *item = NULL;
        HASH_FIND_STR(lookup, name, item);

        if (!item) {
            T zero = {0};
            if (found) *found = false;
            return zero;
        }

        if (found) *found = true;
        return item->value;
    }

    void _set_value(ccstr name, T value) {
        SCOPED_MEM(mem);

        Table_Entry *item = NULL;
        HASH_FIND_STR(lookup, name, item);
        if (!item) {
            item = alloc_object(Table_Entry);
            item->name = name;
            HASH_ADD_KEYPTR(hh, lookup, name, strlen(name), item);
        }
        item->value = value;
    }
};

template<typename T>
bool iszero(T* p) {
    auto ptr = (char*)p;
    for (u32 i = 0; i < sizeof(T); i++)
        if (ptr[i])
            return false;
    return true;
}

struct Timer {
    u64 time;
    u64 start;
    ccstr name;
    bool enabled;
    bool *penabled;
    List<char> *log_output;

    void init(ccstr _name, bool *_penabled) {
        ptr0(this);

        name = _name;
        penabled = _penabled;
        time = current_time_nano();
        start = time;
    }

    void init(ccstr _name, bool _enabled) {
        ptr0(this);

        name = _name;
        enabled = _enabled;
        time = current_time_nano();
        start = time;
    }

    void init() { init(NULL, true); }
    void init(ccstr _name) { init(_name, true); }

    ccstr make_label(ccstr s) {
        if (!name)
            return s;
        return cp_sprintf("[%s] %s", name, s);
    }

    bool is_enabled() {
        return penabled ? *penabled : enabled;
    }

    void output(ccstr s) {
        if (!is_enabled()) return;

        if (!log_output) {
            print("%s", s);
            return;
        }

        for (char *p = (char*)s; *p; p++)
            log_output->append(*p);
        log_output->append('\n');
    }

    void logf(ccstr fmt, ...) {
        va_list args;
        va_start(args, fmt);
        log(cp_vsprintf(fmt, args));
        va_end(args);
    }

    void log(ccstr s) {
        output(cp_sprintf("%s: %.4fms", make_label(s), read_time() / 1000000.f));
    }

    i64 read_time() {
        auto curr = current_time_nano();
        auto ret = curr - time;
        time = curr;
        return ret;
    }

    i64 read_total() {
        auto curr = current_time_nano();
        auto ret = curr - start;
        time = curr;
        return ret;
    }

    void total() {
        output(cp_sprintf("%s: %.4fms", make_label("TOTAL"), read_total() / 1000000.f));
    }
};

template<typename T>
struct Message_Queue {
    Pool mem;
    Lock lock;
    List<T> messages;

    void init() {
        mem.init();
        lock.init();
        messages.init();
    }

    List<T> *start() {
        lock.enter();
        return &messages;
    }

    void softend() {
        lock.leave();
    }

    void end() {
        mem.reset();
        messages.len = 0;
        lock.leave();
    }

    void add(fn<void(T *t)> f) {
        SCOPED_LOCK(&lock);
        SCOPED_MEM(&mem);
        f(messages.append());
    }
};

template<typename T>
bool isempty(List<T> *arr) {
    return !arr || !arr->len;
}
