// The Tree-sitter library can be built by compiling this one source file.
//
// The following directories must be added to the include path:
//   - include

#define _POSIX_C_SOURCE 200112L

void* ts_interop_malloc(size_t size);
void* ts_interop_calloc(size_t x, size_t y);
void* ts_interop_realloc(void *old_mem, size_t new_size);
void ts_interop_free(void *p);

#define ts_malloc ts_interop_malloc
#define ts_calloc ts_interop_calloc
#define ts_realloc ts_interop_realloc
#define ts_free ts_interop_free

#include "./get_changed_ranges.c"
#include "./language.c"
#include "./lexer.c"
#include "./node.c"
#include "./parser.c"
#include "./query.c"
#include "./stack.c"
#include "./subtree.c"
#include "./tree_cursor.c"
#include "./tree.c"
#include "./go.c"
