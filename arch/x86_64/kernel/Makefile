################################################################################
# Kernel Makefile for $(ARCH) (Example for x86_64)
# Recursively finds C and assembly source files in the current directory and all
# subdirectories, excluding files whose names start with "._".
################################################################################

# Use the CROSS_COMPILE variable passed from the top level.
CC  := $(CROSS_COMPILE)gcc
LD  := ld

# Compiler flags.
CFLAGS  := -I../../../lib/libk/include/ -I./include -ffreestanding -O2 -Wall -Wextra -O3 -Wall -Wextra -pedantic -Wshadow -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls -Wnested-externs -Winline -Wno-long-long -Wuninitialized -Wstrict-prototypes -Wunsafe-loop-optimizations -fno-builtin -fno-stack-protector -ffreestanding -nostdlib -mno-red-zone

# Linker flags (assumes a linker script "kernel.ld" exists in this directory).
LDFLAGS := -Tkernel.ld -n -static -Bsymbolic -nostdlib -L../../../lib

# Recursively find all C source files (excluding files starting with "._").
CS_SRCS := $(shell find . -type f -name '*.c' ! -name '._*')
# Recursively find all assembly source files (excluding files starting with "._").
AS_SRCS := $(shell find . -type f -name '*.s' ! -name '._*')

# Convert source files to object files.
CS_OBJS := $(CS_SRCS:.c=.o)
AS_OBJS := $(AS_SRCS:.s=.o)

# Combined list of object files.
OBJS := $(CS_OBJS) $(AS_OBJS)

TARGET := kernel.elf

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "==> Linking kernel..."
	$(LD) $(LDFLAGS) -o $@ $(OBJS) -lk

# Rule to compile C source files.
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Rule to compile assembly source files.
%.o: %.s
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
