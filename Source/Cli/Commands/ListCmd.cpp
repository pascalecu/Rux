// Copyright (c) Rux contributors.
// SPDX-License-Identifier: MIT

#include "Rux/Cli/Cli.h"          // for GlobalOptions, Cli
#include "Rux/Cli/CliInternals.h" // for LoadManifest, RegistryPackagesDir, RequireManifest
#include "Rux/Manifest.h"         // for Dependency, Manifest

#include <algorithm>    // for __sort, sort
#include <filesystem>   // for path, directory_iterator, directory_entry, exists
#include <optional>     // for optional
#include <print>        // for print
#include <span>         // for span
#include <string>       // for basic_string, char_traits, string, operator<=>, swap
#include <string_view>  // for basic_string_view, operator==, string_view
#include <system_error> // for error_code
#include <vector>       // for vector

namespace Rux {
int Cli::RunList(std::span<std::string_view const> args, GlobalOptions const &opts) {
    bool global = false;

    for (std::string_view const arg : args) {
        if (arg == "--global") {
            global = true;
            continue;
        }
        if (arg == "-h" or arg == "--help") {
            PrintHelpFor("list");
            return 0;
        }
        PrintUnknownOption(arg, "list");
        return 1;
    }

    if (global) {
        auto const cacheDir = Misc::RegistryPackagesDir();
        std::vector<std::string> packages;

        if (std::error_code ec; std::filesystem::exists(cacheDir, ec)) {
            for (auto const &entry : std::filesystem::directory_iterator(cacheDir, ec)) {
                if (entry.is_directory()) {
                    packages.push_back(entry.path().filename().string());
                }
            }
            std::ranges::sort(packages);
        }

        if (packages.empty()) {
            if (!opts.quiet) {
                std::print("  Global cache is empty ({})\n", cacheDir.string());
            }
            return 0;
        }

        std::print("Global cache ({} package{} at {}):\n", packages.size(),
                   packages.size() == 1 ? "" : "s", cacheDir.string());

        for (auto const &pkg : packages) {
            std::print("  {}\n", pkg);
        }
        return 0;
    }

    auto const manifestPath = Misc::RequireManifest();
    if (!manifestPath) {
        return 1;
    }

    auto manifest = Misc::LoadManifest(*manifestPath);
    if (!manifest) {
        return 1;
    }

    if (manifest->dependencies.empty()) {
        if (!opts.quiet) {
            std::print("  No dependencies.\n");
        }
        return 0;
    }

    std::print("Dependencies ({}):\n", manifest->dependencies.size());
    for (auto const &dep : manifest->dependencies) {
        if (!dep.path.empty()) {
            std::print("  {} (path: {})\n", dep.name, dep.path);
        }
        else {
            std::string const ver = dep.version.empty() ? "latest" : dep.version;
            std::print("  {} @ {}\n", dep.name, ver);
        }
    }
    return 0;
}
} // namespace Rux
