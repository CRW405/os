cmake_minimum_required(VERSION 3.20)

set(CMAKE_C_COMPILER   i686-elf-gcc)
set(CMAKE_ASM_COMPILER i686-elf-gcc)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(os C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(COMMON_FLAGS "-ffreestanding -O2 -Wall -Wextra -Wpedantic")
set(CMAKE_C_FLAGS   "${COMMON_FLAGS}")
set(CMAKE_ASM_FLAGS "${COMMON_FLAGS}")

set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/src/linker.ld)

set(CMAKE_EXE_LINKER_FLAGS
    "-ffreestanding -O2 -nostdlib -lgcc -T ${LINKER_SCRIPT}"
)

add_executable(kernel.elf
    src/boot.s
    src/kernel.c
)

set_target_properties(kernel.elf PROPERTIES
    LINK_DEPENDS ${LINKER_SCRIPT}
)

add_custom_target(run
    COMMAND qemu-system-i386 -kernel $<TARGET_FILE:kernel.elf>
    DEPENDS kernel.elf
    USES_TERMINAL
)
