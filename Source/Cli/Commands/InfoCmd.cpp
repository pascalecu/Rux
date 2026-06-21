// Copyright (c) Rux contributors.
// SPDX-License-Identifier: MIT

#include "Rux/Cli/Cli.h"          // for Cli, GlobalOptions (ptr only)
#include "Rux/Cli/CliInternals.h" // for RegistryPackagesDir
#include "Rux/Manifest.h"         // for Manifest, Dependency, Package

#include <cstdio>      // for stderr, size_t
#include <filesystem>  // for path, operator/, current_path, exists
#include <optional>    // for optional
#include <print>       // for print
#include <span>        // for span
#include <string>      // for basic_string, char_traits, string
#include <string_view> // for basic_string_view, operator==, string_view
#include <vector>      // for vector

namespace Rux {
// TODO: Make this look in the registry instead of installed packages
// TODO: Extend Package manifest metadata support
int Cli::RunInfo(std::span<std::string_view const> args, GlobalOptions const &opts) {
    (void)opts;
    std::string_view packageName;
    bool jsonOutput = false;

    for (std::string_view const arg : args) {
        if (arg == "-h" or arg == "--help") {
            PrintHelpFor("info");
            return 0;
        }
        if (arg == "--json") {
            jsonOutput = true;
            continue;
        }
        if (!arg.starts_with('-') and packageName.empty()) {
            packageName = arg;
            continue;
        }
        PrintUnknownOption(arg, "info");
        return 1;
    }

    std::filesystem::path manifestPath;
    if (packageName.empty()) {
        auto localManifestOpt = Manifest::Find(std::filesystem::current_path());
        if (!localManifestOpt) {
            std::print(stderr,
                       "error: missing package name, and no Rux.toml found in current directory\n");
            return 1;
        }
        manifestPath = *localManifestOpt;
    }
    else {
        auto const packageDir = Misc::RegistryPackagesDir() / std::string(packageName);
        manifestPath = packageDir / "Rux.toml";

        if (!std::filesystem::exists(manifestPath)) {
            std::print(stderr, "error: package '{}' is not installed\n", packageName);
            return 1;
        }
    }

    auto manifest = Manifest::Load(manifestPath);
    if (!manifest) {
        std::print(stderr, "error: failed to parse '{}'\n", manifestPath.string());
        return 1;
    }

    if (jsonOutput) {
        std::print("{{\n");
        std::print("  \"name\": \"{}\",\n", manifest->package.name);
        std::print("  \"version\": \"{}\",\n", manifest->package.version);
        std::print("  \"type\": \"{}\",\n", manifest->package.type);

        if (!manifest->package.description.empty()) {
            std::print("  \"description\": \"{}\",\n", manifest->package.description);
        }
        if (!manifest->package.authors.empty()) {
            std::print("  \"authors\": \"{}\",\n", manifest->package.authors);
        }
        if (!manifest->package.license.empty()) {
            std::print("  \"license\": \"{}\",\n", manifest->package.license);
        }
        if (!manifest->package.repository.empty()) {
            std::print("  \"repository\": \"{}\",\n", manifest->package.repository);
        }
        if (!manifest->package.homepage.empty()) {
            std::print("  \"homepage\": \"{}\",\n", manifest->package.homepage);
        }

        std::print("  \"dependencies\": [\n");

        for (size_t i = 0; i < manifest->dependencies.size(); ++i) {
            auto const &dep = manifest->dependencies[i];
            std::print("    {{\n");
            std::print("      \"name\": \"{}\"", dep.name);

            if (!dep.path.empty()) {
                std::print(",\n      \"path\": \"{}\"\n", dep.path);
            }
            else {
                std::print(",\n      \"version\": \"{}\"\n",
                           dep.version.empty() ? "*" : dep.version);
            }
            std::print("    }}{}\n", (i + 1 < manifest->dependencies.size()) ? "," : "");
        }

        std::print("  ]\n");
        std::print("}}\n");
    }
    else {
        std::print("Name:        {}\n"
                   "Version:     {}\n"
                   "Type:        {}\n",
                   manifest->package.name, manifest->package.version, manifest->package.type);

        if (!manifest->package.description.empty()) {
            std::print("Description: {}\n", manifest->package.description);
        }
        if (!manifest->package.authors.empty()) {
            std::print("Authors:     {}\n", manifest->package.authors);
        }
        if (!manifest->package.license.empty()) {
            std::print("License:     {}\n", manifest->package.license);
        }
        if (!manifest->package.repository.empty()) {
            std::print("Repository:  {}\n", manifest->package.repository);
        }
        if (!manifest->package.homepage.empty()) {
            std::print("Homepage:    {}\n", manifest->package.homepage);
        }

        if (!manifest->dependencies.empty()) {
            std::print("\nDependencies:\n");
            for (auto const &dep : manifest->dependencies) {
                if (!dep.path.empty()) {
                    std::print("  - {} (path: {})\n", dep.name, dep.path);
                }
                else {
                    std::print("  - {} @ {}\n", dep.name, dep.version.empty() ? "*" : dep.version);
                }
            }
        }
    }

    return 0;
}
} // namespace Rux
