// Copyright (c) Rux contributors.
// SPDX-License-Identifier: MIT

#include "Rux/Cli/Cli.h"          // for GlobalOptions, Cli
#include "Rux/Cli/CliInternals.h" // for RequireManifest, ResolveBuildOutputDir
#include "Rux/Manifest.h"         // for Manifest, Package
#include "Rux/Platform/Defines.h" // for RUX_OS_WINDOWS

#if RUX_OS_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#else
    #include <sys/wait.h> // for pid_t
    #include <unistd.h>   // for waitpid
#endif

#include <algorithm>    // for __sort, sort
#include <cstdio>       // for stderr
#include <cstdlib>      // for WEXITSTATUS, WIFEXITED
#include <filesystem>   // for path, current_path, directory_iterator, operator/, dir...
#include <optional>     // for optional
#include <print>        // for print
#include <span>         // for span
#include <string>       // for basic_string, char_traits, operator==, string, operator+
#include <string_view>  // for basic_string_view, operator==, string_view
#include <system_error> // for error_code
#include <vector>       // for vector

namespace Rux {
int Cli::RunTest(std::span<std::string_view const> args, GlobalOptions const &opts) {
    bool isRelease = false;
    for (auto const &arg : args) {
        if (arg == "--release") {
            isRelease = true;
        }
        else if (arg == "-h" or arg == "--help") {
            PrintHelpFor("test");
            return 0;
        }
        else {
            PrintUnknownOption(arg, "test");
            return 1;
        }
    }

    auto const manifestPath = Manifest::Find();
    std::filesystem::path const projectRoot =
        manifestPath ? manifestPath->parent_path() : std::filesystem::current_path();
    std::filesystem::path const testsDir = projectRoot / "Tests";

    if (!manifestPath and !std::filesystem::exists(testsDir)) {
        Misc::RequireManifest();
        return 1;
    }

    // Identify test packages
    std::vector<std::filesystem::path> testPackages;
    std::error_code ec;
    if (std::filesystem::exists(testsDir, ec)) {
        for (auto const &entry : std::filesystem::directory_iterator(testsDir, ec)) {
            if (!entry.is_directory()) {
                continue;
            }

            auto pkgManifest = Manifest::Load(entry.path() / "Rux.toml");
            if (pkgManifest and
                (pkgManifest->package.type == "bin" or pkgManifest->package.type == "Bin")) {
                testPackages.push_back(entry.path());
            }
        }
        std::ranges::sort(testPackages);
    }

    if (testPackages.empty()) {
        if (!opts.quiet) {
            std::print("  No test packages found in Tests/.\n");
        }
        return 0;
    }

    // Helper to run a single test package
    auto runOne = [&](std::filesystem::path const &pkgDir) -> int {
        auto pkgManifest = Manifest::Load(pkgDir / "Rux.toml");
        if (!pkgManifest) {
            return -1;
        }

        auto const savedCwd = std::filesystem::current_path();
        std::filesystem::current_path(pkgDir, ec);

        GlobalOptions buildOpts = opts;
        buildOpts.quiet = true;
        int const buildRc =
            RunBuild(isRelease ? std::vector<std::string_view>{"--release", "--quiet"}
                               : std::vector<std::string_view>{"--quiet"},
                     buildOpts);
        std::filesystem::current_path(savedCwd, ec);

        if (buildRc != 0) {
            return -1;
        }

        auto const binDir =
            Misc::ResolveBuildOutputDir(pkgDir, *pkgManifest, isRelease ? "Release" : "Debug");
        std::string exePath =
            (binDir / (pkgManifest->package.name + (RUX_OS_WINDOWS ? ".exe" : ""))).string();

        if (opts.verbose) {
            std::print("     Running `{}`\n", exePath);
        }

#if RUX_OS_WINDOWS
        STARTUPINFOA si{sizeof(STARTUPINFOA)};
        PROCESS_INFORMATION pi{};
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

        if (!CreateProcessA(nullptr, exePath.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr,
                            &si, &pi))
            return -1;
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return static_cast<int>(exitCode);
#else
        pid_t pid = fork();
        if (pid < 0) return -1;
        if (pid == 0) {
            execl(exePath.c_str(), exePath.c_str(), nullptr);
            _exit(127);
        }
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
#endif
    };

    int passed = 0, failed = 0;
    for (auto const &pkgDir : testPackages) {
        std::string const label = pkgDir.filename().string();
        if (!opts.quiet) {
            std::print("     Running test package: {}\n", label);
        }

        int const rc = runOne(pkgDir);
        if (rc == 0) {
            ++passed;
            if (!opts.quiet) {
                std::print("   PASS: {}\n", label);
            }
        }
        else {
            ++failed;
            std::print(stderr, "   FAIL: {} (exit {})\n", label,
                       rc == -1 ? "build/launch error" : std::to_string(rc));
        }
    }

    if (!opts.quiet or failed > 0) {
        std::print("{}: {} passed, {} failed, {} total\n", failed == 0 ? "ok" : "FAILED", passed,
                   failed, passed + failed);
    }
    return failed == 0 ? 0 : 1;
}
} // namespace Rux
