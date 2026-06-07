# Helper: Detect max compiler capability
function(rux_detect_max_cxx_capability out_var)
    set(_standards_to_check 26 23 20 17 14 11 98)
    list(LENGTH _standards_to_check _check_len)
    set(_idx 0)
    set(_result 0)

    while (_idx LESS _check_len)
        list(GET _standards_to_check ${_idx} _std)
        if ("cxx_std_${_std}" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
            set(_result ${_std})
            break()
        endif ()
        math(EXPR _idx "${_idx} + 1")
    endwhile ()

    set(${out_var} ${_result} PARENT_SCOPE)
endfunction()

# Helper: Resolve final standard
function(rux_resolve_cxx_standard max_cap preferred min out_var)
    set(_result ${max_cap})

    if (preferred LESS_EQUAL max_cap)
        set(_result ${preferred})
    endif ()

    if (_result LESS min)
        message(FATAL_ERROR
                "Unsupported compiler: requires C++${min}, "
                "but only C++${max_cap} is available")
    endif ()

    set(${out_var} ${_result} PARENT_SCOPE)
endfunction()

# Helper: Build database and extract current version data
function(rux_build_std_database selected_std out_version out_macro out_avail)
    set(_version 0)
    set(_macro_val "")
    set(_avail 0)

    list(LENGTH RUX_CXX_STANDARDS _db_len)
    set(_i 0)

    while (_i LESS _db_len)
        list(GET RUX_CXX_STANDARDS ${_i} _std)
        math(EXPR _j "${_i} + 1")
        list(GET RUX_CXX_STANDARDS ${_j} _macro)

        # Export macro definition to parent scope
        set(RUX_CXX_STD_${_std}_MACRO ${_macro} PARENT_SCOPE)

        math(EXPR _year "${_macro} / 100")
        math(EXPR _month "${_macro} % 100")

        if (_month EQUAL 0)
            set(_month 1)
        endif ()

        # Generate date and export to parent scope for availability checks
        rux_make_date(${_year} ${_month} 1 _temp_date)
        set(RUX_CXX_STD_${_std}_VERSION ${_temp_date} PARENT_SCOPE)

        if (_std EQUAL selected_std)
            set(_version ${_temp_date})
            set(_macro_val ${_macro})
            set(_avail 1)
        endif ()

        math(EXPR _i "${_i} + 2")
    endwhile ()

    # Export active standard properties
    set(${out_version} ${_version} PARENT_SCOPE)
    set(${out_macro} ${_macro_val} PARENT_SCOPE)
    set(${out_avail} ${_avail} PARENT_SCOPE)
endfunction()

# Helper: Print and set feature availability
function(rux_check_feature_availability current_version)
    list(LENGTH RUX_CXX_KNOWN_STANDARDS _known_len)
    set(_k 0)

    while (_k LESS _known_len)
        list(GET RUX_CXX_KNOWN_STANDARDS ${_k} _std)

        if (current_version GREATER_EQUAL RUX_CXX_STD_${_std}_VERSION)
            set(RUX_CXX_AT_LEAST_${_std} 1 PARENT_SCOPE)
            message(STATUS "\tC++${_std}: YES")
        else ()
            set(RUX_CXX_AT_LEAST_${_std} 0 PARENT_SCOPE)
            message(STATUS "\tC++${_std}: NO")
        endif ()

        math(EXPR _k "${_k} + 1")
    endwhile ()
endfunction()

set(RUX_CXX 0)
set(RUX_CXX_AVAILABLE 0)

set(RUX_PREFERRED_STD 26 CACHE STRING "Preferred C++ standard")
set(RUX_MIN_STD 20 CACHE STRING "Minimum supported C++ standard")

# Standard database (std -> __cplusplus)
set(RUX_CXX_STANDARDS
        98 199711
        11 201103
        14 201402
        17 201703
        20 202002
        23 202302
        26 202400
)

set(RUX_CXX_KNOWN_STANDARDS 98 11 14 17 20 23 26)

# Detect compiler capability
rux_detect_max_cxx_capability(RUX_MAX_CAPABILITY)

if (RUX_MAX_CAPABILITY EQUAL 0)
    message(FATAL_ERROR "Failed to detect compiler C++ capability")
endif ()

message(STATUS "Compiler supports up to C++${RUX_MAX_CAPABILITY}")

# Resolve final standard
rux_resolve_cxx_standard(
        ${RUX_MAX_CAPABILITY}
        ${RUX_PREFERRED_STD}
        ${RUX_MIN_STD}
        RUX_CXX_STANDARD
)

message(STATUS "Standard selection:")
message(STATUS "\tPreferred: C++${RUX_PREFERRED_STD}")
message(STATUS "\tMinimum  : C++${RUX_MIN_STD}")
message(STATUS "\tSelected : C++${RUX_CXX_STANDARD}")

set(CMAKE_CXX_STANDARD ${RUX_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build standard database
message(STATUS "Building C++ standard database...")
rux_build_std_database(
        ${RUX_CXX_STANDARD}
        RUX_CXX
        RUX_CXX_MACRO
        RUX_CXX_AVAILABLE
)

if (NOT RUX_CXX_AVAILABLE)
    message(FATAL_ERROR "Failed to resolve active C++ standard")
endif ()

# Feature availability
message(STATUS "C++ standard is at least:")
rux_check_feature_availability(${RUX_CXX})

# Exported values
set(RUX_CXX_VERSION ${RUX_CXX})

message(STATUS "Final summary:")
message(STATUS "\tStandard   : C++${RUX_CXX_STANDARD}")
message(STATUS "\t__cplusplus: ${RUX_CXX_MACRO}")
message(STATUS "\tVersion    : ${RUX_CXX}")

return()