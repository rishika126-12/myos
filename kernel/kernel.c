#include "types.h"
#include "multiboot.h"
#include "drivers/serial.h"
#include "drivers/framebuffer.h"
#include "kernel/gdt.h"
#include "kernel/idt.h"
#include "drivers/timer.h"
#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "kernel/pmm.h"
#include "kernel/heap.h"
#include "drivers/pci.h"
#include "drivers/ata.h"
#include "drivers/e1000.h"
#include "net/net.h"
#include "fs/vfs.h"
#include "gui/gui.h"

void kmain(uint32_t magic, multiboot_info_t* mboot) {
    // 1. Initialize Serial COM1 for early kernel diagnostic output
    serial_init();
    serial_puts("\n=======================================================\n");
    serial_puts("       AeroOS Kernel v1.0 - Academic OS Project\n");
    serial_puts("=======================================================\n");

    // 2. Verify Multiboot 1 Bootloader Compliance
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        serial_puts("[PANIC] Invalid Multiboot magic number!\n");
        while (1) { __asm__ volatile ("hlt"); }
    }
    serial_puts("[BOOT] Multiboot 1 standard verified\n");

    // 3. Initialize Global Descriptor Table (GDT)
    gdt_init();

    // 4. Initialize Interrupt Descriptor Table (IDT) & Remap 8259 PIC
    idt_init();

    // 5. Initialize VBE Linear Framebuffer Subsystem (1024x768x32)
    if (!fb_init(mboot)) {
        serial_puts("[PANIC] Framebuffer initialization failed!\n");
        while (1) { __asm__ volatile ("hlt"); }
    }

    // 6. Initialize Hardware Timers & Input Devices
    timer_init(100);       // 100 Hz Programmable Interval Timer
    keyboard_init();       // PS/2 Keyboard (IRQ 1)
    mouse_init();          // PS/2 Mouse (IRQ 12)
    mouse_update_bounds(fb.width, fb.height);

    // 7. Initialize Memory Subsystems
    pmm_init(mboot);       // Physical Memory Page Frame Allocator
    heap_init();           // 32 MB Dynamic Kernel Heap (kmalloc / kfree)

    // 8. Discover Hardware on PCI Bus
    pci_init();

    // 9. Initialize Storage & Virtual Filesystem
    ata_init();            // Primary IDE Hard Disk Driver
    vfs_init();            // Ramdisk & Virtual File System

    // 10. Locate & Initialize Intel PRO/1000 Gigabit Network Adapter
    pci_device_t* nic_pci = pci_find_device(0x8086, 0x100E);
    if (!nic_pci) {
        // Fallback check for alternate Intel NIC IDs (82543GC, 82545EM, etc.)
        nic_pci = pci_find_device(0x8086, 0xFFFF);
    }

    if (nic_pci) {
        serial_printf("[NET] Found Intel Ethernet Controller at %02x:%02x.%d\n",
                      nic_pci->bus, nic_pci->slot, nic_pci->func);
        e1000_init(nic_pci);
    } else {
        serial_puts("[NET] Intel e1000 NIC not found on PCI bus\n");
    }

    // 11. Initialize TCP/IP Stack & DHCP Client
    net_init();

    // 12. Initialize Modern Desktop GUI & Window Manager
    gui_init();

    // 13. Enable Hardware Interrupts (STI)
    __asm__ volatile ("sti");
    serial_puts("[KERNEL] Interrupts enabled. Entering Main Desktop Loop...\n");

    // 14. Operating System Main Interactive Event Loop
    uint32_t last_tick = 0;
    while (1) {
        // Service networking packets (ARP, ICMP, DHCP)
        net_poll();

        // Process user input (mouse clicks, drags, window focus, typing)
        gui_handle_input();

        // Render desktop, windows, taskbar, clock, cursor, and blit backbuffer
        gui_render_frame();

        // Cap compositor loop to ~60 FPS
        while (timer_get_ticks() == last_tick) {
            __asm__ volatile ("pause");
        }
        last_tick = timer_get_ticks();
    }
}
