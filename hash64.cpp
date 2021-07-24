#include "hash64.hpp"
#include "os.hpp"

#if OS_MAC && defined(CPU_ARM64)
#   define XXH_INLINE_ALL
#   include <xxhash.h>
#else
#   include "meow_hash_x64_aesni.h"
#endif

u64 hash64(void *data, s32 len) {
#if OS_MAC && defined(CPU_ARM64)
    return XXH3_64bits(data, len);
#else
    return MeowU64From(MeowHash(MeowDefaultSeed, len, data), 0);
#endif
}
