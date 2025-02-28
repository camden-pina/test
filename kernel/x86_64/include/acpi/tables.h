#ifndef _ACPI_TABLES_H
#define _ACPI_TABLES_H 1

#include <acpi/acpi.h>

typedef struct acpi_rsdp_t
{
	unsigned char signature[8];
	unsigned char checksum;
	char OEM[6];
	unsigned char rev;
	unsigned int rsdt_ptr;
	unsigned int length;
	unsigned long long xsdt_ptr;
	unsigned char exChecksum;
	char reserved;	
} __attribute__((packed)) acpi_rsdp_t;

typedef struct {
	char signature[4];
	unsigned int len;
	unsigned char rev;
	unsigned char checksum;
	char oem[6];
	char oem_table_id[8];
	unsigned int oem_rev;
	unsigned int creator_id;
	unsigned int creator_rev;
} __attribute__((packed)) acpi_sdt_hdr_t;

/*
 *	Extended System Directory Table
 *	@hdr:		
 *	@ptrs:		Array of 64-bit pointers to Description Headers
 *			Calculation:	((hdr.len - sizeof(hdr) / 8)
*/
typedef struct {
	acpi_sdt_hdr_t hdr;
	unsigned long long tables[];	// flexible array
} __attribute__((packed)) acpi_xsdt_t;

typedef struct {
	acpi_sdt_hdr_t hdr;
	unsigned int local_interrupt_controller_addr;
	unsigned int flags;
} __attribute__((packed)) acpi_madt_t;

typedef struct {
	unsigned char addr_space;
	unsigned char bit_width;
	unsigned char bit_offset;
	unsigned char access_size;
	unsigned long long addr;
} __attribute__((packed)) acpi_gas_t;

/*
 *	Preferred Power Management Profiles
 *	0	Unspecified
 *	1	Desktop
 *	2	Mobile
 *	3	Workstation
 *	4	Enterprise Server
 *	5	SOHO Server
 *	6	Applicance PC
 *	7	Performance Server
 *	8	Tablet
 *	>8	Reserved
*/
/*
 *	Fixed ACPI Description Table
 *	@hdr:
 *	@firmware_ctl:		32-bit address of the FACS
 *	@dsdt:			32-bit address of the DSDT
 *	@reserved:		Unused in ACPI 2.0+
 *	@pref_pm_profile:	Specifies the preferred power management profile to OSPM from OEM
 *	@sci_int
 *	@smi_cmd:
 *	@acpi_enable:		Value to write to SMI_CMD to disable SMI ownership of ACPI hardware registers
 *	@acpi_disable:		Value to write to SMI_CMD to re-enable SMI ownership of ACPI hawrdware registers
 *	@s4bios_req:		Value to write to SMI_CMD to enter S4 power state (if zero, not supported)
 *	@pstate_cnt:		Contains value OSPM writes to SMI_CMD to assume procesor performance state control responsibility (if not zero)
 *	@pm1a_evt_blk:		System port address for PM1a Event Register Block (required). (Ignore if x_pm1a_evt_blk contains a non-zero value)
 *	@pm1b_evt_blk:		System port address for PM1b Event Register Block (optional). (Ignore if x_pm1b_evt_blk contains a non-zero value)
 *	@pm1a_cnt_blk:		System port address for PM1a Control Register BLock (required). (Ignore if x_pm1a_cnt_blk contains a non-zero-value)
 *	@pm1b_cnt_blk:		System port address for PM1b Control Reigtser Block (optional). (Ignore if x_pm1b_cnt_blk contains a non-zero value)
 *	@pm2_cnt_blk:		System port address for PM2 Control Register Block (optioaal). (Ignore if x_pm2_cnt_blk contains a non-zero value)
 *	@pm_tmr_blk:		System port address of the Power Management Timer Control Register Block (optional). (Ignore if x_pm_tmr_blk contains 
 *					a non-zero value)
 *	@gpe0_blk:		System port address for General-Purpose Event 0 Register Block. (Ignore if x_gpe0_blk contains a non-zero value)
 *	@gpe1_blk:		System port address for General-Purpose Event 1 Register Block. (Ignore if x_gpe1_blk contains a non-zero value)
 *	@pm1_evt_len
 *	@pm1_cnt_len
 *	@pm2_cnt_len
 *	@pm_tme_len
 *	@gpe0_blk_len
 *	@gpe1_blk_len
 *	@gpe1_base
 *	@cst_cnt
 *	@p_lvl2_lat
 *	@p_lvl3_lat
 *	@flush_size:
 *	@flush_stride
 *	@duty_offset:
 *	@duty_width:
 *	@day_alrm:
 *	@mon_alrm:
 *	@century:
 *	@iapc_boot_arch:
 *	@reserved:		Must be 0
 *	@flags
 *	@reset_reg:
 *	@reset_value:		Value to write to the RESET_REG port to reset the system
 *	@arm_boot_arch		ARM Boot Architecture flags
 *	@FADT Minor Version
 *	@x_firmware_ctrl:	Extended address for FACS (Ignore if contains zero value)
 *	@x_dsdt:		Extended address for DSDT (Ignore if contains zero value)
 *	@x_pm1a_evt_blk:	Extended address for PM1a Event Register Block (required). (Ignore if contains zero value)
 *	@x_pm1b_evt_blk:	Extended address for PM1b Event Register Block (optional). (Ignore if contains zero value)
 *	@x_pm1a_cnt_blk:	Extended address for PM1a Control Register Block (required). (Ignore if contains zero value)
 *	@x_pm1b_cnt_blk:	Extended address for PM1b Control Register Block (optional). (Ignore if contains zero value)
 *	@x_pm2_cnt_blk:		Extended address for PM2 Control Register Block (optional). (Ignore if contains zero value)
 * 	@x_pm_tmr_blk:		Extended address for Power Management Timer Control Register Block (optional). (Ignore if contains zero value)
 *	@x_gpe0_blk:		Extended address for General-Purpose Event 0 Register Block (optional). (Ignore if contains zero value)
 *	@x_gpe1_blk:		Extended address for General-Purpose Event 1 Register Block (optional). (Ignore if contains zero value)
 *	@sleep_control_ref:	Address for the sleep register
 *	@sleep_status_reg:	
 *	@hypervisor_vendor_identity:
 Descrition: 
*/
typedef struct
{
	acpi_sdt_hdr_t hdr;
	unsigned int firmware_ctl;
	unsigned int dsdt;
	unsigned char reserved0;
	unsigned char pref_pm_profile;
	unsigned short sci_int;
	unsigned int smi_cmd;
	unsigned char acpi_enable;
	unsigned char acpi_disable;
	unsigned char s4bios_req;
	unsigned char pstate_cnt;
	unsigned int pm1a_evt_block;
	unsigned int pm1b_evt_blk;
	unsigned int pm1a_cnt_blk;
	unsigned int pm1b_cnt_blk;
	unsigned int pm2_cnt_blk;
	unsigned int pm_tmr_blk;
	unsigned int gpe0_blk;
	unsigned int gpe1_blk;
	unsigned char pm1_evt_len;
	unsigned char pm1_cnt_len;
	unsigned char pm2_cnt_len;
	unsigned char pm_tme_len;
	unsigned char gpe0_blk_len;
	unsigned char gpe1_blk_len;
	unsigned char gpe1_base;
	unsigned char cst_cnt;
	unsigned short p_lvl2_lat;
	unsigned short p_lvl3_lat;
	unsigned short flush_size;
	unsigned short flush_stride;
	unsigned char duty_offset;
	unsigned char duty_width;
	unsigned char day_alrm;
	unsigned char mon_alrm;
	unsigned char century;
	unsigned short iapc_boot_arch;
	unsigned char reserved1;
	unsigned int flags;
	acpi_gas_t reset_reg;
	unsigned char reset_value;
	unsigned short arm_boot_arch;
	unsigned char fadt_minor_version;
	unsigned long long x_firmware_ctrl;
	unsigned long long x_dsdt;
	acpi_gas_t x_pm1a_evt_blk;
	acpi_gas_t x_pm1b_evt_blk;
	acpi_gas_t x_pm1a_cnt_blk;
	acpi_gas_t x_pm1b_cnt_blk;
	acpi_gas_t x_pm2_cnt_blk;
	acpi_gas_t x_pm_tmr_blk;
	acpi_gas_t x_gpe0_blk;
	acpi_gas_t x_gpe1_blk;
	acpi_gas_t sleep_control_reg;
	acpi_gas_t sleep_status_reg;
	unsigned long long hypervisor_vendor_identity;
} __attribute__((packed)) acpi_fadt_t;

acpi_status_t acpi_table_find(void** table, const char* signature, size_t idx);
void* acpi_scan(const char* name, size_t idx);

#endif