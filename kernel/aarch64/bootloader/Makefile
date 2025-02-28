include ../../../lib/Makefile

BUILD_PATHS_CC=$(wildcard **/*.c)
BUILD_PATHS_AS=$(wildcard **/*.s)

BUILD_CC = arm-none-eabi-gcc
CC_FLAGS = -mcpu=cortex-a7 -fpic -ffreestanding -O2 -Wall -Wextra

BUILD_AS = arm-none-eabi-gcc
AS_FLAGS = -mcpu=cortex-a7 -fpic -ffreestanding

%.o: %.c
	@$(BUILD_CC) $(CC_FLAGS) $< -c -o $@
	
%.o: %.s
	@$(BUILD_AS) $(AS_FLAGS) $< -c -o $@

.PHONY: compile_bootloader
compile_bootloader: $(patsubst %.c,%.o,$(BUILD_PATHS_CC)) $(patsubst %.s,%.o,$(BUILD_PATHS_AS))
