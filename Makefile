################################################################################
# Top-Level Makefile for UEFI OS Build System (Linux/Kbuild style)
################################################################################

# Default architecture; override with "make ARCH=arm" if needed.
ARCH ?= x86_64

# Set up architecture-specific variables.
ifeq ($(ARCH),x86_64)
  CROSS_COMPILE_BOOT   ?= x86_64-w64-mingw32-
  CROSS_COMPILE_KERNEL ?= x86_64-elf-
  ARCH_DIR             := arch/x86_64
  TARGET_BOOT          := BOOTX64.EFI
else ifeq ($(ARCH),arm)
  CROSS_COMPILE_BOOT   ?= arm-linux-
  CROSS_COMPILE_KERNEL ?= arm-linux-
  ARCH_DIR             := arch/arm
  TARGET_BOOT          := BOOTARM.EFI
else
  $(error Unsupported architecture "$(ARCH)")
endif

# Build directories and temporary mount points.
BUILD_DIR := build
MNT_DIR   := $(BUILD_DIR)/mnt
LOOPFILE  := $(BUILD_DIR)/loopdev
DISK_IMG  := $(BUILD_DIR)/OS.img

# Final kernel output.
TARGET_KERNEL := kernel.elf

# Top-level targets.
.PHONY: all clean image run mount-img umount-img

all: $(TARGET_BOOT) $(TARGET_KERNEL) image

################################################################################
# Build Bootloader
################################################################################
$(TARGET_BOOT):
	@echo "==> Building bootloader for $(ARCH)..."
	$(MAKE) -C $(ARCH_DIR)/bootloader CROSS_COMPILE="$(CROSS_COMPILE_BOOT)"
	@echo "==> Copying bootloader binary to top-level..."
	cp $(ARCH_DIR)/bootloader/$(TARGET_BOOT) .

################################################################################
# Build Kernel
################################################################################
$(TARGET_KERNEL):
	@echo "==> Building kernel for $(ARCH)..."
	$(MAKE) -C $(ARCH_DIR)/kernel CROSS_COMPILE="$(CROSS_COMPILE_KERNEL)"
	@echo "==> Copying kernel binary to top-level..."
	cp $(ARCH_DIR)/kernel/$(TARGET_KERNEL) .

################################################################################
# Create Disk Image with Bootloader Installed
################################################################################
image: $(TARGET_BOOT) $(TARGET_KERNEL)
	@echo "==> Creating disk image..."
	@mkdir -p $(BUILD_DIR)
	dd if=/dev/zero of=$(DISK_IMG) bs=1M count=50
	# Create a GPT partition table with one FAT32 partition.
	parted $(DISK_IMG) --script mklabel gpt
	parted $(DISK_IMG) --script mkpart primary fat32 1MiB 100%
	@echo "==> Installing bootloader into disk image..."
	@bash -c '\
		set -e; \
		LOOPDEV=$$(sudo losetup --find --show -P $(DISK_IMG)); \
		sleep 1; \
		sudo mkfs.fat -F32 "$${LOOPDEV}p1"; \
		mkdir -p $(MNT_DIR); \
		sudo mount -t vfat "$${LOOPDEV}p1" $(MNT_DIR); \
		sudo mkdir -p $(MNT_DIR)/EFI/BOOT; \
		sudo cp $(TARGET_BOOT) $(MNT_DIR)/EFI/BOOT/$(TARGET_BOOT); \
		sudo cp $(TARGET_KERNEL) $(MNT_DIR)/$(TARGET_KERNEL); \
		sync; \
		sudo umount $(MNT_DIR); \
		sudo losetup -d $$LOOPDEV; \
		echo "Disk image creation complete."'
	@sudo touch image  # dummy file to update timestamp (prompts for password if needed)

################################################################################
# Run QEMU with the Disk Image
################################################################################
# Ensure an OVMF firmware file exists in build/ (you might generate or download it)
OVMF := ./RELEASE$(shell echo $(ARCH) | tr '[:lower:]' '[:upper:]')_OVMF.fd
run: image
	@echo "==> Launching QEMU for $(ARCH)..."
ifeq ($(ARCH),x86_64)
	qemu-system-x86_64 -cpu qemu64 -bios $(OVMF) -drive file=$(DISK_IMG),if=ide
else ifeq ($(ARCH),arm)
	qemu-system-arm -M virt -cpu cortex-a15 -bios $(OVMF) -drive file=$(DISK_IMG),if=sd,format=raw
endif

################################################################################
# Mount Disk Image for Manual Editing
################################################################################
.PHONY: mount-img
mount-img: $(DISK_IMG)
	@echo "==> Mounting disk image for editing..."
	@mkdir -p $(MNT_DIR)
	@bash -c '\
		set -e; \
		LOOPDEV=$$(sudo losetup --find --show -P $(DISK_IMG)); \
		echo $$LOOPDEV > $(LOOPFILE); \
		sudo mount -t vfat "$${LOOPDEV}p1" $(MNT_DIR); \
		echo "Disk image mounted at $(MNT_DIR).";'

################################################################################
# Unmount Disk Image
################################################################################
.PHONY: umount-img
umount-img:
	@echo "==> Unmounting disk image..."
	@bash -c '\
		set -e; \
		sudo umount $(MNT_DIR); \
		LOOPDEV=$$(cat $(LOOPFILE)); \
		sudo losetup -d $$LOOPDEV; \
		rm -f $(LOOPFILE); \
		echo "Disk image unmounted."'

################################################################################
# Clean Up Build Artifacts
################################################################################
clean:
	@echo "==> Cleaning up build artifacts..."
	$(MAKE) -C $(ARCH_DIR)/bootloader clean
	$(MAKE) -C $(ARCH_DIR)/kernel clean
	rm -f $(TARGET_BOOT) $(TARGET_KERNEL) $(DISK_IMG)
	@rm -rf $(BUILD_DIR)
	@echo "Cleanup complete."
