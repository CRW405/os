ENTRY(_start) /* tell linker the entry point */
SECTIONS      /* memory layout of bin */
{
    . = 1M;                                 /* start after 1 Mib */
    .text   : { *(.multiboot) *(.text) }    /* .text output section */
    .rodata : { *(.rodata) }                /* read onlt data */
    .data   : { *(.data) }                  /* static data */
    .bss    : { *(COMMON) *(.bss) }         /* uninitialized data */
}
