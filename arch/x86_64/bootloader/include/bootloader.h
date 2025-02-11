#ifndef BOOTLOADER_H
#define BOOTLOADER_H

#include <efi.h>
#include <efilib.h>

/* The UEFI entry point for our bootloader */
EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable);

#endif // BOOTLOADER_H

