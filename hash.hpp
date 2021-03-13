#include "uthash.h"
#include "mem.hpp"

void *uthash_malloc_stub(s32 size);

#undef uthash_malloc
#undef uthash_free
#define uthash_malloc(sz) uthash_malloc_stub(sz)
#define uthash_free(ptr, sz)


