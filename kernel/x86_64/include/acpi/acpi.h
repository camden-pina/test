#ifndef _ACPI_H
#define _ACPI_H 1

#include <kernel.h>
#include <string.h>
#include <io.h>
#include <stdint.h>
#include <stddef.h>

typedef unsigned char acpi_status_t;

#define ACPI_SUCCESS   0
#define ACPI_INTEGRITY 1
#define ACPI_MEMORY    2
#define ACPI_NO_TABLE  3

// Global ACPI table pointers shared across modules.
extern struct _acpi_xsdt_t* xsdt;
extern struct _acpi_fadt_t* fadt;

void io_wait(void);

// Public functions
void* acpi_locate_table(const char* table);
_Bool acpi_init(uint64_t rsdp_addr);

unsigned int acpi_checksum(void *table);
void* acpi_find_table(const char *sig, size_t index);

// Utility checksum function (used internally)
unsigned int acpi_checksum(void* ptr);

#endif
