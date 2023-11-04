#include "clibs.h"

#define ts_malloc ts_interop_malloc
#define ts_calloc ts_interop_calloc
#define ts_realloc ts_interop_realloc
#define ts_free ts_interop_free

#include "tree-sitter/lib/src/lib.c"
#include "cwalk.c"
#include "glew.c"
#include "mtwist.c"
