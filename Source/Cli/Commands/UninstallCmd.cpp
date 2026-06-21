// Copyright (c) Rux contributors.
// SPDX-License-Identifier: MIT

#include "Rux/Cli/Cli.h"          // for GlobalOptions, Cli
#include "Rux/Cli/CliInternals.h" // for DependencyPackageName, HostTargetTriple, LoadManifest
#include "Rux/Manifest.h"         // for Dependency, Manifest

#include <cstdio>       // for stderr
#include <filesystem>   // for path, exists, operator/, remove_all
#include <optional>     // for optional
#include <print>        // for print
#include <span>         // for span
#include <string>       // for basic_string, char_traits
#include <string_view>  // for basic_string_view, string_view, operator==
#include <system_error> // for error_code
#include <vector>       // for vector

namespace Rux {
namespace {

enum class UninstallResult {
    Success,
    NotFound,
    Error,
};

UninstallResult PerformUninstall(std::string_view pkgName, bool quiet) {
    std::filesystem::path const pkgDir = Misc::RegistryPackagesDir() / pkgName;
    if (!std::filesystem::exists(pkgDir)) {
        if (!quiet) {
            std::print("  Not installed {}\n", pkgName);
        }
        return UninstallResult::NotFound;
    }

    std::error_code ec;
    std::filesystem::remove_all(pkgDir, ec);
    if (ec) {
        std::print(stderr, "error: failed to remove '{}': {}\n", pkgDir.string(), ec.message());
        return UninstallResult::Error;
    }

    if (!quiet) {
        std::print("    Uninstalled {}\n", pkgName);
    }
    return UninstallResult::Success;
}

} // namespace

int Cli::RunUninstall(std::span<std::string_view const> args, GlobalOptions const &opts) {
    std::string_view packageName;

    for (auto const &arg : args) {
        if (arg == "-h" or arg == "--help") {
            PrintHelpFor("uninstall");
            return 0;
        }
        if (!arg.starts_with('-') and packageName.empty()) {
            packageName = arg;
        }
        else {
            PrintUnknownOption(arg, "uninstall");
            return 1;
        }
    }

    // Single uninstall mode
    if (!packageName.empty()) {
        return (PerformUninstall(packageName, opts.quiet) == UninstallResult::Error) ? 1 : 0;
    }

    // Full manifest uninstall mode
    auto const manifestPath = Misc::RequireManifest();
    if (!manifestPath) {
        return 1;
    }

    auto const manifest = Misc::LoadManifest(*manifestPath);
    if (!manifest) {
        return 1;
    }

    int removed = 0, notFound = 0;
    for (auto const &dep : manifest->EffectiveDependencies(Misc::HostTargetTriple())) {
        if (!dep.path.empty()) {
            continue;
        }

        UninstallResult const result =
            PerformUninstall(Misc::DependencyPackageName(dep), opts.quiet);
        if (result == UninstallResult::Error) {
            return 1;
        }
        (result == UninstallResult::Success) ? ++removed : ++notFound;
    }

    if (!opts.quiet) {
        std::print("     Summary: {} uninstalled, {} not installed\n", removed, notFound);
    }
    return 0;
}
} // namespace Rux
