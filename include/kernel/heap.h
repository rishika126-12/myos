#ifndef HEAP_H
#define HEAP_H

#include "types.h"

void  heap_init(void);
void* kmalloc(size_t size);
void* kcalloc(size_t num, size_t size);
void* krealloc(void* ptr, size_t size);
void  kfree(void* ptr);

size_t heap_get_used_bytes(void);

#endif
