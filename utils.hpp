#pragma once

#include "common.hpp"
#include "os.hpp"
#include "pool.hpp"

ccstr our_format_json(ccstr s);
ccstr our_strcpy(ccstr s);
ccstr our_dirname(ccstr path);
ccstr our_basename(ccstr path);
ccstr our_sprintf(ccstr fmt, ...);
ccstr our_strcat(ccstr a, ccstr b);

bool strcpy_safe(cstr buf, s32 count, ccstr src);

Pool *stub_get_mem();

struct Text_Renderer {
    Pool *mem;
    cstr s;
    s32 len;

    void init() {
        mem = stub_get_mem();
        s = (cstr)mem->alloc(0);
        len = 0;
    }

    void *request_memory(s32 n) {
        if (!mem->can_alloc(n)) {
            auto snew = mem->alloc(len + n);
            memcpy(snew, s, len);
            s = (cstr)snew;
            mem->sp -= n;
        }
        return mem->alloc(n);
    }

    void write(ccstr fmt, ...) {
        va_list args, args2;
        va_start(args, fmt);
        va_copy(args2, args);

        auto n = vsnprintf(NULL, 0, fmt, args);
        auto buf = request_memory(n + 1);
        vsnprintf((char*)buf, n + 1, fmt, args2);
        erasechar(); // lop off the '\0' from vsnprintf
        len += n;

        va_end(args);
        va_end(args2);
    }

    void writestr(ccstr s, s32 len = -1) {
        // write("%s", s);
        if (len == -1) len = strlen(s);
        auto buf = request_memory(len);
        strncpy((char*)buf, s, len);
    }

    void writechar(char ch) { *(char*)request_memory(1) = ch; }
    void erasechar() { mem->sp--; }
    cstr finish() { return writechar('\0'), s; }
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
        if (s[len-1] == ',')
            erasechar();
        writechar('}');
    }

    void arr(lambda f) {
        writechar('[');
        f();
        if (s[len-1] == ',')
            erasechar();
        writechar(']');
    }

    void sep() { writechar(','); }
};

const s32 DEFAULT_QUEUE_SIZE = 128;

void *stub_alloc_memory(s32 size);

template <typename T>
struct In_Memory_Queue {
    T *arr;
    s32 queue_base;
    s32 queue_pos;
    s32 cap;
    Lock lock;

    void init(s32 n = DEFAULT_QUEUE_SIZE) {
        ptr0(this);

        arr = (T*)stub_alloc_memory(sizeof(T) * n);
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
};

Path* make_path(ccstr s);
List<ccstr> *split_string(ccstr str, fn<bool(char)> pred);
