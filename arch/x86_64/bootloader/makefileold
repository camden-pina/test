include ../../../lib/Makefile

BUILD_PATHS=$(wildcard **/*.c)
OBJ_DIR=../obj

BUILD_CC = x86_64-w64-mingw32-gcc
CC_FLAGS = -ffreestanding -I include

%.o: %.c
	@$(BUILD_CC) $(CC_FLAGS) $(lib-gnuefi) $< -c -o $@

.PHONY: compile_bootloader
compile_bootloader: $(patsubst %.c,%.o,$(BUILD_PATHS))
