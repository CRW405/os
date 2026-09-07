#include "interrupts.h"

#include <stdint.h>

#include "io.h"
#include "pic.h"
#include "vga.h"

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
