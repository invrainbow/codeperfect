#include "utils.hpp"
#include "os.hpp"
#include "world.hpp"
#include <stb/stb_sprintf.h>
#include "defer.hpp"

bool cp_strcpy(cstr buf, s32 count, ccstr src) {
    auto len = strlen(src);
    if (count < len + 1) return false;
    strncpy(buf, src, len + 1);
    return true;
}

// TODO: can probably do this in gohelper
ccstr cp_format_json(ccstr s) {
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

ccstr cp_strdup(ccstr s) {
    if (!s) return NULL;

    auto len = strlen(s);
    auto ret = new_array(char, len + 1);
    memcpy(ret, s, sizeof(char) * (len + 1));
    return (ccstr)ret;
}

ccstr cp_strncpy(ccstr s, int n) {
    if (!s) return NULL;

    auto ret = new_array(char, n + 1);
    memcpy(ret, s, sizeof(char) * n);
    ret[n] = '\0';
    return (ccstr)ret;
}

ccstr cp_vsprintf(ccstr fmt, va_list args) {
    va_list args2;
    va_copy(args2, args);

    auto len = stbsp_vsnprintf(NULL, 0, fmt, args);
    auto buf = new_array(char, (len + 1));
    stbsp_vsnprintf(buf, len + 1, fmt, args2);

    va_end(args2);
    return buf;
}

ccstr cp_sprintf(ccstr fmt, ...) {
    va_list args;
    va_start(args, fmt);

    auto buf = cp_vsprintf(fmt, args);

    va_end(args);
    return buf;
}

ccstr cp_strcat(ccstr a, ccstr b) {
    int la = strlen(a);
    int lb = strlen(b);

    auto ret = new_array(char, la+lb+1);
    memcpy(ret+0, a, la);
    memcpy(ret+la, b, lb);
    ret[la+lb] = '\0';
    return ret;
}

List<ccstr> *split_string(ccstr str, char sep) {
    auto pred = [&](char ch) -> bool { return ch == sep; };
    return split_string(str, pred);
}

List<ccstr> *split_string(ccstr str, fn<bool(char)> pred) {
    auto len = strlen(str);
    u32 start = 0;
    auto ret = new_list(ccstr);

    while (start < len) {
        u32 end = start;
        while (end < len && !pred(str[end])) end++;

        auto partlen = end-start;
        auto s = new_array(char, partlen+1);
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

    auto path = new_object(Path);
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
    if (!parts->len) return false;
    parts->len--;
    return true;
}

void Path::goto_child(ccstr child) {
    parts->append(child);
}

ccstr join_array(List<ccstr> *arr, char glue) {
    char gluestr[] = {glue, '\0'};
    return join_array(arr, gluestr);
}

ccstr join_array(List<ccstr> *arr, ccstr glue) {
    if (!arr->len) return "";

    auto ret = new_list(char);

    auto append_str = [&](ccstr s) {
        for (auto p = s; *p; p++)
            ret->append(*p);
    };

    Fori (arr) {
        if (i) append_str(glue);
        append_str(it);
    }

    ret->append('\0');
    return ret->items;
}

ccstr Path::str(char sep) {
    return join_array(parts, sep ? sep : PATH_SEP);
}

bool path_has_descendant(ccstr base_path, ccstr full_path) {
    SCOPED_FRAME();
    return make_path(base_path)->contains(make_path(full_path));
}

// this is slow, but whatever
ccstr str_replace(ccstr s, ccstr find, ccstr replace) {
    int i = 0, len = strlen(s);
    int flen = strlen(find);
    int rlen = strlen(replace);
    auto ret = new_list(char);

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

int binary_search(void *list, s32 num, s32 size, bs_test_func test) {
    int lo = 0, hi = num-1;
    while (lo <= hi) {
        auto mid = (lo+hi)/2;
        auto curr = (void*)((char*)list + mid*size);
        auto result = test(curr);

        if (result > 0)
            lo = mid + 1;
        else if (result < 0)
            hi = mid - 1;
        else
            return mid;
    }
    return -1;
}

ccstr read_file(ccstr filepath) {
    auto fm = map_file_into_memory(filepath);
    if (!fm) return NULL;
    defer { fm->cleanup(); };

    auto ret = new_list(char);
    ret->concat((char*)fm->data, fm->len);
    ret->append('\0');
    return ret->items;
}

bool write_file(ccstr filepath, ccstr s) {
    File f;
    if (!f.init_write(filepath)) return false;
    defer { f.cleanup(); };

    return f.write(s, strlen(s));
}

void *alloc_table_stub(int size) {
    return alloc_mem(size);
}
