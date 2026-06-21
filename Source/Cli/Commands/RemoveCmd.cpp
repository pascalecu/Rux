// Copyright (c) Rux contributors.
// SPDX-License-Identifier: MIT

#include "Rux/Cli/Cli.h"
#include "Rux/Cli/CliInternals.h"

#include <print>

namespace Rux {
int Cli::RunRemove(std::span<std::string_view const> args, GlobalOptions const &opts) {
    std::string_view name;

    for (auto const &arg : args) {
        if (arg == "-h" or arg == "--help") {
            PrintHelpFor("remove");
            return 0;
        }
        if (!arg.starts_with('-') and name.empty()) {
            name = arg;
        }
        else {
            PrintUnknownOption(arg, "remove");
            return 1;
        }
    }

    if (name.empty()) {
        std::print(stderr, "error: missing package name\n\n");
        PrintHelpFor("remove");
        return 1;
    }

    auto const manifestPath = Misc::RequireManifest();
    if (!manifestPath) {
        return 1;
    }

    auto manifest = Misc::LoadManifest(*manifestPath);
    if (!manifest) {
        return 1;
    }

    std::string const pkgName{name};
    if (!manifest->RemoveDependency(pkgName)) {
        std::print(stderr, "error: package '{}' is not a dependency\n", pkgName);
        return 1;
    }

    if (!manifest->Save(*manifestPath)) {
        std::print(stderr, "error: failed to write '{}'\n", manifestPath->string());
        return 1;
    }

    if (!opts.quiet) {
        std::print("     Removed {}\n", pkgName);
    }
    return 0;
}
} // namespace Rux
