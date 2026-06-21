// Copyright (c) Rux contributors.
// SPDX-License-Identifier: MIT

#include "Rux/Cli/Cli.h"          // for GlobalOptions, Cli
#include "Rux/Cli/CliInternals.h" // for RegistryPackagesDir, DependencyPackageName, FetchUrl
#include "Rux/Manifest.h"         // for Manifest, Dependency, ParsePackageSpec

#include <cstdio>        // for stderr
#include <filesystem>    // for path, operator/, exists, create_directories
#include <memory>        // for shared_ptr, make_shared
#include <optional>      // for optional
#include <print>         // for print
#include <span>          // for span
#include <string>        // for basic_string, hash, char_traits, string, operator==
#include <string_view>   // for basic_string_view, string_view, operator==
#include <system_error>  // for error_code
#include <unordered_map> // for unordered_map, operator==
#include <utility>       // for get, move
#include <vector>        // for vector

namespace Rux {
namespace {

enum class DependencyState {
    Visiting,
    Visited,
};

struct RegistryAccess {
    std::optional<std::string> json;

    static RegistryAccess Fetch() {
        return {Misc::FetchUrl(std::string(Misc::kRegistryUrl))};
    }

    std::string Lookup(std::string_view const pkgName) const {
        return json ? Misc::JsonLookupString(*json, pkgName) : "";
    }
};

class DependencyResolver {
public:
    explicit DependencyResolver(std::string_view target)
        : target_(target) {
    }

    bool Resolve(std::string_view pkgName, std::vector<std::string> &installOrder) {
        std::string pkgKey{pkgName};
        auto &state = visitState_[pkgKey];

        if (auto const it = visitState_.find(pkgKey); it != visitState_.end()) {
            if (it->second == DependencyState::Visiting) {
                std::print(stderr, "error: circular dependency detected involving '{}'\n", pkgName);
                return false;
            }
            return true; // Already visited
        }

        visitState_[pkgKey] = DependencyState::Visiting;

        auto const manifestPath = Misc::RegistryPackagesDir() / pkgName / "Rux.toml";
        if (auto const manifest = LoadManifestCached(pkgKey, manifestPath); manifest) {
            for (auto const &dep : manifest->EffectiveDependencies(target_)) {
                if (dep.path.empty()) {
                    if (!Resolve(Misc::DependencyPackageName(dep), installOrder)) {
                        return false;
                    }
                }
            }
        }

        visitState_[pkgKey] = DependencyState::Visited;
        installOrder.push_back(std::move(pkgKey));
        return true;
    }

private:
    std::string target_;
    std::unordered_map<std::string, DependencyState> visitState_;
    std::unordered_map<std::string, std::shared_ptr<Manifest>> cache_;

    std::shared_ptr<Manifest> LoadManifestCached(std::string_view key,
                                                 std::filesystem::path const &path) {
        if (auto const it = cache_.find(std::string(key)); it != cache_.end()) {
            return it->second;
        }

        auto loaded = Manifest::Load(path);
        if (!loaded) {
            return nullptr;
        }

        auto manifest = std::make_shared<Manifest>(std::move(*loaded));
        cache_.emplace(key, manifest);
        return manifest;
    }
};

bool PerformInstall(std::string_view pkgName, RegistryAccess const &registry, bool dev,
                    bool quiet) {
    std::string const repoUrl = registry.Lookup(pkgName);
    if (repoUrl.empty()) {
        std::print(stderr, "error: package '{}' not found in registry\n", pkgName);
        return false;
    }

    std::filesystem::path const pkgDir = Misc::RegistryPackagesDir() / pkgName;
    std::error_code ec;
    std::filesystem::create_directories(pkgDir.parent_path(), ec);
    if (ec) {
        std::print(stderr, "error: failed to create directories: {}\n", ec.message());
        return false;
    }

    if (std::filesystem::exists(pkgDir)) {
        if (!quiet) {
            std::print("   Up-to-date {}\n", pkgName);
        }
        return true;
    }

    if (!quiet) {
        std::print("  Downloading {} from {}...\n", pkgName, repoUrl);
    }

    if (!Misc::GitClone(repoUrl, pkgDir, dev)) {
        std::print(stderr, "error: failed to clone '{}'\n", repoUrl);
        return false;
    }

    if (!quiet) {
        std::print("    Installed {} at {}\n", pkgName, pkgDir.string());
    }
    return true;
}

} // namespace

int Cli::RunInstall(std::span<std::string_view const> args, GlobalOptions const &opts) {
    std::string_view packageSpec;
    bool packageFromDev = false;

    for (auto const &arg : args) {
        if (arg == "-h" or arg == "--help") {
            PrintHelpFor("install");
            return 0;
        }
        if (arg == "--dev") {
            packageFromDev = true;
        }
        else if (!arg.starts_with('-') and packageSpec.empty()) {
            packageSpec = arg;
        }
        else {
            PrintUnknownOption(arg, "install");
            return 1;
        }
    }

    auto const registry = RegistryAccess::Fetch();
    if (!registry.json) {
        std::print(stderr, "error: failed to fetch package registry\n");
        return 1;
    }

    // Direct install mode
    if (!packageSpec.empty()) {
        auto [pkgName, _] = ParsePackageSpec(packageSpec);
        return PerformInstall(pkgName, registry, packageFromDev, opts.quiet) ? 0 : 1;
    }

    // Dependency install mode
    auto const manifestPath = Misc::RequireManifest();
    if (!manifestPath) {
        return 1;
    }
    auto const manifest = Misc::LoadManifest(*manifestPath);
    if (!manifest) {
        return 1;
    }

    DependencyResolver resolver(Misc::HostTargetTriple());
    std::vector<std::string> installOrder;

    for (auto const &dep : manifest->EffectiveDependencies(Misc::HostTargetTriple())) {
        if (dep.path.empty()) {
            if (!resolver.Resolve(Misc::DependencyPackageName(dep), installOrder)) {
                return 1;
            }
        }
    }

    int installed = 0, upToDate = 0;
    for (auto const &pkgName : installOrder) {
        bool exists = std::filesystem::exists(Misc::RegistryPackagesDir() / pkgName);
        if (PerformInstall(pkgName, registry, packageFromDev, opts.quiet)) {
            exists ? ++upToDate : ++installed;
        }
        else {
            return 1;
        }
    }

    if (!opts.quiet) {
        std::print("     Summary: {} installed, {} already up-to-date\n", installed, upToDate);
    }
    return 0;
}
} // namespace Rux
