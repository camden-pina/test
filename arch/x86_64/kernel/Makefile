include ../../../lib/Makefile

SRC_C	= $(wildcard *.c *.c */*.c */*/*.c)
SRC_S	= $(wildcard *.s **/*.s)
OBJ_C	= $(patsubst %.c,%.o,$(SRC_C))
OBJ_S	= $(patsubst %.s,%.o,$(SRC_S))

CC	= x86_64-elf-gcc
CFLAGS	= -O3 -Wall -Wextra -pedantic -Wshadow -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls -Wnested-externs -Winline -Wno-long-long -Wuninitialized -Wstrict-prototypes -Wunsafe-loop-optimizations -fno-builtin -fno-stack-protector -ffreestanding -nostdlib -mno-red-zone

INC	= -I $(lib_dir)/libk/include/ -I ./include

AS	= $(CC)

%.o: %.c
	@$(CC) $(INC) $(CFLAGS) $(lib-gnuefi) $< -c -o $@

%.o: %.s
	@$(AS) -D__ASSEMBLY__ $< -c -o $@

.PHONY: compile_kernel
compile_kernel: $(OBJ_C) $(OBJ_S)
