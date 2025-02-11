include ../../../lib/Makefile

SRC_C	= $(wildcard *.c **/*.c)
SRC_S	= $(wildcard *.s **/*.s)
OBJ_C	= $(patsubst %.c,%.o,$(SRC_C))
OBJ_S	= $(patsubst %.s,%.o,$(SRC_S))

CC	= arm-none-eabi-gcc
CFLAGS	= -mcpu=cortex-a7 -fpic -ffreestanding
INC	= -I $(current_dir)/libk/include/

AS	= arm-none-eabi-gcc

%.o: %.c
	@$(CC) $(CFLAGS) $< -c -o $@

%.o: %.s
	@$(AS) $< -c -o $@

.PHONY: compile_kernel
compile_kernel: $(OBJ_C) $(OBJ_S)
