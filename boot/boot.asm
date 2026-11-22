; ==============================================================================
; AeroOS Bootloader Assembly
; Conforms to Multiboot 1 standard with VBE Linear Framebuffer request
; ==============================================================================

MBALIGN     equ  1 << 0             ; align loaded modules on page boundaries
MEMINFO     equ  1 << 1             ; provide memory map
VIDMODE     equ  1 << 2             ; request video mode information
FLAGS       equ  MBALIGN | MEMINFO | VIDMODE
MAGIC       equ  0x1BADB002         ; multiboot 'magic number'
CHECKSUM    equ -(MAGIC + FLAGS)    ; checksum (magic + flags + checksum must be 0)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

    ; Header address fields (unused since AOUT kludge is not set in flags)
    dd 0
    dd 0
    dd 0
    dd 0
    dd 0

    ; Video mode request: linear graphics mode at 1024x768x32
    dd 0        ; mode_type (0 = linear graphics, 1 = EGA text)
    dd 1024     ; width
    dd 768      ; height
    dd 32       ; depth (32-bit color: ARGB 8:8:8:8)

section .bss
align 16
stack_bottom:
    resb 65536  ; 64 KB kernel stack
stack_top:

section .text
global _start
extern kmain

_start:
    cli                     ; Disable interrupts

    mov esp, stack_top      ; Set up kernel stack pointer
    xor ebp, ebp            ; Clear frame pointer

    ; Push arguments for kmain(uint32_t magic, multiboot_info_t* mboot_ptr)
    push ebx                ; Argument 2: Multiboot info pointer
    push eax                ; Argument 1: Multiboot magic

    call kmain              ; Enter C kernel

.hang:
    cli
    hlt
    jmp .hang
