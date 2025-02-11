#include <efi.h>
#include <efilib.h>
#include "../include/serial.h"

static void char_to_wstring(const char *src, CHAR16 *dest, UINTN max)
{
    UINTN i;
    for (i = 0; i < max - 1 && src[i] != '\0'; i++) {
        dest[i] = (CHAR16) src[i];
    }
    dest[i] = L'\0';
}

void serial_init(void)
{
    /* No initialization needed as UEFI ConOut is already available */
}

void serial_putc(char c)
{
    CHAR16 str[2];
    str[0] = (CHAR16)c;
    str[1] = L'\0';
    Print(str);
}

void serial_print(const char *str)
{
    CHAR16 buffer[256];
    char_to_wstring(str, buffer, 256);
    Print(buffer);
}

