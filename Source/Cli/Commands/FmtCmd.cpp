// Copyright (c) Rux contributors.
// SPDX-License-Identifier: MIT

#include "Rux/Cli/Cli.h"
#include "Rux/Cli/CliInternals.h"

#include <filesystem>
#include <fstream>
#include <print>

namespace Rux {
namespace {
std::optional<std::string> ReadFileContent(std::filesystem::path const &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}
} // namespace

int Cli::RunFmt(std::span<std::string_view const> args, GlobalOptions const &opts) {
    bool check = false;
    bool manifestOnly = false;

    for (std::string_view const arg : args) {
        if (arg == "--check") {
            check = true;
            continue;
        }
        if (arg == "--manifest") {
            manifestOnly = true;
            continue;
        }
        if (arg == "-h" or arg == "--help") {
            PrintHelpFor("fmt");
            return 0;
        }
        PrintUnknownOption(arg, "fmt");
        return 1;
    }

    auto manifestPath = Misc::RequireManifest();
    if (!manifestPath) {
        return 1;
    }
    auto root = manifestPath->parent_path();
    if (manifestOnly) {
        auto manifest = Manifest::Load(*manifestPath);
        if (!manifest) {
            std::print(stderr, "error: failed to parse manifest file '{}'\n",
                       manifestPath->string());
            return 1;
        }

        // Get formatted content by saving to a temp file and reading it
        auto tempPath = root / "Rux.toml.fmt.tmp";
        if (!manifest->Save(tempPath)) {
            std::print(stderr, "error: failed to serialize formatted manifest\n");
            return 1;
        }

        auto const formattedContent = ReadFileContent(tempPath);
        std::error_code ec;
        std::filesystem::remove(tempPath, ec);

        auto const originalContent = ReadFileContent(*manifestPath);

        if (!formattedContent or !originalContent) {
            std::print(stderr, "error: failed to read manifest content during formatting\n");
            return 1;
        }

        if (check) {
            if (originalContent != formattedContent) {
                if (!opts.quiet) {
                    std::print(stderr, "error: manifest '{}' is not formatted\n",
                               manifestPath->string());
                }
                return 1;
            }
            if (!opts.quiet) {
                std::print("  Manifest is already formatted: {}\n", manifestPath->string());
            }
            return 0;
        }

        if (originalContent != formattedContent) {
            if (!opts.quiet) {
                std::print("  Formatting {}\n", manifestPath->string());
            }
            if (!manifest->Save(*manifestPath)) {
                std::print(stderr, "error: failed to write manifest file '{}'\n",
                           manifestPath->string());
                return 1;
            }
        }
        else {
            if (!opts.quiet) {
                std::print("  Manifest is already formatted: {}\n", manifestPath->string());
            }
        }
        return 0;
    }

    auto sourceDir = root / "Source";
    if (!std::filesystem::exists(sourceDir)) {
        if (!opts.quiet) {
            std::print("  No source directory found.\n");
        }
        return 0;
    }

    int fileCount = 0;
    for (auto const &entry : std::filesystem::recursive_directory_iterator(sourceDir)) {
        if (!entry.is_regular_file() or entry.path().extension() != ".rux") {
            continue;
        }

        ++fileCount;
        if (!opts.quiet) {
            std::print("  {} {}\n", check ? "Checking" : "Formatting", entry.path().string());
        }
        // TODO: source formatter
    }

    if (fileCount == 0 and !opts.quiet) {
        std::print("  No .rux files found.\n");
    }
    return 0;
}
} // namespace Rux
