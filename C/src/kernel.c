#include "idt.h"
#include "pic.h"
#include "shell.h"
#include "vga.h"

// =====================================================================
// Entry point
// =====================================================================

void kernel_main(void) {
	idt_init();
	pic_remap();
	__asm__ volatile("sti"); // enable interrupts

	clear_vga();
	vga_init_cursor();
	vga_puts("Hello, World! Goodbye. Space?\n");
	vga_puts("enter 'help' to see list of commands.\n");
	shell_prompt();

	for (;;) {
		// halt until the next interrupt (timer/keyboard) instead of spinning
		__asm__("hlt");
	}
}
