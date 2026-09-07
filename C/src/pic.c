#include "pic.h"

#include "io.h"

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
