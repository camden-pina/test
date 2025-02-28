// ##############################
//			  acpi.h
// ##############################

#ifndef _ACPI_H
#define _ACPI_H 1

#include <kernel.h>

#include <string.h>
#include <io.h>

#include <stdint.h>
#include <stddef.h>

typedef unsigned char acpi_status_t;

#define ACPI_SUCCESS 0
#define ACPI_INTEGRITY 1
#define ACPI_MEMORY 2
#define ACPI_NO_TABLE 3

/*
* System States
* : G - Global-State
* : S - System-State
* 
* : As G* increases, system power state decreases											**%
* : G3				: Mechanical off. All power removed from system
* : G2/S5			: Soft off. All power lost (except wake-up on PCH). Total reboot		**%
* : G1/S3-Cold		: Suspend to RAM (STR)													**%
* : G1/S4			: Suspend-to-Disk (STD). All power lost (except wake-up on PCH)			**%
* : G0/S0			: Full power															100%
* 
* : D3cold(Off)		: Device is powered completely off										0%
* : D3hot			: Device is assumed to be a the lowest level of power consumption		50%
* : D2				: Device is assumed to save power (some context may be lost)			60%
* : D1				: Device is assumed to save less power than D2							85%
* : D0				: Device is assumed to be at the highest level of power consumption		100%
* : 
* : C0
* : C1
* : C2
* : C3				: Best power savings. CPU executes at worst-supported hardware latency
*					: processor's cache maintain intact, but ignore any snoops
*					: operating software is responsible for ensuring the caches maintain coherency.
* 
* : P0				: Device or CPU's performance is at it's max
* : P1				: Device or CPU's performance is between it's max and min
* : Pn				: Device or CPU's perfirmance is at it's min while remaining active
*/

/*
 * Root System Description Pointer
 *	@signature:	Must be "RSD PTR "; Notice the trailing blank character
 *	@checksum:	Bytes must sum to zero
 *	@OEM:		OEM Identifier
 *	@rev:		Revision of structure
 *	@rsdt_ptr:	32-bit physical address of the RSDT
 *	@length:	Length of the ?Table
 *	@xsdt_ptr:	64-bit physical address of the XSDT
 *	@exChecksum:	Bytes must sum to zero
 *	@reserved:	Reserved field
 */

void* acpi_locate_table(const char* table);
_Bool acpi_init(unsigned long long* rsdp_ptr);

#endif
