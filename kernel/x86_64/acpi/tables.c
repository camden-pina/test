#include <acpi/tables.h>
#include <printf.h>
#include <panic.h>

extern acpi_xsdt_t* xsdt;

acpi_status_t acpi_table_find(void** table, const char* signature, size_t idx)
{
    kassert(xsdt != NULL);
    kassert(table != NULL);
    kassert(signature != NULL);

    acpi_sdt_hdr_t* hdr;
    size_t i = 0;
    size_t curr_idx = 0;
    size_t table_count = (xsdt->hdr.len - sizeof(acpi_sdt_hdr_t)) / sizeof(uint64_t);

    kprintf("Total Tables: %llu", table_count);
    kprintf("Searching for ACPI table with signature %4.4s at index %llu\n", signature, idx);

    for (i = 0; i < table_count; i++)
    {
        hdr = (acpi_sdt_hdr_t*)xsdt->tables[i];
        kassert(hdr != NULL);

        if (!memcmp(hdr->signature, signature, 4))
        {
            if (curr_idx == idx)
            {
                *table = (void*)hdr;
                kprintf("ACPI table %4.4s found at index %llu\n", signature, idx);
                return ACPI_SUCCESS;
            }
            curr_idx++;
        }
    }

    kprintf("ACPI table %4.4s not found at index %llu\n", signature, idx);
    return ACPI_NO_TABLE;
}

void* acpi_scan(const char* name, size_t idx)
{
    void* ptr;
    acpi_status_t status = acpi_table_find(&ptr, name, idx);
    if (status == ACPI_SUCCESS)
    {
        kprintf("ACPI table %s found successfully at index %llu\n", name, idx);
        return ptr;
    }
    else
    {
        kprintf("Failed to find ACPI table %s at index %llu\n", name, idx);
        return NULL;
    }
}
