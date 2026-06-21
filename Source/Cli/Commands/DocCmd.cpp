// Copyright (c) Rux contributors.
// SPDX-License-Identifier: MIT

#include "Rux/Cli/Cli.h"
#include "Rux/Cli/CliInternals.h"

namespace Rux {
int Cli::RunDoc(std::span<std::string_view const> args, GlobalOptions const &opts) {
    bool openAfter = false;

    for (std::string_view const arg : args) {
        if (arg == "--open") {
            openAfter = true;
            continue;
        }
        if (arg == "-h" or arg == "--help") {
            PrintHelpFor("doc");
            return 0;
        }
        PrintUnknownOption(arg, "doc");
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

    if (!opts.quiet) {
        std::print("  Generating documentation for {} v{}\n", manifest->package.name,
                   manifest->package.version);
    }

    // TODO: documentation generator

    if (openAfter and !opts.quiet) {
        std::print("     Opening documentation...\n");
    }

    return 0;
}
} // namespace Rux
