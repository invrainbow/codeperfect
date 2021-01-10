#include "list.hpp"
#include "world.hpp"

uchar* alloc_chunk_stub(s32 needed, s32* new_size) {
  return alloc_chunk(needed, new_size);
}

void free_chunk_stub(uchar* buf, s32 cap) {
  return free_chunk(buf, cap);
}

Stack* get_current_mem_stub() {
  return MEM;
}
