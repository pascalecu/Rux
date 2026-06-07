set(RUX_ARCH_RAW "${CMAKE_SYSTEM_PROCESSOR}")

# Solaris sometimes reports i86pc for x86-64
if (RUX_FAMILY_SUNOS AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(RUX_ARCH_RAW "x86_64")
endif ()

string(TOLOWER "${RUX_ARCH_RAW}" RUX_ARCH_RAW)

set(RUX_ARCH_NAME "unknown")
set(RUX_ARCH_FAMILY_NAME "unknown")

macro(_rux_match_arch regex arch family)
    if (RUX_ARCH_NAME STREQUAL "unknown" AND RUX_ARCH_RAW MATCHES "${regex}")
        set(RUX_ARCH_NAME "${arch}")
        set(RUX_ARCH_FAMILY_NAME "${family}")
    endif ()
endmacro()

# x86
_rux_match_arch("^(x86_64|amd64|x64|x86_64h)$" x86_64 x86)
_rux_match_arch("^(i[3-6]86|x86|i86pc)$" x86 x86)

# ARM
_rux_match_arch("^(aarch64|arm64)$" aarch64 arm)
_rux_match_arch("^arm.*$" arm arm)

# RISC-V
_rux_match_arch("^riscv64.*$" riscv64 riscv)
_rux_match_arch("^riscv32.*$" riscv32 riscv)

unset(_rux_match_arch)

foreach (_family
        X86
        ARM
        RISCV)
    set(RUX_ARCH_FAMILY_${_family} OFF)
endforeach ()

if (NOT RUX_ARCH_FAMILY_NAME STREQUAL "unknown")
    string(TOUPPER "${RUX_ARCH_FAMILY_NAME}" _family_upper)
    set(RUX_ARCH_FAMILY_${_family_upper} ON)
endif ()

foreach (_arch
        X86
        X86_64
        ARM
        AARCH64
        RISCV32
        RISCV64)
    set(RUX_ARCH_${_arch} OFF)
endforeach ()

if (NOT RUX_ARCH_NAME STREQUAL "unknown")
    string(TOUPPER "${RUX_ARCH_NAME}" _arch_upper)
    set(RUX_ARCH_${_arch_upper} ON)
endif ()

math(EXPR RUX_ARCH_BITS "${CMAKE_SIZEOF_VOID_P} * 8")

set(RUX_ARCH_32BIT OFF)
set(RUX_ARCH_64BIT OFF)

if (RUX_ARCH_BITS EQUAL 64)
    set(RUX_ARCH_64BIT ON)
elseif (RUX_ARCH_BITS EQUAL 32)
    set(RUX_ARCH_32BIT ON)
endif ()

set(RUX_ARCH_ENDIAN "unknown")

if (DEFINED CMAKE_CXX_BYTE_ORDER)
    if (CMAKE_CXX_BYTE_ORDER STREQUAL "LITTLE_ENDIAN")
        set(RUX_ARCH_ENDIAN "little")
    elseif (CMAKE_CXX_BYTE_ORDER STREQUAL "BIG_ENDIAN")
        set(RUX_ARCH_ENDIAN "big")
    endif ()
endif ()

set(RUX_ARCH_LITTLE_ENDIAN OFF)
set(RUX_ARCH_BIG_ENDIAN OFF)

if (RUX_ARCH_ENDIAN STREQUAL "little")
    set(RUX_ARCH_LITTLE_ENDIAN ON)
elseif (RUX_ARCH_ENDIAN STREQUAL "big")
    set(RUX_ARCH_BIG_ENDIAN ON)
endif ()

message(STATUS "Architecture:")
message(STATUS "\tRaw         : ${RUX_ARCH_RAW}")
message(STATUS "\tArchitecture: ${RUX_ARCH_NAME}")
message(STATUS "\tFamily      : ${RUX_ARCH_FAMILY_NAME}")
message(STATUS "\tBitness     : ${RUX_ARCH_BITS}")
message(STATUS "\tEndianness  : ${RUX_ARCH_ENDIAN}")