#include "os.hpp"
#include "world.hpp"

ccstr get_normalized_path(ccstr path) {
    u32 len = get_normalized_path(path, NULL, 0);
    if (len == 0) return NULL;

    auto old = MEM->sp;
    auto ret = alloc_array(char, len + 1);

    if (get_normalized_path(path, ret, len + 1) == 0) {
        MEM->sp = old;
        return NULL;
    }
    return ret;
}

// Mutates and returns path.
cstr normalize_path_separator(cstr path) {
    for (u32 i = 0; path[i] != '\0'; i++)
        if (path[i] == '/' || path[i] == '\\')
            path[i] = PATH_SEP;
    return path;
}

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

bool is_sep(char ch) {
    return ch == '/' || ch == '\\';
}
