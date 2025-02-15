################################################################################
# Bootloader Makefile for $(ARCH) (Example for x86_64)
################################################################################

# Use the CROSS_COMPILE variable passed from the top level.
CC  := x86_64-w64-mingw32-gcc
AS  := x86_64-w64-mingw32-as
LD  := x86_64-w64-mingw32-ld

# Compiler flags.
CFLAGS  := -I/usr/include/efi -I/usr/include/efi/x86_64 -I./include -fpic -ffreestanding -fno-stack-protector -fno-stack-check -fshort-wchar -mno-red-zone -maccumulate-outgoing-args -DEFI_FUNCTION_WRAPPER
#-nostdlib -ffreestanding -O2 -fpic -Wall -Wextra -mno-red-zone \
#           -I/usr/include/efi -I/usr/include/efi/x86_64 -I./include \
#           -fno-stack-protector -fpic -fshort-wchar
# Linker flags: adjust as needed for UEFI targets.
LDFLAGS := -shared -Bsymbolic -nostdlib -znocombreloc -L../../../lib/gnu-efi-3.0.12/x86_64/lib -L../../../lib/gnu-efi-3.0.12/x86_64/gnuefi -T../../../lib/gnu-efi-3.0.12/gnuefi/elf_x86_64_efi.lds ../../../lib/gnu-efi-3.0.12/x86_64/gnuefi/crt0-efi-x86_64.o

# Find all C and assembly source files in the current directory and subdirectories,
# excluding files starting with "._" (common macOS metadata files).
C_SRCS  := $(shell find . -type f -name '*.c' ! -name '._*')
AS_SRCS := $(shell find . -type f -name '*.S' ! -name '._*')
SRCS    := $(C_SRCS) $(AS_SRCS)

# Generate object file paths in the build directory, mirroring the source directory structure.
BUILD_DIR := build
SO = main.so
OBJS      := $(patsubst ./%.c,$(BUILD_DIR)/%.o,$(C_SRCS)) \
             $(patsubst ./%.S,$(BUILD_DIR)/%.o,$(AS_SRCS))

# Target bootloader binary (name provided by the top-level).
TARGET  := $(if $(filter $(ARCH),x86_64),BOOTX64.EFI,BOOTARM.EFI)

.PHONY: all clean $(TARGET)

all: $(TARGET)

# Rule to create the build directory structure.
$(BUILD_DIR)/%/:
	@mkdir -p $@

# Pattern rule for C files.
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)/%/
	$(CC) $(CFLAGS) -c -o $@ $<

$(SO): $(OBJS)
	@echo "==> Creating Shared Object File"
	$(LD) $(LDFLAGS) $(OBJS) -o $@ -l:libgnuefi.a -l:libefi.a

# Pattern rule for assembly files.
$(BUILD_DIR)/%.o: %.S | $(BUILD_DIR)/%/
	$(AS) -o $@ $<

$(TARGET): $(OBJS) # $(SO)
	@echo "==> Objcopying..."
#	x86_64-w64-mingw32-gcc $(LDFLAGS) $(OBJS) -o $@ -l:libgnuefi.a -l:libefi.a
	x86_64-w64-mingw32-gcc -ffreestanding -nostdlib -mno-stack-arg-probe -mgeneral-regs-only -fno-asynchronous-unwind-tables -pie -s -Wl,--subsystem,10 -march=x86-64 -mtune=generic -Wall -Os -flto -fno-ident -e efi_main $(OBJS) -o $@
#	 objcopy -j .text                \
#          -j .sdata               \
#          -j .data                \
#          -j .dynamic             \
#          -j .dynsym              \
#          -j .rel                 \
#          -j .rela                \
#          -j .reloc               \
#		  --target pei-x86-64 --subsystem=10 main.so BOOTX64.EFI

clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(SO)
