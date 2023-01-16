#include "os.hpp"
#include "world.hpp"
#include <filesystem>
#include <signal.h>
#include "cwalk.h"

#if OS_WINBLOWS
#else
#   include <unistd.h>
#endif

ccstr normalize_path_sep(ccstr path, char sep) {
    if (!sep) sep = PATH_SEP;

    auto len = strlen(path);
    auto ret = alloc_array(char, len+1);

    for (u32 i = 0; i < len; i++)
        ret[i] = is_sep(path[i]) ? sep : path[i];
    ret[len] = '\0';
    return ret;
}

bool is_sep(char ch) {
    return ch == '/' || ch == '\\';
}

bool are_filepaths_equal(ccstr a, ccstr b) {
    auto a2 = normalize_path_sep(get_canon_path(a));
    auto b2 = normalize_path_sep(get_canon_path(b));

    return FILEPATHS_CASE_SENSITIVE ? streq(a2, b2) : streqi(a2, b2);
}

File_Mapping *map_file_into_memory(ccstr path) {
    Frame frame;

    auto fm = alloc_object(File_Mapping);
    if (!fm->init(path)) {
        frame.restore();
        return NULL;
    }

    return fm;
}

File_Mapping *map_file_into_memory(ccstr path, File_Mapping_Opts *opts) {
    Frame frame;

    auto fm = alloc_object(File_Mapping);
    if (!fm->init(path, opts)) {
        frame.restore();
        return NULL;
    }

    return fm;
}

Ask_User_Result ask_user_yes_no_cancel(ccstr text, ccstr title, ccstr yeslabel, ccstr nolabel) {
    return ask_user_yes_no(text, title, yeslabel, nolabel, true);
}

ccstr cp_getcwd() {
    auto ret = alloc_array(char, 256);
    return getcwd(ret, 256);
}

u64 current_time_milli() {
    auto nano = current_time_nano();
    return (u64)(nano / 1000000);
}

// TODO: native?
int get_unix_time() {
    return (int)time(NULL);
}

ccstr cp_dirname(ccstr path) {
    auto ret = _cp_dirname(path);
    if (streq(ret, ".")) ret = "";
    return ret;
}

bool Process::peek(char *out) {
    if (read_buffer_ptr >= read_buffer_len) {
        read_buffer_ptr = 0;
        read_buffer_len = 0;

        int count = readn(read_buffer, _countof(read_buffer));
        if (!count || count == -1) return false;

        read_buffer_len = count;
    }

    *out = read_buffer[read_buffer_ptr];
    return true;
}

bool Process::read1(char* out) {
    if (peek(out)) {
        read_buffer_ptr++;
        return true;
    }
    return false;
}

void install_crash_handlers() {
    signal(SIGSEGV, crash_handler);
    signal(SIGILL, crash_handler);
    signal(SIGABRT, crash_handler);
    signal(SIGFPE, crash_handler);

#if OS_UNIX
    // not supported on windows
    signal(SIGBUS, crash_handler);
#endif
}

ccstr rel_to_abs_path(ccstr path, ccstr cwd) {
    int size = 16;

    if (cwd == NULL) {
        while (true) {
            Frame frame;
            auto newcwd = alloc_array(char, size);
            if (getcwd(newcwd, size)) {
                cwd = newcwd;
                break;
            }
            frame.restore();
            size *= 2;
        }
    }

    int len = cwk_path_get_absolute(cwd, path, NULL, 0);
    if (!len) return NULL;

    Frame frame;
    auto ret = alloc_array(char, len+1);

    if (!cwk_path_get_absolute(cwd, path, ret, len+1)) {
        frame.restore();
        return NULL;
    }

    return ret;
}

