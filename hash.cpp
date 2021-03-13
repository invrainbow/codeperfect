#include "common.hpp"
#include "mem.hpp"

void *uthash_malloc_stub(s32 size) {
    return alloc_memory(size);
}
