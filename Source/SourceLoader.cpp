// Copyright (c) Rux contributors.
// SPDX-License-Identifier: MIT

#include "Rux/SourceLoader.h"

#include <algorithm>
#include <fstream>
#include <print>
#include <sstream>

namespace Rux {
    auto SourceLoader::Load(const std::filesystem::path& manifestDir,
                            SourceManager& manager)
        -> std::optional<std::vector<FailedFile>> {
        const auto srcDir = manifestDir / "Src";

        if (!std::filesystem::exists(srcDir) ||
            !std::filesystem::is_directory(srcDir)) {
            std::println(stderr,
                         "error: source directory '{}' does not exist or is "
                         "not a directory",
                         srcDir.string());
            return std::nullopt;
        }

        const auto paths = CollectSourcePaths(srcDir);
        if (paths.empty()) {
            std::println(stderr,
                         "warning: no *.rux files found under '{}'",
                         srcDir.string());
        }

        std::vector<FailedFile> failures;
        for (const auto& path : paths) {
            if (auto result = LoadFile(path, manager); !result) {
                failures.push_back({path, result.error()});
            }
        }
        return failures;
    }

    std::expected<std::string_view, std::error_code>
    SourceLoader::LoadFile(const std::filesystem::path& path,
                           SourceManager& manager) {
        return manager.LoadFile(path);
    }

    std::vector<std::filesystem::path>
    SourceLoader::CollectSourcePaths(const std::filesystem::path& srcDir) {
        std::vector<std::filesystem::path> paths;
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(srcDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".rux") {
                paths.push_back(entry.path());
            }
        }
        // Sort for deterministic ordering across platforms
        std::ranges::sort(paths);
        return paths;
    }
} // namespace Rux
