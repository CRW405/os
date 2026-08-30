.set MB2_MAGIC,  0xE85250D6
.set MB2_ARCH,   0
.set MB2_HDR_LEN, (mb2_header_end - mb2_header_start)
.set MB2_CHECKSUM, -(MB2_MAGIC + MB2_ARCH + MB2_HDR_LEN)

.section .multiboot, "a"
.align 8
mb2_header_start:
    .long MB2_MAGIC
    .long MB2_ARCH
    .long MB2_HDR_LEN
    .long MB2_CHECKSUM

    # required end tag
    .align 8
    .word 0
    .word 0
    .long 8
mb2_header_end:

.section .bss
.align 4096
p4_table:
    .skip 4096
p3_table:
    .skip 4096
p2_table:
    .skip 4096
.align 16
stack_bottom:
    .skip 16384
stack_top:

.section .rodata
gdt64:
    .quad 0
gdt64_code:
    .quad (1<<43) | (1<<44) | (1<< 47) | (1<<53)
        # execs | code/data type | present | long mode
gdt64_pointer:
    .word . - gdt64 - 1
    .quad gdt64

.section .text
.code32
.global _start
_start:
    mov $stack_top, %esp

    call check_multiboot
    call check_cpuid
    call check_long_mode

    call setup_page_tables
    call enable_paging

    lgdt (gdt64_pointer)
    ljmp $(gdt64_code -gdt64), $long_mode_start

check_multiboot:
    cmp $0x36D76289, %eax
    jne .halt
    ret

check_cpuid:
    pushfl
    pop %eax
    mov %eax, %ecx
    xor $(1<<21), %eax
    push %eax
    popfl
    pushfl
    pop %eax
    push %ecx
    popfl
    xor %ecx, %eax
    jz .halt
    ret

check_long_mode:
    mov $0x80000000, %eax
    cpuid
    cmp $0x80000001, %eax
    jb .halt
    mov $0x80000001, %eax
    cpuid
    test $(1<<29), %edx
    jz .halt
    ret

setup_page_tables:
    mov $p3_table, %eax
    or $0b11, %eax
    mov %eax, (p4_table)

    mov $p2_table, %eax
    or $0b11, %eax
    mov %eax, (p3_table)

    xor %ecx, %ecx
.map_p2_table:
    mov $0x200000, %eax
    mul %ecx
    or $0b10000011, %eax
    mov %eax, p2_table(,%ecx,8)
    inc %ecx
    cmp $512, %ecx
    jne .map_p2_table
    ret

enable_paging:
    mov $p4_table, %eax
    mov %eax, %cr3

    mov %cr4, %eax
    or $(1<<5), %eax
    mov %eax, %cr4

    mov $0xC0000080, %ecx
    rdmsr
    or $(1<<8), %eax
    wrmsr

    mov %cr0, %eax
    or $(1<<31), %eax
    mov %eax, %cr0
    ret

.halt:
    hlt
    jmp .halt

.code64
long_mode_start:
    xor %ax, %ax
    mov %ax, %ss
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    call kernel_main
    jmp .halt
