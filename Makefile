CC = gcc
CFLAGS = -std=gnu11 -m32 -ffreestanding -O2 -Wall -Wextra -Iinclude -fno-pie -fno-stack-protector -fno-builtin -nostdlib -Wno-unused-function

AS = nasm
ASFLAGS = -f elf32

LD = ld
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib

C_SRCS = $(wildcard kernel/*.c) $(wildcard drivers/*.c) $(wildcard net/*.c) $(wildcard fs/*.c) $(wildcard gui/*.c)
C_OBJS = $(C_SRCS:.c=.o)

ASM_SRCS = boot/boot.asm kernel/interrupts.asm
ASM_OBJS = $(ASM_SRCS:.asm=.o)

OBJS = $(ASM_OBJS) $(C_OBJS)

KERNEL_BIN = iso/boot/kernel.bin
ISO_IMAGE = aeroos.iso

all: $(ISO_IMAGE)

$(KERNEL_BIN): $(OBJS) linker.ld
	@mkdir -p iso/boot/grub
	$(LD) $(LDFLAGS) -o $@ $(OBJS)
	@echo ">>> Successfully linked freestanding kernel: $@"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

$(ISO_IMAGE): $(KERNEL_BIN) iso/boot/grub/grub.cfg
	@grub-mkrescue -o $(ISO_IMAGE) iso
	@echo "=========================================================="
	@echo ">>> Bootable ISO build complete: $(ISO_IMAGE)"
	@ls -lh $(ISO_IMAGE)
	@echo "=========================================================="

clean:
	rm -f $(OBJS) $(KERNEL_BIN) $(ISO_IMAGE)

.PHONY: all clean
