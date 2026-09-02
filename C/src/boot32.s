# =====================================================================
# Multiboot header
#
# GRUB scans the first 8 KiB of the kernel image for this magic number.
# If found, it reads FLAGS/CHECKSUM right after it to confirm the image
# is a valid multiboot kernel, then loads it and jumps to _start.
# =====================================================================

# 12 byte GRUB header
.set MAGIC,     0x1BADB002          # GRUB looks for this value
.set FLAGS,     0x0                 # bitmask based GRUB requests,
                                    # explicitly stating none
                                    # bit 0 - align boot modules on 4kb page bounds
                                    # bit 1 - bootloader must pass memory map
                                    # bit 2 - bootloader must pass video mode info
.set CHECKSUM,  -(MAGIC + FLAGS)    # header checksums

# build the header section
.section .multiboot, "a"
.align   4
.long    MAGIC
.long    FLAGS
.long    CHECKSUM

# =====================================================================
# Stack
#
# There's no stack until we make one — GRUB doesn't set one up for us.
# .skip just reserves uninitialized space in .bss; stack_top is where
# %esp gets pointed since the stack grows downward from there.
# =====================================================================

.section .bss
.align   16
stack_bottom:
.skip 16384     # 16 kib stack
stack_top:

# =====================================================================
# Entry point
#
# This is where GRUB first jumps to code-wise. We're in 32-bit protected
# mode at this point, with no C runtime, so the very first thing to do
# is set up a stack before calling into C.
# =====================================================================

.section .text
.global  _start

_start:
    mov  $stack_top, %esp   # set stack pointer
    call kernel_main        # call C main
.hang:
# halt CPU until next interrupt
    hlt
    jmp .hang
