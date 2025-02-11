################################################################################
# Revised Makefile for Building a 64-bit UEFI OS and Creating an Editable Disk Image
################################################################################

# Include any shared definitions (e.g. lib_dir) if needed.
include lib/Makefile

# Targets for bootloader and kernel images.
TARGET_BOOT   = BOOTX64.EFI
TARGET_KERNEL = kernel.elf

# Define directories and files for the disk image and mounting.
DISK_IMG   = build/OS.img
MNT_DIR    = build/mnt
LOOPFILE   = build/loopdev

################################################################################
# Build Bootloader
################################################################################

# Cross-compiler for the bootloader (adjust if necessary).
CC_BOOT         := x86_64-w64-mingw32-gcc
CFLAGS          := -nostdlib -Wall -Wextra
LD_FLAGS_BOOT   := $(CFLAGS) -Wl,-dll -shared -Wl,--subsystem,10 -e efi_main

$(TARGET_BOOT): x86_64/bootloader $(lib_dir)/lib-gnuefi.o
	@echo "Linking bootloader..."
	$(CC_BOOT) $(LD_FLAGS_BOOT) -o $(TARGET_BOOT) \
	    $(shell find . -not -path "./arch/arm/*" -not -path "./arch/x86_64/kernel/*" -not -path "./lib/*" -name '*.o') \
	    $(lib_dir)/lib-gnuefi.o -lgcc

################################################################################
# Build Kernel
################################################################################

# Kernel cross-compiler (adjust as necessary).
CC_KERNEL         := x86_64-modernos-gcc
LD_FLAGS_KERNEL   := -T $(shell find . -not -path "./arch/aarch64/*" -not -path "./lib/*" -name '*.ld') -n -Wl,--gc-sections

$(TARGET_KERNEL): x86_64/kernel $(lib_dir)/lib-libk.a
	@echo "Linking kernel..."
	ld -T $(shell find . -not -path "./arch/aarch64/*" -not -path "./lib/*" -name '*.ld') \
	   -static -Bsymbolic -nostdlib -o $(TARGET_KERNEL) \
	   $(shell find . -not -path "./lib/*" -not -path "./arch/arm/*" -not -path "./arch/x86_64/bootloader/*" -name '*.o') \
	   -L $(lib_dir) -lk

################################################################################
# Overall Build Target
################################################################################

# “all” builds the bootloader and kernel. It also names the disk image
# as a dependency. However, because the disk image rule has no prerequisites,
# if build/OS.img already exists, it is considered up to date and will not be rebuilt.
all: $(TARGET_BOOT) $(TARGET_KERNEL) $(DISK_IMG)

################################################################################
# Call Sub-Makefiles for Architecture-specific Build Steps
################################################################################

x86_64/bootloader:
	@$(MAKE) -C arch/ bootloader

x86_64/kernel:
	@$(MAKE) -C arch/ kernel

################################################################################
# Create a Disk Image with an Editable File System
################################################################################
# This target creates a 50MB raw disk image, partitions it with GPT, formats the
# first partition as FAT32, mounts it briefly to install the bootloader, and then
# cleans up. Notice that there are no prerequisites here. As a result, if
# $(DISK_IMG) already exists, Make considers this target up to date and skips it.
################################################################################

$(DISK_IMG):
	@echo "Creating disk image $(DISK_IMG)..."
	@mkdir -p build
	dd if=/dev/zero of=$(DISK_IMG) bs=1M count=50
	parted $(DISK_IMG) --script mklabel gpt
	parted $(DISK_IMG) --script mkpart primary fat32 1MiB 100%
	@echo "Attaching loop device to format and install bootloader..."
	@bash -c '\
		LOOPDEV=$$(sudo losetup --find --show -P $(DISK_IMG)); \
		echo "Loop device: $$LOOPDEV"; \
		# Allow a brief pause for the partition device to be created. \
		sleep 1; \
		sudo mkfs.fat -F32 "$${LOOPDEV}p1"; \
		mkdir -p $(MNT_DIR); \
		sudo mount "$${LOOPDEV}p1" $(MNT_DIR); \
		sudo mkdir -p $(MNT_DIR)/EFI/BOOT; \
		sudo cp $(TARGET_BOOT) $(MNT_DIR)/EFI/BOOT/BOOTX64.EFI; \
		sync; \
		sudo umount $(MNT_DIR); \
		sudo losetup -d $$LOOPDEV; \
		echo "Disk image created with an editable file system."'

################################################################################
# Mount the Disk Image for Manual Editing
################################################################################
.PHONY: mount-img
mount-img: $(DISK_IMG)
	@echo "Mounting disk image for manual editing..."
	@mkdir -p $(MNT_DIR)
	@bash -c '\
		LOOPDEV=$$(sudo losetup --find --show -P $(DISK_IMG)); \
		echo $$LOOPDEV > $(LOOPFILE); \
		sudo mount "$${LOOPDEV}p1" $(MNT_DIR); \
		echo "Disk image mounted at $(MNT_DIR)."; \
		echo "When finished editing, run '\''make umount-img'\'' to unmount and detach the loop device."'

################################################################################
# Unmount the Disk Image After Editing
################################################################################
.PHONY: umount-img
umount-img:
	@echo "Unmounting disk image..."
	@bash -c '\
		sudo umount $(MNT_DIR); \
		LOOPDEV=$$(cat $(LOOPFILE)); \
		sudo losetup -d $$LOOPDEV; \
		rm -f $(LOOPFILE); \
		echo "Disk image unmounted and loop device detached."'

################################################################################
# Run QEMU with the Disk Image and UEFI Firmware
################################################################################
OVMF = build/RELEASEX64_OVMF.fd
.PHONY: run
run: $(DISK_IMG)
	@echo "Starting QEMU..."
	qemu-system-x86_64 -cpu qemu64 -bios $(OVMF) -drive file=$(DISK_IMG),if=ide

################################################################################
# Clean Up Build Artifacts
################################################################################
.PHONY: clean
clean:
	@echo "Cleaning up..."
	# Remove object files (excluding those in ./lib)
	find . -not -path "./lib/*" -name '*.o' -delete
	rm -f $(TARGET_BOOT) $(TARGET_KERNEL)
	rm -f $(DISK_IMG)
	find ./lib/libk/ -name '*.o' -delete
	rm -f $(lib_dir)/libk.a
	# Remove temporary mount info if it exists.
	rm -f $(LOOPFILE)
	@echo "Cleanup complete."
