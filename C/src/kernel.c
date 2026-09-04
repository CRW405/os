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

// assembly stubs defined in isr.s: one per CPU exception vector (0-31),
// plus the two hardware IRQs we handle
extern void isr0(void);  // vector 0: divide-by-zero
extern void isr1(void);  // vector 1: debug
extern void isr2(void);  // vector 2: non-maskable interrupt
extern void isr3(void);  // vector 3: breakpoint
extern void isr4(void);  // vector 4: overflow
extern void isr5(void);  // vector 5: bound range exceeded
extern void isr6(void);  // vector 6: invalid opcode
extern void isr7(void);  // vector 7: device not available
extern void isr8(void);  // vector 8: double fault
extern void isr9(void);  // vector 9: coprocessor segment overrun
extern void isr10(void); // vector 10: invalid TSS
extern void isr11(void); // vector 11: segment not present
extern void isr12(void); // vector 12: stack-segment fault
extern void isr13(void); // vector 13: general protection fault
extern void isr14(void); // vector 14: page fault
extern void isr15(void); // vector 15: reserved
extern void isr16(void); // vector 16: x87 floating-point exceptions
extern void isr17(void); // vector 17: alignment check
extern void isr18(void); // vector 18: machine check
extern void isr19(void); // vector 19: SIMD floating-point exceptions
extern void isr20(void); // vector 20: virtualization exceptions
extern void isr21(void); // vector 21: control protection exceptions
extern void isr22(void); // vector 22: reserved
extern void isr23(void); // vector 23: reserved
extern void isr24(void); // vector 24: reserved
extern void isr25(void); // vector 25: reserved
extern void isr26(void); // vector 26: reserved
extern void isr27(void); // vector 27: reserved
extern void isr28(void); // vector 28: hypervisor injection exceptions
extern void isr29(void); // vector 29: VMM communication exceptions
extern void isr30(void); // vector 30: security exceptions
extern void isr31(void); // vector 31: reserved
extern void irq0(void);  // IRQ0: timer
extern void irq1(void);  // IRQ1: keyboard

// indexed by vector number so idt_init can fill 0-31 in a loop
static void (*const isr_stub_table[32])(void) = {
	isr0,
	isr1,
	isr2,
	isr3,
	isr4,
	isr5,
	isr6,
	isr7,
	isr8,
	isr9,
	isr10,
	isr11,
	isr12,
	isr13,
	isr14,
	isr15,
	isr16,
	isr17,
	isr18,
	isr19,
	isr20,
	isr21,
	isr22,
	isr23,
	isr24,
	isr25,
	isr26,
	isr27,
	isr28,
	isr29,
	isr30,
	isr31,
};

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
	for (int vector = 0; vector < 32; vector++) {
		idt_set_entry(vector, isr_stub_table[vector], GATE_INTERRUPT); // CPU exceptions
	}

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
// IRQ (Interrupt Request)
// By default the PIC fires IRQs on vectors 0-15, which collide with CPU
// exceptions. Remapping moves them to 32-47 so they don't overlap.
//
// Master PIC handles IRQ0-IRQ7 and is directly connected to the CPU,
// slave PIC handles IRQ8-IRQ15. The slave
// is connected to the master via IRQ2, so the master must be told when
// the slave has finished handling an interrupt.
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

void pic_send_eoi(uint8_t irq) {
	if (irq >= 8) {
		outb(PIC2_COMMAND, 0x20); // acknowledge slave PIC
	}
	outb(PIC1_COMMAND, 0x20); // always acknowledge master PIC
}

// =====================================================================
// Interrupt handlers
//
// Called from the assembly stubs in isr.s after registers are saved.
// =====================================================================

// mirrors the register order isr_common_stub (isr.s) pushes onto the
// stack, so this struct pointer — handed to isr_common_handler in %rdi —
// can walk right over the saved frame
struct registers {
	uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
	uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
	uint64_t vector, error_code;
	uint64_t rip, cs, rflags;
};

// exception names for vectors 0-31, straight from the Intel SDM
static const char *exception_names[32] = {
	"Divide-by-zero Error",
	"Debug",
	"Non-maskable Interrupt",
	"Breakpoint",
	"Overflow",
	"Bound Range Exceeded",
	"Invalid Opcode",
	"Device Not Available",
	"Double Fault",
	"Coprocessor Segment Overrun",
	"Invalid TSS",
	"Segment Not Present",
	"Stack-Segment Fault",
	"General Protection Fault",
	"Page Fault",
	"Reserved",
	"x87 Floating-Point Exception",
	"Alignment Check",
	"Machine Check",
	"SIMD Floating-Point Exception",
	"Virtualization Exception",
	"Control Protection Exception",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Hypervisor Injection Exception",
	"VMM Communication Exception",
	"Security Exception",
	"Reserved",
};

// print a 64-bit value as a 0x-prefixed, zero-padded hex string
static void vga_put_hex64(uint64_t value) {
	static const char hex_digits[] = "0123456789abcdef";
	vga_puts("0x");
	for (int shift = 60; shift >= 0; shift -= 4) {
		vga_putc(hex_digits[(value >> shift) & 0xF]);
	}
}

static void print_reg(const char *name, uint64_t value) {
	vga_puts(name);
	vga_put_hex64(value);
	vga_putc(' ');
}

// unified handler for every CPU exception (vectors 0-31), called from the
// matching isrN stub in isr.s with a pointer to the saved register frame.
// There's no recovering from most of these in a kernel this young, so
// instead of silently triple faulting and rebooting, this dumps
// everything useful to the screen and halts.
void isr_common_handler(struct registers *regs) {
	vga_puts("\n*** Exception: ");
	vga_puts(exception_names[regs->vector]);
	vga_puts(" (vector ");
	vga_put_hex64(regs->vector);
	vga_puts(", error code ");
	vga_put_hex64(regs->error_code);
	vga_puts(") ***\n");

	if (regs->vector == 14) { // page fault: cr2 holds the faulting address
		uint64_t cr2;
		__asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
		vga_puts("Faulting address: ");
		vga_put_hex64(cr2);
		vga_putc('\n');
	}

	print_reg("RAX=", regs->rax);
	print_reg("RBX=", regs->rbx);
	print_reg("RCX=", regs->rcx);
	print_reg("RDX=", regs->rdx);
	vga_putc('\n');
	print_reg("RSI=", regs->rsi);
	print_reg("RDI=", regs->rdi);
	print_reg("RBP=", regs->rbp);
	print_reg("RIP=", regs->rip);
	vga_putc('\n');
	print_reg("R8= ", regs->r8);
	print_reg("R9= ", regs->r9);
	print_reg("R10=", regs->r10);
	print_reg("R11=", regs->r11);
	vga_putc('\n');
	print_reg("R12=", regs->r12);
	print_reg("R13=", regs->r13);
	print_reg("R14=", regs->r14);
	print_reg("R15=", regs->r15);
	vga_putc('\n');
	print_reg("CS= ", regs->cs);
	print_reg("RFLAGS=", regs->rflags);
	vga_putc('\n');

	vga_puts("System halted.\n");
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

static unsigned char cursor_visible = 1;

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

// lowercase a alphabetical character
char clower(char c) {
	if (c >= 'A' && c <= 'Z') {
		return c + ('a' - 'A');
	}
	return c;
}

// lowercase a string
const char *strlower(const char *s) {
	static char buf[256];
	int         i;
	for (i = 0; s[i]; i++) {
		buf[i] = clower(s[i]);
	}
	buf[i] = 0;
	return buf;
}

const char *itoa(int value, char *buffer, int base) {
	if (base < 2 || base > 36) {
		buffer[0] = '\0';
		return buffer;
	}

	char *ptr = buffer, *ptr1 = buffer, tmp_char;
	int   tmp_value;

	do {
		tmp_value = value;
		value /= base;
		*ptr++ = "0123456789abcdefghijklmnopqrstuvwxyz"[tmp_value - value * base];
	} while (value);

	// Apply negative sign for base 10
	if (tmp_value < 0 && base == 10) {
		*ptr++ = '-';
	}
	*ptr-- = '\0';

	while (ptr1 < ptr) {
		tmp_char = *ptr;
		*ptr--   = *ptr1;
		*ptr1++  = tmp_char;
	}
	return buffer;
}

// =====================================================================
// multiboot parsing
// =====================================================================

typedef struct multiboot2_tag {
	uint32_t type;
	uint32_t size;
} multiboot2_tag_t;

void parse_multiboot2_info(uint32_t addr) {
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

void cmd_page_fault(const char *args) {
	(void)args;
	// boot64.s only identity-maps the first 1GB, so this address is
	// guaranteed to be unmapped and trigger a #PF (vector 14)
	volatile uint64_t *bad_ptr = (volatile uint64_t *)0xFFFFFFFF00000000ULL;
	*bad_ptr                   = 0;
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

// saved by boot64.s at boot time, before %ebx gets reused for anything else
extern uint32_t multiboot_info;

void cmd_parse_multiboot(const char *args) {
	(void)args;
	parse_multiboot2_info(multiboot_info);
}

void cmd_print_cmds(const char *args);

struct cmd {
	const char *name;
	void (*handler)(const char *args);
	const char *desc;
};

int                     cmd_table_length = 7;
static const struct cmd cmd_table[]      = {
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
	vga_puts("Commands:\n");
	for (int i = 0; i < cmd_table_length; i++) {
		vga_puts(cmd_table[i].name);
		vga_puts(": ");
		vga_puts(cmd_table[i].desc);
		vga_puts("\n");
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

#define INPUT_BUFFER_SIZE 128
static char input_buffer[INPUT_BUFFER_SIZE];
static int  input_length = 0;

const char *prompt = "O-(^W^)-> ";

void shell_prompt(void) {
	vga_puts(prompt);
}

#define SC_LSHIFT 0x2A
#define SC_RSHIFT 0x36
#define SC_LSHIFT_UP 0xAA
#define SC_RSHIFT_UP 0xB6
#define SC_CAPSLOCK 0x3A
#define SC_LCTRL 0x1D
#define SC_RCTRL 0x38
#define SC_LCTRL_UP 0x9D
#define SC_RCTRL_UP 0xB8
#define SC_LALT 0x38
#define SC_RALT 0x38
#define SC_LALT_UP 0xB8
#define SC_RALT_UP 0xB8

static unsigned char shift_pressed = 0;
static unsigned char caps_lock_on  = 0;
static unsigned char ctrl_pressed  = 0;
static unsigned char alt_pressed   = 0;

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

static const char scancode_ascii_shift[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t',   'Q','W','E','R','T','Y','U','I','O','P','{','}', '\n',
    0,       'A','S','D','F','G','H','J','K','L',':','\"','~',
    0, '|',    'Z','X','C','V','B','N','M', '<', '>', '?', 0,
    '*', 0, ' ',
};
// clang-format on

// vector 33 (IRQ1): keyboard
void irq1_handler(void) {
	uint8_t    scancode = inb(0x60);
	static int extended = 0;

	// handle extended scancodes such as arrow keys, which start with 0xE0
	if (scancode == 0xE0) {
		extended = 1;
		pic_send_eoi(1); // acknowledge the keyboard interrupt
		return;
	}

	// handle arrow keys and other extended keys
	if (extended) {
		extended = 0; // reset extended flag
		if (!(scancode & 0x80)) {
			switch (scancode) {
			case 0x48: // up arrow
				vga_puts("[U]");
				break;
			case 0x50: // down arrow
				vga_puts("[D]");
				break;
			case 0x4B: // left arrow
				vga_puts("[L]");
				break;
			case 0x4D: // right arrow
				vga_puts("[R]");
				break;
			default:
				vga_puts("[?]");
				break;
			}
			extended = 0;
			outb(PIC1_COMMAND, 0x20);
			return;
		}
	}

	// apply modifier keys
	switch (scancode) {
	case SC_LSHIFT:
	case SC_RSHIFT:
		shift_pressed = 1;
		outb(PIC1_COMMAND, 0x20);
		return;
		break;
	case SC_LSHIFT_UP:
	case SC_RSHIFT_UP:
		shift_pressed = 0;
		outb(PIC1_COMMAND, 0x20);
		return;
		break;
	case SC_CAPSLOCK:
		caps_lock_on = !caps_lock_on;
		outb(PIC1_COMMAND, 0x20);
		return;
		break;
	case SC_LCTRL:
		// case SC_RCTRL:
		ctrl_pressed = 1;
		outb(PIC1_COMMAND, 0x20);
		return;
		break;
	case SC_LCTRL_UP:
		// case SC_RCTRL_UP:
		ctrl_pressed = 0;
		outb(PIC1_COMMAND, 0x20);
		return;
		break;
	case SC_LALT:
		// case SC_RALT:
		alt_pressed = 1;
		outb(PIC1_COMMAND, 0x20);
		return;
		break;
	case SC_LALT_UP:
		// case SC_RALT_UP:
		alt_pressed = 0;
		outb(PIC1_COMMAND, 0x20);
		return;
		break;
	default:
		break;
	}

	static unsigned int tab_length = 4;

	// symbol keys
	if (!(scancode & 0x80)) { // key press (not release)
		char c = shift_pressed ? scancode_ascii_shift[scancode] : scancode_ascii[scancode];

		// if cant get char, ignore
		if (!c) {
			outb(PIC1_COMMAND, 0x20);
			return;
		}

		if (ctrl_pressed) {
			if (clower(c) == 'c') {
				// cancel current line
				input_buffer[input_length] = 0;
				input_length               = 0;
				vga_puts("^c\n");
				shell_prompt();
				outb(PIC1_COMMAND, 0x20);
				return;
			}
		}

		if (alt_pressed) {
			(void)0;
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
		case '\t':
			if (input_length < INPUT_BUFFER_SIZE - tab_length - 1) {
				for (int i = 0; i < tab_length; i++) {
					input_buffer[input_length++] = ' ';
					vga_putc(' ');
				}
			}
			break;
		default:
			if (input_length < INPUT_BUFFER_SIZE - 1) {
				input_buffer[input_length++] = c;
				vga_putc(c);
			}
			break;
		}
		update_cursor(cursor_col, cursor_row);
	}

	pic_send_eoi(1); // acknowledge the keyboard interrupt
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
	vga_puts("Hello, World! Goodbye. Space?\n");
	vga_puts("enter help to see list of commands.\n");
	shell_prompt();

	for (;;) {
		// halt until the next interrupt (timer/keyboard) instead of spinning
		__asm__("hlt");
	}
}
