#include "../include/bootloader.h"
#include "../include/kernel_loader.h"
#include "../include/uefi_util.h"

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    /* Initialize the UEFI library */
    InitializeLib(ImageHandle, SystemTable);
    Print(L"UEFI Bootloader starting...\n");

    /* Load and start the kernel image. The kernel image is expected to be a UEFI application (e.g. kernel.efi) */
    EFI_STATUS Status = load_and_start_kernel(ImageHandle, SystemTable, L"kernel.efi");
    if (EFI_ERROR(Status)) {
        uefi_print_error(Status, L"Failed to load kernel.\n");
        return Status;
    }
    
    /* If the kernel image returns, print a message and halt */
    Print(L"Kernel returned unexpectedly. Halting...\n");
    while (1);
    return EFI_SUCCESS;
}

