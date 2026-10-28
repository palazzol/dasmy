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
    src/cpu_6800.c
    src/tables/m6800_tab.c
)
