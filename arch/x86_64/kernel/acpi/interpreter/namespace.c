#include <acpi/interpreter/namespace.h>
#include <acpi/tables.h>
#include <acpi/interpreter/methods.h>

#include <mm/pmm.h>
#include <stdio.h>

static void acpi_namespace_load_table(void* ptr);

static void acpi_finalize_state(struct acpi_state_t* state);
static void acpi_init_state(struct acpi_state_t* state);
static int acpi_populate(struct acpi_namespace_node_t *parent, void *data, size_t size, lai_state_t *state);

static acpi_namespace_node_t* acpi_namespace_node_initialize(void)
{
    acpi_namespace_node_t* node = kmalloc(sizeof(*node));
    if (!node)
        return (void*)0;
    
    memset(node, 0, sizeof(*node));
    return node;
}

#define CODE_WINDOW 131072
#define NAMESPACE_WINDOW 8192

static uint8_t* acpi_namespace_code;
static size_t acpi_namespace_allocation = 0;
static size_t lai_acpins_size;
static size_t acpi_namespace_count;

static acpi_namespace_node_t** acpi_namespace;
static size_t lai_ns_size;
static size_t acpi_namespace_capacity;

static void acpi_namespace_node_install(acpi_namespace_node_t* node)
{
    if (lai_ns_size == acpi_namespace_capacity)
    {
        size_t new_capacity = acpi_namespace_capacity * 2;

        if (!new_capacity)
            new_capacity = NAMESPACE_WINDOW;
        
        acpi_namespace_node_t** new_array;
        new_array = krealloc(acpi_namespace, sizeof(acpi_namespace_node_t*) * new_capacity);

        if (!new_array)
            PANIC("could not kreallocate namespace table");
        
        acpi_namespace = new_array;
        acpi_namespace_capacity = new_capacity;
    }
    acpi_namespace[lai_ns_size++] = node;
}
void acpi_namespace_create(void* dsdt)
{
	for (int i = 0; i < 100; i++)
		printf("%02%x ", *((char*)dsdt + i));
    
    acpi_namespace_code = NULL;
    acpi_namespace_allocation = 0;
    lai_acpins_size = 0;
    acpi_namespace_count = 0;
    acpi_namespace = NULL;
    lai_ns_size = 0;
    acpi_namespace_capacity = NAMESPACE_WINDOW;

    acpi_namespace_load_table(dsdt);

    size_t index = 0;
    acpi_aml_t* ssdt = acpi_scan("SSDT", index);

    while (ssdt != NULL)
    {
        acpi_namespace_load_table(ssdt);
        index++;
        ssdt = acpi_scan("SSDT", index);
    }

    struct acpi_namespace_node_t* OSI = acpi_namespace_node_initialize();
    strcpy(OSI->path, "\\._OSI");
    OSI->type = ACPI_NAMESPACE_METHOD;
    OSI->method_flags = 0x01;
    OSI->method_override = &acpi_namespace_method_osi;
    acpi_namespace_node_install(OSI);

    struct acpi_namespace_node_t* OS = acpi_namespace_node_initialize();
    strcpy(OS->path, "\\._OS_");
    OS->type = ACPI_NAMESPACE_METHOD;
    OS->method_flags = 0x00;
    OS->method_override = &acpi_namespace_method_os;
    acpi_namespace_node_install(OS);

    struct acpi_namespace_node_t* REV = acpi_namespace_node_initialize();
    strcpy(REV->path, "\\._REV");
    REV->type = ACPI_NAMESPACE_METHOD;
    REV->method_flags = 0x00;
    REV->method_override = &acpi_namespace_method_osi;
    acpi_namespace_node_install(REV);

    struct acpi_state_t state;
    acpi_init_state(&state);
    acpi_populate(NULL, acpi_namespace_code, lai_acpins_size, &state);
    acpi_finalize_state(&state);

    PANIC("INITIALIZED ACPI");
    /*








    struct acpi_namespace_node_t* acpi_namespace_root = acpi_namespace_create_root();

    struct acpi_state_t state;

    // Load the DSDT
    void* dsdt_table = acpi_scan("DSDT", 0);
    if (!dsdt_table)
        PANIC("[ERROR] UNABLE TO FIND ACPI DSDT");
    
    void* dsdt_amls = acpi_load_table(dsdt_table, 0);
    acpi_init_state(&state);
    acpi_populate(acpi_namespace_root, dsdt_amls, size, &state);
    acpi_finalize_state(&state);*/
}

// Windozz
static void acpi_namespace_load_table(void* ptr)
{
    acpi_aml_t* table = (acpi_aml_t*)ptr;   // cast ptr to aml table
    
    DEBUG("Loaded AML table '%c%c%c%c', total %l bytes of AML code\n\r", *((char*)ptr + 0), *((char*)ptr + 1), *((char*)ptr + 2), *((char*)ptr + 3));
    
    while ((lai_acpins_size + table->hdr.len) >= acpi_namespace_allocation)
    {
        acpi_namespace_allocation += CODE_WINDOW;
        acpi_namespace_code = krealloc(acpi_namespace_code, acpi_namespace_allocation);
    }

    memcpy((acpi_namespace_code + lai_acpins_size), table->data, (table->hdr.len - sizeof(acpi_sdt_hdr_t)));
    lai_acpins_size += (table->hdr.len - sizeof(acpi_sdt_hdr_t));

    acpi_namespace_count++;
}

static void acpi_init_state(struct acpi_state_t* state)
{
    memset(state, 0, sizeof(struct acpi_state_t));
    state->stack_ptr    = -1;
    state->context_ptr = -1;
}

// Free all memory owned by the state
static void acpi_finalize_state(struct acpi_state_t* state)
{
    kfree(&state->retvalue);

    for (int i = 0; i < 7; i++)
        kfree(&state->arg[i]);
    
    for (int i = 0; i < 8; i++)
        kfree(&state->local[i]);
}

// Pushes a new item to the opstack and returns it.
static struct acpi_object_t *lai_exec_push_opstack_or_die(lai_state_t *state) {
    if(state->opstack_ptr == 16)
        PANIC("operand stack overflow\n");
    struct acpi_object_t *object = &state->opstack[state->opstack_ptr];
    memset(object, 0, sizeof(struct acpi_object_t));
    state->opstack_ptr++;
    return object;
}

static struct acpi_stackitem_t* acpi_exec_push_stack(struct acpi_state_t* state)
{
    state->stack_ptr++;

    if (state->stack_ptr == 16)
        PANIC("execution engine overflow");
    
    return &state->stack[state->stack_ptr];
}

// Returns the n-th item from the top of the stack.
static struct acpi_stackitem_t *lai_exec_peek_stack(struct acpi_state_t *state, int n) {
    if(state->stack_ptr - n < 0)
        return NULL;
    return &state->stack[state->stack_ptr - n];
}

// Returns the last item of the stack.
static lai_stackitem_t *lai_exec_peek_stack_back(lai_state_t *state) {
    return lai_exec_peek_stack(state, 0);
}

// Removes n items from the stack.
static void lai_exec_pop_stack(lai_state_t *state, int n) {
    state->stack_ptr -= n;
}

// Removes the last item from the stack.
static void lai_exec_pop_stack_back(lai_state_t *state) {
    lai_exec_pop_stack(state, 1);
}

// Returns the lai_stackitem_t pointed to by the state's context_ptr.
static lai_stackitem_t *lai_exec_context(lai_state_t *state) {
    return &state->stack[state->context_ptr];
}

#define LAI_STRINGIFY(x) #x
#define LAI_EXPAND_STRINGIFY(x) LAI_STRINGIFY(x)

#define LAI_ENSURE(cond) \
    do { \
        if(!(cond)) \
            PANIC("ASSERTION FAILED"); \
    } while(0)

#define ACPI_MAX_NAME            64
#define ACPI_MAX_RESOURCES        512

#define ACPI_GAS_MMIO            0
#define ACPI_GAS_IO            1
#define ACPI_GAS_PCI            2

#define LAI_NAMESPACE_NAME        1
#define LAI_NAMESPACE_ALIAS        2
#define LAI_NAMESPACE_SCOPE        3
#define LAI_NAMESPACE_FIELD        4
#define LAI_NAMESPACE_METHOD        5
#define LAI_NAMESPACE_DEVICE        6
#define LAI_NAMESPACE_INDEXFIELD    7
#define LAI_NAMESPACE_MUTEX        8
#define LAI_NAMESPACE_PROCESSOR    9
#define LAI_NAMESPACE_BUFFER_FIELD    10
#define LAI_NAMESPACE_THERMALZONE    11

// ----------------------------------------------------------------------------
// Data types defined by AML.
// ----------------------------------------------------------------------------
// Value types: integer, string, buffer, package.
#define LAI_INTEGER            1
#define LAI_STRING             2
#define LAI_BUFFER             3
#define LAI_PACKAGE            4
// Handle type: this is used to represent device (and other) namespace nodes.
#define LAI_HANDLE             5
// Reference types: obtained from RefOp() or Index().
#define LAI_STRING_INDEX       6
#define LAI_BUFFER_INDEX       7
#define LAI_PACKAGE_INDEX      8
// ----------------------------------------------------------------------------
// Internal data types of the interpreter.
// ----------------------------------------------------------------------------
// Name types: unresolved names and names of certain objects.
#define LAI_NULL_NAME          9
#define LAI_UNRESOLVED_NAME   10
#define LAI_ARG_NAME          11
#define LAI_LOCAL_NAME        12
// Reference types: references to object storage.
#define LAI_STRING_REFERENCE  13
#define LAI_BUFFER_REFERENCE  14
#define LAI_PACKAGE_REFERENCE 15

// Device _STA object
#define ACPI_STA_PRESENT        0x01
#define ACPI_STA_ENABLED        0x02
#define ACPI_STA_VISIBLE        0x04
#define ACPI_STA_FUNCTION        0x08
#define ACPI_STA_BATTERY        0x10

// FADT Event/Status Fields
#define ACPI_TIMER            0x0001
#define ACPI_BUSMASTER            0x0010
#define ACPI_GLOBAL            0x0020
#define ACPI_POWER_BUTTON        0x0100
#define ACPI_SLEEP_BUTTON        0x0200
#define ACPI_RTC_ALARM            0x0400
#define ACPI_PCIE_WAKE            0x4000
#define ACPI_WAKE            0x8000

// FADT Control Block
#define ACPI_ENABLED            0x0001
#define ACPI_SLEEP            0x2000

// Parsing Resource Templates
#define ACPI_RESOURCE_MEMORY        1
#define ACPI_RESOURCE_IO        2
#define ACPI_RESOURCE_IRQ        3

// IRQ Flags
#define ACPI_IRQ_LEVEL            0x00
#define ACPI_IRQ_EDGE            0x01
#define ACPI_IRQ_ACTIVE_HIGH        0x00
#define ACPI_IRQ_ACTIVE_LOW        0x08
#define ACPI_IRQ_EXCLUSIVE        0x00
#define ACPI_IRQ_SHARED            0x10
#define ACPI_IRQ_NO_WAKE        0x00
#define ACPI_IRQ_WAKE            0x20

#define ACPI_POPULATE_CONTEXT_STACKITEM 1
#define ACPI_METHOD_CONTEXT_STACKITEM 2
#define ACPI_LOOP_STACKITEM 3
#define ACPI_COND_STACKITEM 4
#define ACPI_PKG_INITIALIZER_STACKITEM 5
#define ACPI_OP_STACKITEM 6
// This implements lai_eval_operand(). // TODO: Eventually remove
// lai_eval_operand() by moving all parsing functionality into acpi_exec_run().
#define ACPI_EVALOPERAND_STACKITEM 10

#define LAI_DATA_MODE 1
// Evaluate dynamic data (and keep result).
//     Primitive objects are parsed.
//     Names are resolved. Methods are executed.
//     Operations are allowed and executed.
#define LAI_OBJECT_MODE 2
// Like LAI_OBJECT_MODE, but discard the result.
#define LAI_EXEC_MODE 3
#define LAI_TARGET_MODE 4

static void lai_exec_update_context(struct acpi_state_t *state) {
    int j = 0;
    struct acpi_stackitem_t *ctx_item;
    while(1) {
        ctx_item = lai_exec_peek_stack(state, j);
        if(!ctx_item)
            break;
        if(ctx_item->kind == ACPI_POPULATE_CONTEXT_STACKITEM
                || ctx_item->kind == ACPI_METHOD_CONTEXT_STACKITEM)
            break;
        j++;
    }

    state->context_ptr = state->stack_ptr - j;
}

static int acpi_exec_run(uint8_t *method, lai_state_t *state);

static int acpi_populate(struct acpi_namespace_node_t *parent, void *data, size_t size, lai_state_t *state)
{
    struct acpi_stackitem_t* item = acpi_exec_push_stack(state);
    item->kind = ACPI_POPULATE_CONTEXT_STACKITEM;
    item->ctx_handle = parent;
    item->ctx_limit = size;
    lai_exec_update_context(state);

    state->pc = 0;
    state->limit = size;
    int status = acpi_exec_run(data, state);
    if (status)
        PANIC("acpi_exec_run() failed in acpi_populate()");
    
    return 0;
}

static int acpi_exec_run(uint8_t *method, lai_state_t *state)
{
    lai_stackitem_t *item;
    while((item = lai_exec_peek_stack_back(state)))
    {
        // Package-size encoding (and similar) needs to know the PC of the opcode.
        // If an opcode sequence contains a pkgsize, the sequence generally ends at:
        //     opcode_pc + pkgsize + opcode size.
        int opcode_pc = state->pc;

        // Whether we use the result of an expression or not.
        // If yes, it will be pushed onto the opstack after the expression is computed.
        int exec_result_mode = LAI_EXEC_MODE;

        if(item->kind == ACPI_POPULATE_CONTEXT_STACKITEM)
        {
            PANIC("ACPI_POPULATE_CONTEXT_STACKITEM");
        }else if(item->kind == ACPI_METHOD_CONTEXT_STACKITEM)
        {
            PANIC("ACPI_METHOD_CONTEXT_STACKITEM");
        }else if(item->kind == ACPI_EVALOPERAND_STACKITEM)
        {
            PANIC("ACPI_EVALOPERAND_STACKITEM");
        }else if(item->kind == ACPI_PKG_INITIALIZER_STACKITEM)
        {
            PANIC("ACPI_PKG_INITIALIZER_STACKITEM");
        }else if(item->kind == ACPI_OP_STACKITEM)
        {
            PANIC("ACPI_OP_STACKITEM");
        }else if(item->kind == ACPI_LOOP_STACKITEM)
        {
            PANIC("ACPI_LOOP_STACKITEM");
        }else if(item->kind == ACPI_COND_STACKITEM)
        {
            PANIC("ACPI_COND_STACKITEM");
        }else
            PANIC("unexpected lai_stackitem_t\n");

        if(state->pc >= state->limit) // This would be an interpreter bug.
            PANIC("execution escaped out of code range (PC is 0x%x with limit 0x%x)\n",
                    state->pc, state->limit);

        lai_stackitem_t *ctx_item = lai_exec_context(state);
        struct acpi_namespace_node_t *ctx_handle = ctx_item->ctx_handle;
    }
    return 0;
}