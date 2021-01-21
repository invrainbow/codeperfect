#pragma once

#include "common.hpp"
#include "list.hpp"

#define DEFAULT_BUCKET_SIZE 65536

struct Pool_Block {
    u8 *base;
    s32 size;
};

struct Pool {
    List<Pool_Block*> obsolete_blocks;
    List<Pool_Block*> used_blocks;
    List<Pool_Block*> unused_blocks;
    Pool_Block *curr;
    s32 sp;
    s32 blocksize;
    ccstr name;

    void init(ccstr _name) {
        name = _name;
        blocksize = DEFAULT_BUCKET_SIZE;
        request_new_block();
    }

    void request_new_block() {
        if (curr != NULL)
            used_blocks.append(&curr);

        if (unused_blocks.len > 0) {
            curr = unused_blocks[unused_blocks.len - 1];
            unused_blocks.len--;
        } else {
            curr = (Pool_Block*)malloc(sizeof(Pool_Block) + blocksize);
        }

        sp = 0;
        curr->size = blocksize;
        curr->base = (u8*)curr + sizeof(Pool_Block);
    }

    void restore(Pool_Block *block, s32 pos) {
        // if this was a previous used block, just reset pos back to beginning
        sp = (block == curr ? pos : 0);
    }

    void ensure_enough(s32 n) {
        if (can_alloc(n)) return;

        auto bs = blocksize;
        while (n > bs) bs *= 2;
        if (bs > blocksize) {
            blocksize = bs;
            if (curr != NULL) obsolete_blocks.append(curr);
            For (used_blocks) obsolete_blocks.append(it);

            used_blocks.len = 0;
            curr = NULL;
        }
        request_new_block();
    }

    bool can_alloc(s32 n) {
        return (curr != NULL) && (sp + n <= curr->size);
    }

    void *alloc(s32 n) {
        ensure_enough(n);
        auto ret = curr->base + sp;
        sp += n;
        return ret;
    }

    void cleanup() {
        For (unused_blocks) free(it);
        For (used_blocks) free(it);
        For (obsolete_blocks) free(it);
        if (curr != NULL) free(curr);
    }

    void reset() {
        if (curr != NULL) {
            unused_blocks.append(curr);
            curr = NULL;
        }

        For (used_blocks) unused_blocks.append(it);
        used_blocks.len = 0;

        For (obsolete_blocks) free(it);
        obsolete_blocks.len = 0;
    }
};

