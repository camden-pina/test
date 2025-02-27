################################################################################
# UEFI Bootloader Makefile using EDK2 Build System (No GNU‑EFI) with Logging
################################################################################

# Set the directory where the fixed EDK2 tree should reside.
EDK2_DIR      := $(shell pwd)/edk2/
EDK2_REPO_URL := https://github.com/tianocore/edk2.git
EDK2_TAG      := edk2-stable202211
WORKSPACE	  := $(EDK2_DIR)

# Path to your platform DSC file (relative to the current directory).
MY_PLATFORM_DSC := MyPlatform.dsc

# Define a logging function that prints messages with a timestamp.
define log
	echo "[`date '+%Y-%m-%d %H:%M:%S'`] $(1)"
endef

.PHONY: edk2_setup all clean

# edk2_setup: If the EDK2 tree isn't present, clone it at the specified tag.
edk2_setup:
	@$(call log, "Checking for EDK2 tree in '$(EDK2_DIR)'...");
	@if [ ! -d "$(EDK2_DIR)" ]; then \
	  $(call log, "EDK2 tree not found. Cloning $(EDK2_TAG)..."); \
	  git clone --branch $(EDK2_TAG) --depth 1 $(EDK2_REPO_URL) $(EDK2_DIR); \
	  cd $(EDK2_DIR) && $(call log, "Updating submodules...") && git submodule update --init --recursive --depth 1; \
	  echo "Press any key to continue..."; read -n 1 -s; \
	  make -C BaseTools; \
	  export CONF_PATH=$$(pwd)/Conf; \
	  export EDK_TOOLS_PATH=$$(pwd)/BaseTools; \
	  echo $$WORKSPACE \
	  echo $$EDK_TOOLS_PATH \
	  echo $$PACKAGES_PATH \
	  echo $$CONF_PATH \
	  echo "Press any key to continue..."; read -n 1 -s; \
	  . ./edksetup.sh; \
	  echo "Press any key to continue..."; read -n 1 -s; \
	  cd ../; \
	  echo "Press any key to continue..."; read -n 1 -s; \
	  make -C edk2/BaseTools; \
	else \
	  $(call log, "EDK2 tree already exists in $(EDK2_DIR)."); \
	fi
	@if [ ! -d "$(EDK2_DIR)/MyBootloader" ]; then \
		ls && \
		cp -r MyBootloader/ $(EDK2_DIR)/; \
	else \
		$(call log, "Found MyBootloader in edk/ directory"); \
	fi

# all: Ensure edk2_setup runs, then build your bootloader via EDK2.
all: edk2_setup
	export WORKSPACE=$(WORKSPACE)
	@$(call log, "Verifying required files...")
	@if [ ! -f "$(WORKSPACE)MyBootloader/$(MY_PLATFORM_DSC)" ]; then \
	  $(call log, "ERROR: DSC file '$(MY_PLATFORM_DSC)' not found!"); \
	  exit 1; \
	fi
	@if [ ! -f "$(EDK2_DIR)/edksetup.sh" ]; then \
	  $(call log, "ERROR: edksetup.sh not found in $(EDK2_DIR)!"); \
	  exit 1; \
	fi
	@$(call log, "Starting build process for UEFI Bootloader using EDK2...")
	@cd $(EDK2_DIR) && \
	  export EDK_TOOLS_PATH=$$(pwd)/BaseTools && \
	  export CONF_PATH=$$(pwd)/Conf && \
	  if [ ! -d "$$(pwd)/BaseTools" ]; then \
	    $(call log, "ERROR: BaseTools directory not found in EDK2 tree!"); \
	    exit 1; \
	  fi && \
	  $(call log, "Sourcing edksetup.sh...") && \
	  . ./edksetup.sh && \
	  $(call log, "Running EDK2 build command...") && \
	  cd ../../ && \
	  build -a X64 -t GCC5 -b RELEASE -p MyBootloader/$(MY_PLATFORM_DSC)
	@if [ $$? -ne 0 ]; then \
	  $(call log, "Build failed!"); \
	  exit 1; \
	fi
	@$(call log, "UEFI Bootloader built successfully.")

# clean: Use EDK2's clean command.
clean:
	@$(call log, "Cleaning EDK2 build artifacts...")
	@cd $(EDK2_DIR) && . ./edksetup.sh && build -a X64 -t GCC5 -b RELEASE clean
	@$(call log, "Clean complete.")