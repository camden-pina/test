#ifndef KERNEL_LOADER_H
#define KERNEL_LOADER_H

#include <efi.h>
#include <efilib.h>

/* Load and start the kernel image from the given file path.
   FilePath should be a UEFI-style file path (e.g., L"kernel.efi").
   Returns EFI_STATUS. */
EFI_STATUS load_and_start_kernel(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, CHAR16 *FilePath);

#endif // KERNEL_LOADER_H

