#include "shell.h"

#include <stdint.h>

#include "multiboot.h"
#include "string.h"
#include "vga.h"

// =====================================================================
// Shell
// =====================================================================

void cmd_echo(const char *args) {
	vga_puts(args);
	vga_putc('\n');
}

void cmd_clear(const char *args) {
	(void)args;
	clear_vga(); // also resets the cursor back to the top-left
}

void cmd_divide_by_zero(const char *args) {
	(void)args;
	// -O should be at 0 to prevent the compiler from optimizing out the divide by zero
	volatile int a = 1, b = 0;
	int c = a / b; // this will trigger the divide by zero exception
	(void)c;
}

void cmd_page_fault(const char *args) {
	(void)args;
	// boot64.s only identity-maps the first 1GB, so this address is
	// guaranteed to be unmapped and trigger a #PF (vector 14)
	volatile uint64_t *bad_ptr = (volatile uint64_t *)0xFFFFFFFF00000000ULL;
	*bad_ptr = 0;
}

void cmd_toggle_cursor(const char *args) {
	(void)args;
	vga_toggle_cursor();
}

void cmd_parse_multiboot(const char *args) {
	(void)args;
	multiboot_print_tags();
}

void cmd_print_cmds(const char *args); // forward declaration

struct cmd {
	const char *name;
	void (*handler)(const char *args);
	const char *desc;
};

static const struct cmd cmd_table[] = {
	{ "e",    cmd_echo,            "echo back the following string"   },
	{ "clr",  cmd_clear,           "clear the screen"                 },
	{ "dbz",  cmd_divide_by_zero,  "trigger a divide by zero error"   },
	{ "pf",   cmd_page_fault,      "trigger a page fault error"       },
	{ "tcur", cmd_toggle_cursor,   "toggle the vga cursor on and off" },
	{ "mb2",  cmd_parse_multiboot, "print the multiboot2 tags"        },
	{ "help", cmd_print_cmds,      "show this list"                   },
	{ 0,      0,	               0	                              }  // sentinel
};

void cmd_print_cmds(const char *args) {
	(void)args;
	vga_puts("Commands:\n");
	int i = 0;
	while (cmd_table[i].name) {
		vga_puts("    ");
		vga_puts(cmd_table[i].name);
		vga_puts(" : ");
		vga_puts(cmd_table[i].desc);
		vga_putc('\n');
		i++;
	}
}

void shell_dispatch(const char *line) {
	for (int i = 0; cmd_table[i].name; i++) {
		const char *args = str_match_prefix(line, cmd_table[i].name);
		if (args) {
			cmd_table[i].handler(args);
			return;
		}
	}

	if (*line) {
		vga_puts("Unknown command: ");
		vga_puts(line);
		vga_putc('\n');
	}
}

static const char *prompt = "O-(^W^)-> ";

void shell_prompt(void) {
	vga_puts(prompt);
}
