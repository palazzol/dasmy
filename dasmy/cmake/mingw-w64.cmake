# Cross-compile DASMy for 64-bit Windows with MinGW-w64.
#
#   cmake -S dasmy -B dasmy/build-win -DCMAKE_TOOLCHAIN_FILE="$PWD/dasmy/cmake/mingw-w64.cmake"
#   cmake --build dasmy/build-win
#
# (The toolchain path must be absolute: CMake resolves a relative one against
# the build directory.)  The result needs no MinGW DLLs; it uses the Universal
# C Runtime, which is part of Windows 10 and later.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(MINGW_PREFIX x86_64-w64-mingw32)
set(CMAKE_C_COMPILER ${MINGW_PREFIX}-gcc)
set(CMAKE_RC_COMPILER ${MINGW_PREFIX}-windres)

set(CMAKE_FIND_ROOT_PATH /usr/${MINGW_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_EXE_LINKER_FLAGS_INIT "-static")
