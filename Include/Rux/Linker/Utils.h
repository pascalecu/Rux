
/*
    Rux Compiler
    Copyright © 2026 Rux Contributors
    Licensed under the MIT License
*/

#include "Rux/Platform/Host.h"

#include <cassert>
#include <concepts>
#include <span>

namespace Rux {

    namespace Detail {
        template <typename T, typename... U>
        concept NoneOf = (!std::same_as<T, U> && ...);

        template <typename T>
        concept EndianConvertible =
            std::integral<T> && NoneOf<std::remove_cv_t<T>, bool, char, char8_t, char16_t, char32_t, wchar_t>;

        template <EndianConvertible T>
        [[nodiscard]]
        constexpr T ByteSwapIfBigEndian(T value) {
            using namespace Platform;
            if constexpr (HostEndianness == Endian::Big) {
                return std::byteswap(value);
            }

            return value;
        }

        [[nodiscard]]
        constexpr size_t AlignUp(size_t value, size_t alignment) {
            assert(alignment != 0);

            if (std::has_single_bit(alignment)) {
                return (value + alignment - 1) & ~(alignment - 1);
            }

            return ((value + alignment - 1) / alignment) * alignment;
        }

        template <typename T>
        concept TriviallySerializable = std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>;
    } // namespace Detail

    namespace Bytes {
        constexpr std::byte B(std::integral auto v) noexcept {
            return std::byte{static_cast<std::uint8_t>(v)};
        }

        template <std::integral... Ts>
        constexpr std::array<std::byte, sizeof...(Ts)> Pack(Ts... v) noexcept {
            return {std::byte{static_cast<std::uint8_t>(v)}...};
        }

        inline std::span<const std::byte> FromString(std::string_view s) noexcept {
            return std::as_bytes(std::span{s});
        }

        template <std::size_t N>
        constexpr std::span<const std::byte, N> FromArray(const std::array<std::byte, N>& a) noexcept {
            return std::span{a};
        }

        template <std::integral... Ts>
        constexpr auto PackSpan(Ts... v) {
            return Pack(v...); // usable directly as span
        }
    } // namespace Bytes
} // namespace Rux
