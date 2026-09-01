cmake_minimum_required(VERSION 3.20)

set(ARCH "x86_64" CACHE STRING "Target architecture: i686 or x86_64")
set_property(CACHE ARCH PROPERTY STRINGS i686 x86_64)

if(ARCH STREQUAL "i686")
    set(CMAKE_C_COMPILER   i686-elf-gcc)
    set(CMAKE_ASM_COMPILER i686-elf-gcc)
    set(BOOT_SOURCE    src/boot32.s)
    set(LINKER_SCRIPT_NAME src/linker32.ld)
    set(QEMU_BIN       qemu-system-i386)
    set(ARCH_FLAGS     "")
elseif(ARCH STREQUAL "x86_64")
    set(CMAKE_C_COMPILER   x86_64-elf-gcc)
    set(CMAKE_ASM_COMPILER x86_64-elf-gcc)
    set(BOOT_SOURCE    src/boot64.s)
    set(LINKER_SCRIPT_NAME src/linker64.ld)
    set(QEMU_BIN       qemu-system-x86_64)
    set(ARCH_FLAGS     "-mno-red-zone -mgeneral-regs-only -fno-stack-protector")
else()
    message(FATAL_ERROR "ARCH must be 'i686' or 'x86_64' (got '${ARCH}')")
endif()

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
project(os C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
# set(COMMON_FLAGS "-ffreestanding -O2 -Wall -Wextra -Wpedantic ${ARCH_FLAGS}")
set(COMMON_FLAGS "-ffreestanding -O0 -g -Wall -Wextra -Wpedantic ${ARCH_FLAGS}")
set(CMAKE_C_FLAGS   "${COMMON_FLAGS}")
set(CMAKE_ASM_FLAGS "${COMMON_FLAGS}")
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/${LINKER_SCRIPT_NAME})

set(CMAKE_EXE_LINKER_FLAGS
    # "-ffreestanding -O2 -nostdlib -lgcc -T ${LINKER_SCRIPT}"
    "-ffreestanding -O0 -g -nostdlib -lgcc -T ${LINKER_SCRIPT}"
)

add_executable(kernel.elf
    ${BOOT_SOURCE}
    src/kernel.c
    src/isr.s
)

set_target_properties(kernel.elf PROPERTIES
    LINK_DEPENDS ${LINKER_SCRIPT}
)

add_custom_target(run
    COMMAND ${QEMU_BIN} -kernel $<TARGET_FILE:kernel.elf>
    DEPENDS kernel.elf
    USES_TERMINAL
)

find_program(GRUB_MKRESCUE grub-mkrescue)
if(GRUB_MKRESCUE)
    set(ISO_DIR  ${CMAKE_BINARY_DIR}/isodir)
    set(ISO_FILE ${CMAKE_BINARY_DIR}/os.iso)
    if(ARCH STREQUAL "x86_64")
        set(GRUB_MB_CMD "multiboot2")
    else()
        set(GRUB_MB_CMD "multiboot")
    endif()
    file(MAKE_DIRECTORY ${ISO_DIR}/boot/grub)
    file(WRITE ${ISO_DIR}/boot/grub/grub.cfg
"menuentry \"os\" {\n    ${GRUB_MB_CMD} /boot/kernel.elf\n}\n"
    )
    add_custom_command(
        OUTPUT ${ISO_FILE}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:kernel.elf> ${ISO_DIR}/boot/kernel.elf
        COMMAND ${GRUB_MKRESCUE} -o ${ISO_FILE} ${ISO_DIR}
        DEPENDS kernel.elf
        VERBATIM
    )
    add_custom_target(iso DEPENDS ${ISO_FILE})
    add_custom_target(run-iso
        COMMAND ${QEMU_BIN} -cdrom ${ISO_FILE}
        DEPENDS iso
        USES_TERMINAL
    )
else()
    message(WARNING "grub-mkrescue not found — 'iso'/'run-iso' targets unavailable. Install grub, libisoburn, and mtools.")
endif()

add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E rm -rf
        ${CMAKE_SOURCE_DIR}/build/*
        ${CMAKE_SOURCE_DIR}/build_x86/*
        ${CMAKE_SOURCE_DIR}/build_i686/*
        ${CMAKE_SOURCE_DIR}/build-i686/*
        ${CMAKE_SOURCE_DIR}/build-x64/*
    COMMENT "Removing build, build_x86, build_i686, build-i686, build-x64"
)
