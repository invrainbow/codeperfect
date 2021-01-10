#include "os.hpp"
#include "world.hpp"

ccstr get_normalized_path(ccstr path) {
  u32 len = get_normalized_path(path, NULL, 0);
  if (len == 0)
    return NULL;

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
