# LeoOS build system
#   make        -> build/leo-os.bin + leo-os.iso
#   make run    -> boot the ISO in QEMU
#   make clean  -> remove build output

CC      ?= gcc
AS      := nasm
QEMU    ?= qemu-system-i386

# Use a cross compiler if one is installed (e.g. on macOS: brew install i686-elf-gcc)
ifneq ($(shell command -v i686-elf-gcc 2>/dev/null),)
  CC := i686-elf-gcc
endif
# grub-mkrescue is called grub2-mkrescue on Fedora, and i686-elf-grub-mkrescue via Homebrew
GRUB_MKRESCUE ?= $(shell command -v grub-mkrescue || command -v grub2-mkrescue || command -v i686-elf-grub-mkrescue)

CFLAGS  := -m32 -std=gnu11 -ffreestanding -fno-builtin -fno-stack-protector -fno-pie \
           -fno-pic -fno-asynchronous-unwind-tables -mno-sse -mno-mmx -mno-80387 \
           -O2 -Wall -Wextra -Iinclude
LDFLAGS := -m32 -nostdlib -no-pie -Wl,--build-id=none -T linker.ld
ASFLAGS := -f elf32

BUILD   := build
KERNEL  := $(BUILD)/leo-os.bin
KERNEL_TEXT := $(BUILD)/leo-os-text.bin
ISO     := leo-os.iso
C_SRCS  := $(wildcard kernel/*.c)
OBJS    := $(BUILD)/boot.o $(patsubst kernel/%.c,$(BUILD)/%.o,$(C_SRCS))

.PHONY: all run run-serial clean

all: $(ISO)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.o: boot/boot.asm | $(BUILD)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/boot-text.o: boot/boot.asm | $(BUILD)
	$(AS) $(ASFLAGS) -DTEXT_MODE $< -o $@

$(BUILD)/%.o: kernel/%.c $(wildcard include/*.h) | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL): $(OBJS) linker.ld
	$(CC) $(LDFLAGS) $(OBJS) -o $@ -lgcc

# Same kernel, but its Multiboot header doesn't ask for graphics (classic text shell)
$(KERNEL_TEXT): $(BUILD)/boot-text.o $(filter-out $(BUILD)/boot.o,$(OBJS)) linker.ld
	$(CC) $(LDFLAGS) $(BUILD)/boot-text.o $(filter-out $(BUILD)/boot.o,$(OBJS)) -o $@ -lgcc

$(ISO): $(KERNEL) $(KERNEL_TEXT) iso/boot/grub/grub.cfg
	cp $(KERNEL) iso/boot/leo-os.bin
	cp $(KERNEL_TEXT) iso/boot/leo-os-text.bin
	$(GRUB_MKRESCUE) -o $@ iso 2>/dev/null
	@echo "Built $@ - run it with: make run"

run: $(ISO)
	$(QEMU) -cdrom $(ISO) -m 128M

# Same as run, but also mirrors the console to your terminal via the serial port
run-serial: $(ISO)
	$(QEMU) -cdrom $(ISO) -m 128M -serial stdio

clean:
	rm -rf $(BUILD) $(ISO) iso/boot/leo-os.bin iso/boot/leo-os-text.bin
