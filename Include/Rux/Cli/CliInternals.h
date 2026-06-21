// Copyright (c) Rux contributors.
// SPDX-License-Identifier: MIT

// File contains functions and values that were inside the Anonymous namespace.
// I gave the functions and values a proper namespace so they can be used
// elsewhere. I also Refactored some of the code to use standard library
// features.
//
// These utilities are primarily used by:
//   - BuildCmd.cpp (build statistics, manifest loading)
//   - CheckCmd.cpp (package resolution, dependency handling)
//   - InstallCmd.cpp (registry operations, git helpers)
//   - PackageCmd.cpp (manifest management)
//   - UtilityCmd.cpp (package listing, info display)

#pragma once

#include "Rux/Asm.h"
#include "Rux/Ast.h"
#include "Rux/Cli/Cli.h"
#include "Rux/Lexer.h"
#include "Rux/Manifest.h"
#include "Rux/Parser.h"
#include "Rux/Platform/Defines.h"
#include "Rux/Platform/Host.h"
#include "Rux/Version.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>
#include <optional>
#include <print>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#if RUX_OS_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#endif

#if RUX_OS_WINDOWS
    #include <psapi.h>
    #include <winhttp.h>
#else
    #include <sys/resource.h>
    #include <sys/wait.h>
    #include <unistd.h>
#endif

namespace Rux::Misc {

using namespace Platform;

inline constexpr std::string_view kRegistryUrl =
    "https://raw.githubusercontent.com/rux-lang/Registry/refs/heads/main/Packages.json";

struct BuildStats {
    std::chrono::milliseconds lexing{0};
    std::chrono::milliseconds parsing{0};
    std::chrono::milliseconds semantic{0};
    std::chrono::milliseconds hir{0};
    std::chrono::milliseconds lir{0};
    std::chrono::milliseconds codegen{0};
    std::chrono::milliseconds linking{0};
    std::chrono::milliseconds total{0};
    double totalSeconds = 0.0;
    std::size_t localFiles = 0;
    std::size_t dependencyFiles = 0;
    std::size_t localLines = 0;
    std::size_t dependencyLines = 0;
    std::size_t localTokens = 0;
    std::size_t dependencyTokens = 0;
    std::uintmax_t localSourceSize = 0;
    std::uintmax_t dependencySourceSize = 0;
    std::uintmax_t executableSize = 0;
    std::uintmax_t peakMemoryBytes = 0;
};

inline std::chrono::milliseconds
ElapsedMs(std::chrono::steady_clock::time_point const start,
          std::chrono::steady_clock::time_point const end = std::chrono::steady_clock::now()) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
}

inline double
ElapsedSeconds(std::chrono::steady_clock::time_point const start,
               std::chrono::steady_clock::time_point const end = std::chrono::steady_clock::now()) {
    return std::chrono::duration<double>(end - start).count();
}

inline std::size_t CountLines(std::string_view const source) {
    if (source.empty()) {
        return 0;
    }
    return std::ranges::count(source, '\n') + (source.back() != '\n' ? 1 : 0);
}

inline std::size_t CountTokens(LexerResult const &result) {
    if (result.tokens.empty()) {
        return 0;
    }
    return result.tokens.size() - static_cast<std::size_t>(result.tokens.back().IsEof());
}

inline std::string FormatNumber(std::uintmax_t value) {
    std::string digits = std::to_string(value);
    for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>(digits.size()) - 3; i > 0; i -= 3) {
        digits.insert(static_cast<std::size_t>(i), 1, ',');
    }
    return digits;
}

inline std::string FormatDecimal(double const value, int const decimals) {
    std::string text = std::format("{:.{}f}", value, decimals);
    if (text.find('.') != std::string::npos) {
        while (!text.empty() && text.back() == '0') {
            text.pop_back();
        }
        if (!text.empty() && text.back() == '.') {
            text.pop_back();
        }
    }
    return text;
}

inline std::string FormatCompactNumber(double const value) {
    double const absValue = std::fabs(value);
    if (absValue >= 1'000'000.0) {
        return FormatDecimal(value / 1'000'000.0, 1) + "M";
    }
    if (absValue >= 1'000.0) {
        return FormatDecimal(value / 1'000.0, 1) + "K";
    }
    return FormatNumber(std::llround(value));
}

inline std::string FormatTokenThroughput(double const tokensPerSecond) {
    double const absValue = std::fabs(tokensPerSecond);
    if (absValue >= 1'000'000.0) {
        return FormatDecimal(tokensPerSecond / 1'000'000.0, 1) + " M tok/s";
    }
    if (absValue >= 1'000.0) {
        return FormatDecimal(tokensPerSecond / 1'000.0, 1) + " K tok/s";
    }
    return FormatNumber(std::llround(tokensPerSecond)) + " tok/s";
}

inline std::string FormatSize(std::uintmax_t const bytes) {
    double const kb = static_cast<double>(bytes) / 1024.0;
    if (kb < 1024.0) {
        return FormatNumber(std::llround(kb)) + " KB";
    }
    return FormatDecimal(kb / 1024.0, 2) + " MB";
}

inline std::string TargetName() {
    if constexpr (HostArch == Arch::Unknown) {
        return std::string{ToString(HostOS)};
    }
    return std::format("{} {}", ToString(HostOS), ToString(HostArch));
}

inline std::string HostTargetTriple() {
    auto triple = std::format("{}-{}", ToString(HostOS), ToString(HostArch));
    std::ranges::transform(triple, std::begin(triple), [](unsigned char const c) {
        return static_cast<char>(std::tolower(c));
    });
    return triple;
}

inline bool IsSupportedTargetTriple(std::string_view const target) {
    constexpr std::array supported_targets{"linux-x64",     "windows-x64",   "macos-x64",
                                           "macos-aarch64", "freebsd-x64",   "openbsd-x64",
                                           "netbsd-x64",    "dragonfly-x64", "illumos-x64"};
    return std::ranges::contains(supported_targets, target);
}

inline std::string_view TargetOsName(std::string_view const target) {
    auto const dash_pos = target.find('-');
    if (dash_pos == std::string_view::npos) {
        return "";
    }

    auto const os_prefix = target.substr(0, dash_pos);

    if (os_prefix == "linux") {
        return "Linux";
    }
    if (os_prefix == "windows") {
        return "Windows";
    }
    if (os_prefix == "macos") {
        return "macOS";
    }
    if (os_prefix == "illumos") {
        return "Illumos";
    }
    if (os_prefix == "freebsd" or os_prefix == "openbsd" or os_prefix == "netbsd" or
        os_prefix == "dragonfly") {
        return "BSD";
    }
    return "";
}

inline bool DeclMatchesTarget(Decl const &decl, std::string_view const target) {
    if (decl.targetOs.empty()) {
        return true;
    }
    std::string_view const targetOs = TargetOsName(target);
    return std::ranges::equal(decl.targetOs, targetOs, [](char const a, char const b) {
        return std::tolower(static_cast<unsigned char>(a)) ==
               std::tolower(static_cast<unsigned char>(b));
    });
}

inline void PruneDeclsForTarget(std::vector<DeclPtr> &decls, std::string_view const target);

inline void PruneDeclForTarget(Decl &decl, std::string_view const target) {
    if (auto *module = dynamic_cast<ModuleDecl *>(&decl)) {
        PruneDeclsForTarget(module->items, target);
    }
    else if (auto *block = dynamic_cast<ExternBlockDecl *>(&decl)) {
        PruneDeclsForTarget(block->items, target);
    }
}

inline void PruneDeclsForTarget(std::vector<DeclPtr> &decls, std::string_view const target) {
    std::erase_if(decls,
                  [&](DeclPtr const &decl) { return !decl or !DeclMatchesTarget(*decl, target); });
    for (auto const &decl : decls) {
        PruneDeclForTarget(*decl, target);
    }
}

inline void PruneModuleForTarget(Module &module, std::string_view const target) {
    PruneDeclsForTarget(module.items, target);
}

inline std::string DependencyPackageName(Dependency const &dep) {
    return dep.package.empty() ? dep.name : dep.package;
}

inline std::string JsonLookupString(std::string_view json, std::string_view key) {
    std::string const needle = std::format("\"{}\"", key);
    std::size_t pos = 0;

    while ((pos = json.find(needle, pos)) != std::string_view::npos) {
        std::size_t i = pos + needle.size();

        while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i]))) {
            ++i;
        }
        if (i >= json.size() or json[i] != ':') {
            pos = i;
            continue;
        }
        ++i;

        while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i]))) {
            ++i;
        }
        if (i >= json.size() or json[i] != '"') {
            pos = i;
            continue;
        }
        ++i;

        auto const end = json.find('"', i);
        if (end == std::string_view::npos) {
            break;
        }
        return std::string(json.substr(i, end - i));
    }
    return {};
}

inline std::optional<std::filesystem::path> RequireManifest() {
    auto path = Manifest::Find();
    if (!path) {
        std::println(stderr, "error: could not find 'Rux.toml' in '{}' or any parent directory",
                     std::filesystem::current_path().string());
    }
    return path;
}

inline std::optional<Manifest> LoadManifest(std::filesystem::path const &path) {
    auto m = Manifest::Load(path);
    if (!m) {
        std::println(stderr, "error: failed to parse '{}'", path.string());
    }
    return m;
}

inline std::filesystem::path ResolveBuildOutputDir(std::filesystem::path const &root,
                                                   Manifest const &manifest,
                                                   std::string_view profileName) {
    std::filesystem::path output = manifest.build.output.empty()
                                     ? std::filesystem::path("Bin")
                                     : std::filesystem::path(manifest.build.output);
    if (output.is_relative()) {
        output = root / output;
    }
    return (output / profileName).lexically_normal();
}

inline void PrintBuildStats(std::filesystem::path const &exePath, std::string_view profileName,
                            BuildStats const &stats) {
    double const seconds = stats.totalSeconds;
    std::size_t const totalFiles = stats.localFiles + stats.dependencyFiles;
    std::size_t const totalLines = stats.localLines + stats.dependencyLines;
    std::size_t const totalTokens = stats.localTokens + stats.dependencyTokens;
    std::uintmax_t const totalSourceSize = stats.localSourceSize + stats.dependencySourceSize;

    double const tokenThroughput = seconds > 0.0 ? static_cast<double>(totalTokens) / seconds : 0.0;
    double const compileSpeed = seconds > 0.0 ? static_cast<double>(totalLines) / seconds : 0.0;
    double const throughput =
        seconds > 0.0 ? static_cast<double>(totalSourceSize) / 1024.0 / 1024.0 / seconds : 0.0;

    std::println("Rux Compiler {}\n"
                 "Target: {}\n"
                 "Mode: {}\n\n"
                 "Build finished successfully.\n\n"
                 "Total build time:            {} ms\n"
                 "  Lexing:                    {} ms\n"
                 "  Parsing:                   {} ms\n"
                 "  Semantic:                  {} ms\n"
                 "  HIR:                       {} ms\n"
                 "  LIR:                       {} ms\n"
                 "  Codegen:                   {} ms\n"
                 "  Linking:                   {} ms\n\n"
                 "Total files:                 {}\n"
                 "  Local files:               {}\n"
                 "  Dependency files:          {}\n\n"
                 "Total lines:                 {}\n"
                 "  Local lines:               {}\n"
                 "  Dependency lines:          {}\n\n"
                 "Total tokens:                {}\n"
                 "  Local tokens:              {}\n"
                 "  Dependency tokens:         {}\n\n"
                 "Total source size:           {}\n"
                 "  Local source size:         {}\n"
                 "  Dependency source size:    {}\n\n"
                 "Output:\n"
                 "  Executable:                {}\n"
                 "  Executable size:           {}\n"
                 "  Peak memory:               {}\n\n"
                 "Performance:\n"
                 "  Compile speed:             {} LOC/s\n"
                 "  Token throughput:          {}\n"
                 "  Total throughput:          {} MB/s",
                 RUX_VERSION, TargetName(), profileName, stats.total.count(), stats.lexing.count(),
                 stats.parsing.count(), stats.semantic.count(), stats.hir.count(),
                 stats.lir.count(), stats.codegen.count(), stats.linking.count(),
                 FormatNumber(totalFiles), FormatNumber(stats.localFiles),
                 FormatNumber(stats.dependencyFiles), FormatNumber(totalLines),
                 FormatNumber(stats.localLines), FormatNumber(stats.dependencyLines),
                 FormatNumber(totalTokens), FormatNumber(stats.localTokens),
                 FormatNumber(stats.dependencyTokens), FormatSize(totalSourceSize),
                 FormatSize(stats.localSourceSize), FormatSize(stats.dependencySourceSize),
                 exePath.filename().string(), FormatSize(stats.executableSize),
                 FormatSize(stats.peakMemoryBytes), FormatNumber(std::llround(compileSpeed)),
                 FormatTokenThroughput(tokenThroughput), FormatDecimal(throughput, 2));
}

inline void PrintBuildSummary(std::filesystem::path const &exePath, std::string_view profileName,
                              BuildStats const &stats) {
    std::size_t const totalFiles = stats.localFiles + stats.dependencyFiles;
    std::size_t const totalLines = stats.localLines + stats.dependencyLines;
    std::size_t const totalTokens = stats.localTokens + stats.dependencyTokens;
    double const compileSpeed =
        stats.totalSeconds > 0.0 ? static_cast<double>(totalLines) / stats.totalSeconds : 0.0;

    std::println("Built `{}` [{}] in {} ms", profileName, exePath.string(), stats.total.count());
    std::println("{} files | {} LOC | {} tokens | {} LOC/s | {} {}", FormatNumber(totalFiles),
                 FormatNumber(totalLines), FormatCompactNumber(static_cast<double>(totalTokens)),
                 FormatCompactNumber(compileSpeed), exePath.filename().string(),
                 FormatSize(stats.executableSize));
}

#if RUX_OS_WINDOWS

inline std::uintmax_t PeakMemoryBytes() noexcept {
    PROCESS_MEMORY_COUNTERS counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
        return static_cast<std::uintmax_t>(counters.PeakWorkingSetSize);
    }
    return 0;
}

inline std::filesystem::path RegistryPackagesDir() {
    wchar_t buf[MAX_PATH]{};
    GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
    return std::filesystem::path(buf) / "Rux" / "Packages";
}

inline std::optional<std::string> FetchUrl(std::string const &url) {
    std::string cmd = std::format("curl -s \"{}\"", url);
    std::array<char, 128> buffer;
    std::string result;

    FILE *pipe = _popen(cmd.c_str(), "r");
    if (!pipe) {
        return std::nullopt;
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result += buffer.data();
    }
    _pclose(pipe);
    return result;
}

inline bool ExecuteGitCommand(std::wstring const &cmd) {
    std::vector<wchar_t> mutableCmd(cmd.begin(), cmd.end());
    mutableCmd.push_back(L'\0');

    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};

    if (!CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr,
                        &si, &pi)) {
        return false;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exitCode == 0;
}

inline bool GitClone(std::string const &repoUrl, std::filesystem::path const &dest,
                     bool const devBranch) {
    std::wstring const wRepoUrl(repoUrl.begin(), repoUrl.end());
    std::wstring cmd =
        devBranch ? std::format(L"git clone --branch dev {} \"{}\"", wRepoUrl, dest.wstring())
                  : std::format(L"git clone {} \"{}\"", wRepoUrl, dest.wstring());
    return ExecuteGitCommand(cmd);
}

inline bool GitPull(std::filesystem::path const &repoDir) {
    std::wstring cmd = std::format(L"git -C \"{}\" pull", repoDir.wstring());
    return ExecuteGitCommand(cmd);
}

#else

inline std::uintmax_t PeakMemoryBytes() noexcept {
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        constexpr std::uintmax_t unitMultiplier = (HostOS == OS::MacOS) ? 1ULL : 1024ULL;
        return static_cast<std::uintmax_t>(usage.ru_maxrss) * unitMultiplier;
    }
    return 0;
}

inline std::filesystem::path RegistryPackagesDir() {
    char const *home = getenv("HOME");
    return std::filesystem::path(home ? home : "/tmp") / ".rux" / "packages";
}

inline std::string ShellQuote(std::string_view const value) {
    std::string quoted;
    quoted.reserve(value.size() + (std::ranges::count(value, '\'') * 3) + 2);
    quoted += '\'';
    for (char const ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
        }
        else {
            quoted += ch;
        }
    }
    quoted += '\'';
    return quoted;
}

inline std::optional<std::string> RunCommandCapture(std::string const &command) {
    FILE *pipe = ::popen(command.c_str(), "r");
    if (!pipe) {
        return std::nullopt;
    }

    std::string output;
    std::array<char, 4096> buffer{};
    while (::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
        output.append(buffer.data());
    }

    if (int const status = ::pclose(pipe); !WIFEXITED(status) or WEXITSTATUS(status) != 0) {
        return std::nullopt;
    }
    return output;
}

inline std::optional<std::string> FetchUrl(std::string const &url) {
    std::string const quotedUrl = ShellQuote(url);
    if (auto body = RunCommandCapture("curl -fsSL " + quotedUrl)) {
        return body;
    }
    return RunCommandCapture("wget -qO- " + quotedUrl);
}

inline bool GitClone(std::string const &repoUrl, std::filesystem::path const &dest,
                     bool const devBranch) {
    std::string const quotedUrl = ShellQuote(repoUrl);
    std::string const quotedDest = ShellQuote(dest.string());
    std::string const cmd =
        std::format("git clone {} {} {}", devBranch ? "-b dev" : "", quotedUrl, quotedDest);
    return std::system(cmd.c_str()) == 0;
}

inline bool GitPull(std::filesystem::path const &repoDir) {
    std::string const quotedDir = ShellQuote(repoDir.string());
    std::string const cmd = std::format("git -C {} pull", quotedDir);
    return std::system(cmd.c_str()) == 0;
}

#endif // RUX_OS_WINDOWS

} // namespace Rux::Misc

namespace Rux {

inline GlobalOptions Cli::ParseGlobalOptions(std::span<std::string_view const> args) {
    GlobalOptions opts;
    for (std::size_t i = 0; i < args.size(); ++i) {
        std::string_view arg = args[i];
        if (arg == "-q" or arg == "--quiet") {
            opts.quiet = true;
            continue;
        }
        if (arg == "-v" or arg == "--verbose") {
            opts.verbose = true;
            continue;
        }
        if (arg == "--color") {
            if (i + 1 < args.size()) {
                if (std::string_view const val = args[++i]; val == "on") {
                    opts.color = ColorMode::On;
                }
                else if (val == "off") {
                    opts.color = ColorMode::Off;
                }
                else {
                    opts.color = ColorMode::Auto;
                }
            }
            continue;
        }
        if (arg.starts_with("--color=")) {
            if (std::string_view const val = arg.substr(8); val == "on") {
                opts.color = ColorMode::On;
            }
            else if (val == "off") {
                opts.color = ColorMode::Off;
            }
            else {
                opts.color = ColorMode::Auto;
            }
        }
    }
    return opts;
}

} // namespace Rux
