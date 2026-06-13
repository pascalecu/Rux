// Copyright (c) Rux contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Rux {
    struct SourceFile {
        std::string name;
        std::string buffer;

        [[nodiscard]]
        std::string_view GetView() const noexcept {
            return buffer;
        }
    };

    class SourceManager {
    public:
        SourceManager() = default;
        ~SourceManager() = default;

        SourceManager(const SourceManager&) = delete;
        SourceManager& operator=(const SourceManager&) = delete;

        [[nodiscard]]
        auto LoadFile(const std::filesystem::path& path)
            -> std::expected<std::string_view, std::error_code>;

        [[nodiscard]]
        auto LoadVirtual(std::string name, std::string content)
            -> std::string_view;

        [[nodiscard]]
        const SourceFile* GetFile(std::string_view name) const;

        [[nodiscard]]
        std::vector<const SourceFile*> GetAllFiles() const;

    private:
        const SourceFile* RegisterFile(std::string name, std::string content);
        const SourceFile* GetFileUnlocked(std::string_view name) const noexcept;

        mutable std::shared_mutex rwMutex;
        std::vector<std::unique_ptr<SourceFile>> files;
        std::unordered_map<std::string_view, const SourceFile*> fileLookup;
    };

} // namespace Rux
