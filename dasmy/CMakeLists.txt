cmake_minimum_required(VERSION 3.10)
project(dasmy C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra)
endif()

add_executable(dasmy
    src/main.c
    src/cpu.c
    src/image.c
    src/numfmt.c
    src/output.c
    src/symfile.c
    src/symtab.c
    src/thread.c
    src/cpu_1802.c
    src/cpu_2650.c
    src/cpu_6502.c
    src/cpu_6800.c
    src/cpu_6805.c
    src/cpu_6809.c
    src/cpu_8048.c
    src/cpu_8051.c
    src/cpu_8080.c
    src/cpu_pic16.c
    src/cpu_z80.c
    src/tables/m6502_tab.c
    src/tables/s2650_tab.c
    src/tables/r1802_tab.c
    src/tables/m6800_tab.c
    src/tables/m6805_tab.c
    src/tables/m6809_tab.c
    src/tables/i8048_tab.c
    src/tables/i8051_tab.c
    src/tables/i8080_tab.c
    src/tables/pic16_tab.c
    src/tables/z80_tab.c
    src/tables/lr35902_tab.c
)
