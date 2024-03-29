#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

#include <stdexcept>
#include <functional>
#include <utility>

#include <inttypes.h>

#ifdef __GNUC__
#define NORETURN __attribute__((noreturn))
#elif __MINGW32__
#define NORETURN __attribute__((noreturn))
#elif __clang__
#define NORETURN __attribute__((noreturn))
#elif _MSC_VER
#define NORETURN __declspec(noreturn)
#endif

// tools for macros
#define TOKENPASTE0(a, b) a##b
#define TOKENPASTE(a, b) TOKENPASTE0(a, b)
#define GENSYM(a) TOKENPASTE(a, __LINE__)

template <typename T> using fn = std::function<T>;

typedef fn<void()> lambda;

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

#define FALLTHROUGH()

inline u32 max(u32 a, u32 b) { return a > b ? a : b; }
inline u32 min(u32 a, u32 b) { return a < b ? a : b; }
inline u32 relu_sub(u32 a, u32 b) { return a < b ? 0 : a - b; }
inline float relu_subf(float a, float b) { return a < b ? 0 : a - b; }

enum Chunk_Size {
    CHUNK0 = 20,
    CHUNK1 = 100,
    CHUNK2 = 500,
    CHUNK3 = 2000,
    CHUNK4 = 5000,
    CHUNK5 = 10000,
    CHUNK6 = 20000,
    CHUNK7 = 100000, // !!
};

#define CHUNKMAX CHUNK7

typedef uchar Chunk0[CHUNK0];
typedef uchar Chunk1[CHUNK1];
typedef uchar Chunk2[CHUNK2];
typedef uchar Chunk3[CHUNK3];
typedef uchar Chunk4[CHUNK4];
typedef uchar Chunk5[CHUNK5];
typedef uchar Chunk6[CHUNK6];
typedef uchar Chunk7[CHUNK7];

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
    ccstr str();
};

extern const cur2 NULL_CUR;

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

    ccstr str();
};

struct vec2f {
    float x;
    float y;

    ccstr str();

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

    bool equals(vec3f v) {
        return v.x == x && v.y == y && v.z == z;
    }

    bool operator==(vec3f v) { return equals(v); }
    bool operator!=(vec3f v) { return !equals(v); }
    ccstr str();
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

    bool equals(vec4f b) {
        return b.xyz == xyz && b.w == w;
    }

    bool operator==(vec4f b) { return equals(b); }
    bool operator!=(vec4f b) { return !equals(b); }
    ccstr str();
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

    ccstr str();
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

    bool equals(boxf b) {
        if (b.x != x) return false;
        if (b.y != y) return false;
        if (b.w != w) return false;
        if (b.h != h) return false;
        return true;
    }

    bool operator!=(boxf b) { return !equals(b); }
    bool operator==(boxf b) { return equals(b); }

    ccstr str();
    bool contains(vec2f point);
};

vec2 new_vec2(i32 x, i32 y);
cur2 new_cur2(i32 x, i32 y);
vec2f new_vec2f(float x, float y);
vec3f new_vec3f(float x, float y, float z);
// TODO: add others as needed

typedef float mat3f[9];
typedef float mat4f[16];

struct Panic_Exception : std::runtime_error {
    Panic_Exception(ccstr error) : std::runtime_error(error) {}
};

#define strcmpi strcasecmp

void _error(ccstr fmt, ...);

// convenience macros
#define streq(a, b) !strcmp(a, b)
#define streqi(a, b) !strcmpi(a, b)
#define strneq(a, b, n) !strncmp(a, b, n)
#define strneqi(a, b, n) !strncmpi(a, b, n)
#define print(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#define error(fmt, ...) _error("error: " fmt "\n", ##__VA_ARGS__)
#define mem0(ptr, n) memset(ptr, 0, n)
#define ptr0(ptr) memset(ptr, 0, sizeof(*ptr))
#define For(arr) for (auto &&it : *arr)

// https://github.com/therocode/enumerate/blob/master/enumerate.hpp
template <typename L>
struct enumerate {
    using itertype = typename L::iter;
    using reftype = typename L::type&;

    struct iter {
        size_t index;
        itertype value;

        constexpr bool operator!=(const itertype& other) const {
            return value != other;
        }

        constexpr iter& operator++() {
            ++index;
            ++value;
            return *this;
        }

        constexpr std::pair<size_t, reftype> operator*() {
            return std::pair<size_t, reftype>{index, *value};
        }
    };

    L& container;
    constexpr enumerate(L& c): container(c) {}
    constexpr iter begin() { return {0, std::begin(container)}; }
    constexpr itertype end() { return std::end(container); }
};

#define Fori(arr) for (auto &&[i, it] : enumerate(*arr))

#define define_str_case(x) case x: return #x

void* cp_malloc(size_t size);
void cp_free(void* p);
bool str_ends_with(ccstr a, ccstr suf);
bool str_starts_with(ccstr a, ccstr pre);

extern s32 global_mem_allocated;
extern const u64 MAX_U64;

extern thread_local bool is_main_thread;
void assert_main_thread();
