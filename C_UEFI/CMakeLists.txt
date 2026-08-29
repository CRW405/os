cmake_minimum_required(VERSION 3.15)
project(MyUEFIKernel C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffreestanding -fno-stack-protector -fpic -fshort-wchar -mno-red-zone -nostdlib")

add_executable(kernel main.c)

set_target_properties(kernel PROPERTIES
    LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/link.ld"
    LINK_FLAGS "-nostdlib -znocombreloc -shared -Bsymbolic -T ${CMAKE_CURRENT_SOURCE_DIR}/link.ld"
    OUTPUT_NAME "kernel"
    SUFFIX ".so"
)

add_custom_command(TARGET kernel POST_BUILD
    COMMAND ${CMAKE_OBJCOPY}
        -j .text -j .sdata -j .data -j .rodata -j .bss
        --set-section-flags .bss=alloc,load,contents
        --output-target efi-app-x86_64
        $<TARGET_FILE:kernel>
        ${CMAKE_BINARY_DIR}/mnt/EFI/BOOT/BOOTX64.EFI
    COMMENT "Converting ELF binary to PE32+ UEFI application..."
)

add_custom_target(run
    COMMAND qemu-system-x86_64
        -drive if=pflash,format=raw,readonly=on,file=/usr/share/edk2/x64/OVMF_CODE.4m.fd
        -drive format=raw,file=fat:rw:${CMAKE_BINARY_DIR}/mnt
        -serial stdio
        -net none
    DEPENDS kernel
    COMMENT "Starting QEMU..."
    USES_TERMINAL
)
