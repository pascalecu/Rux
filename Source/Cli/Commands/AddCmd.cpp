// Copyright (c) Rux contributors.
// SPDX-License-Identifier: MIT

#include "Rux/Cli/Cli.h"
#include "Rux/Cli/CliInternals.h"

#include <print>

namespace Rux {
int Cli::RunAdd(std::span<std::string_view const> args, GlobalOptions const &opts) {
    std::string_view spec;
    std::string_view pathArg;

    for (std::size_t i = 0; i < args.size(); ++i) {
        std::string_view arg = args[i];
        if (arg == "-h" or arg == "--help") {
            PrintHelpFor("add");
            return 0;
        }
        if (arg == "--path") {
            if (++i >= args.size()) {
                std::print(stderr, "error: '--path' requires an argument\n");
                return 1;
            }
            pathArg = args[i];
        }
        else if (!args[i].starts_with('-') and spec.empty()) {
            spec = args[i];
        }
        else {
            PrintUnknownOption(args[i], "add");
            return 1;
        }
    }

    if (spec.empty()) {
        std::print(stderr, "error: missing package name\n\n");
        PrintHelpFor("add");
        return 1;
    }

    auto manifestPath = Misc::RequireManifest();
    if (!manifestPath) {
        return 1;
    }

    auto manifest = Misc::LoadManifest(*manifestPath);
    if (!manifest) {
        return 1;
    }

    auto [pkgName, pkgVersion] = ParsePackageSpec(spec);

    if (!pathArg.empty()) {
        bool const changed = manifest->AddPathDependency(pkgName, std::string(pathArg));
        if (!manifest->Save(*manifestPath)) {
            std::print(stderr, "error: failed to write '{}'\n", manifestPath->string());
            return 1;
        }
        if (!opts.quiet) {
            std::print("{} {} @ path '{}'\n", changed ? "Added" : "Up-to-date", pkgName, pathArg);
        }
        return 0;
    }

    if (!opts.quiet) {
        std::print("     Fetching registry...\n");
    }

    auto const jsonOpt = Misc::FetchUrl(std::string(Misc::kRegistryUrl));
    if (!jsonOpt) {
        std::print(stderr, "error: failed to fetch package registry\n");
        return 1;
    }

    if (Misc::JsonLookupString(*jsonOpt, pkgName).empty()) {
        std::print(stderr, "error: package '{}' not found in registry\n", pkgName);
        return 1;
    }

    bool const changed = manifest->AddDependency(pkgName, pkgVersion);
    if (!manifest->Save(*manifestPath)) {
        std::print(stderr, "error: failed to write '{}'\n", manifestPath->string());
        return 1;
    }

    if (!opts.quiet) {
        std::string const ver = pkgVersion.empty() ? "latest" : pkgVersion;
        std::print("{} {} @ {}\n", changed ? "Added" : "Up-to-date", pkgName, ver);
    }
    return 0;
}
} // namespace Rux
