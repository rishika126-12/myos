#include "kernel/heap.h"
#include "string.h"
#include "drivers/serial.h"

#define HEAP_START 0x01000000 // 16 MB mark
#define HEAP_SIZE  (32 * 1024 * 1024) // 32 MB heap pool
#define BLOCK_MAGIC 0x48454150 // 'HEAP'

typedef struct heap_header {
    uint32_t            magic;
    size_t              size; // payload size
    bool                is_free;
    struct heap_header* next;
    struct heap_header* prev;
} heap_header_t;

static heap_header_t* heap_first = NULL;
static size_t heap_used = 0;

void heap_init(void) {
    heap_first = (heap_header_t*)HEAP_START;
    heap_first->magic = BLOCK_MAGIC;
    heap_first->size = HEAP_SIZE - sizeof(heap_header_t);
    heap_first->is_free = true;
    heap_first->next = NULL;
    heap_first->prev = NULL;
    heap_used = 0;

    serial_printf("[HEAP] Dynamic Kernel Heap initialized at %p (size %d MB)\n",
                  (void*)HEAP_START, HEAP_SIZE / (1024 * 1024));
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    // 8-byte alignment
    size = (size + 7) & ~7;

    heap_header_t* curr = heap_first;
    while (curr) {
        if (curr->magic != BLOCK_MAGIC) {
            serial_puts("[HEAP] Error: Heap corruption detected!\n");
            return NULL;
        }

        if (curr->is_free && curr->size >= size) {
            // Split block if excess size is large enough for another header + min payload
            if (curr->size >= size + sizeof(heap_header_t) + 16) {
                heap_header_t* next_block = (heap_header_t*)((uint32_t)curr + sizeof(heap_header_t) + size);
                next_block->magic = BLOCK_MAGIC;
                next_block->size = curr->size - size - sizeof(heap_header_t);
                next_block->is_free = true;
                next_block->next = curr->next;
                next_block->prev = curr;

                if (curr->next) {
                    curr->next->prev = next_block;
                }
                curr->next = next_block;
                curr->size = size;
            }

            curr->is_free = false;
            heap_used += curr->size;
            return (void*)((uint32_t)curr + sizeof(heap_header_t));
        }
        curr = curr->next;
    }

    serial_printf("[HEAP] Out of heap memory! (requested %d bytes)\n", size);
    return NULL;
}

void* kcalloc(size_t num, size_t size) {
    size_t total = num * size;
    void* ptr = kmalloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void* krealloc(void* ptr, size_t size) {
    if (!ptr) return kmalloc(size);
    if (size == 0) {
        kfree(ptr);
        return NULL;
    }

    heap_header_t* header = (heap_header_t*)((uint32_t)ptr - sizeof(heap_header_t));
    if (header->magic != BLOCK_MAGIC) return NULL;

    if (header->size >= size) {
        return ptr;
    }

    void* new_ptr = kmalloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, header->size);
        kfree(ptr);
    }
    return new_ptr;
}

void kfree(void* ptr) {
    if (!ptr) return;

    heap_header_t* header = (heap_header_t*)((uint32_t)ptr - sizeof(heap_header_t));
    if (header->magic != BLOCK_MAGIC) {
        serial_puts("[HEAP] Error: Invalid pointer passed to kfree!\n");
        return;
    }

    header->is_free = true;
    if (heap_used >= header->size) {
        heap_used -= header->size;
    }

    // Coalesce with next block if free
    if (header->next && header->next->is_free) {
        header->size += sizeof(heap_header_t) + header->next->size;
        header->next = header->next->next;
        if (header->next) {
            header->next->prev = header;
        }
    }

    // Coalesce with previous block if free
    if (header->prev && header->prev->is_free) {
        header->prev->size += sizeof(heap_header_t) + header->size;
        header->prev->next = header->next;
        if (header->next) {
            header->next->prev = header->prev;
        }
    }
}

size_t heap_get_used_bytes(void) {
    return heap_used;
}
