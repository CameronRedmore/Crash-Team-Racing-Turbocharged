# Cross-compile for Windows on ARM64 from Linux with llvm-mingw
# (Arch: llvm-mingw, installed in /opt/llvm-mingw). Tests run under wine only if
# it can run ARM64 Windows binaries; otherwise run them on the device.
#   cmake -S . -B build-winarm64 -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-w64-mingw32.cmake \
#         -DCTR_NATIVE_64BIT=ON -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
set(LLVM_MINGW_ROOT /opt/llvm-mingw CACHE PATH "llvm-mingw install prefix")
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR ARM64)
set(CMAKE_C_COMPILER ${LLVM_MINGW_ROOT}/bin/aarch64-w64-mingw32-clang)
set(CMAKE_CXX_COMPILER ${LLVM_MINGW_ROOT}/bin/aarch64-w64-mingw32-clang++)
set(CMAKE_RC_COMPILER ${LLVM_MINGW_ROOT}/bin/aarch64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH ${LLVM_MINGW_ROOT}/aarch64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
