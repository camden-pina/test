#include "../include/uefi_util.h"
#include <efilib.h>

void uefi_print_error(EFI_STATUS Status, CHAR16 *Message)
{
    Print(Message);
    Print(L" Error: %r\n", Status);
}

