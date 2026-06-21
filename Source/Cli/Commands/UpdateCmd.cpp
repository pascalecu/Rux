// Copyright (c) Rux contributors.
// SPDX-License-Identifier: MIT

#include "Rux/Cli/Cli.h"          // for GlobalOptions, Cli
#include "Rux/Cli/CliInternals.h" // for DependencyPackageName, GitPull, RegistryPackagesDir
#include "Rux/Manifest.h"         // for Dependency, Manifest

#include <cstdio>        // for stderr, size_t
#include <filesystem>    // for path, directory_iterator, directory_entry, exists, ope...
#include <optional>      // for optional
#include <print>         // for print
#include <span>          // for span
#include <string>        // for basic_string, hash, char_traits, string, operator==
#include <string_view>   // for basic_string_view, operator==, string_view
#include <system_error>  // for error_code
#include <unordered_set> // for unordered_set
#include <utility>       // for pair
#include <vector>        // for vector

namespace Rux {
int Cli::RunUpdate(std::span<std::string_view const> args, GlobalOptions const &opts) {
    bool global = false;

    for (std::string_view const arg : args) {
        if (arg == "--global") {
            global = true;
            continue;
        }
        if (arg == "-h" or arg == "--help") {
            PrintHelpFor("update");
            return 0;
        }
        PrintUnknownOption(arg, "update");
        return 1;
    }

    if (global) {
        auto const cacheDir = Misc::RegistryPackagesDir();
        std::vector<std::filesystem::path> pkgDirs;

        if (std::error_code ec; std::filesystem::exists(cacheDir, ec)) {
            for (auto const &entry : std::filesystem::directory_iterator(cacheDir, ec)) {
                if (entry.is_directory()) {
                    pkgDirs.push_back(entry.path());
                }
            }
        }

        if (pkgDirs.empty()) {
            if (!opts.quiet) {
                std::print("  No packages in global cache to update.\n");
            }
            return 0;
        }

        int updated = 0;
        for (auto const &pkgDir : pkgDirs) {
            std::string const pkgName = pkgDir.filename().string();
            if (!opts.quiet) {
                std::print("    Updating {}...\n", pkgName);
            }
            if (!Misc::GitPull(pkgDir)) {
                std::print(stderr, "error: failed to update '{}'\n", pkgName);
                return 1;
            }
            ++updated;
        }

        if (!opts.quiet) {
            std::print("     Summary: {} updated\n", updated);
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

    std::vector<std::string> queue;
    std::unordered_set<std::string> queued;
    std::string const updateTarget = Misc::HostTargetTriple();

    for (auto const &dep : manifest->EffectiveDependencies(updateTarget)) {
        std::string const packageName = Misc::DependencyPackageName(dep);
        if (dep.path.empty() and queued.insert(packageName).second) {
            queue.push_back(packageName);
        }
    }

    if (queue.empty()) {
        if (!opts.quiet) {
            std::print("  No registry dependencies to update.\n");
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

    int updated = 0;
    int installed = 0;

    for (std::size_t i = 0; i < queue.size(); ++i) {
        std::string const pkgName = queue[i];
        std::string const repoUrl = Misc::JsonLookupString(*jsonOpt, pkgName);

        if (repoUrl.empty()) {
            std::print(stderr, "error: package '{}' not found in registry\n", pkgName);
            return 1;
        }

        std::filesystem::path const pkgDir = Misc::RegistryPackagesDir() / pkgName;
        std::error_code ec;
        std::filesystem::create_directories(pkgDir.parent_path(), ec);

        if (std::filesystem::exists(pkgDir)) {
            if (!opts.quiet) {
                std::print("    Updating {}...\n", pkgName);
            }
            if (!Misc::GitPull(pkgDir)) {
                std::print(stderr, "error: failed to update '{}'\n", pkgName);
                return 1;
            }
            ++updated;
        }
        else {
            if (!opts.quiet) {
                std::print("  Downloading {} from {}...\n", pkgName, repoUrl);
            }
            if (!Misc::GitClone(repoUrl, pkgDir, /* devBranch = */ false)) {
                std::print(stderr, "error: failed to clone '{}'\n", repoUrl);
                return 1;
            }
            if (!opts.quiet) {
                std::print("    Installed {} at {}\n", pkgName, pkgDir.string());
            }
            ++installed;
        }

        // Enqueue registry deps declared by this package
        if (auto const depManifest = Manifest::Load(pkgDir / "Rux.toml")) {
            for (auto const &dep : depManifest->EffectiveDependencies(updateTarget)) {
                std::string const depPackageName = Misc::DependencyPackageName(dep);
                if (dep.path.empty() and queued.insert(depPackageName).second) {
                    queue.push_back(depPackageName);
                }
            }
        }
    }

    if (!opts.quiet) {
        std::print("     Summary: {} updated, {} newly installed\n", updated, installed);
    }
    return 0;
}
} // namespace Rux
