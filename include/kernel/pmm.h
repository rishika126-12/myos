#ifndef PMM_H
#define PMM_H

#include "types.h"
#include "multiboot.h"

#define PAGE_SIZE 4096

void     pmm_init(multiboot_info_t* mboot);
void*    pmm_alloc_page(void);
void     pmm_free_page(void* addr);
uint32_t pmm_get_total_memory(void);
uint32_t pmm_get_free_memory(void);
uint32_t pmm_get_used_memory(void);

#endif
