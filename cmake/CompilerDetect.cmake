# Compiler flags initialization
set(_RUX_ALL_COMPILERS GCC CLANG APPLECLANG CLANGCL MSVC INTEL INTELLLVM)

foreach (_comp IN LISTS _RUX_ALL_COMPILERS)
    set(RUX_COMPILER_${_comp} OFF)
endforeach ()

set(_id "${CMAKE_CXX_COMPILER_ID}")
set(_variant "${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}")

message(STATUS "Compiler ID: ${_id}")
if (_variant)
    message(STATUS "Frontend variant: ${_variant}")
endif ()

# Detection table
set(_compiler_table
        GNU GCC
        Clang CLANG
        AppleClang APPLECLANG
        MSVC MSVC
        Intel INTEL
        IntelLLVM INTELLLVM
)

set(_found OFF)

# Special case: clang-cl
if (_id STREQUAL "Clang" AND _variant STREQUAL "MSVC")
    set(RUX_COMPILER_CLANGCL ON)
    set(_found ON)
    message(STATUS "Detected compiler: CLANGCL")
else ()
    list(LENGTH _compiler_table _len)
    math(EXPR _pairs "${_len} / 2")
    math(EXPR _last "${_pairs} - 1")

    foreach (idx RANGE 0 ${_last})
        math(EXPR _i "${idx} * 2")
        list(GET _compiler_table ${_i} _cmake_id)
        math(EXPR _j "${_i} + 1")
        list(GET _compiler_table ${_j} _rux_name)

        if (_id STREQUAL _cmake_id)
            set(RUX_COMPILER_${_rux_name} ON)
            set(_found ON)
            message(STATUS "Detected compiler: ${_rux_name}")
            break()
        endif ()
    endforeach ()
endif ()

# Fallback handling
if (NOT _found)
    message(WARNING "Unknown compiler ID '${_id}'. Using default configuration.")
endif ()

# Active compiler report
foreach (_comp IN LISTS _RUX_ALL_COMPILERS)
    if (RUX_COMPILER_${_comp})
        message(STATUS "Active compiler: ${_comp}")
    endif ()
endforeach ()

# Version handling
set(_raw_version "${CMAKE_CXX_COMPILER_VERSION}")

if (NOT _raw_version)
    set(_raw_version "0.0.0")
    message(STATUS "Compiler version not detected, defaulting to 0.0.0")
else ()
    message(STATUS "Raw compiler version: ${_raw_version}")
endif ()

rux_version_parse("${_raw_version}" RUX_COMPILER_VERSION)
message(STATUS "Packed version: ${RUX_COMPILER_VERSION}")