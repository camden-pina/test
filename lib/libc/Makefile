# ---------- toolchain -------------------------------------------------
TRIPLE   ?= x86_64-elf
AR       :=$(TRIPLE)-ar
AS       :=$(TRIPLE)-as
CC       :=$(TRIPLE)-gcc
LD       :=$(TRIPLE)-ld
OBJCOPY  :=$(TRIPLE)-objcopy

# ---------- flags -----------------------------------------------------
CFLAGS := -ffreestanding -fno-stack-protector -nostdlib \
          -Wall -Wextra -O0 -g -static -fno-pie -no-pie -fno-omit-frame-pointer
INC    := -Iinclude

# ---------- sources & objects ----------------------------------------
LIB   := libc.a
CRT0  := src/crt0.o                       # special‑case start‑file

SRC_C := $(wildcard src/*.c)
SRC_S := $(filter-out src/crt0.s,$(wildcard src/*.s))  # all .s except crt0

OBJ_C := $(SRC_C:.c=.o)
OBJ_S := $(SRC_S:.s=.o)
OBJ   := $(OBJ_C) $(OBJ_S)                # objects that go into libc.a

# ---------- top‑level targets ----------------------------------------
all: $(LIB)

clean:
	$(RM) $(OBJ) $(CRT0) $(LIB) examples/*.elf

# ---------- static libc ----------------------------------------------
$(LIB): $(OBJ)
	$(AR) rcs $@ $^

# ---------- pattern rules --------------------------------------------
# C sources
src/%.o: src/%.c include/*.h
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

# generic .s sources (everything except crt0.s, which has its own rule)
src/%.o: src/%.s
	$(CC) $(CFLAGS) -c $< -o $@

# assemble crt0.s with the same flags but keep it outside the archive
$(CRT0): src/crt0.s
	$(CC) $(CFLAGS) -c $< -o $@

# ---------- demo programme -------------------------------------------
EXE := examples/hello.elf

examples/hello.elf: examples/hello.c $(CRT0) $(LIB)
	$(CC) $(CFLAGS) -static -z max-page-size=0x1000 $(INC) -Tlinker.ld $^ -o $@

.PHONY: all clean
