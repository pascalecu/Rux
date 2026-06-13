// Copyright (c) Rux contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include "SourceManager.h"

namespace Rux {
    struct FailedFile {
        std::filesystem::path path;
        std::error_code error;
    };

    class SourceLoader {
    public:
        // Load all *.rux files from the Src/ directory of a package.
        // manifestDir  - the directory that contains Rux.toml
        // Returns nullopt if the Src/ directory does not exist or cannot be
        // opened.
        [[nodiscard]] static std::optional<std::vector<FailedFile>>
        Load(const std::filesystem::path& manifestDir, SourceManager& manager);

        // Load a single *.rux file by explicit path.
        // Returns nullopt if the file cannot be opened.
        static std::expected<std::string_view, std::error_code>
        LoadFile(const std::filesystem::path& path, SourceManager& manager);

    private:
        // Collect all *.rux paths under a directory tree (recursive).
        static std::vector<std::filesystem::path>
        CollectSourcePaths(const std::filesystem::path& srcDir);
    };
} // namespace Rux
