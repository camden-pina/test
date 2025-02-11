#include "../include/kernel_loader.h"
#include "../include/uefi_util.h"
#include <efilib.h>

EFI_STATUS load_and_start_kernel(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, CHAR16 *FilePath)
{
    EFI_STATUS Status;
    EFI_DEVICE_PATH_PROTOCOL *KernelDevicePath;

    /* Create a device path for the kernel image using the loaded image handle */
    KernelDevicePath = FileDevicePath(ImageHandle, FilePath);
    if (KernelDevicePath == NULL) {
        uefi_print_error(EFI_NOT_FOUND, L"Failed to create device path for kernel file.\n");
        return EFI_NOT_FOUND;
    }
    
    Print(L"Loading kernel image: %s\n", FilePath);
    
    EFI_HANDLE KernelImageHandle = NULL;
    Status = uefi_call_wrapper(SystemTable->BootServices->LoadImage, 6,
                FALSE, ImageHandle, KernelDevicePath, NULL, 0, &KernelImageHandle);
    if (EFI_ERROR(Status)) {
        uefi_print_error(Status, L"LoadImage failed.\n");
        return Status;
    }

    /* Obtain the memory map and exit boot services before starting the kernel.
       This is required by the UEFI spec.
    */
    UINTN MapKey, MemoryMapSize = 0, DescriptorSize;
    UINT32 DescriptorVersion;
    EFI_MEMORY_DESCRIPTOR *MemoryMap = NULL;

    Status = uefi_call_wrapper(SystemTable->BootServices->GetMemoryMap, 5,
                &MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
    if (Status == EFI_BUFFER_TOO_SMALL) {
        MemoryMapSize += 2 * DescriptorSize;  /* add some slack */
        MemoryMap = AllocatePool(MemoryMapSize);
        if (MemoryMap == NULL) {
            uefi_print_error(EFI_OUT_OF_RESOURCES, L"Failed to allocate memory for memory map.\n");
            return EFI_OUT_OF_RESOURCES;
        }
        Status = uefi_call_wrapper(SystemTable->BootServices->GetMemoryMap, 5,
                    &MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
        if (EFI_ERROR(Status)) {
            uefi_print_error(Status, L"GetMemoryMap failed.\n");
            return Status;
        }
    } else {
        uefi_print_error(Status, L"GetMemoryMap failed unexpectedly.\n");
        return Status;
    }

    Status = uefi_call_wrapper(SystemTable->BootServices->ExitBootServices, 2, ImageHandle, MapKey);
    if (EFI_ERROR(Status)) {
        uefi_print_error(Status, L"ExitBootServices failed.\n");
        return Status;
    }

    /* Start the kernel image. If the kernel is a UEFI application, it will take control. */
    UINTN ExitDataSize = 0;
    Status = uefi_call_wrapper(SystemTable->BootServices->StartImage, 3,
                KernelImageHandle, &ExitDataSize, NULL);
    if (EFI_ERROR(Status)) {
        uefi_print_error(Status, L"StartImage failed.\n");
        return Status;
    }

    return EFI_SUCCESS;
}

