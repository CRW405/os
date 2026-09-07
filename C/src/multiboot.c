#include "multiboot.h"

#include <stdint.h>

#include "string.h"
#include "vga.h"

// =====================================================================
// multiboot parsing
// =====================================================================

typedef struct multiboot2_tag {
	uint32_t type;
	uint32_t size;
} multiboot2_tag_t;

// saved by boot64.s at boot time, before %ebx gets reused for anything else
extern uint32_t multiboot_info;

static void parse_multiboot2_info(uint32_t addr) {
	if (addr & 7) {
		vga_puts("Error: multiboot2 info address is not 8-byte aligned\n");
		return;
	}

	uint32_t total_size = *(uint32_t *)addr;

	multiboot2_tag_t *tag = (multiboot2_tag_t *)(addr + 8);
	while (tag->type != 0) { // type 0 is the end tag
		switch (tag->type) {
		case 1: // boot command line
			vga_puts("Boot command line: ");
			vga_puts((const char *)tag + sizeof(multiboot2_tag_t));
			vga_putc('\n');
			break;
		case 2: // boot loader name
			vga_puts("Boot loader name: ");
			vga_puts((const char *)tag + sizeof(multiboot2_tag_t));
			vga_putc('\n');
			break;
		// Add more cases here for other tag types as needed
		case 6: // memory map
			vga_puts("Memory map tag found\n");
			break;
		default:
			vga_puts("Unknown tag type: ");
			int  tag_type = tag->type;
			char buffer[12];
			itoa(tag_type, buffer, 10);
			vga_puts(buffer);
			vga_putc('\n');
			break;
		}
		uint32_t size = (tag->size + 7) & ~7; // align to 8 bytes
		tag           = (multiboot2_tag_t *)((uint8_t *)tag + size);
	}
}

void multiboot_print_tags(void) {
	parse_multiboot2_info(multiboot_info);
}
