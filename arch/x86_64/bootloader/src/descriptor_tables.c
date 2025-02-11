#include "../include/descriptor_tables.h"
#include "../include/uefi_util.h"

/* In a UEFI environment, the firmware sets up the GDT/IDT.
   This stub is provided for compatibility and simply prints a message.
*/
int init_descriptor_tables(void)
{
    Print(L"UEFI environment: descriptor tables already initialized by firmware.\n");
    return 0;
}

