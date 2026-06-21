// Copyright (c) Rux contributors.
// SPDX-License-Identifier: MIT

#include "Rux/Cli/Cli.h"
#include "Rux/Package.h"

#include <print>

namespace Rux {
int Cli::RunNew(std::span<std::string_view const> const args, GlobalOptions const &opts) {
    std::string_view name;
    bool bin = false;
    bool lib = false;
    std::string_view customPath;

    for (std::size_t i = 0; i < args.size(); ++i) {
        std::string_view arg = args[i];
        if (arg == "--bin") {
            bin = true;
            continue;
        }
        if (arg == "--lib") {
            lib = true;
            continue;
        }
        if (arg == "--path" and i + 1 < args.size()) {
            customPath = args[++i];
            continue;
        }
        if (arg == "-h" or arg == "--help") {
            PrintHelpFor("new");
            return 0;
        }
        if (!arg.starts_with('-') and name.empty()) {
            name = arg;
            continue;
        }
        PrintUnknownOption(arg, "new");
        return 1;
    }

    if (name.empty()) {
        std::print(stderr, "error: missing package name\n\n");
        PrintHelpFor("new");
        return 1;
    }

    auto const type = (lib and !bin) ? PackageType::SharedLibrary : PackageType::Executable;
    auto const root = customPath.empty() ? std::filesystem::current_path() / name
                                         : std::filesystem::path(customPath) / name;

    if (!opts.quiet) {
        std::print("Creating {} package '{}'\n",
                   type == PackageType::Executable ? "binary" : "library", name);
    }

    if (!ScaffoldPackage(root, std::string(name), type, /*initMode=*/false)) {
        return 1;
    }

    if (!opts.quiet) {
        std::print("Created package '{}' at {}\n", name, root.string());
    }
    return 0;
}
} // namespace Rux
