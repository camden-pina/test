#ifndef _ACPI_NAMESPACE_H
#define _ACPI_NAMESPACE_H 1

#include <acpi/tables.h>
#include <acpi/interpreter/methods.h>

#define ACPI_MAX_NAME 64

#define ACPI_NAMESPACE_ROOT             1
#define ACPI_NAMESPACE_NAME             2
#define ACPI_NAMESPACE_ALIAS            3
#define ACPI_NAMESPACE_FIELD            4
#define ACPI_NAMESPACE_METHOD           5
#define ACPI_NAMESPACE_DEVICE           6
#define ACPI_NAMESPACE_INDEXFIELD       7
#define ACPI_NAMESPACE_MUTEX            8
#define ACPI_NAMESPACE_PROCESSOR        9
#define ACPI_NAMESPACE_BUFFER_FIELD     10
#define ACPI_NAMESPACE_THERMALZZONE     11
#define ACPI_NAMESPACE_EVENT            12
#define ACPI_NAMESPACE_POWERSOURCE      13
#define ACPI_NAMESPACE_BANK_FIELD       14
#define ACPI_NAMESPACE_OPREGION         15

typedef struct acpi_aml_t
{
	acpi_sdt_hdr_t hdr;
	uint8_t data[];
} __attribute__((packed)) acpi_aml_t;

typedef struct acpi_namespace_node_t
{
	char path[64];

	uint8_t op_address_space;
	uint64_t op_base;
	uint64_t op_length;

	int type;
	struct acpi_namespace_node_t* parent;
	struct acpi_aml_segment_t* aml;

        uint8_t method_flags;
        int (*method_override)(struct acpi_namespace_variable_t* args, struct acpi_namespace_variable_t* result);
} acpi_namespace_node_t;

void acpi_namespace_create(void* dsdt);

#endif // _ACPI_NAMESPACE_H