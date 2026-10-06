# Cross-compile for ARM64 Linux from an x86-64 host (Arch: aarch64-linux-gnu-gcc,
# aarch64-linux-gnu-glibc, qemu-user, qemu-user-binfmt). Tests run under qemu.
#   cmake -S . -B build-arm64 -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake \
#         -DCTR_NATIVE_64BIT=ON -DCMAKE_BUILD_TYPE=Release
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
# Not CMAKE_SYSROOT: the cross gcc already finds its own libc, and Ubuntu's
# libc.so linker scripts use absolute paths that a --sysroot would prefix.
set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_CROSSCOMPILING_EMULATOR qemu-aarch64 -L /usr/aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
