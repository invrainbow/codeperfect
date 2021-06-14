#include "os.hpp"
#include "world.hpp"
#include <filesystem>

ccstr normalize_path_sep(ccstr path, char sep) {
    if (sep == 0) sep = PATH_SEP;

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

ccstr fs_event_type_str(Fs_Event_Type t) {
    switch (t) {
    define_str_case(FSEVENT_CHANGE);
    define_str_case(FSEVENT_DELETE);
    define_str_case(FSEVENT_CREATE);
    define_str_case(FSEVENT_RENAME);
    }
    return NULL;
}

bool are_filepaths_equal(ccstr a, ccstr b) {
    auto a2 = normalize_path_sep(get_canon_path(a));
    auto b2 = normalize_path_sep(get_canon_path(b));

#if FILEPATHS_CASE_SENSITIVE
    return streq(a2, b2);
#else
    return streqi(a2, b2);
#endif
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
