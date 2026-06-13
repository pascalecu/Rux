// Copyright (c) Rux contributors.
// SPDX-License-Identifier: MIT

#include "Rux/SourceManager.h"

#include <fstream>
#include <print>

namespace Rux {
    namespace {
        auto ReadFile(const std::filesystem::path& path)
            -> std::optional<std::string> {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file) {
                return std::nullopt;
            }

            const auto end = file.tellg();
            if (end < 0) {
                return std::nullopt;
            }

            file.seekg(0, std::ios::beg);

            std::string buffer;
            buffer.resize_and_overwrite(
                end, [&file](char* buf, const size_t buf_size) {
                    file.read(buf, static_cast<std::streamsize>(buf_size));
                    return file.gcount();
                });

            if (file.fail() && !file.eof()) {
                return std::nullopt;
            }

            return buffer;
        }
    } // namespace

    auto SourceManager::LoadFile(const std::filesystem::path& path)
        -> std::optional<std::string_view> {
        std::string pathStr = path.string();

        {
            std::shared_lock lock(rwMutex);
            if (const auto* file = GetFileUnlocked(pathStr)) {
                return file->GetView();
            }
        }

        auto content = ReadFile(path);
        if (!content) {
            std::println(stderr, "error: failed to load '{}'", pathStr);
            return std::nullopt;
        }

        auto sourceFile = std::make_unique<SourceFile>();
        sourceFile->name = std::move(pathStr);
        sourceFile->buffer = std::move(*content);

        std::unique_lock lock(rwMutex);

        if (const auto* file = GetFileUnlocked(sourceFile->name)) {
            return file->GetView();
        }

        auto* ptr = sourceFile.get();
        const auto view = ptr->GetView();

        files.push_back(std::move(sourceFile));
        fileLookup.emplace(ptr->name, ptr);

        return view;
    }

    auto SourceManager::LoadVirtual(std::string name, std::string content)
        -> std::string_view {
        {
            std::shared_lock lock(rwMutex);
            if (const auto* file = GetFileUnlocked(name)) {
                return file->GetView();
            }
        }

        auto sourceFile = std::make_unique<SourceFile>();
        sourceFile->name = std::move(name);
        sourceFile->buffer = std::move(content);

        std::unique_lock lock(rwMutex);

        if (const auto* file = GetFileUnlocked(sourceFile->name)) {
            return file->GetView();
        }

        auto* ptr = sourceFile.get();
        const auto view = ptr->GetView();

        files.push_back(std::move(sourceFile));
        fileLookup.emplace(ptr->name, ptr);

        return view;
    }

    const SourceFile*
    SourceManager::GetFile(const std::string_view name) const {
        std::shared_lock read_lock(rwMutex);
        return GetFileUnlocked(name);
    }

    const SourceFile*
    SourceManager::GetFileUnlocked(const std::string_view name) const noexcept {
        const auto it = fileLookup.find(name);
        return it != fileLookup.end() ? it->second : nullptr;
    }
} // namespace Rux
