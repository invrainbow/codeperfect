#pragma once

#include "common.hpp"
#include "list.hpp"

template <typename T> struct Fridge {
    // An element is the object we want to allocate.
    // We use that same space to store next pointers, to save space.
    // Game Engine Architecture, 5.2.1.2. Pool Allocators.
    //
    // We call this a fridge allocator; our pool allocator is something else (see
    // `struct Pool`).

    union Elem {
        T obj;
        Elem* next;
    };

    bool managed;
    Elem* head;
    u32 size;

    void init_with_data(T* data, s32 n, s32 new_size) {
        head = (Elem*)data;
        for (i32 i = 0; i < n - 1; i++)
            head[i].next = &head[i + 1];
        head[n - 1].next = NULL;
        size = new_size;
    }

    void init_unmanaged(T* data, s32 _size) {
        managed = false;
        init_with_data(data, _size, _size);
    }

    void init_managed(s32 _size) {
        managed = true;

        T* data = (T*)our_malloc(sizeof(T) * _size);
        if (data == NULL)
            panic("our_malloc failed for Pool::init_managed");
        init_with_data(data, _size, _size);
    }

    T* alloc() {
        if (head == NULL) {
            if (!managed)
                return NULL;

            T* new_data = (T*)our_malloc(sizeof(T) * size);
            if (new_data == NULL)
                panic("our_malloc failed for Pool::alloc");

            init_with_data(new_data, size, size * 2);
            assert(head != NULL);
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

    void init(ccstr _name = NULL) {
        ptr0(this);

        name = _name;
        blocksize = DEFAULT_BUCKET_SIZE;
        obsolete_blocks.init(LIST_MALLOC, 32);
        used_blocks.init(LIST_MALLOC, 32);
        unused_blocks.init(LIST_MALLOC, 32);

        request_new_block();
    }

    void cleanup() {
        For (unused_blocks) free(it);
        For (used_blocks) free(it);
        For (obsolete_blocks) free(it);
        if (curr != NULL) free(curr);

        unused_blocks.len = 0;
        used_blocks.len = 0;
        obsolete_blocks.len = 0;
        curr = NULL;
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

    void reset() {
        if (curr != NULL) {
            unused_blocks.append(curr);
            curr = NULL;
        }

        For (used_blocks) unused_blocks.append(it);
        used_blocks.len = 0;

        For (obsolete_blocks) free(it);
        obsolete_blocks.len = 0;

        request_new_block();
    }
};

extern thread_local Pool *MEM;

void* _alloc_memory(s32 size, bool zero);

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
    ret->init(LIST_POOL, 32);
    return ret;
}

struct Frame {
    Pool *pool;
    Pool_Block *block;
    s32 pos;

    Frame() {
        pool = MEM;
        block = pool->curr;
        pos = pool->sp;
    }

    void restore() {
        pool->restore(block, pos);
    }
};

struct Scoped_Frame {
    Frame frame;
    ~Scoped_Frame() { frame.restore(); }
};

#define SCOPED_FRAME() Scoped_Frame GENSYM(SCOPED_FRAME)

struct Scoped_Mem {
    Pool* old;
    Scoped_Mem(Pool *pool) { old = MEM; MEM = pool; }
    ~Scoped_Mem() { MEM = old; }
};

#define SCOPED_MEM(x) Scoped_Mem GENSYM(SCOPED_MEM)(x)

uchar* alloc_chunk(s32 needed, s32* new_size);
void free_chunk(uchar* buf, s32 cap);
