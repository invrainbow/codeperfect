#include "list.hpp"
#include "world.hpp"
#include "pool.hpp"

uchar* alloc_chunk_stub(s32 needed, s32* new_size) {
    return alloc_chunk(needed, new_size);
}

void free_chunk_stub(uchar* buf, s32 cap) {
    return free_chunk(buf, cap);
}

void *get_current_pool_stub() {
    return (void*)MEM;
}

void *alloc_from_pool_stub(void *pool, s32 n) {
    return ((Pool*)pool)->alloc(n);
}
