/* Minimal GNU-EFI linker script for a UEFI application.
   Adjust the base address as necessary.
*/
OUTPUT_FORMAT(pei-x86-64)
ENTRY(efi_main)
SECTIONS
{
    . = 0x200000;
    .text : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) }
}

