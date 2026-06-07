# Packed format:
#
#   MMmmppppp
#
#   MM    = major (0-99)
#   mm    = minor (0-99)
#   ppppp = patch (0-99999)
#
# Examples:
#
#   0.0.1       -> 1
#   1.2.3       -> 10200003
#   99.99.99999 -> 999999999

# Internal helpers
function(_rux_require_range name value min max)
    if (value LESS min OR value GREATER max)
        message(FATAL_ERROR
                "${name} (${value}) must be in range [${min}, ${max}]")
    endif ()
endfunction()

# Version packing / unpacking
function(rux_version_number major minor patch out)
    _rux_require_range("major" "${major}" 0 99)
    _rux_require_range("minor" "${minor}" 0 99)
    _rux_require_range("patch" "${patch}" 0 99999)

    math(EXPR _value
            "(${major} * 10000000) +
         (${minor} * 100000) +
         ${patch}"
    )

    set(${out} ${_value} PARENT_SCOPE)
endfunction()

function(rux_version_major value out)
    math(EXPR _major "((${value} / 10000000) % 100)")
    set(${out} ${_major} PARENT_SCOPE)
endfunction()

function(rux_version_minor value out)
    math(EXPR _minor "((${value} / 100000) % 100)")
    set(${out} ${_minor} PARENT_SCOPE)
endfunction()

function(rux_version_patch value out)
    math(EXPR _patch "(${value} % 100000)")
    set(${out} ${_patch} PARENT_SCOPE)
endfunction()

function(rux_version_unpack value major minor patch)
    rux_version_major(${value} _major)
    rux_version_minor(${value} _minor)
    rux_version_patch(${value} _patch)

    set(${major} ${_major} PARENT_SCOPE)
    set(${minor} ${_minor} PARENT_SCOPE)
    set(${patch} ${_patch} PARENT_SCOPE)
endfunction()

# Version strings
function(rux_version_string major minor patch out)
    set(${out} "${major}.${minor}.${patch}" PARENT_SCOPE)
endfunction()

function(rux_version_to_string value out)
    rux_version_unpack(${value} _major _minor _patch)

    set(${out}
            "${_major}.${_minor}.${_patch}"
            PARENT_SCOPE)
endfunction()

function(rux_version_parse version out)
    string(REPLACE "." ";" _parts "${version}")

    list(LENGTH _parts _count)

    if (NOT _count EQUAL 3)
        message(FATAL_ERROR
                "Version must have format MAJOR.MINOR.PATCH")
    endif ()

    list(GET _parts 0 _major)
    list(GET _parts 1 _minor)
    list(GET _parts 2 _patch)

    rux_version_number(
            ${_major}
            ${_minor}
            ${_patch}
            ${out}
    )
endfunction()

# Version comparisons
function(rux_version_compare lhs op rhs out)
    if (${lhs} ${op} ${rhs})
        set(${out} TRUE PARENT_SCOPE)
    else ()
        set(${out} FALSE PARENT_SCOPE)
    endif ()
endfunction()

function(rux_version_equal lhs rhs out)
    rux_version_compare(${lhs} EQUAL ${rhs} ${out})
endfunction()

function(rux_version_not_equal lhs rhs out)
    rux_version_compare(${lhs} NOT EQUAL ${rhs} ${out})
endfunction()

function(rux_version_less lhs rhs out)
    rux_version_compare(${lhs} LESS ${rhs} ${out})
endfunction()

function(rux_version_less_equal lhs rhs out)
    rux_version_compare(${lhs} LESS_EQUAL ${rhs} ${out})
endfunction()

function(rux_version_greater lhs rhs out)
    rux_version_compare(${lhs} GREATER ${rhs} ${out})
endfunction()

function(rux_version_greater_equal lhs rhs out)
    rux_version_compare(${lhs} GREATER_EQUAL ${rhs} ${out})
endfunction()

# Legacy version encodings
function(rux_pack_vrp value out)
    math(EXPR _v "((${value} >> 8) & 0xF)")
    math(EXPR _r "((${value} >> 4) & 0xF)")
    math(EXPR _p "((${value}) & 0xF)")

    rux_version_number(${_v} ${_r} ${_p} ${out})
endfunction()

function(rux_pack_vvrp value out)
    math(EXPR _v "((${value} >> 8) & 0xFF)")
    math(EXPR _r "((${value} >> 4) & 0xF)")
    math(EXPR _p "((${value}) & 0xF)")

    rux_version_number(${_v} ${_r} ${_p} ${out})
endfunction()

# Date encoding
function(rux_make_date Y M D out)
    _rux_require_range("year" "${Y}" 1970 2069) # 00-99
    _rux_require_range("month" "${M}" 1 12)
    _rux_require_range("day" "${D}" 1 31)

    math(EXPR _value "((${Y} - 1970) * 10000000) + (${M} * 100000) + ${D}")
    set(${out} ${_value} PARENT_SCOPE)
endfunction()

function(rux_make_date_auto format value out)
    if (format STREQUAL "YYYY")
        rux_make_date(${value} 1 1 ${out})
    elseif (format STREQUAL "YYYYMM")
        math(EXPR _y "(${value} / 100)")
        math(EXPR _m "(${value} % 100)")

        rux_make_date(${_y} ${_m} 1 ${out})
    elseif (format STREQUAL "YYYYMMDD")
        math(EXPR _y "(${value} / 10000)")
        math(EXPR _m "((${value} / 100) % 100)")
        math(EXPR _d "(${value} % 100)")

        rux_make_date(${_y} ${_m} ${_d} ${out})
    else ()
        message(FATAL_ERROR "Unknown date format: ${format}")
    endif ()
endfunction()

function(rux_version_parse version out)
    string(REPLACE "." ";" _parts "${version}")

    set(_major 0)
    set(_minor 0)
    set(_patch 0)

    list(LENGTH _parts _count)

    list(GET _parts 0 _major)

    if (_count GREATER 1)
        list(GET _parts 1 _minor)
    endif ()

    if (_count GREATER 2)
        list(GET _parts 2 _patch)
    endif ()

    rux_version_number(
            ${_major}
            ${_minor}
            ${_patch}
            ${out}
    )

    set(${out} ${${out}} PARENT_SCOPE)
endfunction()

# Constants
rux_version_number(0 0 0 RUX_VERSION_ZERO)
rux_version_number(0 0 1 RUX_VERSION_MIN)
rux_version_number(99 99 99999 RUX_VERSION_MAX)

set(RUX_MIN_SUPPORTED_VERSION ${RUX_VERSION_MIN})

# Maximum packed value: 99.99.99999 = 999999999
set(RUX_VERSION_MAX_VALUE 999999999)