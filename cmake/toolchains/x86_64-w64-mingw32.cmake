# Cross-compile for Windows x64 from Linux with llvm-mingw
# (Arch: llvm-mingw, installed in /opt/llvm-mingw). The tests run under wine.
#   cmake -S . -B build-winx64 -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/x86_64-w64-mingw32.cmake \
#         -DCTR_NATIVE_64BIT=ON -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
set(LLVM_MINGW_ROOT /opt/llvm-mingw CACHE PATH "llvm-mingw install prefix")
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)
set(CMAKE_C_COMPILER ${LLVM_MINGW_ROOT}/bin/x86_64-w64-mingw32-clang)
set(CMAKE_CXX_COMPILER ${LLVM_MINGW_ROOT}/bin/x86_64-w64-mingw32-clang++)
set(CMAKE_RC_COMPILER ${LLVM_MINGW_ROOT}/bin/x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH ${LLVM_MINGW_ROOT}/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
