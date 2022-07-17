#include "os.hpp"
#include "world.hpp"
#include <filesystem>

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

/*
bool list_directory(ccstr path, list_directory_cb_void cb) {
    list_directory(path, [&](Dir_Entry *it) -> bool {
        cb(it);
        return true;
    });
}
*/

Ask_User_Result ask_user_yes_no_cancel(ccstr text, ccstr title, ccstr yeslabel, ccstr nolabel) {
    return ask_user_yes_no(text, title, yeslabel, nolabel, true);
}

#if OS_WINBLOWS
#   define OPEN_COMMAND "start"
#elif OS_MAC
#   define OPEN_COMMAND "open"
#elif OS_LINUX
#   define OPEN_COMMAND "xdg-open"
#endif

void open_webbrowser(ccstr url) {
    // for highlevel stuff like this, shell out to go?
    Process p;
    p.init();
    p.run(cp_sprintf("%s %s", OPEN_COMMAND, url));
    p.cleanup();
}

#undef OPEN_COMMAND

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

