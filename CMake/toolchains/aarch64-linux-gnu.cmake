# CMake toolchain file: cross-compile for 64-bit ARM Linux (aarch64) with the
# Debian/Ubuntu gcc-aarch64-linux-gnu package.
#
# Usage (from an empty build directory):
#   cmake .. -DCMAKE_TOOLCHAIN_FILE=../CMake/toolchains/aarch64-linux-gnu.cmake \
#            -DUSE_OPENSSL=OFF -DUSE_AWS_LC=ON -DBUILD_STATIC=ON
#
# The file is handed down to the dependency sub-builds (AWS-LC, libcurl) by
# CMake/Utilities.cmake so they target the same architecture; AWS-LC in
# particular selects its assembly set from CMAKE_SYSTEM_PROCESSOR.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# No CMAKE_FIND_ROOT_PATH restrictions on purpose: the SDK locates its
# self-built dependencies in <source>/open-source via OPENSSL_ROOT_DIR and
# PKG_CONFIG_PATH, which an ONLY-mode re-root would hide.
