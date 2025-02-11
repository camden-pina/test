#ifndef MEMORY_H
#define MEMORY_H

#include <efi.h>
#include <efilib.h>
#include "bootloader.h"
#include "header.h"

/* Set up any temporary memory mappings or perform additional memory preparation.
   In our simple example, this is a stub returning success.
*/
int setup_memory(boot_header_t* hdr);

#endif // MEMORY_H

