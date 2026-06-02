/*
    Rux Compiler
    Copyright © 2026 Rux Contributors
    Licensed under the MIT License
*/

#include <concepts>
#include <cstdint>
#include <cstring>
#include <ranges>
#include <span>
#include <vector>

#include "./Utils.h"

namespace Rux {
    using namespace Detail;
    namespace rng = std::ranges;

    class BinaryWriter {
    public:
        using Byte = std::byte;

        [[nodiscard]]
        size_t Position() const noexcept {
            return pos_;
        }

        [[nodiscard]]
        size_t Size() const noexcept {
            return buf_.size();
        }

        [[nodiscard]]
        bool Empty() const noexcept {
            return buf_.empty();
        }

        [[nodiscard]]
        std::span<const Byte> Span() const noexcept {
            return buf_;
        }

        void WriteBytes(std::span<const Byte> b) {
            Ensure(b.size());
            std::memcpy(buf_.data() + pos_, b.data(), b.size());
            pos_ += b.size();
        }

        void WriteBytes(std::span<const std::uint8_t> bytes) {
            WriteBytes(std::as_bytes(bytes));
        }

        template <rng::contiguous_range R>
            requires std::same_as<std::remove_cvref_t<rng::range_value_t<R>>, Byte>
        void WriteBytes(const R& r) {
            WriteBytes(std::span{rng::data(r), rng::size(r)});
        }

        void Write(Byte b) {
            WriteBytes(std::span{&b, 1});
        }

        void Write(std::uint8_t v) {
            Write(static_cast<Byte>(v));
        }

        template <EndianConvertible T>
        void Write(T v) {
            v = ByteSwapIfBigEndian(v);
            std::array<Byte, sizeof(T)> bytes;
            std::memcpy(bytes.data(), &v, sizeof(T));

            WriteBytes(bytes);
        }

        void WriteStruct(const TriviallySerializable auto& v) {
            WriteBytes(std::as_bytes(std::span{&v, 1}));
        }

        template <TriviallySerializable T>
        void WriteRawSpan(std::span<const T> v) {
            WriteBytes(std::as_bytes(v));
        }

        void WriteString(std::string_view s, bool nullTerminated = false) {
            WriteBytes(Bytes::FromString(s));
            if (nullTerminated) Write(Byte{0});
        }

        void WriteCString(std::string_view s) {
            WriteString(s, true);
        }

        void WriteName8(std::string_view s) {
            std::array<Byte, 8> buf{};
            const size_t n = std::min(s.size(), buf.size());
            std::memcpy(buf.data(), s.data(), n);
            WriteBytes(buf);
        }

        void WriteZeros(size_t n) {
            Ensure(n);
            std::memset(buf_.data() + pos_, 0, n);
            pos_ += n;
        }

        void Skip(size_t n) {
            Ensure(n);
            pos_ += n;
        }

        [[nodiscard]]
        size_t AlignTo(size_t alignment, Byte fill = Byte{0}) {
            const size_t start = pos_;
            const size_t aligned = AlignUp(pos_, alignment);

            const size_t pad = aligned - pos_;
            Ensure(pad);

            std::memset(buf_.data() + pos_, static_cast<unsigned char>(fill), pad);

            pos_ = aligned;
            return start;
        }

        template <EndianConvertible T>
        void Patch(size_t off, T v) {
            assert(off + sizeof(T) <= buf_.size());
            v = ByteSwapIfBigEndian(v);
            std::memcpy(buf_.data() + off, &v, sizeof(T));
        }

        template <TriviallySerializable T>
        void PatchStruct(size_t off, const T& v) {
            assert(off + sizeof(T) <= buf_.size());
            std::memcpy(buf_.data() + off, &v, sizeof(T));
        }

        template <EndianConvertible T>
        [[nodiscard]]
        size_t Reserve() {
            size_t off = pos_;
            Skip(sizeof(T));
            return off;
        }

        [[nodiscard]]
        size_t ReserveBytes(size_t n) {
            size_t off = pos_;
            Skip(n);
            return off;
        }

    private:
        std::vector<Byte> buf_;
        size_t pos_ = 0;

        void Ensure(size_t n) {
            if (pos_ + n > buf_.size()) {
                size_t newSize = buf_.empty() ? n : buf_.size();
                while (newSize < pos_ + n)
                    newSize *= 2;

                buf_.resize(newSize);
            }
        }
    };
} // namespace Rux
