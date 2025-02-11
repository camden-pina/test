/* Minimal GNU-EFI linker script for a UEFI application.
   Adjust the base address as necessary.
*/
ENTRY(efi_main)
SECTIONS
{
    . = 0x200000;
    .text : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) }
}

