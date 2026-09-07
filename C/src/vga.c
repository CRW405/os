#include "vga.h"

#include "io.h"

// =====================================================================
// VGA text mode output
// =====================================================================

static const int            VGA_WIDTH               = 80;
static const int            VGA_HEIGHT              = 25;
static const unsigned short VGA_WHITE_ON_BLACK_STYLE = 0x0700;

// vga text memory, ASCII byte + color byte
// no interrupts needed in protected mode
static volatile unsigned short *vga = (unsigned short *)0xB8000;

static int cursor_row = 0;
static int cursor_col = 0;

void write_vga_line(int line, const char *string) {
	for (int i = 0; string[i]; i++) {
		// write each char into vga with white on black
		vga[line * VGA_WIDTH + i] = (unsigned short)string[i] | VGA_WHITE_ON_BLACK_STYLE;
	}
}

void clear_vga(void) {
	for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
		vga[i] = (unsigned short)' ' | VGA_WHITE_ON_BLACK_STYLE;
	}
	// a cleared screen should always start typing from the top-left
	cursor_row = 0;
	cursor_col = 0;
}

static void vga_scroll(void) {
	// move all lines up by one
	for (int row = 1; row < VGA_HEIGHT; row++) {
		for (int col = 0; col < VGA_WIDTH; col++) {
			vga[(row - 1) * VGA_WIDTH + col] = vga[row * VGA_WIDTH + col];
		}
	}
	// clear the last line
	for (int col = 0; col < VGA_WIDTH; col++) {
		vga[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = (unsigned short)' ' | VGA_WHITE_ON_BLACK_STYLE;
	}
	cursor_row = VGA_HEIGHT - 1;
}

void vga_putc(char c) {
	switch (c) {
	case '\n':
		cursor_col = 0;
		cursor_row++;
		break;
	case '\b':
		if (cursor_col > 0) {
			cursor_col--;
			vga[cursor_row * VGA_WIDTH + cursor_col] = (unsigned short)' ' | VGA_WHITE_ON_BLACK_STYLE;
		}
		break;
	default:
		vga[cursor_row * VGA_WIDTH + cursor_col] = (unsigned short)c | VGA_WHITE_ON_BLACK_STYLE;
		cursor_col++;
		if (cursor_col >= VGA_WIDTH) {
			cursor_col = 0;
			cursor_row++; // wrap to the next line instead of overwriting this one
		}
		break;
	}
	if (cursor_row >= VGA_HEIGHT) {
		vga_scroll();
	}
}

void vga_puts(const char *s) {
	while (*s) {
		vga_putc(*s++);
	}
}

// print a 64-bit value as a 0x-prefixed, zero-padded hex string
void vga_put_hex64(uint64_t value) {
	static const char hex_digits[] = "0123456789abcdef";
	vga_puts("0x");
	for (int shift = 60; shift >= 0; shift -= 4) {
		vga_putc(hex_digits[(value >> shift) & 0xF]);
	}
}

// =====================================================================
// Cursor
// =====================================================================

static unsigned char cursor_visible = 1;

static void enable_cursor(uint8_t start, uint8_t end) {
	outb(0x3D4, 0x0A);
	outb(0x3D5, (inb(0x3D5) & 0xC0) | start);

	outb(0x3D4, 0x0B);
	outb(0x3D5, (inb(0x3D5) & 0xE0) | end);
}

static void disable_cursor(void) {
	outb(0x3D4, 0x0A);
	outb(0x3D5, 0x20);
}

static void update_cursor(int x, int y) {
	uint16_t pos = y * VGA_WIDTH + x;

	outb(0x3D4, 0x0F);
	outb(0x3D5, (uint8_t)(pos & 0xFF));
	outb(0x3D4, 0x0E);
	outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

// turn the hardware cursor on at boot, as a thin underline (scanlines 13-14
// of the 16-scanline glyph box)
void vga_init_cursor(void) {
	enable_cursor(13, 14);
}

// re-sync the blinking hardware cursor to wherever vga_putc last left off;
// callers (e.g. the keyboard handler) shouldn't need to know cursor_row/col
void vga_sync_hw_cursor(void) {
	update_cursor(cursor_col, cursor_row);
}

// hand back the software write-cursor's position — lets a caller (e.g. the
// keyboard driver) remember "where a line of input started" without vga.c
// exposing cursor_row/col directly
void vga_get_cursor(int *row, int *col) {
	*row = cursor_row;
	*col = cursor_col;
}

// move the write cursor to `offset` columns past (base_row, base_col),
// wrapping into following rows the same way typing would — without
// printing or erasing anything. This is what lets the keyboard driver
// reposition within a line of input it's editing (e.g. after an arrow
// key) purely by column math, never touching cursor_row/col itself.
void vga_place_cursor(int base_row, int base_col, int offset) {
	int total  = base_col + offset;
	cursor_row = base_row + total / VGA_WIDTH;
	cursor_col = total % VGA_WIDTH;
	if (cursor_row >= VGA_HEIGHT) {
		// line ran past the bottom of the screen — this driver doesn't
		// track how much vga_scroll() has shifted things mid-edit, so
		// just clamp instead of writing to a bogus row
		cursor_row = VGA_HEIGHT - 1;
	}
}

void vga_toggle_cursor(void) {
	if (cursor_visible > 0) {
		cursor_visible = 0;
		disable_cursor();
	} else {
		cursor_visible = 1;
		enable_cursor(13, 14);
		update_cursor(cursor_col, cursor_row);
	}
}
