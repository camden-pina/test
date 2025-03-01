// ##############################
//			  acpi.c
// ##############################

#include <acpi/acpi.h>

#include <node.h>

#include <stdint.h>

#include <string.h>
#include <acpi/tables.h>

#include <acpi/aml.h>
#include <printf.h>
#include <panic.h>

#include <acpi/interpreter/namespace.h>

static unsigned int acpi_smi_cmd;
static unsigned char acpi_enable;
static unsigned char acpi_disable;
static unsigned int acpi_pm1a_cnt;
static unsigned int acpi_pm1b_cnt;
static unsigned short acpi_slp_typ_a;
static unsigned short acpi_slp_typ_b;
static unsigned short acpi_slp_en;
static unsigned short acpi_sci_en;
static unsigned char acpi_pm1_cnt_len;

acpi_xsdt_t* xsdt;
static acpi_fadt_t* fadt;

static unsigned int acpi_checksum(void* ptr)
{
	acpi_sdt_hdr_t* hdr = (acpi_sdt_hdr_t*)ptr;
	unsigned char* bytes = (unsigned char*)ptr;
	unsigned char* end_bytes = bytes + hdr->len;

	unsigned char val = 0;

	while (bytes < end_bytes)
	{
		val += bytes[0];
		bytes++;
	}

	if (val == 0)
		return 1;
	
	return 0;
}

/*static unsigned char acpi_sdt_checksum(acpi_sdt_hdr_t* xsdt) {
	unsigned char sum = 0;
	for (unsigned long long i = 0; i < xsdt->len; i++) {
		unsigned char byte = *(unsigned char*)((unsigned long long)xsdt + i);
		sum += byte;
	}
	return (sum == 0);
}*/

void* acpi_locate_table(const char* table) {
	unsigned long long xsdt_entries = (xsdt->hdr.len - sizeof(xsdt->hdr)) / 4;
	for (unsigned long long e = 0; e < xsdt_entries; e++) {
		acpi_sdt_hdr_t* hdr = (acpi_sdt_hdr_t*)(unsigned long long)(xsdt->tables[e]);
		if (memcmp(hdr->signature, table, 4) == 0)
			return (void*)hdr;
	}
	return (void*)0;
}

_Bool acpi_init(unsigned long long* rsdp_ptr)
{
	acpi_smi_cmd = 0;
	acpi_enable = 0;
	acpi_disable = 0;
	acpi_pm1a_cnt = 0;
	acpi_pm1b_cnt = 0;
	acpi_slp_typ_a = 0;
	acpi_slp_typ_b = 0;
	acpi_slp_en = 0;
	acpi_sci_en = 0;
	acpi_pm1_cnt_len = 0;

	xsdt = (void*)0;
	fadt = (void*)0;
	
	kprintf("Initializing ACPI...\n\r");
	xsdt = (acpi_xsdt_t*)(((acpi_rsdp_t*)rsdp_ptr)->xsdt_ptr);
	
	kprintf("Verfiying XSDT\n\r");
	// ensure XSDT is valid
	if (!acpi_checksum(&xsdt))
		PANIC("ACPI Init: Invalid XSDT Checksum");
	// locate FADT
	kprintf("Setting FADT\n\r");
	fadt = (acpi_fadt_t*)acpi_locate_table("FACP");

	kprintf("Verifying FADT\n\r");
	// ensure valid FADT
	if (!acpi_checksum(&fadt))
		PANIC("ACPI Init: Invalud FADT Header");

	kprintf("Verifying X_DSDT\n\r");
	// locate & ensure valid X_DSDT
	if (!acpi_checksum(&fadt->x_dsdt))
		PANIC("ACPI Init: Invalid XDSDT");

	acpi_namespace_create((void*)fadt->x_dsdt);

	//void* dsst_amls = acpi_load_table(dsdt);
	//dsdt_addr = (char*)(unsigned long long)fadt->x_dsdt + sizeof(acpi_sdt_hdr_t);
	//dsdt_len = *((unsigned int*)(unsigned long long)fadt->x_dsdt + 1) - sizeof(acpi_sdt_hdr_t);

	return 1;
}