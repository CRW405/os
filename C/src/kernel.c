const int VGA_WIDTH = 80;
const int VGA_HEIGHT = 25;
const unsigned short VGA_WHITE_ON_BLACK_STYLE = 0x0700;

// vga text memory, ASCII byte + color byte
// no interrupts needed in protected mode
volatile unsigned short *vga = (unsigned short *)0xB8000;

void write_vga_line(int line, const char *string) {
	for (int i = 0; string[i]; i++) {
		// write each char into vga with white on black
		vga[line * VGA_WIDTH + i] = (unsigned short)string[i] | VGA_WHITE_ON_BLACK_STYLE;
	}
}

void clear_vga() {
	for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
		vga[i] = (unsigned short)' ' | VGA_WHITE_ON_BLACK_STYLE;
	}
}

void kernel_main(void) {
	const char *msg = "Hello, World! Goodbye. Space?";

	clear_vga();
	write_vga_line(VGA_HEIGHT / 2, msg);

	for (;;) {
		// halt loop
		__asm__("hlt");
	}
}
