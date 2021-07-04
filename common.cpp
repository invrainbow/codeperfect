#include "common.hpp"
#include "os.hpp"
#include "world.hpp"

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <assert.h>

#include <stdexcept>
#include <functional>

// #include <windows.h>

const u64 MAX_U64 = (u64)(-1);

vec2 new_vec2(i32 x, i32 y) {
    vec2 v;
    v.x = x;
    v.y = y;
    return v;
}

vec2 new_vec2(u32 x, u32 y) { return new_vec2((i32)x, (i32)y); }

cur2 new_cur2(i32 x, i32 y) {
    cur2 v;
    v.x = x;
    v.y = y;
    return v;
}

cur2 new_cur2(u32 x, u32 y) { return new_cur2((i32)x, (i32)y); }

vec2f new_vec2f(float x, float y) { vec2f v;
    v.x = x;
    v.y = y;
    return v;
}

vec3f new_vec3f(float x, float y, float z) {
    vec3f v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

vec2::operator vec2f() {
    vec2f v;
    v.x = (float)x;
    v.y = (float)y;
    return v;
}

void _error(ccstr fmt, ...) {
    va_list args, args2;
    va_start(args, fmt);
    va_copy(args2, args);

    auto len = vsnprintf(NULL, 0, fmt, args);
    auto buf = (char*)our_malloc(sizeof(char) * (len + 1));
    defer { our_free(buf); };

    vsnprintf(buf, len + 1, fmt, args2);

    va_end(args);
    va_end(args2);

    fprintf(stderr, "%s", buf); // break here
}

void* our_malloc(size_t size) {
    return malloc(size);
}

void our_free(void* p) {
    free(p);
}

bool str_ends_with(ccstr a, ccstr suf) {
    auto na = strlen(a);
    auto nb = strlen(suf);
    return nb <= na && strneq(a + (na - nb), suf, nb);
}

bool str_starts_with(ccstr a, ccstr pre) {
    auto na = strlen(a);
    auto nb = strlen(pre);
    return na >= nb && strneq(a, pre, nb);
}

bool boxf::contains(vec2f point) {
    if (x <= point.x && point.x < x + w)
        if (y <= point.y && point.y < y + h)
            return true;
    return false;
}

s32 global_mem_allocated = 0;

void panic(ccstr s) {
	tell_user(s, "An error has occurred");
	throw Panic_Exception(s); // TODO: replace with exit(0)
}

bool streqi(ccstr a, ccstr b) {
    for(; *a != '\0' || *b != '\0'; a++, b++)
        if (tolower(*a) != tolower(*b))
            return false;
    return true;
}
