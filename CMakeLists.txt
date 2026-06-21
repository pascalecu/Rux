cmake_minimum_required(VERSION 3.31...4.3)
project(Rux
        VERSION 0.3.1
        DESCRIPTION "The Rux Compiler"
        LANGUAGES CXX)

include(CTest)
include(GNUInstallDirs)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND UNIX AND NOT APPLE)
    add_compile_options(-stdlib=libc++)
    add_link_options(-stdlib=libc++)
endif ()

string(TIMESTAMP RUX_BUILD_DATE "%Y-%m-%d")
string(TIMESTAMP RUX_BUILD_TIME "%H:%M:%S")

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/Include/Rux/Version.h.in
        ${CMAKE_CURRENT_BINARY_DIR}/generated/Rux/Version.h
        @ONLY
)

add_library(rux_common INTERFACE)

target_include_directories(rux_common INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/Include
        ${CMAKE_CURRENT_BINARY_DIR}/generated
)

add_library(rux_frontend STATIC
        Source/Token.cpp
        Source/Lexer.cpp
        Source/Parser.cpp
        Source/Sema.cpp
)

target_link_libraries(rux_frontend
        PUBLIC rux_common
)

add_library(rux_ir STATIC
        Source/Hir.cpp
        Source/Lir.cpp
        Source/Optimizer.cpp
)

target_link_libraries(rux_ir
        PUBLIC rux_frontend
)

add_library(rux_backend STATIC
        Source/Asm.cpp
        Source/Linker.cpp
        Source/Rcu.cpp
)

target_link_libraries(rux_backend
        PUBLIC rux_ir
)

add_library(rux_tooling STATIC
        Source/Manifest.cpp
        Source/Package.cpp
        Source/SourceLoader.cpp
)

target_link_libraries(rux_tooling
        PUBLIC rux_backend
)

add_library(rux_cli STATIC
        Source/Cli/Cli.cpp
        Source/Cli/Commands/HelpCmd.cpp
        Source/Cli/Commands/Base/BuildCmd.cpp
        Source/Cli/Commands/Base/CheckCmd.cpp
        Source/Cli/Commands/Base/InstallCmd.cpp
        Source/Cli/Commands/Base/PackageCmd.cpp
        Source/Cli/Commands/Base/UtilityCmd.cpp
)

target_link_libraries(rux_cli
        PUBLIC rux_tooling
)

add_executable(rux
        Source/Main.cpp
)

target_link_libraries(rux
        PRIVATE rux_cli
)

if (WIN32)
    target_link_libraries(rux PRIVATE Psapi Winhttp)
endif ()

if (MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(rux PRIVATE stdc++exp)
endif ()

set_property(TARGET rux PROPERTY
        MSVC_RUNTIME_LIBRARY
        "$<IF:$<CONFIG:Release>,MultiThreaded,MultiThreadedDebugDLL>"
)

install(TARGETS rux RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if (BUILD_TESTING)
    add_subdirectory(Tests)
endif ()