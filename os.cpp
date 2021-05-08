#include "os.hpp"
#include "world.hpp"
#include <filesystem>

ccstr get_normalized_path(ccstr path) {
    u32 len = get_normalized_path(path, NULL, 0);
    if (len == 0) return NULL;

    Frame frame;
    auto ret = alloc_array(char, len + 1);

    if (get_normalized_path(path, ret, len + 1) == 0) {
        frame.restore();
        return NULL;
    }
    return ret;
}

ccstr normalize_path_sep(ccstr path, char sep) {
    if (sep == 0) sep = PATH_SEP;

    auto len = strlen(path);
    auto ret = alloc_array(char, len+1);

    for (u32 i = 0; i < len; i++)
        ret[i] = is_sep(path[i]) ? sep : path[i];
    ret[len] = '\0';
    return ret;
}

#if 0
Entire_File *read_entire_file(ccstr path) {
    auto f = fopen(path, "rb");
    if (f == NULL) return NULL;
    defer { fclose(f); };

    fseek(f, 0, SEEK_END);
    auto len = ftell(f);
    fseek(f, 0, SEEK_SET);

    auto data = malloc(len);
    if (data == NULL) return NULL;

    if (fread(data, len, 1, f) != 1) {
        free(data);
        return NULL;
    }

    auto ret = alloc_object(Entire_File);
    ret->data = data;
    ret->len = len;
    return ret;
}

void free_entire_file(Entire_File *file) {
    free(file->data);
}
#endif

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
