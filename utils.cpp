#include "utils.hpp"
#include "os.hpp"
#include "world.hpp"

#if OS_WIN
#include <shlwapi.h>
#elif OS_LINUX
#include <libgen.h>
#endif

bool strcpy_safe(cstr buf, s32 count, ccstr src) {
        auto len = strlen(src);
        if (count < len + 1) return false;
        strncpy(buf, src, len +1);
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
    auto len = strlen(s);
    auto ret = alloc_array(char, len + 1);
    memcpy(ret, s, sizeof(char) * (len + 1));
    return (ccstr)ret;
}

ccstr our_dirname(ccstr path) {
#ifdef _WIN32
    auto old = MEM->sp;
    auto ret = alloc_array(char, strlen(path) + 1);

    _splitpath(path, ret, NULL, NULL, NULL);
    _splitpath(path, NULL, ret + strlen(ret), NULL, NULL);

    MEM->sp = old + strlen(ret) + 1;
    return ret;
#else
    return dirname((char*)our_strcpy(path));
#endif
}

ccstr our_basename(ccstr path) {
#ifdef _WIN32
    auto old = MEM->sp;
    auto ret = (cstr)our_strcpy(path);
    PathStripPathA(ret);
    MEM->sp = old + strlen(ret) + 1;
    return (ccstr)ret;
#else
    auto ret = our_strcpy(path);
    return basename((char*)our_strcpy(ret));
#endif
}

ccstr our_sprintf(ccstr fmt, ...) {
    va_list args, args2;
    va_start(args, fmt);
    va_copy(args2, args);

    auto len = vsnprintf(NULL, 0, fmt, args);
    auto buf = alloc_array(char, (len + 1));

    vsnprintf(buf, len + 1, fmt, args2);

    va_end(args);
    va_end(args2);
    return buf;
}

ccstr our_strcat(ccstr a, ccstr b) {
    // TODO: do this properly lol
    return our_sprintf("%s%s", a, b);
}

void *stub_alloc_memory(s32 size) {
        return alloc_memory(size);
}

Pool *stub_get_mem() {
    return MEM;
}
