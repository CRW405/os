#include "io.h"

// =====================================================================
// Port I/O helpers
//
// `in`/`out` talk to hardware over the x86 I/O port space (separate from
// memory addresses) — used here to program the PIC and read the keyboard.
//
// These used to be `static inline` so the compiler could fold them away
// entirely, but that means the definition has to live in a header (a
// `static inline` in a .c file is invisible to every other .c file).
// Since the notes belong here and not in io.h, they're plain functions
// now — a real `call`/`ret` instead of the instruction being inlined
// directly at the call site. Not worth worrying about in a learning
// kernel; a real OS would put these back in the header for speed.
// =====================================================================

void outb(uint16_t port, uint8_t val) {
	__asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

uint8_t inb(uint16_t port) {
	uint8_t ret;
	__asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}
