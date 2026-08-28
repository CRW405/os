# os
Learning about how operating systems work

- build via: `nasm -f bin <program>.asm -o <output>`
- start via: `qemu-system-x86_64 -fda <bin>`
- view via:  `vncviewer localhost:<port>`
- or, start and view via: `qemu-system-x86_64 -drive format=raw,file=hello.bin -nographic`. exit with ctrl+A then X
