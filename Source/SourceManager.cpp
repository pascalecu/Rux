// Copyright (c) Rux contributors.
// SPDX-License-Identifier: MIT

#include "Rux/SourceManager.h"

#include <expected>
#include <fstream>
#include <print>

namespace Rux {
    namespace {
        std::optional<std::string> ReadFile(const std::filesystem::path& path,
                                            std::error_code& ec) {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file) {
                ec = std::make_error_code(std::errc::no_such_file_or_directory);
                return std::nullopt;
            }

            const auto end = file.tellg();
            if (end < 0) {
                ec = std::make_error_code(std::errc::io_error);
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
                ec = std::make_error_code(std::errc::io_error);
                return std::nullopt;
            }

            return buffer;
        }
    } // namespace

    const SourceFile*
    SourceManager::GetFile(const std::string_view name) const {
        std::shared_lock read_lock(rwMutex);
        return GetFileUnlocked(name);
    }

    const SourceFile* SourceManager::RegisterFile(std::string name,
                                                  std::string content) {
        std::unique_lock lock(rwMutex);

        if (const auto* existing = GetFileUnlocked(name)) {
            return existing;
        }

        auto sourceFile = std::make_unique<SourceFile>();
        sourceFile->name = std::move(name);
        sourceFile->buffer = std::move(content);

        const auto* ptr = sourceFile.get();
        fileLookup.emplace(ptr->name, ptr);
        files.push_back(std::move(sourceFile));

        return ptr;
    }

    auto SourceManager::LoadFile(const std::filesystem::path& path)
        -> std::expected<std::string_view, std::error_code> {
        std::string pathStr = path.string();

        {
            std::shared_lock lock(rwMutex);
            if (const auto* file = GetFileUnlocked(pathStr)) {
                return file->GetView();
            }
        }

        std::error_code ec;
        auto content = ReadFile(path, ec);
        if (!content) {
            return std::unexpected(ec);
        }

        return RegisterFile(std::move(pathStr), std::move(*content))->GetView();
    }

    auto SourceManager::LoadVirtual(std::string name, std::string content)
        -> std::string_view {
        {
            std::shared_lock lock(rwMutex);
            if (const auto* file = GetFileUnlocked(name)) {
                return file->GetView();
            }
        }

        return RegisterFile(std::move(name), std::move(content))->GetView();
    }

    const SourceFile*
    SourceManager::GetFileUnlocked(const std::string_view name) const noexcept {
        const auto it = fileLookup.find(name);
        return it != fileLookup.end() ? it->second : nullptr;
    }
} // namespace Rux
