#include <stdint.h>

// =====================================================================
// IDT (Interrupt Descriptor Table)
//
// The IDT tells the CPU where to jump when an interrupt or exception
// fires (e.g. divide-by-zero, keyboard input, timer ticks). Each entry
// points at a small assembly "stub" (in isr.s) that saves registers and
// calls into the matching *_handler() function below.
// =====================================================================

#define IDT_SIZE 256
#define GATE_INTERRUPT 0x8E // present, ring 0, 64-bit interrupt gate

// assembly stubs defined in isr.s
extern void isr0(void); // divide by zero exception
extern void irq0(void); // IRQ0: timer
extern void irq1(void); // IRQ1: keyboard

// mirrors the CPU's 64-bit interrupt descriptor table entry layout
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

// fills in one IDT entry so `vector` jumps to `handler` on interrupt
void idt_set_entry(int vector, void (*handler)(), uint8_t type_attr) {
	uint64_t address = (uint64_t)handler; // handler address as a 64-bit integer

	idt[vector].offset_low  = address & 0xFFFF;
	idt[vector].selector    = 0x08;
	idt[vector].ist         = 0;
	idt[vector].type_attr   = type_attr;
	idt[vector].offset_mid  = (address >> 16) & 0xFFFF;
	idt[vector].offset_high = (address >> 32) & 0xFFFFFFFF;
	idt[vector].zero        = 0;
}

// builds the IDT and loads it with the `lidt` instruction
void idt_init(void) {
	idt_set_entry(0, isr0, GATE_INTERRUPT);  // divide by zero exception
	idt_set_entry(32, irq0, GATE_INTERRUPT); // IRQ0 for timer
	idt_set_entry(33, irq1, GATE_INTERRUPT); // IRQ1 for keyboard, 32 + 1

	idt_ptr.limit = sizeof(idt) - 1;
	idt_ptr.base  = (uint64_t)&idt;

	__asm__ volatile("lidt %0" : : "m"(idt_ptr));
}

// =====================================================================
// VGA text mode output
// =====================================================================

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

static int cursor_row = 0;
static int cursor_col = 0;

void vga_scroll(void) {
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

// =====================================================================
// Port I/O helpers
//
// `in`/`out` talk to hardware over the x86 I/O port space (separate from
// memory addresses) — used here to program the PIC and read the keyboard.
// =====================================================================

static inline void outb(uint16_t port, uint8_t val) {
	__asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
	uint8_t ret;
	__asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

// =====================================================================
// PIC (8259 Programmable Interrupt Controller)
//
// By default the PIC fires IRQs on vectors 0-15, which collide with CPU
// exceptions. Remapping moves them to 32-47 so they don't overlap.
// =====================================================================

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1

void pic_remap(void) {
	// save current masks
	uint8_t mask1 = inb(PIC1_DATA);
	uint8_t mask2 = inb(PIC2_DATA);

	// start init sequence in cascade mode
	outb(PIC1_COMMAND, 0x11);
	outb(PIC2_COMMAND, 0x11);

	// remap master PIC -> 32-39, slave PIC -> 40-47
	outb(PIC1_DATA, 0x20);
	outb(PIC2_DATA, 0x28);

	outb(PIC1_DATA, 0x04); // tell master PIC there's a slave at IRQ2
	outb(PIC2_DATA, 0x02); // tell slave PIC its cascade identity

	// set 8086 mode
	outb(PIC1_DATA, 0x01);
	outb(PIC2_DATA, 0x01);

	// restore masks, but unmask IRQ1 (keyboard) on master
	outb(PIC1_DATA, mask1 & ~0x02);
	outb(PIC2_DATA, mask2);
}

// =====================================================================
// Interrupt handlers
//
// Called from the assembly stubs in isr.s after registers are saved.
// =====================================================================

// vector 0: divide by zero
void isr_handler(void) {
	write_vga_line(0, "Divide by zero exception!");
	for (;;) {
		__asm__("hlt");
	}
}

// vector 32 (IRQ0): timer, currently just acknowledged and ignored
void irq0_handler(void) {
	outb(PIC1_COMMAND, 0x20); // send end-of-interrupt to master PIC
}

// =====================================================================
// Cursor
// =====================================================================

unsigned char cursor_visible = 1;

void enable_cursor(uint8_t start, uint8_t end) {
	outb(0x3D4, 0x0A);
	outb(0x3D5, (inb(0x3D5) & 0xC0) | start);

	outb(0x3D4, 0x0B);
	outb(0x3D5, (inb(0x3D5) & 0xE0) | end);
}

void disable_cursor(void) {
	outb(0x3D4, 0x0A);
	outb(0x3D5, 0x20);
}

void update_cursor(int x, int y) {
	uint16_t pos = y * VGA_WIDTH + x;

	outb(0x3D4, 0x0F);
	outb(0x3D5, (uint8_t)(pos & 0xFF));
	outb(0x3D4, 0x0E);
	outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

// =====================================================================
// String Helpers
// =====================================================================

int strlen(const char *s) {
	int len = 0;
	while (s[len]) {
		len++;
	}
	return len;
}

int strcmp(const char *s1, const char *s2) {
	while (*s1 && (*s1 == *s2)) {
		s1++;
		s2++;
	}
	return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

// check if s1 starts with s2, and if so return a pointer to the first
// character after the match or the first space after the match, otherwise null.
// differentiates between 'echoes' and 'echo'
const char *str_match_prefix(const char *s1, const char *s2) {
	while (*s2) {
		if (*s1 != *s2)
			return 0;
		s1++;
		s2++;
	}
	if (*s1 == ' ')
		return s1 + 1;
	if (*s1 == 0)
		return s1;
	return 0;
}

// =====================================================================
// Shell
// =====================================================================

void cmd_echo(const char *args) {
	vga_puts(args);
	vga_putc('\n');
}

void cmd_clear(const char *args) {
	(void)args;
	cursor_row = 0;
	cursor_col = 0;
	clear_vga();
}

void cmd_divide_by_zero(const char *args) {
	(void)args;
	// -O should be at 0 to prevent the compiler from optimizing out the divide by zero
	volatile int a = 1, b = 0;
	int          c = a / b; // this will trigger the divide by zero exception
	(void)c;
}

void cmd_toggle_cursor(const char *args) {
	(void)args;
	if (cursor_visible > 0) {
		cursor_visible = 0;
		disable_cursor();
	} else {
		cursor_visible = 1;
		enable_cursor(13, 14);
		update_cursor(cursor_col, cursor_row);
	}
}

struct cmd {
	const char *name;
	void (*handler)(const char *args);
};

static const struct cmd cmd_table[] = {
	{ "e",    cmd_echo           },
	{ "clr",  cmd_clear          },
	{ "dbz",  cmd_divide_by_zero },
	{ "tcur", cmd_toggle_cursor  },
	{ 0,      0	              }  // sentinel
};

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

#define INPUT_BUFFER_SIZE 128
static char input_buffer[INPUT_BUFFER_SIZE];
static int  input_length = 0;

const char *prompt = "O-(^W^)-> ";

void shell_prompt(void) {
	vga_puts(prompt);
}

// scancode set 1 -> ASCII, indexed by the raw byte read from the keyboard
// controller. 0 means "no ASCII equivalent" (shift, ctrl, arrow keys, etc).
// clang-format off
static const char scancode_ascii[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t',   'q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,       'a','s','d','f','g','h','j','k','l',';','\'','`',
    0, '\\',  'z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' ',
};
// clang-format on

// vector 33 (IRQ1): keyboard
void irq1_handler(void) {
	uint8_t scancode = inb(0x60);

	if (!(scancode & 0x80)) { // key press (not release)
		char c = scancode_ascii[scancode];
		if (!c) {
			outb(PIC1_COMMAND, 0x20); // if cant get char, ignore
			return;
		}

		switch (c) {
		case '\n':
			input_buffer[input_length] = 0;
			vga_putc('\n');
			shell_dispatch(input_buffer);
			input_length = 0;
			shell_prompt();
			break;
		case '\b':
			if (input_length > 0) {
				input_length--;
				vga_putc('\b');
			}
			break;
		default:
			if (input_length < INPUT_BUFFER_SIZE - 1) {
				input_buffer[input_length++] = c;
				vga_putc(scancode_ascii[scancode]);
			}
			break;
		}
		update_cursor(cursor_col, cursor_row);
	}

	outb(PIC1_COMMAND, 0x20); // send end-of-interrupt to master PIC
}

// =====================================================================
// Entry point
// =====================================================================

void kernel_main(void) {
	idt_init();
	pic_remap();
	__asm__ volatile("sti"); // enable interrupts

	clear_vga();
	enable_cursor(13, 14);
	const char *msg = "Hello, World! Goodbye. Space?\n";
	vga_puts(msg);
	shell_prompt();

	for (;;) {
		// halt until the next interrupt (timer/keyboard) instead of spinning
		__asm__("hlt");
	}
}
