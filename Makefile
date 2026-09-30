# ============================================================
#  Notux OS — Makefile
#
#  Install dependencies:
#    sudo apt install gcc clang lld nasm parted mtools \
#                     dosfstools ntfs-3g qemu-system-x86 ovmf
#
#  Targets:
#    make           → build all + 200 MiB bootable disk image
#    make check     → compile-check every source (no link)
#    make run       → boot in QEMU (SDL window + serial on stdout)
#    make run-log   → boot headless, serial → build/serial.log
#    make clean     → remove build/
# ============================================================

BUILD    := build
IMG      := $(BUILD)/notux.img
EFI_OUT  := $(BUILD)/BOOTX64.EFI
KERNEL   := $(BUILD)/notux.elf
ESP_FAT  := $(BUILD)/esp.fat
DATA_FAT := $(BUILD)/data.ntfs
APPS_BLOB := $(BUILD)/apps_blob.bin

CC       := gcc
CLANG    := clang
LD       := ld
NASM     := nasm
LLDLINK  := lld-link
GCCINC   := $(shell $(CC) -print-file-name=include)
OVMF     := /usr/share/ovmf/OVMF.fd

# ── Kernel compile flags ──────────────────────────────────────
KFLAGS := \
    -m64 -ffreestanding -fno-stack-protector -mno-red-zone \
    -fno-pic -fno-pie \
    -isystem $(GCCINC) \
    -Ikernel -Ilibnotux/include \
    -std=c11 -Wall -Wextra \
    -Wno-unused-parameter -Wno-unused-function \
    -Wno-missing-field-initializers \
    -O2 -g

# ── Bootloader compile flags (PE32+ UEFI) ─────────────────────
BFLAGS := \
    -target x86_64-unknown-windows \
    -ffreestanding -fno-stack-protector -mno-red-zone \
    -fno-pic -fno-pie -fshort-wchar \
    -Iboot -O2

# ── Kernel sources ────────────────────────────────────────────
KERNEL_C := \
    kernel/mm/pmm.c \
    kernel/mm/vmm.c \
    kernel/mm/heap.c \
    kernel/arch/x86_64/gdt.c \
    kernel/arch/x86_64/idt.c \
    kernel/arch/x86_64/isr.c \
    kernel/arch/x86_64/pit.c \
    kernel/arch/x86_64/syscall.c \
    kernel/proc/process.c \
    kernel/proc/scheduler.c \
    kernel/proc/elf_loader.c \
    kernel/proc/process_manager.c \
    kernel/proc/proc_utils.c \
    kernel/fs/vfs.c \
    kernel/fs/ntfs/ntfs.c \
    kernel/fs/ntfs/ntfs_debug.c \
    kernel/fs/rootmgr.c \
    kernel/fs/pathconv.c \
    kernel/drivers/gfx/font.c \
    kernel/drivers/gfx/fonts/sysfont.c \
    kernel/drivers/input/ps2.c \
    kernel/tty/tty.c \
    kernel/drivers/disk/ata.c \
    kernel/drivers/disk/gpt.c \
    kernel/drivers/usb/usb.c \
    kernel/drivers/usb/usb_keyboard.c \
    kernel/drivers/usb/usb_mouse.c \
    kernel/drivers/usb/usb_init.c \
    kernel/kserial.c \
    kernel/apps_install.c \
    kernel/stubs.c \
    kernel/main.c

KERNEL_ASM := \
    kernel/arch/x86_64/entry.asm \
    kernel/arch/x86_64/isr_stubs.asm \
    kernel/arch/x86_64/syscall_entry.asm \
    kernel/arch/x86_64/apps_blob.asm

BOOT_C := boot/efi_main.c

KERNEL_OBJS := \
    $(KERNEL_C:%.c=$(BUILD)/%.o) \
    $(KERNEL_ASM:%.asm=$(BUILD)/%.o)

BOOT_OBJS := $(BOOT_C:%.c=$(BUILD)/%.o)

.PHONY: all check run run-log clean

# ── Default target ────────────────────────────────────────────
all: $(IMG)
	@echo ""
	@echo "  ╔══════════════════════════════════════╗"
	@echo "  ║  Notux OS build complete             ║"
	@echo "  ║                                      ║"
	@echo "  ║  make run      → QEMU with display   ║"
	@echo "  ║  make run-log  → headless + log      ║"
	@echo "  ║  make apps     → build and install   ║"
	@echo "  ╚══════════════════════════════════════╝"

# ── Build user applications and install to NTFS ───────────────
apps: build-user-apps install-user-apps
	@echo ""
	@echo "  ╔══════════════════════════════════════╗"
	@echo "  ║  User applications installed         ║"
	@echo "  ╚══════════════════════════════════════╝"

# ── Disk image ────────────────────────────────────────────────
# 200 MiB GPT disk:
#   Part 1 (1–97 MiB):   FAT32 ESP  — EFI app + kernel ELF
#   Part 2 (97–197 MiB): NTFS DATA  — Notux root filesystem
#
# At first boot, rootmgr detects the NTFS partition, installs
# the base directory layout, writes notux.cfg, then proceeds.
$(IMG): $(EFI_OUT) $(KERNEL) $(APPS_BLOB)
	@echo "  [IMG]  Building 200 MiB bootable disk..."

	@# ── ESP (FAT32) ─────────────────────────────────────
	dd if=/dev/zero of=$(ESP_FAT) bs=1M count=96 status=none
	mkfs.fat -F 32 -n "NOTUX_EFI" $(ESP_FAT) 2>/dev/null
	mmd   -i $(ESP_FAT) ::/EFI ::/EFI/BOOT
	mcopy -i $(ESP_FAT) $(EFI_OUT) ::/EFI/BOOT/BOOTX64.EFI
	mcopy -i $(ESP_FAT) $(KERNEL)  ::/notux.elf

	@# ── GPT layout ──────────────────────────────────────
	dd if=/dev/zero of=$@ bs=1M count=200 status=none
	parted -s $@ \
	    mklabel gpt \
	    mkpart ESP  fat32 1MiB  97MiB \
	    mkpart DATA       97MiB 197MiB \
	    set 1 esp on 2>/dev/null

	@# ── Embed ESP at 1 MiB ──────────────────────────────
	dd if=$(ESP_FAT) of=$@ bs=1M seek=1 conv=notrunc status=none

	@# ── NTFS data partition at 97 MiB ───────────────────
	@if command -v mkntfs >/dev/null 2>&1; then \
	    dd if=/dev/zero of=$(DATA_FAT) bs=1M count=96 status=none; \
    mkntfs -F -Q -q -s 512 -p 0 -S 63 -H 255 -z 4 -L "NotuxData" $(DATA_FAT) >/dev/null 2>&1 && \
	    dd if=$(DATA_FAT) of=$@ bs=1M seek=97 conv=notrunc status=none; \
	    echo "  [NTFS] Data partition formatted (Notux will install on first boot)"; \
	else \
	    echo "  [WARN] mkntfs not found — install ntfs-3g for data partition"; \
	fi

	@echo "  [IMG]  $@ ready"

# ── Kernel ELF ────────────────────────────────────────────────
$(KERNEL): $(KERNEL_OBJS)
	@mkdir -p $(@D)
	$(LD) -T kernel/linker.ld -nostdlib -no-pie -o $@ $^
	@echo "  [ELF]  $@ (entry=0x100000, $(shell size $@ | tail -1 | awk '{print $$4}') bytes)"

# ── EFI bootloader ────────────────────────────────────────────
$(EFI_OUT): $(BOOT_OBJS)
	@mkdir -p $(@D)
	$(LLDLINK) \
	    /out:$@ \
	    /entry:efi_main \
	    /subsystem:efi_application \
	    /nodefaultlib \
	    /machine:x64 \
	    /dll \
	    $(BOOT_OBJS)
	@echo "  [EFI]  $@"

# ── Compile rules ─────────────────────────────────────────────
$(BUILD)/kernel/%.o: kernel/%.c
	@mkdir -p $(@D)
	$(CC) $(KFLAGS) -c $< -o $@
	@echo "  [CC]   $<"

$(BUILD)/kernel/%.o: kernel/%.asm
	@mkdir -p $(@D)
	$(NASM) -f elf64 $< -o $@
	@echo "  [ASM]  $<"

# ── Embedded user-application payload ──────────────────────────
$(APPS_BLOB): build-user-apps
	@mkdir -p $(@D)
	python3 tools/make_apps_blob.py build/bin $@

$(BUILD)/kernel/arch/x86_64/apps_blob.o: $(APPS_BLOB)

$(BUILD)/boot/%.o: boot/%.c
	@mkdir -p $(@D)
	$(CLANG) $(BFLAGS) -c $< -o $@
	@echo "  [CC]   $<"

# ── QEMU ──────────────────────────────────────────────────────
run: $(IMG)
	qemu-system-x86_64 \
	    -bios $(OVMF) \
	    -drive if=ide,format=raw,file=$(IMG) \
	    -m 256M \
	    -serial stdio \
	    -no-reboot

run-log: $(IMG)
	@rm -f $(BUILD)/serial.log
	@qemu-system-x86_64 \
	    -bios $(OVMF) \
	    -drive if=ide,format=raw,file=$(IMG) \
	    -m 256M \
	    -display none \
	    -chardev file,id=ser,path=$(BUILD)/serial.log \
	    -serial chardev:ser \
	    -no-reboot -no-shutdown & \
	sleep 20; kill $$! 2>/dev/null || true; \
	echo "=== Serial log ==="; \
	strings $(BUILD)/serial.log 2>/dev/null \
	    | grep -v "^\[" | grep -v "^=$" | head -60

# ── Compile check ─────────────────────────────────────────────
check:
	@echo "Checking all sources..."
	@FAIL=0; \
	for f in $(KERNEL_C); do \
	    out=$$($(CC) $(KFLAGS) -c $$f -o /dev/null 2>&1); \
	    if [ -z "$$out" ]; then printf "  OK   %s\n" $$f; \
	    else printf "  FAIL %s\n" $$f; echo "$$out" | head -3; FAIL=1; fi; \
	done; \
	for f in $(KERNEL_ASM); do \
	    out=$$($(NASM) -f elf64 $$f -o /dev/null 2>&1 | grep -v warning); \
	    if [ -z "$$out" ]; then printf "  OK   %s\n" $$f; \
	    else printf "  FAIL %s\n" $$f; echo "$$out" | head -3; FAIL=1; fi; \
	done; \
	for f in $(BOOT_C); do \
	    out=$$($(CLANG) $(BFLAGS) -c $$f -o /dev/null 2>&1); \
	    if [ -z "$$out" ]; then printf "  OK   %s\n" $$f; \
	    else printf "  FAIL %s\n" $$f; echo "$$out" | head -3; FAIL=1; fi; \
	done; \
	[ $$FAIL -eq 0 ] && echo "All sources OK" || { echo "Build check FAILED"; exit 1; }

# Include user applications build system
include userapps.mk

# ── Clean target ──────────────────────────────────────────────
clean:
	rm -rf $(BUILD)
