# Toolchain file for STM32MP15 OpenSTLinux Weston SDK (scarthgap)
# Usage:
#   source /opt/st/stm32mp1/*/environment-setup-cortexa7t2hf-neon-vfpv4-ostl-linux-gnueabi
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=toolchain-stm32mp1.cmake

if(NOT DEFINED ENV{SDKTARGETSYSROOT})
    message(FATAL_ERROR "SDK environment not sourced. Run: source /opt/st/stm32mp1/*/environment-setup-*")
endif()

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# The Yocto SDK embeds arch flags inside $CC/$CXX variables.
# Extract the compiler executable (first word) and the flags (rest).
separate_arguments(CC_PARTS UNIX_COMMAND "$ENV{CC}")
list(GET CC_PARTS 0 CMAKE_C_COMPILER)
list(SUBLIST CC_PARTS 1 -1 CC_ARCH_FLAGS)
string(REPLACE ";" " " CC_ARCH_FLAGS "${CC_ARCH_FLAGS}")

separate_arguments(CXX_PARTS UNIX_COMMAND "$ENV{CXX}")
list(GET CXX_PARTS 0 CMAKE_CXX_COMPILER)
list(SUBLIST CXX_PARTS 1 -1 CXX_ARCH_FLAGS)
string(REPLACE ";" " " CXX_ARCH_FLAGS "${CXX_ARCH_FLAGS}")

# Binutils
set(CMAKE_AR arm-ostl-linux-gnueabi-ar CACHE FILEPATH "")
set(CMAKE_NM arm-ostl-linux-gnueabi-nm CACHE FILEPATH "")
set(CMAKE_OBJCOPY arm-ostl-linux-gnueabi-objcopy CACHE FILEPATH "")
set(CMAKE_OBJDUMP arm-ostl-linux-gnueabi-objdump CACHE FILEPATH "")
set(CMAKE_RANLIB arm-ostl-linux-gnueabi-ranlib CACHE FILEPATH "")
set(CMAKE_STRIP arm-ostl-linux-gnueabi-strip CACHE FILEPATH "")

# Sysroot
set(CMAKE_SYSROOT "$ENV{SDKTARGETSYSROOT}")
set(CMAKE_FIND_ROOT_PATH "$ENV{SDKTARGETSYSROOT}")

# Only search sysroot for libs/includes/packages, never for programs
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Compiler flags: combine arch flags from CC/CXX + SDK's CFLAGS/CXXFLAGS
set(CMAKE_C_FLAGS   "${CC_ARCH_FLAGS} $ENV{CFLAGS}"   CACHE STRING "")
set(CMAKE_CXX_FLAGS "${CXX_ARCH_FLAGS} $ENV{CXXFLAGS}" CACHE STRING "")
set(CMAKE_EXE_LINKER_FLAGS "$ENV{LDFLAGS}" CACHE STRING "")

# pkg-config from native SDK sysroot
set(PKG_CONFIG_EXECUTABLE "$ENV{OECORE_NATIVE_SYSROOT}/usr/bin/pkg-config" CACHE FILEPATH "")
