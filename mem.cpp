#include "mem.hpp"
#include "world.hpp"
#include "common.hpp"

thread_local Pool *MEM;
thread_local bool use_pool_for_tree_sitter = false;

void* _ts_alloc_mem(s32 size, int zero) {
    auto ret = _alloc_mem(size + sizeof(int), (bool)zero);
    *(int*)ret = size;
    return (char*)ret + sizeof(int);
}

void* ts_interop_malloc(size_t size) {
    if (!use_pool_for_tree_sitter) {
        return malloc(size);
    }

    return _ts_alloc_mem(size, 0);
}

void* ts_interop_calloc(size_t x, size_t y) {
    if (!use_pool_for_tree_sitter) {
        return calloc(x, y);
    }
    return _ts_alloc_mem(x * y, 1);
}

void* ts_interop_realloc(void *old_mem, size_t new_size) {
    if (!use_pool_for_tree_sitter) {
        return realloc(old_mem, new_size);
    }

    if (!new_size) return NULL;

    auto new_mem = _ts_alloc_mem(new_size, 0);
    if (old_mem) {
        auto old_size = *((int*)old_mem - 1);
        memcpy(new_mem, old_mem, min(old_size, new_size));
    }
    return new_mem;
}

void ts_interop_free(void *p) {
    if (!use_pool_for_tree_sitter) free(p);
}

void* _alloc_mem(s32 size, bool zero) {
    auto ret = MEM->alloc(size);
    if (zero) mem0(ret, size);
    return ret;
}

uchar* alloc_chunk(s32 needed, s32* new_size) {
    Chunk_Size sizes[] = { CHUNK0, CHUNK1, CHUNK2, CHUNK3, CHUNK4, CHUNK5, CHUNK6, CHUNK7 };

    for (auto size : sizes) {
        if (size < needed) continue;

        *new_size = size;
        switch (size) {
            case CHUNK0: return (uchar*)world.chunk0_fridge.alloc();
            case CHUNK1: return (uchar*)world.chunk1_fridge.alloc();
            case CHUNK2: return (uchar*)world.chunk2_fridge.alloc();
            case CHUNK3: return (uchar*)world.chunk3_fridge.alloc();
            case CHUNK4: return (uchar*)world.chunk4_fridge.alloc();
            case CHUNK5: return (uchar*)world.chunk5_fridge.alloc();
            case CHUNK6: return (uchar*)world.chunk6_fridge.alloc();
            case CHUNK7: return (uchar*)world.chunk7_fridge.alloc();
        }
    }

    return NULL;

    /*
    auto size = (int)sizes[_countof(sizes) - 1]; // last largest size
    while (size < needed) size *= 2;
    *new_size = size;
    return (uchar*)cp_malloc(sizeof(uchar) * size);
    */
}

void free_chunk(uchar* buf, s32 cap) {
    switch (cap) {
        case CHUNK0: world.chunk0_fridge.free((Chunk0*)buf); break;
        case CHUNK1: world.chunk1_fridge.free((Chunk1*)buf); break;
        case CHUNK2: world.chunk2_fridge.free((Chunk2*)buf); break;
        case CHUNK3: world.chunk3_fridge.free((Chunk3*)buf); break;
        case CHUNK4: world.chunk4_fridge.free((Chunk4*)buf); break;
        case CHUNK5: world.chunk5_fridge.free((Chunk5*)buf); break;
        case CHUNK6: world.chunk6_fridge.free((Chunk6*)buf); break;
        case CHUNK7: world.chunk7_fridge.free((Chunk7*)buf); break;
        default: cp_free(buf); break;
    }
}

void memhpp_assert_stub(bool cond) { cp_assert(cond); }

void add_pool(Pool *pool) {
    SCOPED_LOCK(&world.all_pools_lock);

    if (world.all_pools)
        world.all_pools->prev = pool;
    pool->prev = NULL;
    pool->next = world.all_pools;
    world.all_pools = pool;
}

void remove_pool(Pool *pool) {
    SCOPED_LOCK(&world.all_pools_lock);

    for (auto it = world.all_pools; it; it = it->next) {
        if (it == pool) {
            if (it->prev)
                it->prev->next = it->next;
            else
                world.all_pools = it->next;
            if (it->next) it->next->prev = it->prev;
            break;
        }
    }
}
