OUTPUT_FORMAT(elf64-x86-64)
ENTRY(efi_main)
SECTIONS
{
    . = 0x40000;
    .text : { *(.text*) }
    .data : { *(.data*) }
    .rodata : { *(.rodata*) }
    .bss : { *(.bss*) }
}
