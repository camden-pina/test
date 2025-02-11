#############################################################
#	Preset value of 'arch' in case value isn't
#	supplied at compile time
#############################################################

ifndef arch
override arch=x86_64
endif


#############################################################
#	Sort compilation for different architectures
#############################################################

bootloader:
ifeq ($(arch),x86_64)
	@$(MAKE) -C x86_64 compile_bootloader
else ifeq ($(arch),aarch64)
	@$(MAKE) -C aarch64 compile_bootloader
	$(info arch=$(arch))
else ifeq ($(arch),)
	$(info arch=$(arch))
else
	$(error Unknown Architecture: $(arch))
endif

kernel:
ifeq ($(arch),x86_64)
	@$(MAKE) -C x86_64 compile_kernel
else ifeq ($(arch),aarch64)
	@$(MAKE) -C aarch64 compile_kernel
else ifeq ($(arch),)
	$(info arch=$(arch))
else
	$(error Unknown Architecture: $(arch))
endif
