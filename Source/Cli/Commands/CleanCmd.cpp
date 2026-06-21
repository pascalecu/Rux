// Copyright (c) Rux contributors.
// SPDX-License-Identifier: MIT

#include "Rux/Cli/Cli.h"
#include "Rux/Cli/CliInternals.h"

namespace Rux {
int Cli::RunClean(std::span<std::string_view const> const args, GlobalOptions const &opts) {
    bool tempOnly = false;
    for (auto &arg : args) {
        if (arg == "--temp") {
            tempOnly = true;
            continue;
        }
        if (arg == "-h" or arg == "--help") {
            PrintHelpFor("clean");
            return 0;
        }
        PrintUnknownOption(arg, "clean");
        return 1;
    }
    auto const manifestPath = Misc::RequireManifest();
    if (!manifestPath) {
        return 1;
    }
    auto const manifest = Misc::LoadManifest(*manifestPath);
    if (!manifest) {
        return 1;
    }
    auto const root = manifestPath->parent_path();
    auto const outputDir = manifest->build.output.empty()
                             ? root / "Bin"
                             : (std::filesystem::path(manifest->build.output).is_relative()
                                    ? root / manifest->build.output
                                    : std::filesystem::path(manifest->build.output));
    auto removeDir = [&](std::filesystem::path const &dir) -> bool {
        std::error_code ec;
        if (!std::filesystem::exists(dir)) {
            return true;
        }
        std::filesystem::remove_all(dir, ec);
        if (ec) {
            std::print(stderr, "error: failed to remove '{}': {}\n", dir.string(), ec.message());
            return false;
        }
        if (!opts.quiet) {
            std::print("     Removed {}\n", dir.string());
        }
        return true;
    };
    bool ok = true;
    if (!tempOnly) {
        ok &= removeDir(outputDir);
    }
    ok &= removeDir(root / "Temp");
    return ok ? 0 : 1;
}
} // namespace Rux
