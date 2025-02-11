#ifndef UEFI_UTIL_H
#define UEFI_UTIL_H

#include <efi.h>
#include <efilib.h>

/* Print an error message along with the EFI_STATUS code */
void uefi_print_error(EFI_STATUS Status, CHAR16 *Message);

#endif // UEFI_UTIL_H

