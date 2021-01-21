#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

#include <stdexcept>
#include <functional>

// tools for macros
#define TOKENPASTE0(a, b) a##b
#define TOKENPASTE(a, b) TOKENPASTE0(a, b)
#define GENSYM(a) TOKENPASTE(a, __LINE__)

template <typename T> using fn = std::function<T>;

typedef fn<void()> lambda;

// defer macro
template <typename F> struct Defer {
    Defer(F f) : f(f) {};
    ~Defer() { f(); }
    F f;
};
template <typename F> Defer<F> make_defer(F f) { return Defer<F>(f); };
struct defer_dummy {};
template <typename F> Defer<F> operator+(defer_dummy, F&& f) {
    return make_defer<F>(f);
}
#define defer auto GENSYM(defer) = defer_dummy() + [&]()

struct Run_Function {};
template <typename F> int operator+(Run_Function, F&& f) {
    f();
    return 0;
};

#define run_before_main int GENSYM(run_before_main) = Run_Function() + []()

// typedefs & aliases
typedef size_t s32;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef uintptr_t uptr;
typedef intptr_t iptr;
typedef char* cstr;
typedef const char* ccstr;
typedef u32 uchar;
typedef uchar* ustr;
typedef const uchar* custr;

#ifndef _countof
#define _countof(x) (sizeof(x) / sizeof(*x))
#endif

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#define _offsetof(type, field) ((size_t)&(((type *)0)->field))

inline u32 max(u32 a, u32 b) { return a > b ? a : b; }
inline u32 min(u32 a, u32 b) { return a < b ? a : b; }
inline u32 relu_sub(u32 a, u32 b) { return a < b ? 0 : a - b; }

enum ChunkSize {
    CHUNK0 = 20,
    CHUNK1 = 100,
    CHUNK2 = 500,
    CHUNK3 = 2000,
    CHUNK4 = 5000,
    CHUNK5 = 10000,
    CHUNK6 = 20000,
};

typedef uchar Chunk0[CHUNK0];
typedef uchar Chunk1[CHUNK1];
typedef uchar Chunk2[CHUNK2];
typedef uchar Chunk3[CHUNK3];
typedef uchar Chunk4[CHUNK4];
typedef uchar Chunk5[CHUNK5];
typedef uchar Chunk6[CHUNK6];

struct vec2f;

struct vec2 {
    i32 x;
    i32 y;

    vec2 operator+(vec2 v) {
        vec2 ret;
        ret.x = x + v.x;
        ret.y = y + v.y;
        return ret;
    }

    vec2 operator-(vec2 v) {
        vec2 ret;
        ret.x = x - v.x;
        ret.y = y - v.y;
        return ret;
    }

    operator vec2f();
};

struct cur2 : vec2 {
    i32 cmp(cur2 b) {
        if (y < b.y)
            return -1;
        if (y > b.y)
            return 1;
        if (x == b.x)
            return 0;
        return (x < b.x ? -1 : 1);
    }

    bool operator<(cur2 b) { return cmp(b) < 0; }
    bool operator<=(cur2 b) { return cmp(b) <= 0; }
    bool operator>(cur2 b) { return cmp(b) > 0; }
    bool operator>=(cur2 b) { return cmp(b) >= 0; }
    bool operator==(cur2 b) { return cmp(b) == 0; }
    bool operator!=(cur2 b) { return cmp(b) != 0; }
};

struct vec3 {
    union {
        struct {
            i32 r;
            i32 g;
            i32 b;
        };

        struct {
            i32 x;
            i32 y;
            i32 z;
        };

        struct {
            vec2 xy;
            i32 _ignored0;
        };

        struct {
            i32 _ignored1;
            vec2 yz;
        };
    };

    cstr str();
};

struct vec2f {
    float x;
    float y;

    cstr str();

    vec2f operator+(vec2f v) {
        vec2f ret;
        ret.x = x + v.x;
        ret.y = y + v.y;
        return ret;
    }

    vec2f operator-(vec2f v) {
        vec2f ret;
        ret.x = x - v.x;
        ret.y = y - v.y;
        return ret;
    }

    operator vec2() {
        vec2 ret;
        ret.x = (i32)x;
        ret.y = (i32)y;
        return ret;
    }
};

struct cur2f : vec2f {
    i32 cmp(cur2 b) {
        if (y < b.y)
            return -1;
        if (y > b.y)
            return 1;
        if (x == b.x)
            return 0;
        return (x < b.x ? -1 : 1);
    }

    bool operator<(cur2 b) { return cmp(b) < 0; }
    bool operator<=(cur2 b) { return cmp(b) <= 0; }
    bool operator>(cur2 b) { return cmp(b) > 0; }
    bool operator>=(cur2 b) { return cmp(b) >= 0; }
    bool operator==(cur2 b) { return cmp(b) == 0; }
    bool operator!=(cur2 b) { return cmp(b) != 0; }
};

struct vec3f {
    union {
        struct {
            float x;
            float y;
            float z;
        };

        struct {
            float r;
            float g;
            float b;
        };

        struct {
            vec2f xy;
            float _ignored0;
        };

        struct {
            float _ignored1;
            vec2f yz;
        };
    };

    cstr str();
};

struct vec4f {
    union {
        struct {
            float x;
            float y;
            float z;
            float w;
        };

        struct {
            float r;
            float g;
            float b;
            float a;
        };

        struct {
            vec2f xy;
            float _ignored0;
            float _ignored1;
        };

        struct {
            float _ignored2;
            vec2f yz;
            float _ignored3;
        };

        struct {
            float _ignored4;
            float _ignored5;
            vec2f zw;
        };

        struct {
            vec3f xyz;
            float _ignored6;
        };

        struct {
            float _ignored7;
            vec3f yzw;
        };

        struct {
            vec3f rgb;
            float _ignored8;
        };
    };

    cstr str();
};

struct box {
    union {
        struct {
            i32 x;
            i32 y;
            i32 w;
            i32 h;
        };
        struct {
            vec2 pos;
            vec2 size;
        };
    };

    cstr str();
};

struct boxf {
    union {
        struct {
            float x;
            float y;
            float w;
            float h;
        };
        struct {
            vec2f pos;
            vec2f size;
        };
    };

    cstr str();
    bool contains(vec2f point);
};

vec2 new_vec2(i32 x, i32 y);
vec2 new_vec2(u32 x, u32 y);
cur2 new_cur2(i32 x, i32 y);
cur2 new_cur2(u32 x, u32 y);
vec2f new_vec2f(float x, float y);
vec3f new_vec3f(float x, float y, float z);
// TODO: add others as needed

typedef float mat3f[9];
typedef float mat4f[16];

struct Panic_Exception : std::runtime_error {
    Panic_Exception(ccstr error) : std::runtime_error(error) {}
};

void _error(ccstr fmt, ...);

// convenience macros
#define streq(a, b) (strcmp(a, b) == 0)
#define strneq(a, b, n) (strncmp(a, b, n) == 0)
#define print(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#define error(fmt, ...) _error("error: " fmt "\n", ##__VA_ARGS__)
#define panic(err) throw Panic_Exception(err)
#define mem0(ptr, n) memset(ptr, 0, n)
#define ptr0(ptr) memset(ptr, 0, sizeof(*ptr))
#define For(arr) for (auto &&it : arr)
#define define_str_case(x) case x: return #x

void* our_malloc(size_t size);
void our_free(void* p);
bool str_ends_with(ccstr a, ccstr suf);
bool str_starts_with(ccstr a, ccstr pre);

template <typename T> struct Fridge {
    // An element is the object we want to allocate.
    // We use that same space to store next pointers, to save space.
    // Game Engine Architecture, 5.2.1.2. Pool Allocators.
    //
    // We call this a fridge allocator; our pool allocator is something else (see
    // `struct Pool`).

    union Elem {
        T obj;
        Elem* next;
    };

    bool managed;
    Elem* head;
    u32 size;

    void init_with_data(T* data, s32 n, s32 new_size) {
        head = (Elem*)data;
        for (i32 i = 0; i < n - 1; i++)
            head[i].next = &head[i + 1];
        head[n - 1].next = NULL;
        size = new_size;
    }

    void init_unmanaged(T* data, s32 _size) {
        managed = false;
        init_with_data(data, _size, _size);
    }

    void init_managed(s32 _size) {
        managed = true;

        T* data = (T*)our_malloc(sizeof(T) * _size);
        if (data == NULL)
            panic("our_malloc failed for Pool::init_managed");
        init_with_data(data, _size, _size);
    }

    T* alloc() {
        if (head == NULL) {
            if (!managed)
                return NULL;

            T* new_data = (T*)our_malloc(sizeof(T) * size);
            if (new_data == NULL)
                panic("our_malloc failed for Pool::alloc");

            init_with_data(new_data, size, size * 2);
            assert(head != NULL);
        }

        T* ret = (T*)head;
        head = head->next;

        mem0(ret, sizeof(T));
        return ret;
    }

    void free(T* obj) {
        Elem* el = (Elem*)obj;
        el->next = head;
        head = el;
    }
};
