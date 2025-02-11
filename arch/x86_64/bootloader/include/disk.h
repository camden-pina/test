#ifndef DISK_H
#define DISK_H

#include <efi.h>
#include <efilib.h>

/* In our UEFI bootloader, we will load the kernel image using UEFI Boot Services.
   We provide a function that creates a device path from a file name.
   (We rely on the FileDevicePath() helper from efilib.) */
EFI_DEVICE_PATH_PROTOCOL* create_kernel_device_path(EFI_HANDLE ImageHandle, CHAR16 *FilePath);

#endif // DISK_H

