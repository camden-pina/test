TARGET	= libk.a
SRC_C	= $(wildcard *.c **/*.c)
OBJ_C	= $(patsubst %.c,%.o,$(SRC_C))

CC	= x86_64-elf-gcc
AS	= x86_64-elf-as
AR	= x86_64-elf-ar

INC	= -I include/

all: $(TARGET)

$(TARGET): $(OBJ_C) crt0.o
	@$(AR) rcs ../libk.a $(OBJ_C) $(OBJ_S)
crt0.o: crt0.s
	@$(AS) $< -o $@

%.o: %.c
	@$(CC) -nostdlib -ffreestanding $(INC) $< -c -o $@
	
.PHONY: clean
clean:
	$(shell rm -f *.o)
	$(shell rm -f ../$(TARGET))
