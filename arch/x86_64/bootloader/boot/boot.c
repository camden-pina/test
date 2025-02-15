#include <elf.h>
#include "../../kernel/include/kernel.h"

#include <efi.h>
#include <efilib.h>

#include <stdint.h>
#include <stddef.h>

EFI_STATUS Status;

void* memcpy(void* destination, const void* source, unsigned long long length)
{
	for (unsigned long long i = 0; i < length; ++i) {
		((unsigned char*)destination)[i] = ((unsigned char*)source)[i];
	}
	return destination;
}

void* memset(void* buffer, int value, unsigned long long length)
{
	for (unsigned long long i = 0; i < length; ++i) {
		((unsigned char*)buffer)[i] = (unsigned char)value;
	}
	return buffer;
}

int memcmp(const void* aptr, const void* bptr, unsigned long long size)
{
	const unsigned char* a = (const unsigned char*)aptr;
	const unsigned char* b = (const unsigned char*)bptr;
	for (unsigned long long i = 0; i < size; i++) {
		if (a[i] < b[i])
			return -1;
		else if (b[i] < a[i])
			return 1;
	}
	return 0;
}

EFI_STATUS PANIC(CHAR16* str)
{
	ST->ConOut->OutputString(ST->ConOut, str);
	ST->ConOut->OutputString(ST->ConOut, L"    Press Any Key To Reboot");

	EFI_INPUT_KEY Key;
	ST->ConIn->Reset, 2, ST->ConIn, FALSE;
	while ((Status = ST->ConIn->ReadKeyStroke(ST->ConIn, &Key)) == EFI_NOT_READY);
	return Status;
}

void PRINT_EFI(EFI_STATUS Status)
{
	if (Status == EFI_SUCCESS)
		ST->ConOut->OutputString(ST->ConOut, L"EFI_SUCCESS");
	if (Status == EFI_BUFFER_TOO_SMALL)
		ST->ConOut->OutputString(ST->ConOut, L"EFI_BUFFER_TOO_SMALL");
	if (Status == EFI_INVALID_PARAMETER)
		ST->ConOut->OutputString(ST->ConOut, L"EFI_INVALID_PARAMETER");
	if (Status == EFI_OUT_OF_RESOURCES)
		ST->ConOut->OutputString (ST->ConOut, L"EFI_OUT_OF_RESOURCES");
	if (Status == EFI_NOT_FOUND)
		ST->ConOut->OutputString(ST->ConOut, L"EFI_NOT_FOUND");
}

EFI_FILE* fops_open_file(EFI_FILE* root_dir, CHAR16* filename, UINT64 mode, UINT64 attributes) {
	EFI_STATUS Status = root_dir->Open(root_dir, &root_dir, filename, mode, attributes);

	if (Status == EFI_SUCCESS) {
		return root_dir;
	}
	else {
		ST->ConOut->OutputString(ST->ConOut, L"fops_open_file() error:");
		return NULL;
	}
}

EFI_FILE* fops_open_kernel(CHAR16* kernel_fname) {
	EFI_FILE* kernel_file = NULL;
	UINTN handleCount = 0;
	EFI_HANDLE* handleBuffer = (void*)0;
	EFI_GUID simpleFileSystemProtocol = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

	ST->ConOut->OutputString(ST->ConOut, L"Locating Simple File System Handle");
	Status = ST->BootServices->LocateHandleBuffer(
		ByProtocol,
		&simpleFileSystemProtocol,
		(void*)0,
		&handleCount,
		&handleBuffer);
	if (EFI_ERROR(Status))
		PANIC(L"    [FAILED]");
	ST->ConOut->OutputString(ST->ConOut, L"    [SUCCESS]\n\r");

	ST->ConOut->OutputString(ST->ConOut, L"Loading Handles");
	for (unsigned long long i = 0; i < handleCount; ++i)
	{

		EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fileSystem = (void*)0;

		ST->ConOut->OutputString(ST->ConOut, L"Looking For File System");
		Status = ST->BootServices->HandleProtocol(
			handleBuffer[i],
			&simpleFileSystemProtocol,
			(VOID**)&fileSystem);
		if (EFI_ERROR(Status))
		{
			ST->ConOut->OutputString(ST->ConOut, L"    [FAILED] No File System Detected on Handle\n\r");
			continue;
		}
		ST->ConOut->OutputString(ST->ConOut, L"    [SUCCESS]\n\r");

		EFI_FILE_PROTOCOL* root = (void*)0;

		ST->ConOut->OutputString(ST->ConOut, L"Opening Volume");
		Status = fileSystem->OpenVolume(fileSystem, &root);
		if (EFI_ERROR(Status))
		{
			ST->ConOut->OutputString(ST->ConOut, L"    [FAILED] Volume Opened With An Error\n\r");
			continue;
		}
		ST->ConOut->OutputString(ST->ConOut, L"    [SUCCESS]\n\r");

		ST->ConOut->OutputString(ST->ConOut, L"Loading Asset");
		kernel_file = fops_open_file(root, kernel_fname, EFI_FILE_MODE_READ, 0);
		if (kernel_file == NULL)
		{
			ST->ConOut->OutputString(ST->ConOut, L"    [FAILED] Load Asset\n\r");
			continue;
		}
		ST->ConOut->OutputString(ST->ConOut, L"    [SUCCESS]\n\r");

		return kernel_file;
	}
	ST->ConOut->OutputString(ST->ConOut, L"    [FAILED] Asset Not Found\n\r");
	return (void*)0;
}

/*static _Bool valid_elf64_header(Elf64_Ehdr* elf_hdr) {
	// Check header
	if (elf_hdr->e_ident[EI_MAG0] != ELFMAG0 ||
		elf_hdr->e_ident[EI_MAG1] != ELFMAG1 ||
		elf_hdr->e_ident[EI_MAG2] != ELFMAG2 ||
		elf_hdr->e_ident[EI_MAG3] != ELFMAG3) return 0;

	// Check for 64-bit
	if (elf_hdr->e_ident[EI_CLASS] != ELFCLASS64) return 0;

	// Check for System V (Unix) ABI
	if (elf_hdr->e_ident[EI_OSABI] != ELFOSABI_NONE &&
		elf_hdr->e_ident[EI_OSABI] != ELFOSABI_GNU) return 0;

	// Check for executable file
	if (elf_hdr->e_type != ET_EXEC) return 0;

	// Check for x86_64 architecture
	if (elf_hdr->e_machine != EM_X86_64) return 0;

	// All valid
	return 1;
}*/

/*
gcc -I/home/camdenp/ModernOS/PonchoOS-main/gnu-efi//bootloader -I/home/camdenp/ModernOS/PonchoOS-main/gnu-efi//bootloader/../inc -I/home/camdenp/ModernOS/PonchoOS-main/gnu-efi//bootloader/../inc/x86_64 -I/home/camdenp/ModernOS/PonchoOS-main/gnu-efi//bootloader/../inc/protocol -Wno-error=pragmas -mno-red-zone -mno-avx -fpic  -g -O2 -Wall -Wextra -Werror -fshort-wchar -fno-strict-aliasing -ffreestanding -fno-stack-protector -fno-stack-check -fno-stack-check -fno-merge-all-constants -Wno-error=unused-parameter -Wno-error=unused-variable -DCONFIG_x86_64 -DGNU_EFI_USE_MS_ABI -maccumulate-outgoing-args --std=c11 -D__KERNEL__ -I/usr/src/sys/build/include -c /home/camdenp/ModernOS/PonchoOS-main/gnu-efi//bootloader/main.c -o main.o
ld -nostdlib --warn-common --no-undefined --fatal-warnings --build-id=sha1 -shared -Bsymbolic -L../lib -L../gnuefi ../gnuefi/crt0-efi-x86_64.o main.o -o main.so -lefi -lgnuefi /usr/lib/gcc/x86_64-linux-gnu/9/libgcc.a -T /home/camdenp/ModernOS/PonchoOS-main/gnu-efi//bootloader/../gnuefi/elf_x86_64_efi.lds
objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel \
	    -j .rela -j .rel.* -j .rela.* -j .rel* -j .rela* \
	    -j .reloc --target efi-app-x86_64 main.so main.efi

*/

static UINTN round_up(UINTN x, UINTN align) {
    return (x + align - 1) & -align;
}

static inline void
stosb(void *addr, int data, int cnt)
{
  asm volatile("cld; rep stosb" :
               "=D" (addr), "=c" (cnt) :
               "0" (addr), "1" (cnt), "a" (data) :
               "memory", "cc");
}

EFI_STATUS load_kernel(CHAR16* kernel_fname, OUT void** entry_address) {
	EFI_FILE* Kernel = NULL;

	Kernel = fops_open_kernel(kernel_fname);

	ST->ConOut->OutputString(ST->ConOut, L"Loading ELF Header\n\r");

	// load the elf header from the kernel
	Elf64_Ehdr header;
	{
		UINTN FileInfoSize;
		EFI_FILE_INFO *FileInfo;
		Kernel->GetInfo(Kernel, &gEfiFileInfoGuid, &FileInfoSize, NULL);
		Status = ST->BootServices->AllocatePool(EfiLoaderData, FileInfoSize, (void**)&FileInfo);
		if (EFI_ERROR(Status))
			PANIC(L"Error Reading Header");
		
		ST->ConOut->OutputString(ST->ConOut, L"EEER\n\r");
		Kernel->GetInfo(Kernel, &gEfiFileInfoGuid, &FileInfoSize, (void**)&FileInfo);
		ST->ConOut->OutputString(ST->ConOut, L"DSSD\n\r");
		UINTN size = sizeof(header);
		ST->ConOut->OutputString(ST->ConOut, L"HDSJ\n\r");
		Kernel->Read(Kernel, &size, &header);
		ST->ConOut->OutputString(ST->ConOut, L"OKDS\n\r");
	}

	ST->ConOut->OutputString(ST->ConOut, L"Verfiying ELF Binary\n\r");
	// verify the kernel binary
	if (
		memcmp(&header.e_ident[EI_MAG0], ELFMAG, SELFMAG) != 0 ||
		header.e_ident[EI_CLASS] != ELFCLASS64 ||
		header.e_ident[EI_DATA] != ELFDATA2LSB ||
		header.e_type != ET_EXEC ||
		header.e_machine != EM_AMD64 ||
		header.e_version != EV_CURRENT
	) {
		ST->ConOut->OutputString(ST->ConOut, L"kernel format is bad\r\n");
		return -1;
	}

	ST->ConOut->OutputString(ST->ConOut, L"Loading ELF Program Headers\n\r");
	// load the kernel segment headers
	Elf64_Phdr *phdrs;
	{
		Kernel->SetPosition(Kernel, header.e_phoff);
		UINTN size = header.e_phnum * header.e_phentsize;
		Status = ST->BootServices->AllocatePool(EfiLoaderData, size, (void**)&phdrs);
		if (EFI_ERROR(Status))
			PANIC(L"Error Reading Program Headers!");
			
		Kernel->Read(Kernel, &size, phdrs);
	}

	ST->ConOut->OutputString(ST->ConOut, L"Loading ELF Binary\n\r");
	// load the actual kernel binary based on its segment headers
	for (
		Elf64_Phdr *phdr = phdrs;
		(char*)phdr < (char*)phdrs + header.e_phnum * header.e_phentsize;
		phdr = (Elf64_Phdr*)((char*)phdr + header.e_phentsize)
	) {
		switch (phdr->p_type)
		{
			case PT_LOAD:
			{
				UINTN /*Elf64_Addr*/ segment = phdr->p_paddr;
				if (phdr->p_filesz != 0)
				{
					int pages = (phdr->p_memsz + 0x1000 - 1) / 0x1000; // round up
					Status = ST->BootServices->AllocatePages(AllocateAddress, EfiLoaderData, pages, &segment);
					if (EFI_ERROR(Status))
						PANIC(L"Error Allocating Pages for Kernel!");
					
					Kernel->SetPosition(Kernel, phdr->p_offset);
					UINTN size = phdr->p_filesz;
					Kernel->Read(Kernel, &size, (void*)segment);
				}
				if (phdr->p_filesz < phdr->p_memsz)
					stosb((uint8_t*)phdr->p_paddr + phdr->p_filesz, 0, phdr->p_memsz - phdr->p_filesz);
			}
		}
	}
ST->ConOut->OutputString(ST->ConOut, L"ehyuh");

	ST->ConOut->OutputString(ST->ConOut, L"Reading ELF Entry Address\n\r");
	*entry_address = (void*)header.e_entry;

	ST->ConOut->OutputString(ST->ConOut, L"Retutning From ELF Read Function\n\r");
	return EFI_SUCCESS;
}

// ##############################
//			  vesa.c
// ##############################
#define GRAPHICS_MOST_APPROPRIATE_W 3840
#define GRAPHICS_MOST_APPROPRIATE_H 2160

struct graphicsInfo {
	EFI_GRAPHICS_OUTPUT_PROTOCOL* protocol;
	EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  outputMode;
	void* bufferBase;
	size_t                                 bufferSize;
} graphicsInfo;

EFI_STATUS Vesa_Select_Mode(EFI_GRAPHICS_OUTPUT_PROTOCOL* graphics, OUT unsigned int* mode)
{
	*mode = graphics->Mode->Mode;
	EFI_GRAPHICS_OUTPUT_MODE_INFORMATION most_approporiate_info;
	EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info;
	UINTN size;

	// Init info of current mode
	Status = graphics->QueryMode(graphics, *mode, &size, &info);
	//ASSERT_EFI_STATUS(Status, L"select_mode");
	most_approporiate_info = *info;

	for (UINT32 i = 0; i < graphics->Mode->MaxMode; i += 1)
	{
		Status = graphics->QueryMode(graphics, i, &size, &info);
		//ASSERT_EFI_STATUS(Status, L"select_mode");
		if (info->PixelFormat != PixelRedGreenBlueReserved8BitPerColor &&
			info->PixelFormat != PixelBlueGreenRedReserved8BitPerColor) continue;
		if (info->HorizontalResolution > GRAPHICS_MOST_APPROPRIATE_W ||
			info->VerticalResolution > GRAPHICS_MOST_APPROPRIATE_H) continue;

		if (info->VerticalResolution == GRAPHICS_MOST_APPROPRIATE_H &&
			info->HorizontalResolution == GRAPHICS_MOST_APPROPRIATE_W)
		{
			most_approporiate_info = *info;
			*mode = i;
			break;
		}
		if (info->VerticalResolution > most_approporiate_info.VerticalResolution)
		{
			most_approporiate_info = *info;
			*mode = i;
		}
	}
	graphicsInfo.outputMode = most_approporiate_info;
	return EFI_SUCCESS;
}

EFI_STATUS Vesa_Init(EFI_SYSTEM_TABLE* ST)
{
	EFI_GRAPHICS_OUTPUT_PROTOCOL* graphics;
	EFI_GUID graphics_proto = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	ST->BootServices->LocateProtocol(&graphics_proto, (void*)0, (void**)&graphics);

	unsigned int new_mode;
	Status = Vesa_Select_Mode(graphics, &new_mode);
	//ASSERT_EFI_STATUS(Status, L"init_grapphics select_mode");
	Status = graphics->SetMode(graphics, new_mode);
	//ASSERT_EFI_STATUS(Status, L"init_graphics SetMode");
	graphicsInfo.protocol = graphics;
	graphicsInfo.bufferBase = (void*)graphics->Mode->FrameBufferBase;
	graphicsInfo.bufferSize = graphics->Mode->FrameBufferSize;

	return EFI_SUCCESS;
}

_Bool guids_match(EFI_GUID guid1, EFI_GUID guid2) {
  _Bool first_part_good =
      (guid1.Data1 == guid2.Data1 && guid1.Data2 == guid2.Data2 &&
       guid1.Data3 == guid2.Data3);

  if (!first_part_good) return 0;

  for (int i = 0; i < 8; ++i) {
    if (guid1.Data4[i] != guid2.Data4[i]) return 0;
  }

  return 1;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable)
{
	ST = SystemTable;
	ST->ConOut->ClearScreen(ST->ConOut);
	ST->BootServices->SetWatchdogTimer(0, 0, 0, NULL);

	flexboot_header_t* boot_hdr = (void*)0;

	// Enter Graphics Mode
	Vesa_Init(ST);
	ST->ConOut->OutputString(ST->ConOut, L"VESA Initialized\n\r");

	// Load the kernel ELF file into memory and get the entry address
	void* kernel_main_addr = NULL;
	Status = load_kernel(L"\\kernel.elf", &kernel_main_addr);
	if (Status != EFI_SUCCESS) {
		ST->ConOut->OutputString(ST->ConOut, L"Error loading kernel:");
		return EFI_ABORTED;
	}
	
	EFI_CONFIGURATION_TABLE *configuration_tables = SystemTable->ConfigurationTable;
	
	void *rsdp_ptr = NULL;
	static EFI_GUID acpi_guid = ACPI_20_TABLE_GUID;
	for (unsigned i = 0; i < SystemTable->NumberOfTableEntries; ++i) {
		if (guids_match(acpi_guid, configuration_tables[i].VendorGuid)) {
			ST->ConOut->OutputString(ST->ConOut, L"Found ACPI Table pointer");
			rsdp_ptr = configuration_tables[i].VendorTable;
		}
	}

	// Verify xdsp_address
	if (rsdp_ptr == (void*)0)
		ST->ConOut->OutputString(ST->ConOut, L"No RSDP Table Found!");
	ST->ConOut->OutputString(ST->ConOut, L"RSDP Table Found!\n\r");
	
	ST->ConOut->OutputString(ST->ConOut, L"Loading Memory Map\n\r");

	// Get Memory Map
	void* memoryMap = (void*)0;
	UINTN mapSize = 0;
	UINTN mapKey = 0;
	UINTN descriptorSize = 0;
	unsigned int descriptorVersion = 0;

	Status = ST->BootServices->GetMemoryMap(&mapSize, memoryMap, &mapKey, &descriptorSize, &descriptorVersion);
	Status = ST->BootServices->AllocatePool(EfiLoaderData, mapSize, (void**)&memoryMap);
	Status = ST->BootServices->GetMemoryMap(&mapSize, memoryMap, &mapKey, &descriptorSize, &descriptorVersion);
	
	/*
	Status = uefi_call_wrapper(SystemTable->BootServices->GetMemoryMap, 5, &mapSize, memoryMap, (void*)0, &descriptorSize, (void*)0);

	mapSize += 2 * descriptorSize;

	Status = uefi_call_wrapper(SystemTable->BootServices->AllocatePool, 3, EfiLoaderData, mapSize, (void**)&memoryMap);
	Status = uefi_call_wrapper(SystemTable->BootServices->GetMemoryMap, 5, &mapSize, memoryMap, &mapKey, &descriptorSize, (unsigned int*)&descriptorVersion);
	*/

	Status = SystemTable->BootServices->ExitBootServices(ImageHandle, mapKey);
	if (EFI_ERROR(Status))
		return EFI_SUCCESS;
	
	boot_hdr->memoryMap = memoryMap;
	boot_hdr->mapSize = mapSize;
	boot_hdr->mapKey = mapKey;
	boot_hdr->descriptorSize = descriptorSize;
	boot_hdr->descriptorVersion = descriptorVersion;
	
	boot_hdr->fb = graphicsInfo.bufferBase;
	boot_hdr->fb_w = graphicsInfo.outputMode.HorizontalResolution;
	boot_hdr->fb_h = graphicsInfo.outputMode.VerticalResolution;
	boot_hdr->fb_bpp = graphicsInfo.outputMode.PixelFormat;
	boot_hdr->fb_pps = graphicsInfo.outputMode.PixelsPerScanLine;
	boot_hdr->fb_sz = graphicsInfo.bufferSize;
	
	boot_hdr->rsdp_addr = (unsigned long long*)rsdp_ptr;

	// Jump to kernel
	((__attribute__((sysv_abi)) void (*)(flexboot_header_t*))kernel_main_addr)(
		boot_hdr
	);

	PANIC(L"Error Loading Kernel");

	return EFI_SUCCESS;
}

/*
	// Get the required memory pool size for the memory map...
	result = uefi_call_wrapper(SystemTable->BootServices->GetMemoryMap, 5, &efiMemoryMap.mapSize, efiMemoryMap.memoryMap, (void*)0, &efiMemoryMap.descriptorSize, (void*)0);

	// Allocating the pool creates at least one new descriptor... for the chunk of memory changed to EfiLoaderData
	// Not sure that UEFI firmware must allocate on a memory type boundry... if not, then two descriptors might be created
	efiMemoryMap.mapSize += 2 * efiMemoryMap.descriptorSize;

	// Get a pool of memory to hold the map...
	result = uefi_call_wrapper(SystemTable->BootServices->AllocatePool, 3, EfiLoaderData, efiMemoryMap.mapSize, (void**)&efiMemoryMap.memoryMap);

	// Get the actual memory map...
	result = uefi_call_wrapper(SystemTable->BootServices->GetMemoryMap, 5, &efiMemoryMap.mapSize, efiMemoryMap.memoryMap, &efiMemoryMap.mapKey, &efiMemoryMap.descriptorSize, (unsigned int*)&efiMemoryMap.descriptorVersion);

	// ExitBootServices
	result = uefi_call_wrapper(SystemTable->BootServices->ExitBootServices, 2, ImageHandle, efiMemoryMap.mapKey);
	if (EFI_ERROR(result))
		return EFI_SUCCESS;
*/

/*
	// Jump to kernel
	((KernelMainFunc)kernel_main_addr)(1,
		(unsigned long long*)graphicsInfo.bufferBase,
		(unsigned long long)graphicsInfo.bufferSize,
		(unsigned long long)graphicsInfo.outputMode.HorizontalResolution,
		(unsigned long long)graphicsInfo.outputMode.VerticalResolution,
		(unsigned long long)graphicsInfo.outputMode.PixelFormat,
		(unsigned long long)graphicsInfo.outputMode.PixelsPerScanLine);
*/