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

# create stack
.section .bss
.align   16
stack_bottom:
.skip 16384     # 16 kib stack
stack_top:

# set entry point
# this is where GRUB first jumps to code-wise
.section .text
.global  _start

_start:
    mov  $stack_top, %esp   # set stack pointer
    call kernel_main        # call C main
.hang:
# halt CPU until next interrupt
    hlt
    jmp .hang
