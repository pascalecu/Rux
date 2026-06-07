set(_RUX_ALL_OS WINDOWS MACOS LINUX FREEBSD OPENBSD NETBSD DRAGONFLYBSD SOLARIS ILLUMOS)
set(_RUX_ALL_FAM WINDOWS UNIX BSD SUNOS POSIX)
set(_RUX_ALL_OBJ ELF MACHO PE)

foreach (_os IN LISTS _RUX_ALL_OS)
    set(RUX_OS_${_os} OFF)
endforeach ()

foreach (_fam IN LISTS _RUX_ALL_FAM)
    set(RUX_FAMILY_${_fam} OFF)
endforeach ()

foreach (_obj IN LISTS _RUX_ALL_OBJ)
    set(RUX_OBJECT_${_obj} OFF)
endforeach ()

set(_sys "${CMAKE_SYSTEM_NAME}")

if (WIN32)
    set(RUX_OS_WINDOWS ON)
    set(RUX_FAMILY_WINDOWS ON)
    set(RUX_OBJECT_PE ON)
else ()
    set(RUX_FAMILY_UNIX ON)
    set(RUX_FAMILY_POSIX ON)

    if (APPLE)
        set(RUX_OS_MACOS ON)
        set(RUX_OBJECT_MACHO ON)
    else ()
        set(RUX_OBJECT_ELF ON)

        if (_sys STREQUAL "Linux")
            set(RUX_OS_LINUX ON)

        elseif (_sys MATCHES "^(Free|Open|Net)BSD$")
            string(TOUPPER "${CMAKE_MATCH_1}" _bsd_prefix)
            set(RUX_OS_${_bsd_prefix}BSD ON)
            set(RUX_FAMILY_BSD ON)

        elseif (_sys STREQUAL "DragonFly")
            set(RUX_OS_DRAGONFLYBSD ON)
            set(RUX_FAMILY_BSD ON)

        elseif (_sys STREQUAL "SunOS")
            set(RUX_FAMILY_SUNOS ON)
            set(RUX_OS_ILLUMOS ON)

            if (NOT CMAKE_CROSSCOMPILING AND EXISTS "/etc/release")
                file(READ "/etc/release" _release LIMIT 256)
                string(TOLOWER "${_release}" _release_lower)

                if (_release_lower MATCHES "oracle|solaris")
                    set(RUX_OS_SOLARIS ON)
                    set(RUX_OS_ILLUMOS OFF)
                endif ()
            endif ()
        endif ()
    endif ()
endif ()