#include "utils.hpp"
#include "os.hpp"
#include "world.hpp"
#include "stb_sprintf.h"

#if OS_WIN
#include <shlwapi.h>
#elif OS_MAC
#include <libgen.h>
#endif

bool strcpy_safe(cstr buf, s32 count, ccstr src) {
    auto len = strlen(src);
    if (count < len + 1) return false;
    strncpy(buf, src, len + 1);
    return true;
}

ccstr our_format_json(ccstr s) {
    Process proc;

    proc.init();
    defer { proc.cleanup(); };

    proc.use_stdin = true;
    proc.run("jq .");

    for (int i = 0; s[i] != '\0'; i++)
        proc.write1(s[i]);
    proc.done_writing();

    Text_Renderer r;
    r.init();
    char ch;
    while (proc.read1(&ch)) r.writechar(ch);
    return r.finish();
}

ccstr our_strcpy(ccstr s) {
    if (s == NULL) return NULL;

    auto len = strlen(s);
    auto ret = alloc_array(char, len + 1);
    memcpy(ret, s, sizeof(char) * (len + 1));
    return (ccstr)ret;
}

ccstr our_strncpy(ccstr s, int n) {
    if (s == NULL) return NULL;

    auto ret = alloc_array(char, n + 1);
    memcpy(ret, s, sizeof(char) * n);
    ret[n] = '\0';
    return (ccstr)ret;
}

// why isn't this in os.hpp, btw? (along with our_basename)
ccstr _our_dirname(ccstr path) {
#if OS_WIN
    auto s = (char*)our_strcpy(path);
    auto len = strlen(s);
    if (is_sep(s[len-1]))
        s[len-1] = '\0';

    auto ret = alloc_array(char, strlen(s) + 1);
    _splitpath(s, ret, NULL, NULL, NULL);
    _splitpath(s, NULL, ret + strlen(ret), NULL, NULL);
    return ret;
#elif OS_MAC
    // dirname might overwrite mem we pass it
    // and also it might return pointer to statically allocated mem
    // so just send it a copy and copy what it gives us
    return our_strcpy(dirname((char*)our_strcpy(path)));
#endif
}

ccstr our_dirname(ccstr path) {
    auto ret = _our_dirname(path);
    if (streq(ret, ".")) ret = "";
    return ret;
}

ccstr our_basename(ccstr path) {
#if OS_WIN
    auto ret = (cstr)our_strcpy(path);
    PathStripPathA(ret);
    return (ccstr)ret;
#elif OS_MAC
    auto ret = our_strcpy(path);
    return basename((char*)our_strcpy(ret));
#endif
}

ccstr our_vsprintf(ccstr fmt, va_list args) {
    va_list args2;
    va_copy(args2, args);

    auto len = stbsp_vsnprintf(NULL, 0, fmt, args);
    auto buf = alloc_array(char, (len + 1));
    stbsp_vsnprintf(buf, len + 1, fmt, args2);
    return buf;
}

ccstr our_sprintf(ccstr fmt, ...) {
    va_list args;
    va_start(args, fmt);

    auto buf = our_vsprintf(fmt, args);

    va_end(args);
    return buf;
}

ccstr our_strcat(ccstr a, ccstr b) {
    // TODO: do this properly lol
    return our_sprintf("%s%s", a, b);
}

List<ccstr> *split_string(ccstr str, char sep) {
    auto pred = [&](char ch) -> bool { return ch == sep; };
    return split_string(str, pred);
}

List<ccstr> *split_string(ccstr str, fn<bool(char)> pred) {
    auto len = strlen(str);
    u32 start = 0;
    auto ret = alloc_list<ccstr>();

    while (start < len) {
        u32 end = start;
        while (end < len && !pred(str[end])) end++;

        auto partlen = end-start;
        auto s = alloc_array(char, partlen+1);
        strncpy(s, str + start, partlen);
        s[partlen] = '\0';
        ret->append(s);

        start = end+1;
    }
    return ret;
}

Path* make_path(ccstr s) {
    auto pred = [&](char ch) -> bool { return is_sep(ch); };
    auto parts = split_string(s, pred);

    auto path = alloc_object(Path);
    path->init(parts);
    return path;
}

void Path::init(List<ccstr> *_parts) {
    ptr0(this);
    parts = _parts;
}

// Check if `this` contains `other`, e.g. this = "a/b/c" and other =
// "a/b/c/d/e".
bool Path::contains(Path *other) {
    if (parts->len > other->parts->len) return false;

    for (u32 i = 0; i < parts->len; i++) {
        auto x = parts->at(i);
        auto y = other->parts->at(i);
        if (!streqi(x, y)) return false;
    }
    return true;
}

bool Path::goto_parent() {
    if (parts->len == 0) return false;
    parts->len--;
    return true;
}

void Path::goto_child(ccstr child) {
    parts->append(child);
}

ccstr Path::str(char sep) {
    if (parts->len == 0) return "";

    if (sep == 0) sep = PATH_SEP;

    auto len = 0;
    For (*parts) len += strlen(it);
    len += parts->len - 1;

    auto ret = alloc_array(char, len+1);
    u32 k = 0;

    for (u32 i = 0; i < parts->len; i++) {
        auto it = parts->at(i);

        auto len = strlen(it);
        memcpy(ret + k, it, len);
        k += len;

        if (i + 1 < parts->len)
            ret[k++] = sep;
    }

    ret[k] = '\0';
    return ret;
}

bool path_contains_in_subtree(ccstr base_path, ccstr full_path) {
    SCOPED_FRAME();
    return make_path(base_path)->contains(make_path(full_path));
}

// this is slow, but whatever
ccstr str_replace(ccstr s, ccstr find, ccstr replace) {
    int i = 0, len = strlen(s);
    int flen = strlen(find);
    int rlen = strlen(replace);
    auto ret = alloc_list<char>();

    while (i < len) {
        if (str_starts_with(&s[i], find)) {
            for (int j = 0; j < rlen; j++)
                ret->append(replace[j]);
            i += flen;
            continue;
        }
        ret->append(s[i]);
        i++;
    }

    ret->append('\0');
    return ret->items;
}
