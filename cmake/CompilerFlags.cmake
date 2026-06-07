add_library(rux_compiler_flags INTERFACE)

if (MSVC OR RUX_COMPILER_CLANG_CL)
    target_compile_options(rux_compiler_flags INTERFACE
            /W4 /WX /permissive- /Zc:__cplusplus /std:c++latest
            /Zc:inline # Exclude unreferenced functions/data from object files
            /volatile:iso # Strict volatile semantics
            /guard:cf # Enable Control Flow Guard for security hardening

            # Disable warnings from all headers included with angle brackets, like <utility>
            /external:anglebrackets
            /external:W0
    )

    # MSVC-only warning flags
    target_compile_options(rux_compiler_flags INTERFACE $<$<NOT:$<CXX_COMPILER_ID:Clang>>:
            /w14242 # Conversion from 'type1' to 'type2', possible loss of data
            /w14254 # 'operator': conversion from 'type1:bitfield' to 'type2:bitfield'
            /w14263 # Member function does not override any base class virtual member function
            /w14265 # Class has virtual functions, but destructor is not virtual
            /w14287 # Unsigned/negative constant mismatch
            /we4289 # nonstandard extension used: 'variable': loop control variable declared in the for-loop is used outside the for-loop scope
            /w14296 # Expression is always true/false
            /w14311 # Pointer truncation
            /w14545 # Expression before comma evaluates to a function missing argument list
            /w14546 # Function call before comma missing argument list
            /w14547 # 'operator': operator before comma has no effect
            /w14549 # 'operator': operator before comma has no effect; did you intend 'operator'?
            /w14555 # Expression has no effect
            /w14619 # Pragma warning: there is no warning number
            /w14640 # Enable warning on thread-unsafe local static initialization
            /w14826 # Conversion from 'type1' to 'type2' is sign-extended
            /w14905 # Wide string literal cast to 'LPSTR'
            /w14906 # String literal cast to 'LPWSTR'
            /w14928 # Illegal copy-initialization; multiple user-defined conversions applied
    >)

    target_compile_options(rux_compiler_flags INTERFACE
            $<$<CXX_COMPILER_ID:Clang>:-fcolor-diagnostics>
    )

elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(rux_compiler_flags INTERFACE
            -Wall # Reasonable and standard baseline warnings
            -Wextra # Extra useful verification warnings
            -Wpedantic # Warn if non-standard C++ extensions are used
            -Werror # Treat all warnings as errors
            -Wshadow # Warn if a local variable declaration shadows one from a parent context
            -Wnon-virtual-dtor # Warn if a class with virtual functions has a non-virtual destructor
            -Wold-style-cast # Warn for old C-style casts; forces static_cast/reinterpret_cast
            -Wcast-align # Warn for potential performance problem casts (alignment increases)
            -Wunused # Catch unused variables, functions, and parameters
            -Woverloaded-virtual # Warn if you overload (not override) a virtual function
            -Wconversion # Warn on implicit type conversions that may lose data
            -Wsign-conversion # Warn on implicit sign conversions
            -Wnull-dereference # Warn if a compile-time null dereference is detected
            -Wdouble-promotion # Warn if float is implicitly promoted to double
            -Wformat=2 # Warn on security issues around functions that format output (e.g. printf)
            -Wmisleading-indentation # Warn if indentation implies blocks where blocks do not exist
            -Wimplicit-fallthrough # Warns when case statements fall-through without explicit annotation
            -fstack-protector-strong # Emit extra stack-smashing protection code for arrays/buffers
    )
    # Flags exclusively supported by GCC
    target_compile_options(rux_compiler_flags INTERFACE $<$<CXX_COMPILER_ID:GNU>:
            -Wduplicated-cond # Warn if an if/else chain has duplicated conditions
            -Wduplicated-branches # Warn if if/else branches have duplicated code
            -Wlogical-op # Warn about logical operations used where bitwise were probably wanted
            -Wuseless-cast # Warn if you perform a cast to the exact same type
            >)

    target_compile_options(rux_compiler_flags INTERFACE
            $<$<CXX_COMPILER_ID:GNU>:-fdiagnostics-color=always>
            $<$<CXX_COMPILER_ID:Clang>:-fcolor-diagnostics>
    )

    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND UNIX AND NOT APPLE)
        target_compile_options(rux_compiler_flags INTERFACE -stdlib=libc++)
        target_link_options(rux_compiler_flags INTERFACE -stdlib=libc++)
    endif ()

    if (MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_libraries(rux_compiler_flags INTERFACE stdc++exp)
    endif ()

    target_compile_definitions(rux_compiler_flags INTERFACE $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>:
            # GCC libstdc++ assertions (active for Debug and RelWithDebInfo)
            $<$<CONFIG:Debug,RelWithDebInfo>:_GLIBCXX_ASSERTIONS>
            # Clang libc++ hardening modes mapped to build configs
            $<$<CONFIG:Debug>:_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG>
            $<$<CONFIG:RelWithDebInfo>:_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE>
            $<$<CONFIG:Release,MinSizeRel>:_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST>>
    )
endif ()
