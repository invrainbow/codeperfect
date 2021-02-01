#include "mem.hpp"
#include "world.hpp"
#include "common.hpp"

thread_local Pool *MEM;

void* _alloc_memory(s32 size, bool zero) {
    auto mem = MEM;
    auto ret = mem->alloc(size);
    if (zero) mem0(ret, size);
    return ret;
}

uchar* alloc_chunk(s32 needed, s32* new_size) {
    ChunkSize sizes[] = { CHUNK0, CHUNK1, CHUNK2, CHUNK3, CHUNK4, CHUNK5, CHUNK6 };

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
        }
    }

    return NULL;
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
    }
}
