include ../../../lib/Makefile

# Compiler and flags (can be overridden by the top-level makefile)
BUILD_CC = x86_64-elf-gcc
CC_FLAGS = -ffreestanding -O2 -Wall -Wextra -mno-red-zone \
           -I/usr/include/efi -I/usr/include/efi/x86_64 \
           -fno-stack-protector -fpic -fshort-wchar

# Linking flags for GNU-EFI
LDFLAGS = -nostdlib -znocombreloc -T bootloader.lds -shared -Bsymbolic \
          -L/usr/lib -lefi -lgnuefi

# Source and object directories
SRC_DIR = src
OBJ_DIR = obj

# Find all source files recursively
BUILD_PATHS = $(wildcard $(SRC_DIR)/**/*.c)
OBJECTS = $(patsubst %.c,%.o,$(BUILD_PATHS))

# Pattern rule to compile object files
%.o: %.c
	@$(BUILD_CC) $(CC_FLAGS) $(lib-gnuefi) $< -c -o $@

# Bootloader compilation target
.PHONY: compile_bootloader
compile_bootloader: $(OBJECTS)
	$(BUILD_CC) $(OBJECTS) $(LDFLAGS) -o bootloader.efi

clean:
	rm -rf $(OBJ_DIR) bootloader.efi
