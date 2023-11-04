#pragma once

#include <stdlib.h>

void* ts_interop_malloc(size_t size);
void* ts_interop_calloc(size_t x, size_t y);
void* ts_interop_realloc(void *old_mem, size_t new_size);
void ts_interop_free(void *p);
