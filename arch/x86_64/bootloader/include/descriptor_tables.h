#ifndef DESCRIPTOR_TABLES_H
#define DESCRIPTOR_TABLES_H

#include <efi.h>
#include <efilib.h>

/* UEFI applications run in a flat protected mode provided by the firmware.
   Typically you do not need to manually set up GDT/IDT.
   This file is provided as a stub in case you want to do additional setup.
*/
int init_descriptor_tables(void);

#endif // DESCRIPTOR_TABLES_H

