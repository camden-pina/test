#############################################################
#	This is the only file that can be included
#	within another makefile without the build
#	system corrupting. Maybe there will be a
#	more modular way in the future, but because
#	of the time crunch, this will do for now.
#############################################################


#############################################################
#	Special variables holding the location of
#	this directory on the computer. Allows inc
#	variables to be accessed from any Makefile
#	within the project.
#############################################################

mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
lib_dir := $(patsubst %/,%,$(dir $(mkfile_path)))

lib-gnuefi = -I $(lib_dir)/gnu-efi-3.0.12/inc/ -I $(lib_dir)/gnu-efi-3.0.12/inc/x86_64/ -I $(lib_dir)/gnu-efi-3.0.12/inc/protocol

$(lib_dir)/lib-gnuefi.o:
	@(x86_64-w64-mingw32-gcc -ffreestanding $(lib-gnuefi) -c -o $(lib_dir)/lib-gnuefi.o $(lib_dir)/gnu-efi-3.0.12/lib/data.c)

$(lib_dir)/lib-libk.a:
	@$(MAKE) -C $(lib_dir)/libk
