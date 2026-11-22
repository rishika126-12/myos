#include "kernel/gdt.h"
#include "drivers/serial.h"

static gdt_entry_t gdt_entries[5];
static gdt_ptr_t   gdt_pointer;

extern void gdt_flush(uint32_t gdt_ptr_addr);

static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;

    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt_entries[num].access      = access;
}

void gdt_init(void) {
    gdt_pointer.limit = (sizeof(gdt_entry_t) * 5) - 1;
    gdt_pointer.base  = (uint32_t)&gdt_entries;

    // 0x00: Null segment
    gdt_set_gate(0, 0, 0, 0, 0);

    // 0x08: Kernel Code segment (Base 0, Limit 4GB, 32-bit, Ring 0)
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    // 0x10: Kernel Data segment (Base 0, Limit 4GB, 32-bit, Ring 0)
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    // 0x18: User Code segment (Ring 3)
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

    // 0x20: User Data segment (Ring 3)
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    gdt_flush((uint32_t)&gdt_pointer);
    serial_puts("[GDT] GDT initialized (Kernel CS=0x08, DS=0x10)\n");
}
