#include <stdint.h>

#define IDT_SIZE 256
#define GATE_INTERRUPT 0x8E
// 0x8E = present, ring 0, 64-bit interrupt gate

extern void isr0(void); // divide by zero

// mirror the 64 bit interrupt descriptor table
struct idt_entry {
	uint16_t offset_low;  // handler address bits 0..15
	uint16_t selector;    // code segment selector
	uint8_t  ist;         // interrupt stack table index, 0 = not used
	uint8_t  type_attr;   // gate type, privilege level, present bit
	uint16_t offset_mid;  // handler address bits 16..31
	uint32_t offset_high; // handler address bits 32..63
	uint32_t zero;        // reserved, must be zero
} __attribute__((packed));

struct idt_ptr {
	uint16_t limit; // size of the IDT in bytes - 1
	uint64_t base;  // address of the first element in the IDT
} __attribute__((packed));

static struct idt_entry idt[IDT_SIZE];
static struct idt_ptr   idt_ptr;

void idt_set_entry(int vector, void (*handler)(), uint8_t type_attr) {
	uint64_t address        = (uint64_t)handler; // addresss of handler as 64 bit integer
	idt[vector].offset_low  = address & 0xFFFF;
	idt[vector].selector    = 0x08;
	idt[vector].ist         = 0;
	idt[vector].type_attr   = type_attr;
	idt[vector].offset_mid  = (address >> 16) & 0xFFFF;
	idt[vector].offset_high = (address >> 32) & 0xFFFFFFFF;
	idt[vector].zero        = 0;
}

void idt_init(void) {
	idt_set_entry(0, isr0, GATE_INTERRUPT); // divide by zero exception

	idt_ptr.limit = sizeof(idt) - 1;
	idt_ptr.base  = (uint64_t)&idt;

	__asm__ volatile("lidt %0" : : "m"(idt_ptr)); // load the IDT pointer into the CPU
}

const int            VGA_WIDTH                = 80;
const int            VGA_HEIGHT               = 25;
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

void clear_vga(void) {
	for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
		vga[i] = (unsigned short)' ' | VGA_WHITE_ON_BLACK_STYLE;
	}
}

void isr_handler(void) {
	// handle the interrupt
	write_vga_line(0, "Divide by zero exception!");
	for (;;) {
		__asm__("hlt");
	}
}

void kernel_main(void) {
	idt_init();

	const char *msg = "Hello, World! Goodbye. Space?";

	clear_vga();
	write_vga_line(VGA_HEIGHT / 2, msg);
	const char *hi = "hi";
	write_vga_line(VGA_HEIGHT - 1, hi);

	// !!! make sure -O is set to 0 for this to not be optimized away
	volatile int a = 1, b = 0;
	int          c = a / b; // this will trigger the divide by zero exception
	(void)c;                // avoid unused variable warning

	for (;;) {
		// halt loop
		__asm__("hlt");
	}
}
