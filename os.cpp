#include "os.hpp"
#include "world.hpp"
#include "cwalk.h"
#include "defer.hpp"
#if !OS_WINBLOWS
#   include <unistd.h>
#endif

ccstr normalize_path_sep(ccstr path, char sep) {
    if (!sep) sep = PATH_SEP;

    auto len = strlen(path);
    auto ret = new_array(char, len+1);

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

    auto fm = new_object(File_Mapping);
    if (!fm->init(path)) {
        frame.restore();
        return NULL;
    }

    return fm;
}

File_Mapping *map_file_into_memory(ccstr path, File_Mapping_Opts *opts) {
    Frame frame;

    auto fm = new_object(File_Mapping);
    if (!fm->init(path, opts)) {
        frame.restore();
        return NULL;
    }

    return fm;
}

Ask_User_Result ask_user_yes_no_cancel(ccstr text, ccstr title, ccstr yeslabel, ccstr nolabel) {
    return ask_user_yes_no(text, title, yeslabel, nolabel, true);
}

Ask_User_Result ask_user_yes_no(ccstr text, ccstr title, ccstr yeslabel, ccstr nolabel, bool cancel) {
    auto ret = os_ask_user_yes_no(text, title, yeslabel, nolabel, cancel);
    world.message_queue.try_add([&](auto msg) {
        msg->type = MTM_RESET_AFTER_DEFOCUS;
    });
    return ret;
}

void tell_user(ccstr text, ccstr title) {
    os_tell_user(text, title);
    world.message_queue.try_add([&](auto msg) {
        msg->type = MTM_RESET_AFTER_DEFOCUS;
    });
}

bool let_user_select_file(Select_File_Opts* opts) {
    auto ret = os_let_user_select_file(opts);
    world.message_queue.try_add([&](auto msg) {
        msg->type = MTM_RESET_AFTER_DEFOCUS;
    });
    return ret;
}

ccstr cp_getcwd() {
    auto ret = new_array(char, 256);
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

bool Process::can_read() {
    if (read_buffer_ptr < read_buffer_len)
        return true;
    return os_can_read();
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

ccstr rel_to_abs_path(ccstr path, ccstr cwd) {
    int size = 16;

    if (cwd == NULL) {
        while (true) {
            Frame frame;
            auto newcwd = new_array(char, size);
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
    auto ret = new_array(char, len+1);

    if (!cwk_path_get_absolute(cwd, path, ret, len+1)) {
        frame.restore();
        return NULL;
    }

    return ret;
}

// for now just use this for all platforms, but specialize if we start copying
// lots of files (do we ever even do that?)
bool copy_file(ccstr src, ccstr dest) {
    auto fm = map_file_into_memory(src);
    if (!fm) return false;
    defer { fm->cleanup(); };

    // TODO: map this into memory too?
    File f;
    if (f.init_write(dest) != FILE_RESULT_OK)
        return false;
    defer { f.cleanup(); };

    return f.write((char*)fm->data, fm->len);
}

bool move_file_or_directory(ccstr src, ccstr dest) {
    return !rename(src, dest);
}

NORETURN void cp_exit(ccstr s) {
#ifdef DEBUG_BUILD
    BREAK_HERE();
#endif

    if (is_main_thread) {
        tell_user(s, "An error has occurred");
        exit(1);
    } else {
        world.message_queue.try_add([&](auto msg) {
            msg->type = MTM_EXIT;
            msg->exit_message = cp_strdup(s);
            msg->exit_code = 1;
        });
        exit_thread(1);
    }
}

