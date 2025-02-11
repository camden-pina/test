#ifndef _METHODS_H
#define _METHODS_H 1

#include <stddef.h>
#include <stdint.h>

#define ZERO_OP				0x00
#define ONE_OP				0x01
#define ALIAS_OP			0x06
#define NAME_OP				0x08
#define BYTEPREFIX			0x0A
#define WORDPREFIX			0x0B
#define DWORDPREFIX			0x0C
#define STRINGPREFIX			0x0D
#define QWORDPREFIX			0x0E
#define SCOPE_OP			0x10
#define BUFFER_OP			0x11
#define PACKAGE_OP			0x12
#define VARPACKAGE_OP			0x13
#define METHOD_OP			0x14
#define DUAL_PREFIX			0x2E
#define MULTI_PREFIX			0x2F
#define EXTOP_PREFIX			0x5B
#define ROOT_CHAR			0x5C
#define PARENT_CHAR			0x5E
#define LOCAL0_OP			0x60
#define LOCAL1_OP			0x61
#define LOCAL2_OP			0x62
#define LOCAL3_OP			0x63
#define LOCAL4_OP			0x64
#define LOCAL5_OP			0x65
#define LOCAL6_OP			0x66
#define LOCAL7_OP			0x67
#define ARG0_OP				0x68
#define ARG1_OP				0x69
#define ARG2_OP				0x6A
#define ARG3_OP				0x6B
#define ARG4_OP				0x6C
#define ARG5_OP				0x6D
#define ARG6_OP				0x6E
#define STORE_OP			0x70
#define ADD_OP				0x72
#define SUBTRACT_OP			0x74
#define INCREMENT_OP			0x75
#define DECREMENT_OP			0x76
#define MULTIPLY_OP			0x77
#define DIVIDE_OP			0x78
#define SHL_OP				0x79
#define SHR_OP				0x7A
#define AND_OP				0x7B
#define OR_OP				0x7D
#define XOR_OP				0x7F
#define NOT_OP				0x80
#define DEREF_OP			0x83
#define SIZEOF_OP			0x87
#define INDEX_OP			0x88
#define DWORDFIELD_OP			0x8A
#define WORDFIELD_OP			0x8B
#define BYTEFIELD_OP			0x8C
#define BITFIELD_OP			0x8D
#define QWORDFIELD_OP			0x8F
#define LAND_OP				0x90
#define LOR_OP				0x91
#define LNOT_OP				0x92
#define LEQUAL_OP			0x93
#define LGREATER_OP			0x94
#define LLESS_OP			0x95
#define CONTINUE_OP			0x9F
#define IF_OP				0xA0
#define ELSE_OP				0xA1
#define WHILE_OP			0xA2
#define NOP_OP				0xA3
#define RETURN_OP			0xA4
#define BREAK_OP			0xA5
#define ONES_OP				0xFF

// Extended opcodes
#define MUTEX				0x01
#define CONDREF_OP			0x12
#define ARBFIELD_OP			0x13
#define SLEEP_OP			0x22
#define DEBUG_OP			0x31
#define OPREGION			0x80
#define FIELD				0x81
#define DEVICE				0x82
#define PROCESSOR			0x83
#define THERMALZONE			0x85
#define INDEXFIELD			0x86	// ACPI spec v5.0 section 19.5.60

// OpRegion Address Spaces
#define OPREGION_MEMORY			0x00
#define OPREGION_IO			0x01
#define OPREGION_PCI			0x02
#define OPREGION_EC			0x03
#define OPREGION_SMBUS			0x04
#define OPREGION_CMOS			0x05
#define OPREGION_OEM			0x80

// Field Access Type
#define FIELD_ANY_ACCESS		0x00
#define FIELD_BYTE_ACCESS		0x01
#define FIELD_WORD_ACCESS		0x02
#define FIELD_DWORD_ACCESS		0x03
#define FIELD_QWORD_ACCESS		0x04
#define FIELD_LOCK			0x10
#define FIELD_PRESERVE			0x00
#define FIELD_WRITE_ONES		0x01
#define FIELD_WRITE_ZEROES		0x02

// Methods
#define METHOD_ARGC_MASK		0x07
#define METHOD_SERIALIZED		0x08

typedef struct acpi_namespace_variable_t
{
        int type;
        uint64_t integer;
        char* string;

        int package_size;
        struct acpi_namespace_variable_t* package;

        size_t buffer_size;
        void* buffer;
        char name[64];
        struct acpi_namespace_node_t* handle;

        int index;
} lai_ns_variable_t;

typedef struct acpi_stackitem_t
{
        int kind;
        int opstack_frame;

        union
        {
                struct
                {
                        struct acpi_namespace_node_t* ctx_handle;
                        int ctx_limit;
                };

                struct
                {
                        int loop_pred;
                        int loop_end;
                };

                struct
                {
                        int cond_taken;
                        int cond_end;
                };

                struct
                {
                        int pkg_index;
                        int pkg_end;
                        uint8_t pkg_result_mode;
                };

                struct
                {
                        int op_opcode;
                        uint8_t op_arg_modes[8];
                        uint8_t op_result_mode;
                };
        };
} lai_stackitem_t;

struct acpi_object_t
{
        int type;
        uint64_t integer;
        char* string;

        int package_size;
        struct acpi_object_t* package;

        size_t buffer_size;
        void* buffer;

        char name[64];
        struct acpi_namespace_node_t* handle;

        int index;
};

typedef struct acpi_state_t
{
        int pc;
        int limit;

        struct acpi_object_t retvalue;
        struct acpi_object_t arg[7];
        struct acpi_object_t local[8];

        // Stack to track current execution state
        int stack_ptr;
        int opstack_ptr;
        struct acpi_stackitem_t stack[16];
        struct acpi_object_t opstack[16];
        int context_ptr;
} lai_state_t;

int acpi_namespace_method_osi(struct acpi_namespace_variable_t* args, struct acpi_namespace_variable_t* result);
int acpi_namespace_method_os(struct acpi_namespace_variable_t* args, struct acpi_namespace_variable_t* result);
int acpi_namespace_method_rev(struct acpi_namespace_variable_t* args, struct acpi_namespace_variable_t* result);

#endif