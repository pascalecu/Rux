/*
    Rux Compiler
    Copyright © 2026 Rux Contributors
    Licensed under the MIT License
*/

#include <cassert>
#include <cstddef>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>

#include "./Utils.h"

namespace Rux {
    using namespace Detail;

    class BinaryReader {
    public:
        using Byte = std::byte;

        explicit BinaryReader(std::span<const Byte> data) noexcept
            : data_(data) {
        }

        [[nodiscard]] size_t Size() const noexcept {
            return data_.size();
        }

        [[nodiscard]] size_t Remaining(size_t off) const noexcept {
            return (off <= data_.size()) ? data_.size() - off : 0;
        }

        [[nodiscard]] bool Contains(size_t off, size_t n) const noexcept {
            return off <= data_.size() && n <= data_.size() - off;
        }

        [[nodiscard]] std::span<const Byte> Span() const noexcept {
            return data_;
        }

        [[nodiscard]] std::span<const Byte> Subspan(size_t off, size_t n) const {
            assert(Contains(off, n));
            return data_.subspan(off, n);
        }

        [[nodiscard]] BinaryReader SubReader(size_t off) const {
            assert(off <= data_.size());
            return BinaryReader(data_.subspan(off));
        }

        [[nodiscard]] std::optional<std::span<const Byte>> ReadBytes(size_t off, size_t n) const {
            if (!Contains(off, n)) return std::nullopt;
            return data_.subspan(off, n);
        }

        template <EndianConvertible T>
        [[nodiscard]] std::optional<T> Read(size_t off) const {
            if (!Contains(off, sizeof(T))) return std::nullopt;

            T v{};
            std::memcpy(&v, data_.data() + off, sizeof(T));

            return ByteSwapIfBigEndian(v);
        }

        template <TriviallySerializable T>
        [[nodiscard]] std::optional<T> ReadStruct(size_t off) const {
            if (!Contains(off, sizeof(T))) return std::nullopt;

            T v{};
            std::memcpy(&v, data_.data() + off, sizeof(T));
            return v;
        }

        template <TriviallySerializable T>
        [[nodiscard]] std::optional<std::span<const T>> ReadSpan(size_t off, size_t count) const {
            const size_t total = sizeof(T) * count;
            if (!Contains(off, total)) return std::nullopt;

            // NOTE: assumes T alignment is safe under TriviallySerializable
            auto ptr = reinterpret_cast<const T*>(data_.data() + off);
            return std::span<const T>{ptr, count};
        }

        [[nodiscard]] std::optional<std::string_view> ReadString(size_t off, size_t len) const {
            if (!Contains(off, len)) return std::nullopt;

            const char* ptr = reinterpret_cast<const char*>(data_.data() + off);

            return std::string_view(ptr, len);
        }

        [[nodiscard]] std::optional<std::string_view> ReadCString(size_t off) const {
            if (off >= data_.size()) return std::nullopt;

            const char* begin = reinterpret_cast<const char*>(data_.data() + off);

            const size_t maxLen = data_.size() - off;

            const void* found = std::memchr(begin, 0, maxLen);

            if (!found) return std::nullopt;

            const auto* end = static_cast<const Byte*>(found);

            const size_t len = static_cast<size_t>(end - data_.data() - off);

            return std::string_view(begin, len);
        }

        template <TriviallySerializable T>
        [[nodiscard]] std::optional<std::span<const T>> AsArray(size_t off) const {
            if (!Contains(off, sizeof(T))) return std::nullopt;

            const size_t count = (data_.size() - off) / sizeof(T);

            auto ptr = reinterpret_cast<const T*>(data_.data() + off);

            return std::span<const T>{ptr, count};
        }

        template <EndianConvertible T>
        [[nodiscard]] std::optional<T> ReadAndAdvance(size_t& off) const {
            auto v = Read<T>(off);
            if (v) off += sizeof(T);
            return v;
        }

        template <TriviallySerializable T>
        [[nodiscard]] std::optional<T> ReadStructAndAdvance(size_t& off) const {
            auto v = ReadStruct<T>(off);
            if (v) off += sizeof(T);
            return v;
        }

    private:
        std::span<const Byte> data_;
    };

} // namespace Rux
