#include <acpi/tables.h>
#include <printf.h>
#include <panic.h>

extern acpi_xsdt_t* xsdt;

acpi_status_t acpi_table_find(void** table, const char* signature, size_t idx)
{
        acpi_sdt_hdr_t* hdr;
        size_t i = 0;
        size_t curr_idx = 0;
        size_t table_count = 0;

        if (xsdt)
        {
                table_count = (xsdt->hdr.len - sizeof(acpi_sdt_hdr_t)) / sizeof(uint64_t);

                for (i = 0; i < table_count; i++)
                {
                        hdr = (acpi_sdt_hdr_t*)xsdt->tables[i];

                        if (!hdr)
                                return ACPI_MEMORY;
                        
                        if (!memcmp(hdr->signature, signature, 4))
                                curr_idx++;
                        
                        if (curr_idx > idx)
                        {
                                hdr = (acpi_sdt_hdr_t*)xsdt->tables[i];
                                if (!hdr)
                                        return ACPI_MEMORY;
                                
                                *table = (void*)hdr;
                                return ACPI_SUCCESS;
                        }
                }
        }
        else
                PANIC("[ERROR] XSDT IS REQUIRED TO SET UP THE ACPI NAMESPACE");
        return ACPI_NO_TABLE;
        
}

void* acpi_scan(const char* name, size_t idx)
{
        void* ptr;
        acpi_status_t status = acpi_table_find(&ptr, name, idx);
        if (status == ACPI_SUCCESS)
                return ptr;
        else
                return NULL;
        
}