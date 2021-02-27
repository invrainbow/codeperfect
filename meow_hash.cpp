#include "meow_hash.hpp"

// due to casey being casey, this can only be included once in the whole project
#include "meow_hash_x64_aesni.h"

u64 meow_hash(void *data, s32 len) { 
    return MeowU64From(MeowHash(MeowDefaultSeed, len, data), 0);
}
