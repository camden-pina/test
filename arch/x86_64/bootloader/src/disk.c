#include "../include/disk.h"
#include "../include/uefi_util.h"
#include <efilib.h>

/* In a UEFI system, rather than performing low‑level disk I/O,
   we create a device path for the kernel file using the UEFI helper.
   This function wraps FileDevicePath() provided by the GNU‑EFI library.
*/
EFI_DEVICE_PATH_PROTOCOL* create_kernel_device_path(EFI_HANDLE ImageHandle, CHAR16 *FilePath)
{
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    DevicePath = FileDevicePath(ImageHandle, FilePath);
    if (DevicePath == NULL) {
        uefi_print_error(EFI_NOT_FOUND, L"Failed to create device path for kernel file.\n");
    }
    return DevicePath;
}

