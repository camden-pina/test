#include <efi.h>
#include <efilib.h>
#include <elf.h>
#include <stddef.h>
#include <stdint.h>
#include <../../kernel/include/kernel.h>   // Use unified definitions from kernel.h

#ifndef PF_X
#define PF_X (1 << 0)
#endif

// --- Linking Constants (do not change) ---
#define KERNEL_PHYSICAL 0x100000ULL
#define KERNEL_VIRTUAL  0xffffffff80000000ULL

// --- Page Table Flags & Size ---
#define PAGE_SIZE    0x1000ULL
#define PAGE_PRESENT 0x001ULL
#define PAGE_WRITE   0x002ULL
#define PAGE_PSE     (1ULL << 7)

// --- Global Variables ---
EFI_HANDLE         gImageHandle;
EFI_SYSTEM_TABLE*  st;  // We'll use 'st' for all conout calls
EFI_BOOT_SERVICES* gBootServices;

// --- Basic Memory & String Functions ---
int memcmp(const void* aptr, const void* bptr, size_t n) {
    const unsigned char* a = aptr;
    const unsigned char* b = bptr;
    for (size_t i = 0; i < n; i++) {
        if (a[i] < b[i]) { return -1; }
        else if (a[i] > b[i]) { return 1; }
    }
    return 0;
}

UINTN strcmp(CHAR8* a, CHAR8* b, UINTN length) {
    for (UINTN i = 0; i < length; ++i) {
        if (*a != *b)
            return 0;
        ++a;
        ++b;
    }
    return 1;
}

// --- EFI_PRINT: Debug helper for EFI_STATUS values ---
void EFI_PRINT(EFI_STATUS Status) {
    if (Status == EFI_SUCCESS)
        st->ConOut->OutputString(st->ConOut, L"EFI_SUCCESS");
    else if (Status == EFI_BUFFER_TOO_SMALL)
        st->ConOut->OutputString(st->ConOut, L"EFI_BUFFER_TOO_SMALL");
    else if (Status == EFI_INVALID_PARAMETER)
        st->ConOut->OutputString(st->ConOut, L"EFI_INVALID_PARAMETER");
    else if (Status == EFI_OUT_OF_RESOURCES)
        st->ConOut->OutputString(st->ConOut, L"EFI_OUT_OF_RESOURCES");
    else if (Status == EFI_NOT_FOUND)
        st->ConOut->OutputString(st->ConOut, L"EFI_NOT_FOUND");
    else
        st->ConOut->OutputString(st->ConOut, L"UNKNOWN EFI STATUS");
}

// --- File Loading Helpers ---
EFI_FILE* LoadFile(EFI_FILE* dir, CHAR16* path) {
    EFI_FILE* loadedFile;
    static EFI_LOADED_IMAGE_PROTOCOL* loadedImage;
    if (loadedImage == NULL) {
        gBootServices->HandleProtocol(gImageHandle,
                                      &gEfiLoadedImageProtocolGuid,
                                      (void**)&loadedImage);
    }
    static EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fileSystem;
    if (fileSystem == NULL) {
        gBootServices->HandleProtocol(loadedImage->DeviceHandle,
                                      &gEfiSimpleFileSystemProtocolGuid,
                                      (void**)&fileSystem);
    }
    if (dir == NULL) {
        fileSystem->OpenVolume(fileSystem, &dir);
    }
    EFI_STATUS status = dir->Open(dir, &loadedFile, path,
                                  EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY);
    if (status == EFI_SUCCESS) {
        return loadedFile;
    }

    EFI_PRINT(status);

    return NULL;
}

// --- PSF1 Font Loader ---
// (Using psf1_header and psf1_font from kernel.h)
#define PSF1_MAGIC0 0x36
#define PSF1_MAGIC1 0x04

psf1_font* LoadPSF1Font(EFI_FILE* dir, CHAR16* path) {
    EFI_FILE* font = LoadFile(dir, path);
    if (font == NULL) { 
        st->ConOut->OutputString(st->ConOut, L"ERROR: Could not load font file\n");
        return NULL;
    }
    psf1_header* font_hdr;
    gBootServices->AllocatePool(EfiLoaderData, sizeof(psf1_header), (VOID**)&font_hdr);
    if (font_hdr == 0) {
        st->ConOut->OutputString(st->ConOut, L"ERROR: Failed to allocate pool for PSF1 font header\n");
        return NULL;
    }
    UINTN size = sizeof(psf1_header);
    font->Read(font, &size, font_hdr);
    if (font_hdr->Magic[0] != PSF1_MAGIC0 || font_hdr->Magic[1] != PSF1_MAGIC1) {
        st->ConOut->OutputString(st->ConOut, L"ERROR: Invalid PSF1 font format\n");
        return NULL;
    }
    UINTN glyphBufferSize = font_hdr->CharacterSize * 256;
    if (font_hdr->Mode == 1) {
        glyphBufferSize  = font_hdr->CharacterSize * 512;
    }
    VOID* glyphBuffer = NULL;
    gBootServices->AllocatePool(EfiLoaderData, glyphBufferSize, &glyphBuffer);
    if (glyphBuffer == NULL) {
        st->ConOut->OutputString(st->ConOut, L"ERROR: Failed to allocate pool for PSF1 font glyph buffer\n");
        return NULL;
    }
    font->SetPosition(font, sizeof(psf1_header));
    font->Read(font, &glyphBufferSize, glyphBuffer);
    psf1_font* final_font = NULL;
    gBootServices->AllocatePool(EfiLoaderData, sizeof(psf1_font), (VOID**)&final_font);
    if (final_font == NULL) {
        st->ConOut->OutputString(st->ConOut, L"ERROR: Failed to allocate pool for final PSF1 font\n");
        return NULL;
    }
    final_font->header = font_hdr;
    final_font->GlyphBuffer = glyphBuffer;
    return final_font;
}

// --- GOP Initialization ---
// (Using the 'framebuffer' type from kernel.h)
framebuffer* InitializeGOP() {
    EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    EFI_STATUS status;
    status = gBootServices->LocateProtocol(&gopGuid, NULL, (void**)&gop);
    if (EFI_ERROR(status)) {
        st->ConOut->OutputString(st->ConOut, L"ERROR: Unable to locate Graphics Output Protocol\n");
        return NULL;
    }
    st->ConOut->OutputString(st->ConOut, L"GOP located successfully\n");
    
    // Allocate and fill a framebuffer structure from kernel.h
    framebuffer* fb;
    gBootServices->AllocatePool(EfiLoaderData, sizeof(framebuffer), (void**)&fb);
    if (!fb) {
        st->ConOut->OutputString(st->ConOut, L"ERROR: Failed to allocate pool for framebuffer\n");
        return NULL;
    }
    fb->base_addr = (void*)gop->Mode->FrameBufferBase;
    fb->buffer_sz = gop->Mode->FrameBufferSize;
    fb->px_width = gop->Mode->Info->HorizontalResolution;
    fb->px_height = gop->Mode->Info->VerticalResolution;
    fb->pps = gop->Mode->Info->PixelsPerScanLine;
    return fb;
}

// --- Simple Page Allocation and Memory Copy Helper ---
void* allocate_page(void) {
    UINTN addr = 0;
    EFI_STATUS status = gBootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &addr);
    if (EFI_ERROR(status)) {
         st->ConOut->OutputString(st->ConOut, L"ERROR: allocate_page() failed\n");
         while (1);
    }
    void *page = (void*)addr;
    gBootServices->SetMem(page, PAGE_SIZE, 0);
    return page;
}

void copy_mem(void *dest, const void *src, UINTN size) {
    const UINT8 *s = src;
    UINT8 *d = dest;
    for (UINTN i = 0; i < size; i++) {
        d[i] = s[i];
    }
}

// --- Identity Mapping Helpers ---
// Map a single 2MB page for identity mapping.
static void map_identity_page(uint64_t *pml4, uint64_t phys_addr) {
    uint64_t aligned_addr = phys_addr & ~(0x200000ULL - 1);
    // For identity mapping, virtual address == physical address.
    uint64_t vaddr = aligned_addr;
    uint64_t pml4_index = (vaddr >> 39) & 0x1FF;
    uint64_t pdpt_index = (vaddr >> 30) & 0x1FF;
    uint64_t pd_index   = (vaddr >> 21) & 0x1FF;
    
    uint64_t *pdpt;
    if (!(pml4[pml4_index] & PAGE_PRESENT)) {
        pdpt = (uint64_t*) allocate_page();
        pml4[pml4_index] = (uint64_t)pdpt | PAGE_PRESENT | PAGE_WRITE;
    } else {
        pdpt = (uint64_t*)(pml4[pml4_index] & 0xFFFFFFFFFFFFF000ULL);
    }
    
    uint64_t *pd;
    if (!(pdpt[pdpt_index] & PAGE_PRESENT)) {
        pd = (uint64_t*) allocate_page();
        pdpt[pdpt_index] = (uint64_t)pd | PAGE_PRESENT | PAGE_WRITE;
    } else {
        pd = (uint64_t*)(pdpt[pdpt_index] & 0xFFFFFFFFFFFFF000ULL);
    }
    
    pd[pd_index] = aligned_addr | PAGE_PRESENT | PAGE_WRITE | PAGE_PSE;
}

// Identity map a physical address range using 2MB pages.
static void map_identity_range(uint64_t *pml4, uint64_t phys_start, uint64_t phys_end) {
    uint64_t start = phys_start & ~(0x200000ULL - 1);
    uint64_t end = (phys_end + 0x200000ULL - 1) & ~(0x200000ULL - 1);
    for (uint64_t addr = start; addr < end; addr += 0x200000ULL) {
        map_identity_page(pml4, addr);
    }
}

// --- Build New Page Tables (without switching CR3) ---
// This function builds the new page table structures and returns the pointer
// to the PML4. It maps an extended identity range (0 to 1GB) plus the bootloader’s region,
// and it sets up the higher-half mapping for the kernel.
static uint64_t *build_page_tables(uint64_t kphys_start, uint64_t kphys_size, uint64_t kvirt_base,
                              uint64_t boot_base, uint64_t boot_size) {
    st->ConOut->OutputString(st->ConOut, L"Building page tables...\n");
    
    uint64_t *pml4 = (uint64_t *)allocate_page();

    // Extended identity mapping (0 to 1GB) to cover bootloader pages, page tables, etc.
    map_identity_range(pml4, 0, 1ULL * 1024 * 1024 * 1024);
    // Also explicitly map the bootloader’s own region.
    map_identity_range(pml4, boot_base, boot_base + boot_size);

    // Higher-half mapping for the kernel.
    uint64_t *pdpt_kernel = (uint64_t *)allocate_page();
    // Place the kernel mapping in PML4 entry 511 so that virtual addresses start at KERNEL_VIRTUAL.
    pml4[511] = (uint64_t)pdpt_kernel | PAGE_PRESENT | PAGE_WRITE;
    uint64_t num_pages = (kphys_size + 0x200000ULL - 1) / 0x200000ULL;
    for (uint64_t i = 0; i < num_pages; i++) {
        uint64_t vaddr = kvirt_base + i * 0x200000ULL;
        uint64_t pdpt_index = (vaddr >> 30) & 0x1FF;
        uint64_t pd_index   = (vaddr >> 21) & 0x1FF;
        uint64_t *pd;
        if ((pdpt_kernel[pdpt_index] & 0xFFFFFFFFFFFFF000ULL) == 0) {
            pd = (uint64_t *)allocate_page();
            pdpt_kernel[pdpt_index] = (uint64_t)pd | PAGE_PRESENT | PAGE_WRITE;
        } else {
            pd = (uint64_t *)(pdpt_kernel[pdpt_index] & 0xFFFFFFFFFFFFF000ULL);
        }
        uint64_t phys_addr = kphys_start + i * 0x200000ULL;
        pd[pd_index] = phys_addr | PAGE_PRESENT | PAGE_WRITE | PAGE_PSE;
    }
    
    return pml4;
}

// --- Two-Stage Trampoline Code ---
// This is the second-stage entry point. It is placed into its own section
// so that we can locate and copy it into an identity-mapped area.
__attribute__((section(".stage2"), used))
void stage2_main(flexboot_header_t *header, uint64_t final_entry, uint64_t *new_pml4) {
    // Now that UEFI services are gone, switch to the new page tables.
    __asm__ volatile("mov %0, %%cr3" :: "r"(new_pml4));
    
    // Jump to the kernel's entry point.
    typedef void (*KernelMainFunc)(flexboot_header_t*);
    KernelMainFunc KernelStart = (KernelMainFunc) final_entry;
    KernelStart(header);
    
    // In case the kernel returns (it shouldn't), halt.
    while (1) { __asm__("hlt"); }
}

// Extern symbols to mark the boundaries of the stage2 code.
// (Ensure your linker script defines _stage2_start and _stage2_end accordingly.)
extern char _stage2_start[];
extern char _stage2_end[];

// --- Helper to Compare GUIDs ---
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

// --- Global variables to track kernel physical bounds ---
static uint64_t kernel_phys_min = ~0ULL;
static uint64_t kernel_phys_max = 0;

// --- EFI Entry Point (Stage 1) ---
EFI_STATUS efi_main (EFI_HANDLE IH, EFI_SYSTEM_TABLE* ST) {
    EFI_STATUS status;
    gImageHandle = IH;
    st = ST;
    gBootServices = ST->BootServices;
    
    st->ConOut->OutputString(st->ConOut, L"!==-- ModernOS BOOTLOADER --==!\n");

    // --- Load ModernOS Directory ---
    EFI_FILE* bin = LoadFile(NULL, L"ModernOS");
    if (bin == NULL) {
        st->ConOut->OutputString(st->ConOut, L"ERROR: Could not load directory: \"/ModernOS/\"\n");
        return EFI_LOAD_ERROR;
    }
    st->ConOut->OutputString(st->ConOut, L"Directory \"/ModernOS/\" loaded successfully\n");

    // --- Load Kernel ---
    EFI_FILE* kernel = LoadFile(bin, L"kernel.elf");
    if (kernel == NULL) {
        st->ConOut->OutputString(st->ConOut, L"ERROR: Could not load kernel from /ModernOS/kernel.elf\n");
        return EFI_LOAD_ERROR;
    }
    st->ConOut->OutputString(st->ConOut, L"Kernel has been found\n");
    bin->Close(bin);

    // --- Load Fonts Directory ---
    bin = LoadFile(NULL, L"ModernOS");
    if (bin == NULL) {
        st->ConOut->OutputString(st->ConOut, L"ERROR: Could not load ModernOS directory: \"/ModernOS/\"\n");
        return EFI_LOAD_ERROR;
    }
    st->ConOut->OutputString(st->ConOut, L"Directory \"/ModernOS/\" loaded successfully\n");
    bin = LoadFile(bin, L"fonts");
    if (bin == NULL) {
        st->ConOut->OutputString(st->ConOut, L"ERROR: Could not load fonts directory: \"/ModernOS/\"\n");
        return EFI_LOAD_ERROR;
    }
    st->ConOut->OutputString(st->ConOut, L"Directory \"/ModernOS/fonts/\" loaded successfully\n");

    // --- Load Default Font ---
    psf1_font* dflt_font = LoadPSF1Font(bin, L"dfltfont.psf");
    if (dflt_font == NULL) {
        st->ConOut->OutputString(st->ConOut, L"ERROR: Failed to load default font\n");
    } else {
        st->ConOut->OutputString(st->ConOut, L"Default font loaded successfully\n");
        st->ConOut->OutputString(st->ConOut, L"  Mode and Character Size info omitted\n");
    }
    bin->Close(bin);

    // --- Load and Verify Kernel ELF Header ---
    Elf64_Ehdr elf_header;
    UINTN elfHeaderSize = sizeof(elf_header);
    kernel->Read(kernel, &elfHeaderSize, &elf_header);
    if (memcmp(&elf_header.e_ident[EI_MAG0], ELFMAG, SELFMAG) != 0 ||
        elf_header.e_ident[EI_CLASS] != ELFCLASS64 ||
        elf_header.e_ident[EI_DATA] != ELFDATA2LSB ||
        elf_header.e_type != ET_EXEC ||
        elf_header.e_machine != EM_X86_64 ||
        elf_header.e_version != EV_CURRENT)
    {
        st->ConOut->OutputString(st->ConOut, L"ERROR: Invalid kernel format\n");
        return EFI_LOAD_ERROR;
    }
    st->ConOut->OutputString(st->ConOut, L"Kernel format verified successfully\n");

    // --- Load Program Headers ---
    Elf64_Phdr* program_hdrs;
    kernel->SetPosition(kernel, elf_header.e_phoff);
    UINTN programHdrTableSize = elf_header.e_phnum * elf_header.e_phentsize;
    status = gBootServices->AllocatePool(EfiLoaderData, programHdrTableSize, (void**)&program_hdrs);
    if (EFI_ERROR(status) || program_hdrs == 0) {
        st->ConOut->OutputString(st->ConOut, L"ERROR: Failed to allocate memory for program headers\n");
        return status;
    }
    kernel->Read(kernel, &programHdrTableSize, program_hdrs);

    // --- Load Each PT_LOAD Segment ---
    for (Elf64_Phdr* phdr = program_hdrs;
         (char*)phdr < (char*)program_hdrs + elf_header.e_phnum * elf_header.e_phentsize;
         phdr = (Elf64_Phdr*)((char*)phdr + elf_header.e_phentsize))
    {
        if (phdr->p_type == PT_LOAD) {
            UINTN pages = (phdr->p_memsz + PAGE_SIZE - 1) / PAGE_SIZE;
            UINTN desired_addr = (UINTN)(phdr->p_vaddr - KERNEL_VIRTUAL + KERNEL_PHYSICAL);
            EFI_MEMORY_TYPE MemoryType = EfiLoaderData;
            if (phdr->p_flags & PF_X) {
                MemoryType = EfiLoaderCode;
            }
            status = gBootServices->AllocatePages(AllocateAddress, MemoryType, pages, &desired_addr);
            if (EFI_ERROR(status)) {
                st->ConOut->OutputString(st->ConOut, L"Warning: Fixed address allocation failed; falling back\n");
                desired_addr = 0;
                status = gBootServices->AllocatePages(AllocateAnyPages, MemoryType, pages, &desired_addr);
                if (EFI_ERROR(status)) {
                    st->ConOut->OutputString(st->ConOut, L"ERROR: Failed to allocate pages for kernel segment\n");
                    return status;
                }
            }
            if (desired_addr < kernel_phys_min)
                kernel_phys_min = desired_addr;
            if ((uint64_t)desired_addr + pages * PAGE_SIZE > kernel_phys_max)
                kernel_phys_max = desired_addr + pages * PAGE_SIZE;
            kernel->SetPosition(kernel, phdr->p_offset);
            UINTN fileSize = phdr->p_filesz;
            kernel->Read(kernel, &fileSize, (VOID*)desired_addr);
            if (phdr->p_filesz < phdr->p_memsz) {
                gBootServices->SetMem((VOID*)(desired_addr + phdr->p_filesz),
                                      phdr->p_memsz - phdr->p_filesz, 0);
            }
            st->ConOut->OutputString(st->ConOut, L"LOADED: Kernel segment loaded\n");
        }
    }
    st->ConOut->OutputString(st->ConOut, L"Kernel loaded successfully\n");

    // --- Initialize GOP ---
    framebuffer* gop_fb = InitializeGOP();
    if (gop_fb == NULL) {
        st->ConOut->OutputString(st->ConOut, L"ERROR: Failed to initialize GOP\n");
        return EFI_LOAD_ERROR;
    }
    st->ConOut->OutputString(st->ConOut, L"GOP Info: (details omitted)\n");

    // --- Get EFI Memory Map ---
    EFI_MEMORY_DESCRIPTOR* Map = NULL;
    UINTN MapSize = 0, MapKey = 0;
    UINTN DescriptorSize = 0;
    UINT32 DescriptorVersion = 0;
    gBootServices->GetMemoryMap(&MapSize, Map, &MapKey, &DescriptorSize, &DescriptorVersion);
    MapSize += DescriptorSize * 2;  // margin
    gBootServices->AllocatePool(EfiLoaderData, MapSize, (void**)&Map);
    gBootServices->GetMemoryMap(&MapSize, Map, &MapKey, &DescriptorSize, &DescriptorVersion);
    st->ConOut->OutputString(st->ConOut, L"EFI memory map successfully parsed\n");

    // --- Locate ACPI 2.0 RSDP ---
    EFI_CONFIGURATION_TABLE* ConfigTable = st->ConfigurationTable;
    void* rsdp = NULL;
    EFI_GUID ACPI2TableGuid = ACPI_20_TABLE_GUID;
    for (UINTN index = 0; index < st->NumberOfTableEntries; index++) {
        if (guids_match(ConfigTable[index].VendorGuid, ACPI2TableGuid)) {
            if (strcmp((CHAR8*)"RSD PTR ", (CHAR8*)ConfigTable[index].VendorTable, 8)) {
                st->ConOut->OutputString(st->ConOut, L"Found ACPI RSDP\n");
                rsdp = ConfigTable[index].VendorTable;
            }
        }
    }

    // --- Retrieve Bootloader's Own Location ---
    EFI_LOADED_IMAGE_PROTOCOL* loadedImage = NULL;
    status = gBootServices->HandleProtocol(gImageHandle,
                                             &gEfiLoadedImageProtocolGuid,
                                             (void**)&loadedImage);
    if (EFI_ERROR(status) || loadedImage == NULL) {
        st->ConOut->OutputString(st->ConOut, L"ERROR: Failed to retrieve bootloader image info\n");
        return EFI_LOAD_ERROR;
    }
    uint64_t bootloader_base = (uint64_t)loadedImage->ImageBase;
    uint64_t bootloader_size = loadedImage->ImageSize;
    st->ConOut->OutputString(st->ConOut, L"Bootloader image info retrieved\n");

    // --- Build New Page Tables (but do not switch CR3 yet) ---
    uint64_t *new_pml4 = build_page_tables(kernel_phys_min, kernel_phys_max - kernel_phys_min,
                                            KERNEL_VIRTUAL, bootloader_base, bootloader_size);

    // --- Build flexboot_header_t to Pass to the Kernel ---
    flexboot_header_t header;
    header.memoryMap = Map;
    header.mapSize = MapSize;
    header.mapKey = MapKey;
    header.descriptorSize = DescriptorSize;
    header.descriptorVersion = DescriptorVersion;
    header.fb = gop_fb;
    header.font = dflt_font;
    header.rsdp_addr = (unsigned long long*) rsdp;
    header.kernel_offset = KERNEL_VIRTUAL - KERNEL_PHYSICAL;

    // --- Compute Final Kernel Entry Point ---
    UINT64 final_entry = KERNEL_VIRTUAL + (elf_header.e_entry - KERNEL_PHYSICAL);
    st->ConOut->OutputString(st->ConOut, L"Kernel entry point computed\n");

    // --- Prepare Stage 2 Trampoline ---
    // Locate stage2 code boundaries from linker symbols.
    UINTN stage2_size = (UINTN)(_stage2_end - _stage2_start);
    UINTN stage2_pages = (stage2_size + PAGE_SIZE - 1) / PAGE_SIZE;
    VOID* stage2_buffer;
    status = gBootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, stage2_pages, (UINTN*)&stage2_buffer);
    if (EFI_ERROR(status)) {
        st->ConOut->OutputString(st->ConOut, L"ERROR: Failed to allocate pages for stage2\n");
        return status;
    }
    copy_mem(stage2_buffer, _stage2_start, stage2_size);

    st->ConOut->OutputString(st->ConOut, L"Stage2 loaded. Exiting boot services...\n");
    status = gBootServices->ExitBootServices(gImageHandle, MapKey);
    if (EFI_ERROR(status)) {
        st->ConOut->OutputString(st->ConOut, L"ERROR: ExitBootServices failed\n");
        return status;
    }

    // --- Jump to Stage 2 ---
    // The prototype of stage2_main is:
    //    void stage2_main(flexboot_header_t*, uint64_t final_entry, uint64_t* new_pml4)
    ((void (*)(flexboot_header_t*, uint64_t, uint64_t*)) stage2_buffer)(&header, final_entry, new_pml4);

    // Should never return.
    while (1) { __asm__("hlt"); }
    return EFI_SUCCESS;
}
