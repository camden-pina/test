# ============================================
# User Configuration (override these as needed)
# ============================================
PROJECT_DIR   ?= $(shell pwd)
BUILD_DIR     ?= $(PROJECT_DIR)/build
TOOL_ROOT     ?= $(BUILD_DIR)/toolchain
SYS_ROOT      ?= $(PROJECT_DIR)/sysroot

NPROC         ?= $(shell nproc)
MAKE          := make
MAKE_J        := $(MAKE) -j$(NPROC)

# Colors for messages
CYAN          := \033[36m
RESET         := \033[0m

# EDK2 repository settings
EDK2_VERSION  ?= edk2-stable202211
EDK2_REPO     ?= https://github.com/tianocore/edk2.git
EDK2_DIR      ?= $(PROJECT_DIR)/edk2

# Build control flags (set these to 1 to skip steps)
SKIP_CONFIGURE ?=
SKIP_BUILD     ?=
SKIP_INSTALL   ?=

# Build options for EDK2
BUILD_TYPE    ?= RELEASE# or DEBUG
ARCH          ?= X64        # Supported values: X64 or IA32
TOOLCHAIN     ?= GCC5# Using GCC toolchain

ARCH := $(strip $(ARCH))

# ============================================
# Phony targets
# ============================================
.PHONY: help setup copy_arch build install bootloader symlink all clean

# --------------------------------------------
# help - Show usage information
# --------------------------------------------
help:
	@echo "$(CYAN)Usage:$(RESET)"
	@echo "  make setup                         - Clone and prepare the EDK2 repository and build BaseTools."
	@echo "  make bootloader [ARCH=<arch>]      - Build the bootloader for the specified architecture (default ARCH=X64)."
	@echo "  make build                         - Build the bootloader using MyPlatform.dsc and MyBootloader.inf in the ARCH directory."
	@echo "  make install                       - Copy the generated bootable EFI file to the build directory."
	@echo "  make symlink SRC=<src> DEST=<dest>   - Create a relative symlink from SRC to DEST."
	@echo "  make all                           - Run setup, build, and install sequentially."
	@echo "  make clean                         - Clean up build artifacts and EDK2 build files."

# --------------------------------------------
# setup - Clone and set up EDK2 repository and build BaseTools
# --------------------------------------------
setup:
	@echo "$(CYAN)[Setup] Creating build directory...$(RESET)"
	@mkdir -p $(BUILD_DIR)
	@if [ ! -d "$(EDK2_DIR)" ]; then \
	  echo "$(CYAN)[Setup] Cloning EDK2 repository (tag $(EDK2_VERSION)) from $(EDK2_REPO)...$(RESET)"; \
	  git clone --depth 1 -c advice.detachedHead=false --branch $(EDK2_VERSION) $(EDK2_REPO) $(EDK2_DIR); \
	  cd $(EDK2_DIR) && git submodule update --init; \
	else \
	  echo "$(CYAN)[Setup] EDK2 repository already exists at $(EDK2_DIR).$(RESET)"; \
	fi
	@echo "$(CYAN)[Setup] Building BaseTools...$(RESET)"
	@cd $(EDK2_DIR) && . ./edksetup.sh && $(MAKE) -C BaseTools all

# --------------------------------------------
# copy_arch - Copy architecture-specific bootloader files to the EDK2 directory
# --------------------------------------------
copy_arch:
	@if [ ! -d "$(ARCH)" ]; then \
	  echo "$(CYAN)[Error] Directory '$(ARCH)' not found in $(PROJECT_DIR).$(RESET)"; \
	  exit 1; \
	fi
	@echo "$(CYAN)[Copy] Copying $(ARCH) to $(EDK2_DIR)/$(ARCH)...$(RESET)"
	@mkdir -p $(EDK2_DIR)
	@rm -rf $(EDK2_DIR)/$(ARCH)
	@cp -r $(ARCH) $(EDK2_DIR)/

# --------------------------------------------
# build - Build the bootloader using MyPlatform.dsc
# --------------------------------------------
build:
	@if [ ! -d "$(EDK2_DIR)" ]; then \
	  echo "$(CYAN)[Build] EDK2 directory not found. Running setup...$(RESET)"; \
	  $(MAKE) setup; \
	fi
	if [ "$(SKIP_BUILD)" = "1" ]; then \
	  echo "$(CYAN)[Build] Skipping build step as SKIP_BUILD is set.$(RESET)"; \
	else \
	  echo "$(CYAN)[Build] Building bootloader (ARCH=$(ARCH), BUILD_TYPE=$(BUILD_TYPE), TOOLCHAIN=$(TOOLCHAIN))...$(RESET)"; \
	  cd $(EDK2_DIR) && bash -c '. ./edksetup.sh --reconfig; export GCC5_BIN=x86_64-elf-; build -p "$(ARCH)/MyPlatform.dsc" -a $(ARCH) -t $(TOOLCHAIN) -b $(BUILD_TYPE)'; \
	fi
	@echo "$(CYAN)[Build] Build step completed.$(RESET)"
	@$(MAKE) install

# --------------------------------------------
# install - Copy the generated bootable EFI file to the build directory
# --------------------------------------------
install:
	if [ "$(SKIP_INSTALL)" = "1" ]; then \
	  echo "$(CYAN)[Install] Skipping install step as SKIP_INSTALL is set.$(RESET)"; \
	else \
	  echo "$(CYAN)[Install] Installing output files...$(RESET)"; \
	  cp $(EDK2_DIR)/Build/Bootloader_X64/Output/$(BUILD_TYPE)_$(TOOLCHAIN)/$(ARCH)/BOOTX64.efi $(BUILD_DIR)/BOOTX64.EFI; \
	  echo "$(CYAN)[Install] Copied BOOTX64.EFI to $(BUILD_DIR)/BOOTX64.EFI$(RESET)"; \
	fi

# --------------------------------------------
# bootloader - Build the bootloader (sets up, copies ARCH folder, builds, and installs)
# --------------------------------------------
bootloader: setup copy_arch build

# --------------------------------------------
# all - Run setup, build, and install sequentially
# --------------------------------------------
all: setup build install

# --------------------------------------------
# symlink - Create a relative symbolic link
# --------------------------------------------
symlink:
	@if [ -z "$(SRC)" ] || [ -z "$(DEST)" ]; then \
	  echo "Usage: make symlink SRC=<source file> DEST=<destination file>"; \
	  exit 1; \
	fi
	@mkdir -p $(dir $(DEST))
	@ln -sf $(shell realpath --relative-to=$(dir $(DEST)) $(SRC)) $(DEST)
	@echo "$(CYAN)[Symlink] Created symlink: $(DEST) -> $(SRC)$(RESET)"

# --------------------------------------------
# clean - Clean up the build directory and EDK2 build artifacts
# --------------------------------------------
clean:
	@echo "$(CYAN)[Clean] Cleaning up build artifacts in $(BUILD_DIR)...$(RESET)"
	@rm -rf $(BUILD_DIR)
	@echo "$(CYAN)[Clean] Cleaning up EDK2 build artifacts in $(EDK2_DIR)...$(RESET)"
	@cd $(EDK2_DIR) && . ./edksetup.sh && build -p "$(ARCH)/MyPlatform.dsc" -a $(ARCH) -t $(TOOLCHAIN) -b $(BUILD_TYPE) clean

