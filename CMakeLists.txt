cmake_minimum_required(VERSION 3.31)
project(Rux VERSION 0.3.0 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(ProjectOptions)
include(Version)
include(Language)
include(PlatformDetect)
include(CompilerDetect)
include(Architecture)
include(CompilerFlags)

string(TIMESTAMP RUX_BUILD_TIMESTAMP "%Y-%m-%dT%H:%M:%SZ" UTC)

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/Include/Rux/Version.h.in
        ${CMAKE_CURRENT_BINARY_DIR}/generated/Version.h
        @ONLY
)

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/Include/Rux/PlatformConfig.h.in
        ${CMAKE_CURRENT_BINARY_DIR}/generated/PlatformConfig.h
        @ONLY
)

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/Include/Rux/CompilerConfig.h.in
        ${CMAKE_CURRENT_BINARY_DIR}/generated/CompilerConfig.h
        @ONLY
)

add_executable(rux
        Source/Main.cpp
        Source/Platform.cpp
        Source/Cli.cpp
        Source/Manifest.cpp
        Source/Package.cpp
        Source/Token.cpp
        Source/Lexer.cpp
        Source/SourceLoader.cpp
        Source/Parser.cpp
        Source/Sema.cpp
        Source/Hir.cpp
        Source/Lir.cpp
        Source/Asm.cpp
        Source/Rcu.cpp
        Source/Linker.cpp
)

target_link_libraries(rux PRIVATE rux_compiler_flags)

target_include_directories(rux PRIVATE
        Include
        ${CMAKE_CURRENT_BINARY_DIR}/generated
)

if (WIN32)
    target_link_libraries(rux PRIVATE Psapi Winhttp)
endif ()

target_compile_definitions(rux PRIVATE
        $<$<CONFIG:Debug>:RUX_BUILD_DEBUG=1>
        $<$<NOT:$<CONFIG:Debug>>:RUX_BUILD_DEBUG=0>

        $<$<CONFIG:Debug>:RUX_BUILD_RELEASE=0>
        $<$<NOT:$<CONFIG:Debug>>:RUX_BUILD_RELEASE=1>

        RUX_CXX_AT_LEAST_98=${RUX_CXX_AT_LEAST_98}
        RUX_CXX_AT_LEAST_11=${RUX_CXX_AT_LEAST_11}
        RUX_CXX_AT_LEAST_14=${RUX_CXX_AT_LEAST_14}
        RUX_CXX_AT_LEAST_17=${RUX_CXX_AT_LEAST_17}
        RUX_CXX_AT_LEAST_20=${RUX_CXX_AT_LEAST_20}
        RUX_CXX_AT_LEAST_23=${RUX_CXX_AT_LEAST_23}
        RUX_CXX_AT_LEAST_26=${RUX_CXX_AT_LEAST_26}
)

set_property(TARGET rux
        PROPERTY MSVC_RUNTIME_LIBRARY "$<IF:$<CONFIG:Release>,MultiThreaded,MultiThreadedDebugDLL>"
)

include(GNUInstallDirs)
install(TARGETS rux RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
