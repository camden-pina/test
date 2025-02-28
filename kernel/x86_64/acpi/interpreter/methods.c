#include <acpi/interpreter/methods.h>
#include <panic.h>

//    Called by the system firmware to query OSPM about interfaces features
//    supported by the host operating system

int acpi_namespace_method_osi(struct acpi_namespace_variable_t* args, struct acpi_namespace_variable_t* result)
{
        PANIC("CFVGBHNJ, %x, %x", args->type, result->type);
        return 0;
}

int acpi_namespace_method_os(struct acpi_namespace_variable_t* args, struct acpi_namespace_variable_t* result)
{
        PANIC("CGFVHBJNJ, %x, %x", args->type, result->type);
        return 0;
}

int acpi_namespace_method_rev(struct acpi_namespace_variable_t* args, struct acpi_namespace_variable_t* result)
{
        PANIC("FGVHBJNKM, %x, %x", args->type, result->type);
        return 0;
}