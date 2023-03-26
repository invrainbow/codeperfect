#include "list.hpp"
#include "os.hpp"
#include "mem.hpp"
#include "utils.hpp"

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

int binary_search_stub(void *list, s32 num, s32 size, bs_stub_test_func test) {
    return binary_search(list, num, size, [&](const void *it) {
        return test(it);
    });
}

NORETURN void panic_stub(ccstr s) {
    cp_panic(s);
}
