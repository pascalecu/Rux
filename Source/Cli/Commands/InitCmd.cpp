// Copyright (c) Rux contributors.
// SPDX-License-Identifier: MIT

#include "Rux/Cli/Cli.h" // for GlobalOptions, Cli
#include "Rux/Package.h" // for PackageType, ScaffoldPackage

#include <filesystem>  // for current_path, path
#include <print>       // for print
#include <span>        // for span
#include <string>      // for basic_string, char_traits
#include <string_view> // for basic_string_view, operator==, string_view

namespace Rux {
int Cli::RunInit(std::span<std::string_view const> args, GlobalOptions const &opts) {
    bool bin = false, lib = false;
    for (auto const &arg : args) {
        if (arg == "--bin") {
            bin = true;
        }
        else if (arg == "--lib") {
            lib = true;
        }
        else if (arg == "-h" or arg == "--help") {
            PrintHelpFor("init");
            return 0;
        }
        else {
            PrintUnknownOption(arg, "init");
            return 1;
        }
    }

    auto const type = (lib and !bin) ? PackageType::SharedLibrary : PackageType::Executable;
    auto const name = std::filesystem::current_path().filename().string();

    if (!opts.quiet) {
        std::print("  Initializing {} package '{}'\n",
                   type == PackageType::Executable ? "binary" : "library", name);
    }

    if (!ScaffoldPackage(std::filesystem::current_path(), name, type, true)) {
        return 1;
    }

    if (!opts.quiet) {
        std::print("    Initialized package '{}'\n", name);
    }
    return 0;
}
} // namespace Rux
