/* =====================================================================
 * Linker script
 *
 * Tells the linker how to lay out the final kernel binary: where each
 * section (.text, .rodata, .data, .bss) goes in memory, and in what
 * order. GRUB loads the kernel starting at 1 MiB, so that's where the
 * image begins; ENTRY() records which symbol GRUB should jump to first.
 * ===================================================================== */

ENTRY(_start) /* tell linker the entry point */
SECTIONS      /* memory layout of bin */
{
    . = 1M;                                 /* start after 1 MiB */
    .text   : { *(.multiboot) *(.text) }    /* code, multiboot header must be near the front */
    .rodata : { *(.rodata) }                /* read-only data (e.g. GDT, string literals) */
    .data   : { *(.data) }                  /* initialized static data */
    .bss    : { *(COMMON) *(.bss) }         /* uninitialized data (stack, page tables, etc) */
}
