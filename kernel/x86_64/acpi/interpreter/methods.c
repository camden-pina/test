#include <acpi/interpreter/methods.h>
#include <panic.h>
#include <printf.h>

// Called by the system firmware to query OSPM about interfaces features
// supported by the host operating system
int acpi_namespace_method_osi(struct acpi_namespace_variable_t* args, struct acpi_namespace_variable_t* result)
{
    kassert(args != NULL);
    kassert(result != NULL);

    kprintf("OSI method called with arg type %x, returning result type %x\n", args->type, result->type);
    // Implement OSI functionality here
    return 0;
}

int acpi_namespace_method_os(struct acpi_namespace_variable_t* args, struct acpi_namespace_variable_t* result)
{
    kassert(args != NULL);
    kassert(result != NULL);

    kprintf("OS method called with arg type %x, returning result type %x\n", args->type, result->type);
    // Implement OS functionality here
    return 0;
}

int acpi_namespace_method_rev(struct acpi_namespace_variable_t* args, struct acpi_namespace_variable_t* result)
{
    kassert(args != NULL);
    kassert(result != NULL);

    kprintf("REV method called with arg type %x, returning result type %x\n", args->type, result->type);
    // Implement REV functionality here
    return 0;
}
