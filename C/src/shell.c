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

void cmd_print_help(const char *args);      // forward declaration
void cmd_print_cmds(const char *args);      // forward declaration
void cmd_print_shortcuts(const char *args); // forward declaration

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
	{ "help", cmd_print_help,      "show this list"                   },
	{ 0,      0,	               0	                              }  // sentinel
};

// handler left null due to shortcuts not directly calling a command handler
static const struct cmd shortcut_table[] = {
	{ "CTRL + C",   0, "cancel the current line"                },
	{ "CTRL + U",   0, "clear the current line"                 },
	{ "CTRL + W",   0, "delete the word before the cursor"      },
	{ "CTRL + L",   0, "clear the screen"                       },
	{ "LEFT/RIGHT", 0, "move the cursor one character"          },
	{ "HOME/END",   0, "jump to the start/end of the line"      },
	{ "BACKSPACE",  0, "delete the character before the cursor" },
	{ "DELETE",     0, "delete the character under the cursor"  },
	{ "TAB",        0, "insert spaces"                          },
	{ 0,	        0, 0	                                    }  // sentinel
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

void cmd_print_shortcuts(const char *args) {
	(void)args;
	vga_puts("Shortcuts:\n");
	int i = 0;
	while (shortcut_table[i].name) {
		vga_puts("    ");
		vga_puts(shortcut_table[i].name);
		vga_puts(" : ");
		vga_puts(shortcut_table[i].desc);
		vga_putc('\n');
		i++;
	}
}

void cmd_print_help(const char *args) {
	cmd_print_cmds(args);
	cmd_print_shortcuts(args);
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
