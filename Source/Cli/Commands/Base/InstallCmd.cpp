// Copyright (c) Rux contributors.
// SPDX-License-Identifier: MIT

#include "Rux/Cli/Cli.h"          // for GlobalOptions, Cli
#include "Rux/Cli/CliInternals.h" // for RegistryPackagesDir, DependencyPackageName, FetchUrl
#include "Rux/Manifest.h"         // for Dependency, Manifest, ParsePackageSpec
#include "Rux/Platform/Defines.h" // for RUX_OS_WINDOWS
#include "Rux/Platform/Types.h"   // for Platform

#include <cstdio>        // for stderr, size_t
#include <filesystem>    // for path, operator/, exists, create_directories, remove_all
#include <optional>      // for optional
#include <print>         // for print
#include <span>          // for span
#include <string>        // for basic_string, char_traits, hash, string, operator==
#include <string_view>   // for basic_string_view, operator==, string_view
#include <system_error>  // for error_code
#include <unordered_set> // for unordered_set
#include <utility>       // for get
#include <vector>        // for vector

/*
 * This is separate from the other ifdef because otherwise clang-format attempts
 * to change the order, which makes MSVC cry.
 */

#if RUX_OS_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif

    #ifndef NOMINMAX
        #define NOMINMAX
    #endif

    #include <windows.h>
#endif

#if RUX_OS_WINDOWS
    #include <psapi.h>
#else
    #include <sys/resource.h>
    #include <sys/wait.h>
    #include <unistd.h>
#endif

using namespace Rux;
using namespace Platform;
using namespace Misc;

int Cli::RunInstall(std::span<std::string_view const> args, GlobalOptions const &opts) {
    std::string_view packageSpec;
    bool packageFromDev = false;

    for (auto arg : args) {
        if (arg == "-h" || arg == "--help") {
            PrintHelpFor("install");
            return 0;
        }

        if (arg == "--dev") {
            packageFromDev = true;
            continue;
        }

        if (!arg.starts_with('-') && packageSpec.empty()) {
            packageSpec = arg;
            continue;
        }

        PrintUnknownOption(arg, "install");
        return 1;
    }


    // Install a specific package without requiring a manifest
    if (!packageSpec.empty()) {
        auto [pkgName, pkgVersion] = ParsePackageSpec(packageSpec);

        if (!opts.quiet) {
            std::print("     Fetching registry...\n");
        }

        auto const jsonOpt = FetchUrl(std::string(kRegistryUrl));
        if (!jsonOpt) {
            std::print(stderr, "error: failed to fetch package registry\n");
            return 1;
        }

        std::string const repoUrl = JsonLookupString(*jsonOpt, pkgName);
        if (repoUrl.empty()) {
            std::print(stderr, "error: package '{}' not found in registry\n", pkgName);
            return 1;
        }

        std::filesystem::path const pkgDir = RegistryPackagesDir() / pkgName;
        std::error_code ec;
        create_directories(pkgDir.parent_path(), ec);

        if (!exists(pkgDir)) {
            if (!opts.quiet) {
                std::print("  Downloading {} from {}...\n", pkgName, repoUrl);
            }

            if (!GitClone(repoUrl, pkgDir, packageFromDev)) {
                std::print(stderr, "error: failed to clone '{}'\n", repoUrl);
                return 1;
            }

            if (!opts.quiet) {
                std::print("    Installed {} at {}\n", pkgName, pkgDir.string());
            }
        }
        else {
            if (!opts.quiet) {
                std::print("   Up-to-date {}\n", pkgName);
            }
        }
    }

    // Install dependencies from current project
    auto const manifestPath = RequireManifest();
    if (!manifestPath) {
        return 1;
    }

    auto manifest = LoadManifest(*manifestPath);
    if (!manifest) {
        return 1;
    }

    std::vector<std::string> queue;
    std::unordered_set<std::string> queued;

    std::string const installTarget = HostTargetTriple();

    for (auto const &dep : manifest->EffectiveDependencies(installTarget)) {
        if (std::string const packageName = DependencyPackageName(dep);
            dep.path.empty() && !queued.contains(packageName)) {
            queue.push_back(packageName);
            queued.insert(packageName);
        }
    }

    if (queue.empty()) {
        if (!opts.quiet) {
            std::print("  No registry dependencies to install.\n");
        }

        return 0;
    }

    if (!opts.quiet) {
        std::print("     Fetching registry...\n");
    }

    auto const jsonOptInstall = FetchUrl(std::string(kRegistryUrl));
    if (!jsonOptInstall) {
        std::print(stderr, "error: failed to fetch package registry\n");
        return 1;
    }

    int installed = 0;
    int upToDate = 0;

    for (std::size_t i = 0; i < queue.size(); ++i) {
        std::string const &pkgName = queue[i];

        std::string const repoUrl = JsonLookupString(*jsonOptInstall, pkgName);
        if (repoUrl.empty()) {
            std::print(stderr, "error: package '{}' not found in registry\n", pkgName);
            return 1;
        }

        std::filesystem::path const pkgDir = RegistryPackagesDir() / pkgName;

        std::error_code ec;
        create_directories(pkgDir.parent_path(), ec);

        if (exists(pkgDir)) {
            if (!opts.quiet) {
                std::print("   Up-to-date {}\n", pkgName);
            }

            ++upToDate;
        }
        else {
            if (!opts.quiet) {
                std::print("  Downloading {} from {}...\n", pkgName, repoUrl);
            }

            if (!GitClone(repoUrl, pkgDir, packageFromDev)) {
                std::print(stderr, "error: failed to clone '{}'\n", repoUrl);
                return 1;
            }

            if (!opts.quiet) {
                std::print("    Installed {} at {}\n", pkgName, pkgDir.string());
            }

            ++installed;
        }

        if (auto const depManifest = Manifest::Load(pkgDir / "Rux.toml")) {
            for (auto const &dep : depManifest->EffectiveDependencies(installTarget)) {
                if (std::string const depPackageName = DependencyPackageName(dep);
                    dep.path.empty() && !queued.contains(depPackageName)) {
                    queue.push_back(depPackageName);
                    queued.insert(depPackageName);
                }
            }
        }
    }

    if (!opts.quiet) {
        std::print("     Summary: {} installed, {} already up-to-date\n", installed, upToDate);
    }

    return 0;
}

int Cli::RunUninstall(std::span<std::string_view const> args, GlobalOptions const &opts) {
    std::string_view packageName;
    for (auto arg : args) {
        if (arg == "-h" || arg == "--help") {
            PrintHelpFor("uninstall");
            return 0;
        }
        if (!arg.starts_with('-') && packageName.empty()) {
            packageName = arg;
            continue;
        }
        PrintUnknownOption(arg, "uninstall");
        return 1;
    }

    if (!packageName.empty()) {
        std::filesystem::path const pkgDir = RegistryPackagesDir() / std::string(packageName);
        if (!std::filesystem::exists(pkgDir)) {
            std::print(stderr, "error: package '{}' is not installed\n", packageName);
            return 1;
        }
        std::error_code ec;
        std::filesystem::remove_all(pkgDir, ec);
        if (ec) {
            std::print(stderr, "error: failed to remove '{}': {}\n", pkgDir.string(), ec.message());
            return 1;
        }
        if (!opts.quiet) {
            std::print("   Uninstalled {}\n", packageName);
        }
        return 0;
    }

    auto const manifestPath = RequireManifest();
    if (!manifestPath) {
        return 1;
    }
    auto manifest = LoadManifest(*manifestPath);
    if (!manifest) {
        return 1;
    }

    std::vector<std::string> toRemove;
    for (auto const &dep : manifest->EffectiveDependencies(HostTargetTriple())) {
        if (dep.path.empty()) {
            toRemove.push_back(DependencyPackageName(dep));
        }
    }

    if (toRemove.empty()) {
        if (!opts.quiet) {
            std::print("  No registry dependencies to uninstall.\n");
        }
        return 0;
    }

    int removed = 0;
    int notFound = 0;
    for (auto const &pkgName : toRemove) {
        std::filesystem::path const pkgDir = RegistryPackagesDir() / pkgName;
        if (!std::filesystem::exists(pkgDir)) {
            if (!opts.quiet) {
                std::print("  Not installed {}\n", pkgName);
            }
            ++notFound;
            continue;
        }
        std::error_code ec;
        std::filesystem::remove_all(pkgDir, ec);
        if (ec) {
            std::print(stderr, "error: failed to remove '{}': {}\n", pkgDir.string(), ec.message());
            return 1;
        }
        if (!opts.quiet) {
            std::print("   Uninstalled {}\n", pkgName);
        }
        ++removed;
    }
    if (!opts.quiet) {
        std::print("     Summary: {} uninstalled, {} not installed\n", removed, notFound);
    }
    return 0;
}
