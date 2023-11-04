#include "hash64.hpp"
#define XXH_INLINE_ALL
#include "xxhash.h"

u64 hash64(void *data, s32 len) {
    return XXH3_64bits(data, len);
}
