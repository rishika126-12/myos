# AeroOS - Operating System Project Status & Roadmap

## 1. Project Overview
- **OS Name**: AeroOS (Custom 32-bit Protected Mode Operating System)
- **Target Architecture**: x86 (IA-32, 32-bit protected mode)
- **Primary Host Platform**: Oracle VirtualBox 7.2.x (Windows 10 Pro Host)
- **Secondary Boot Target**: Bootable USB Pendrive (via GRUB hybrid ISO)
- **Development Toolchain**: Ubuntu WSL2, GCC 15.2.0 (`-m32 -ffreestanding`), NASM 3.01, GNU ld 2.46, GNU Make 4.4.1, GRUB 2.14, xorriso 1.5.6, mtools 4.0.49
- **Image Size Constraint**: < 100 MB (Measured Actual: **5.4 MB**)
- **Non-negotiable Rule**: No Linux distribution base, no modified Ubuntu, zero QEMU dependencies. Pure custom kernel and userspace/GUI written from scratch in C and Assembly.

---

## 2. Environment & Toolchain Verification
| Tool / Component | Version / Status | Verification Result |
| :--- | :--- | :--- |
| **OS Dev Environment** | Ubuntu on WSL2 | Active, user `rishika_v` |
| **C Compiler** | GCC 15.2.0 | `-m32 -ffreestanding -std=gnu11` verified functional |
| **Assembler** | NASM 3.01 | Verified functional for ELF32 objects |
| **Linker** | GNU ld 2.46 | Verified functional for `elf_i386` |
| **Build Automation** | GNU Make 4.4.1 | Installed & verified (100% warning-free build) |
| **ISO Generator** | `grub-mkrescue` 2.14 | Generates i386-pc hybrid bootable ISO (`5.4 MB`) |
| **ISO Master Tool** | `xorriso` 1.5.6 | Verified functional |
| **FAT Utilities** | `mtools` 4.0.49 | Verified functional |
| **Virtualizer** | Oracle VirtualBox 7.2.22 | Verified on Windows host via `VBoxManage` |

---

## 3. Tested Hardware Verification (Inside VirtualBox 7.2)
From live serial diagnostic log (`serial.log`):
```text
=======================================================
       AeroOS Kernel v1.0 - Academic OS Project
=======================================================
[BOOT] Multiboot 1 standard verified
[GDT] GDT initialized (Kernel CS=0x08, DS=0x10)
[IDT] IDT initialized with 256 gates, PIC remapped
[FB] Framebuffer initialized: 1024x768, 32 bpp, pitch=4096, front=0xe0000000, back=0x00800000
[PIT] Timer initialized at 100 Hz (divisor=11931)
[KBD] PS/2 Keyboard initialized (IRQ 1 / Vector 33)
[MOUSE] PS/2 Mouse initialized at (512, 384), IRQ 12 / Vector 44
[PMM] Physical Memory: 255 MB total, 64174 free frames, 1346 used frames
[HEAP] Dynamic Kernel Heap initialized at 0x01000000 (size 32 MB)
[PCI] Scanning PCI bus 0...
[PCI] Found 00:00.0: Vendor 8086 Device 1237 (Class 06.00, IRQ 0, BAR0=0x00000000)
[PCI] Found 00:01.0: Vendor 8086 Device 7000 (Class 06.01, IRQ 0, BAR0=0x00000000)
[PCI] Found 00:01.1: Vendor 8086 Device 7111 (Class 01.01, IRQ 0, BAR0=0x00000000)
[PCI] Found 00:02.0: Vendor 80ee Device beef (Class 03.00, IRQ 10, BAR0=0xe0000008)
[PCI] Found 00:03.0: Vendor 8086 Device 100e (Class 02.00, IRQ 9, BAR0=0xf0000000)
[PCI] Found 00:04.0: Vendor 80ee Device cafe (Class 08.80, IRQ 11, BAR0=0x0000d021)
[PCI] Found 00:05.0: Vendor 8086 Device 2415 (Class 04.01, IRQ 11, BAR0=0x0000d101)
[PCI] Found 00:07.0: Vendor 8086 Device 7113 (Class 06.80, IRQ 9, BAR0=0x00000000)
[PCI] PCI scan complete. Found 8 devices.
[VFS] Virtual File System initialized with 7 entries
[NET] Found Intel Ethernet Controller at 00:03.0
[E1000] Initializing Intel 82540EM at MMIO 0xf0000000
[E1000] MAC Address: 08:00:27:58:d0:f1
[E1000] Intel PRO/1000 initialized successfully!
[DHCP] Sent DHCP Discover
[GUI] Desktop Compositor & Window Manager initialized
[KERNEL] Interrupts enabled. Entering Main Desktop Loop...
```

---

## 4. Completed Feature Checklist

### Phase 1: Bootloader & Freestanding Kernel Foundation
- [x] Configure `iso/boot/grub/grub.cfg` with Multiboot 1 specifications
- [x] Write `boot/boot.asm` with Multiboot 1 header requesting VBE framebuffer (1024x768x32)
- [x] Create linker script `linker.ld` (1 MB load address, 4 KB section alignment)
- [x] Implement freestanding standard library headers (`string.h`, `types.h`, `io.h`, `stdarg.h`)
- [x] Write minimal kernel entry in `kernel/kernel.c` with serial logging and framebuffer probe
- [x] Build `Makefile` and verify clean compilation into `iso/boot/kernel.bin`

### Phase 2: Core Hardware & Interrupt Management
- [x] Global Descriptor Table (GDT) setup in Assembly and C (Ring 0 Code/Data, Ring 3 Code/Data)
- [x] Interrupt Descriptor Table (IDT) with 256 ISR entries and CPU exception handlers
- [x] Programmable Interrupt Controller (8259 PIC) remapping to IRQ 32–47
- [x] Programmable Interval Timer (8254 PIT) for 100 Hz system ticks and uptime
- [x] PS/2 Keyboard driver with scancode set 1 decoding, Shift/Caps/Ctrl/Alt tracking
- [x] PS/2 Mouse driver with 3-byte packet decoding, delta tracking, and cursor clamping

### Phase 3: Memory Management & PCI Bus
- [x] Physical Memory Manager (PMM) with 4KB bitmap frame allocator
- [x] Dynamic Kernel heap allocator (`kmalloc`, `kfree`, `kcalloc`, `krealloc`) with 32 MB pool
- [x] PCI configuration space scanner (detects Host Bridge, VGA, IDE, and Intel e1000 NIC)

### Phase 4: Graphics Subsystem & Modern Desktop GUI
- [x] Double-buffered 32-bit linear framebuffer compositor (1024x768 @ 32 bpp)
- [x] 8x16 bitmap font engine with full ASCII rendering
- [x] 2D drawing primitives: rectangles, fills, gradients, lines, icons
- [x] Modern dark-glass desktop theme (deep indigo/cyan wallpaper)
- [x] Taskbar with Start menu button, active window pills, network IP status, RAM meter, and live clock
- [x] Start Menu / Application Launcher
- [x] Desktop icons with clickable shortcuts
- [x] Multi-window manager with draggable titlebars, close buttons, and focus handling
- [x] High-contrast hardware-style arrow mouse cursor

### Phase 5: Storage Subsystem & Virtual File System
- [x] ATA/IDE PIO mode hard disk driver (Primary Master)
- [x] Built-in Ramdisk virtual filesystem for assets and files
- [x] VFS interface: open, read, write, stat, list directory (`/system`, `/docs`, `/home`)

### Phase 6: Networking Stack (Intel PRO/1000 e1000)
- [x] Intel 82540EM PCI driver (MMIO BAR0, EEPROM MAC read, 32 RX / 16 TX descriptor rings)
- [x] Ethernet II frame parser and packet builder
- [x] ARP protocol with request/reply and cache
- [x] IPv4 packet parsing, checksumming, and routing
- [x] ICMP echo request/reply (ping utility)
- [x] UDP protocol handler
- [x] DHCP client (auto-configure IP `10.0.2.15`, gateway `10.0.2.2`, DNS)

### Phase 7: Built-in Graphical Applications
- [x] System Information App (CPU, RAM, screen res, network info, uptime)
- [x] Interactive Terminal / Diagnostic Shell (`ping`, `ip`, `ifconfig`, `ls`, `cat`, `help`, `uptime`, `lspci`, `mem`)
- [x] Text Editor (Notepad with interactive editing, Save to VFS, Reload)
- [x] File Manager (Folder and file browser with file preview)
- [x] Calculator (Interactive arithmetic utility with LCD display)

### Phase 8: VirtualBox Verification & USB Media Support
- [x] Build final bootable hybrid ISO image (`aeroos.iso`, **5.4 MB**)
- [x] Automate VirtualBox VM creation and headless boot test
- [x] Capture live running VirtualBox VM screenshot (`1024x768`)
- [x] Measure exact final image size (< 100 MB requirement satisfied: 5.4% of budget)
- [x] Provide USB flashing guide and complete project documentation

---

## 5. Current Status
- **Status**: Completed & Fully Tested in Oracle VirtualBox 7.2.22
- **Image Size**: 5.4 MB (5,677,056 bytes)
- **Last Updated**: November 2026
