#include "idt.h"

#include <stdint.h>

// =====================================================================
// IDT (Interrupt Descriptor Table)
//
// The IDT tells the CPU where to jump when an interrupt or exception
// fires (e.g. divide-by-zero, keyboard input, timer ticks). Each entry
// points at a small assembly "stub" (in isr.s) that saves registers and
// calls into the matching *_handler() function (see interrupts.c and
// keyboard.c).
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
static void idt_set_entry(int vector, void (*handler)(), uint8_t type_attr) {
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
