#include "../include/memory.h"
#include "../include/uefi_util.h"

/* For a UEFI bootloader, most memory management is handled by firmware.
   This stub simply prints a message; you can extend it as needed.
*/
int setup_memory(boot_header_t* hdr)
{
    Print(L"Setting up memory (stub)...\n");
    return 0;
}

