#include "kernel/pmm.h"
#include "string.h"
#include "drivers/serial.h"

extern uint32_t kernel_start;
extern uint32_t kernel_end;

// Bitmap: 1 bit represents one 4KB frame (1 = used/reserved, 0 = free)
// Supports up to 512 MB physical RAM (131072 frames = 16384 bytes bitmap)
#define MAX_FRAMES 131072
#define BITMAP_SIZE (MAX_FRAMES / 8)

static uint8_t pmm_bitmap[BITMAP_SIZE];
static uint32_t total_memory_bytes = 0;
static uint32_t used_frames_count = 0;
static uint32_t total_frames_count = 0;

static inline void bitmap_set(uint32_t frame) {
    if (frame < MAX_FRAMES) {
        if (!(pmm_bitmap[frame / 8] & (1 << (frame % 8)))) {
            pmm_bitmap[frame / 8] |= (1 << (frame % 8));
            used_frames_count++;
        }
    }
}

static inline void bitmap_clear(uint32_t frame) {
    if (frame < MAX_FRAMES) {
        if (pmm_bitmap[frame / 8] & (1 << (frame % 8))) {
            pmm_bitmap[frame / 8] &= ~(1 << (frame % 8));
            used_frames_count--;
        }
    }
}

static inline bool bitmap_test(uint32_t frame) {
    if (frame >= MAX_FRAMES) return true;
    return (pmm_bitmap[frame / 8] & (1 << (frame % 8))) != 0;
}

void pmm_init(multiboot_info_t* mboot) {
    if (!mboot || !(mboot->flags & MULTIBOOT_INFO_MEM_MAP)) {
        serial_puts("[PMM] Warning: Multiboot memory map not provided, using 128MB default fallback\n");
        total_memory_bytes = 128 * 1024 * 1024;
    } else {
        multiboot_memory_map_t* mmap = (multiboot_memory_map_t*)mboot->mmap_addr;
        uint32_t mmap_end = mboot->mmap_addr + mboot->mmap_length;

        total_memory_bytes = 0;
        while ((uint32_t)mmap < mmap_end) {
            if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
                uint32_t end_addr = mmap->addr_low + mmap->len_low;
                if (end_addr > total_memory_bytes) {
                    total_memory_bytes = end_addr;
                }
            }
            mmap = (multiboot_memory_map_t*)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
        }
    }

    total_frames_count = total_memory_bytes / PAGE_SIZE;
    if (total_frames_count > MAX_FRAMES) total_frames_count = MAX_FRAMES;

    // Mark all frames within total_frames_count as used
    memset(pmm_bitmap, 0xFF, sizeof(pmm_bitmap));
    used_frames_count = total_frames_count;

    if (mboot && (mboot->flags & MULTIBOOT_INFO_MEM_MAP)) {
        multiboot_memory_map_t* mmap = (multiboot_memory_map_t*)mboot->mmap_addr;
        uint32_t mmap_end = mboot->mmap_addr + mboot->mmap_length;

        while ((uint32_t)mmap < mmap_end) {
            if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
                uint32_t start_frame = mmap->addr_low / PAGE_SIZE;
                uint32_t count = mmap->len_low / PAGE_SIZE;

                for (uint32_t f = 0; f < count; f++) {
                    uint32_t frame = start_frame + f;
                    if (frame < total_frames_count) {
                        bitmap_clear(frame);
                    }
                }
            }
            mmap = (multiboot_memory_map_t*)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
        }
    } else {
        for (uint32_t f = 0; f < total_frames_count; f++) {
            bitmap_clear(f);
        }
    }

    // Reserve low 1MB (BIOS, IVT, BDA, VGA hardware buffers)
    for (uint32_t f = 0; f < (1024 * 1024) / PAGE_SIZE; f++) {
        bitmap_set(f);
    }

    // Reserve Kernel image memory
    uint32_t k_start_frame = ((uint32_t)&kernel_start) / PAGE_SIZE;
    uint32_t k_end_frame   = (((uint32_t)&kernel_end) + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint32_t f = k_start_frame; f <= k_end_frame; f++) {
        bitmap_set(f);
    }

    // Reserve GUI double buffer (0x00800000 to 0x00C00000, 4MB)
    uint32_t fb_start_frame = 0x00800000 / PAGE_SIZE;
    uint32_t fb_end_frame   = 0x00C00000 / PAGE_SIZE;
    for (uint32_t f = fb_start_frame; f < fb_end_frame; f++) {
        bitmap_set(f);
    }

    serial_printf("[PMM] Physical Memory: %d MB total, %d free frames, %d used frames\n",
                  total_memory_bytes / (1024 * 1024),
                  total_frames_count - used_frames_count,
                  used_frames_count);
}

void* pmm_alloc_page(void) {
    for (uint32_t f = 0; f < total_frames_count; f++) {
        if (!bitmap_test(f)) {
            bitmap_set(f);
            return (void*)(f * PAGE_SIZE);
        }
    }
    serial_puts("[PMM] Out of physical memory frames!\n");
    return NULL;
}

void pmm_free_page(void* addr) {
    uint32_t frame = ((uint32_t)addr) / PAGE_SIZE;
    bitmap_clear(frame);
}

uint32_t pmm_get_total_memory(void) {
    return total_memory_bytes;
}

uint32_t pmm_get_free_memory(void) {
    return (total_frames_count - used_frames_count) * PAGE_SIZE;
}

uint32_t pmm_get_used_memory(void) {
    return used_frames_count * PAGE_SIZE;
}
