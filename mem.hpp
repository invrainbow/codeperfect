#pragma once

#include "common.hpp"
#include "list.hpp"

struct Fridge_Block {
    Fridge_Block *next;
    s32 size;
};

#define FRIDGE_BLOCK_SIZE 128

template <typename T>
struct Fridge {
    union Elem {
        T obj;
        Elem* next;
    };

    Fridge_Block *blocks;
    Elem* head;
    u32 blocksize;

    void init(s32 bsize) {
        ptr0(this);

        blocksize = bsize;
        add_new_block();
    }

    void cleanup() {
        auto block = blocks;
        while (block) {
            auto next = block->next;
            global_mem_allocated -= block->size;
            cp_free(block);
            block = next;
        }
    }

    void add_new_block() {
        auto next_block = (Fridge_Block*)cp_malloc(sizeof(Fridge_Block) + sizeof(T) * blocksize);
        next_block->size = sizeof(T) * blocksize;
        global_mem_allocated += next_block->size;

        head = (Elem*)((u8*)next_block + sizeof(Fridge_Block));
        for (i32 i = 0; i < blocksize - 1; i++)
            head[i].next = &head[i + 1];
        head[blocksize - 1].next = NULL;

        next_block->next = blocks;
        blocks = next_block;
    }

    T* alloc() {
        if (!head) {
            add_new_block();
            cp_assert(head);
        }

        T* ret = (T*)head;
        head = head->next;
        mem0(ret, sizeof(T));
        return ret;
    }

    void free(T* obj) {
        Elem* el = (Elem*)obj;
        el->next = head;
        head = el;
    }
};

#define POOL_DEFAULT_BUCKET_SIZE 65536
#define POOL_MEMORY_ALIGNMENT 8

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
    u64 mem_allocated;

    bool owns_address(void *addr) {
        auto check_block = [&](Pool_Block *block) -> bool {
            if (!block) return false;

            auto start = (u64)block->base;
            auto end = start + block->size;
            auto x = (u64)addr;
            return start <= x && x < end;
        };

#define CHECK(x) if (check_block(x)) return true

        For (&obsolete_blocks) CHECK(it);
        For (&used_blocks) CHECK(it);
        For (&unused_blocks) CHECK(it);
        CHECK(curr);

#undef CHECK

        return false;
    }

    u64 recount_total_allocated() {
        u64 ret = 0;
        For (&obsolete_blocks) ret += it->size;
        For (&used_blocks) ret += it->size;
        For (&unused_blocks) ret += it->size;
        if (curr) ret += curr->size;
        return ret;
    }

    void init(ccstr _name = NULL, u32 override_blocksize = 0) {
        ptr0(this);

        name = _name;
        blocksize = !override_blocksize ? POOL_DEFAULT_BUCKET_SIZE : override_blocksize;
        obsolete_blocks.init(LIST_MALLOC, 32);
        used_blocks.init(LIST_MALLOC, 32);
        unused_blocks.init(LIST_MALLOC, 32);

        request_new_block();
    }

    Pool_Block *alloc_block(s32 blocksize) {
        auto ret = (Pool_Block*)cp_malloc(sizeof(Pool_Block) + blocksize);
        mem_allocated += blocksize;
        global_mem_allocated += blocksize;
        return ret;
    }

    void free_block(Pool_Block *block) {
        mem_allocated -= block->size;
        global_mem_allocated -= block->size;
        cp_free(block);
    }

    void cleanup() {
        For (&unused_blocks) free_block(it);
        For (&used_blocks) free_block(it);
        For (&obsolete_blocks) free_block(it);
        if (curr) free_block(curr);

        unused_blocks.len = 0;
        used_blocks.len = 0;
        obsolete_blocks.len = 0;
        curr = NULL;
        mem_allocated = 0;

        obsolete_blocks.cleanup();
        used_blocks.cleanup();
        unused_blocks.cleanup();
    }

    void request_new_block() {
        if (curr) {
            used_blocks.append(&curr);
        }

        if (unused_blocks.len > 0) {
            curr = unused_blocks[unused_blocks.len - 1];
            unused_blocks.len--;
        } else {
            curr = alloc_block(blocksize);
        }

        sp = 0;
        curr->size = blocksize;
        curr->base = (u8*)curr + sizeof(Pool_Block);
    }

    void restore(Pool_Block *block, s32 pos) {
        if (block != curr) {
            auto pblock = used_blocks.find([&](auto it) -> bool {
                return *it == block;
            });

            if (!pblock) {
                pblock = obsolete_blocks.find([&](auto it) -> bool {
                    return *it == block;
                });
                cp_assert(pblock);
                // if the block was obsoleted, we're not going to use it,
                // but we *can* just reset all the current used blocks
                if (curr) {
                    unused_blocks.append(curr);
                    curr = NULL;
                }
                For (&used_blocks) unused_blocks.append(it);
                used_blocks.len = 0;
                request_new_block();
            } else {
                auto idx = pblock - used_blocks.items;
                if (curr) unused_blocks.append(curr);
                for (u32 i = idx + 1; i < used_blocks.len; i++)
                    unused_blocks.append(used_blocks[i]);
                used_blocks.len = idx;
                curr = block;
            }
        }
        sp = pos;
    }

    void ensure_enough(s32 n) {
        if (can_alloc(n)) return;

        auto bs = blocksize;
        while (n > bs) bs *= 2;
        if (bs > blocksize) {
            blocksize = bs;
            if (curr) obsolete_blocks.append(curr);
            For (&used_blocks) obsolete_blocks.append(it);
            used_blocks.len = 0;
            For (&unused_blocks) obsolete_blocks.append(it);
            unused_blocks.len = 0;
            curr = NULL;
        }
        request_new_block();
    }

    bool can_alloc(s32 n) {
        return (curr) && (sp + n <= curr->size);
    }

    void *alloc(s32 n) {
        if (n % POOL_MEMORY_ALIGNMENT != 0)
            n += (POOL_MEMORY_ALIGNMENT - (n % POOL_MEMORY_ALIGNMENT));

        ensure_enough(n);
        auto ret = curr->base + sp;
        sp += n;
        return ret;
    }

    void reset() {
        if (curr) {
            unused_blocks.append(curr);
            curr = NULL;
        }

        For (&used_blocks) unused_blocks.append(it);
        used_blocks.len = 0;

        For (&obsolete_blocks) free_block(it);
        obsolete_blocks.len = 0;

        request_new_block();
    }
};

extern thread_local Pool *MEM;
extern thread_local bool use_pool_for_tree_sitter;

void* _alloc_memory(size_t size, bool zero);

#define alloc_memory(n) _alloc_memory(n, true)
#define alloc_array(T, n) (T *)alloc_memory(sizeof(T) * (n))
#define alloc_object(T) alloc_array(T, 1)

template <typename T>
void alloc_list(List<T>* list, s32 len) {
    list->init(LIST_FIXED, len, alloc_array(T, len));
}

template <typename T>
List<T>* alloc_list(s32 len) {
    auto ret = alloc_object(List<T>);
    alloc_list(ret, len);
    return ret;
}

template <typename T>
List<T>* alloc_list() {
    auto ret = alloc_object(List<T>);
    ret->init();
    return ret;
}

struct Frame {
    Pool *pool;
    Pool_Block *block;
    s32 pos;

    void init(Pool *mem) {
        pool = mem;
        block = pool->curr;
        pos = pool->sp;
    }

    Frame(Pool *mem) { init(mem); }
    Frame() { init(MEM); }

    void restore() {
        pool->restore(block, pos);
    }
};

struct Scoped_Frame {
    Frame frame;
    Scoped_Frame() : frame(MEM) {}
    Scoped_Frame(Pool *mem) : frame(mem) {}
    ~Scoped_Frame() { frame.restore(); }
};

#define SCOPED_FRAME() Scoped_Frame GENSYM(SCOPED_FRAME)
#define SCOPED_FRAME_WITH_MEM(mem) Scoped_Frame GENSYM(SCOPED_FRAME)(mem)

struct Scoped_Mem {
    Pool* old;
    Scoped_Mem(Pool *pool) { old = MEM; MEM = pool; }
    ~Scoped_Mem() { MEM = old; }
};

#define SCOPED_MEM(x) Scoped_Mem GENSYM(SCOPED_MEM)(x)

uchar* alloc_chunk(s32 needed, s32* new_size);
void free_chunk(uchar* buf, s32 cap);

extern "C" {
void* ts_interop_malloc(size_t size);
void* ts_interop_calloc(size_t x, size_t y);
void* ts_interop_realloc(void *old_mem, size_t new_size);
void ts_interop_free(void *p);
}
