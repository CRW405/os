
# os

## OS

Learning about how operating systems work

- build via: `nasm -f bin <program>.asm -o <output>`
- start via: `qemu-system-x86_64 -fda <bin>`
- view via:  `vncviewer localhost:<port>`
- or, start and view via: `qemu-system-x86_64 -drive format=raw,file=hello.bin -nographic`. exit with ctrl+A then X

## Notes

| Register | Name | Use | C Equivelent |
| - | - | - | - |
| ax | Accumulator | math, BIOS returns | int ax; |
| bx | base | mem index / pointer | void* bx; |
| cx | count | loop counter | int i; |
| dx | data | math, io ports | int dx |
| si / di | source / destiniation | string and array pointers | char *src, *dst; |
| sp / bp | stack / base pointer | stack frame management | ... |

```
mov ax, 5       ; ax = 5
mov bx, ax      ; bx = ax
add ax, 10      ; ax += 10
sub bx, 1       ; bx -= 1
```

destiniation <- source ; right to left

[] dereferences
```
mov si, 0x7c00  ; put an address into si
mov al, [si]    ; get byte at that address
                ; write 'A' at si
mov byte [si], 'A'
```

| Command | Action |
| - | - |
| je | jump if equal |
| jz | jump if zero |
| jne | jump if not equal |
| jnz | jump if not equal |
| jg | jump if greater |
| jl | jump if lesser |
| jmp | jump unconditionally |


