/*
    Rux Compiler
    Copyright © 2026 Rux Contributors
    Licensed under the MIT License
*/

#pragma once

#include "Platform/Host.h"
#include "Platform/Types.h"

namespace Rux::Platform {
    inline constexpr bool IsWindows() noexcept {
        return HostOS == OS::Windows;
    }
    inline constexpr bool IsLinux() noexcept {
        return HostOS == OS::Linux;
    }
    inline constexpr bool IsMacOS() noexcept {
        return HostOS == OS::MacOS;
    }
    inline constexpr bool IsFreeBSD() noexcept {
        return HostOS == OS::FreeBSD;
    }
    inline constexpr bool IsOpenBSD() noexcept {
        return HostOS == OS::OpenBSD;
    }
    inline constexpr bool IsNetBSD() noexcept {
        return HostOS == OS::NetBSD;
    }
    inline constexpr bool IsDragonFlyBSD() noexcept {
        return HostOS == OS::DragonFlyBSD;
    }
    inline constexpr bool IsSolaris() noexcept {
        return HostOS == OS::Solaris;
    }
    inline constexpr bool IsIllumos() noexcept {
        return HostOS == OS::Illumos;
    }
    inline constexpr bool IsBSD() noexcept {
        return IsFreeBSD() || IsOpenBSD() || IsNetBSD() || IsDragonFlyBSD();
    }
    inline constexpr bool IsSunOS() noexcept {
        return IsSolaris() || IsIllumos();
    }
    inline constexpr bool IsUnixLike() noexcept {
        return IsLinux() || IsMacOS() || IsBSD() || IsSunOS();
    }

    inline constexpr bool IsX64() noexcept {
        return HostArch == Arch::X86_64;
    }
    inline constexpr bool IsX86() noexcept {
        return HostArch == Arch::X86_32;
    }
    inline constexpr bool IsARM64() noexcept {
        return HostArch == Arch::ARM64;
    }
    inline constexpr bool IsARM32() noexcept {
        return HostArch == Arch::ARM32;
    }
    inline constexpr bool IsRISCV64() noexcept {
        return HostArch == Arch::RISCV64;
    }

    inline constexpr bool Is64Bit() noexcept {
        return HostPointerSize == 8;
    }

    inline constexpr bool Is32Bit() noexcept {
        return HostPointerSize == 4;
    }

    inline constexpr bool IsMSVC() noexcept {
        return HostCompiler == Compiler::MSVC;
    }
    inline constexpr bool IsClang() noexcept {
        return HostCompiler == Compiler::Clang;
    }
    inline constexpr bool IsGCC() noexcept {
        return HostCompiler == Compiler::GCC;
    }

    inline constexpr bool IsDebug() noexcept {
        return HostBuildMode == BuildMode::Debug;
    }
    inline constexpr bool IsRelease() noexcept {
        return HostBuildMode == BuildMode::Release;
    }

    inline constexpr ABI GetABI() noexcept {
        return HostABI;
    }
    inline constexpr CallingConv GetCallingConv() noexcept {
        return HostCC;
    }

} // namespace Rux::Platform
