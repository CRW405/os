#pragma once

#include <stdint.h>

void clear_vga(void);
void write_vga_line(int line, const char *string);
void vga_putc(char c);
void vga_puts(const char *s);
void vga_put_hex64(uint64_t value);

void vga_init_cursor(void);
void vga_sync_hw_cursor(void);
void vga_toggle_cursor(void);

void vga_get_cursor(int *row, int *col);
void vga_place_cursor(int base_row, int base_col, int offset);
