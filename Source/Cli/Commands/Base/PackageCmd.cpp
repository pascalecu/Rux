// Copyright (c) Rux contributors.
// SPDX-License-Identifier: MIT

#include "Rux/Cli/Cli.h"          // for GlobalOptions, Cli
#include "Rux/Cli/CliInternals.h" // for LoadManifest, RequireManifest, FetchUrl, JsonLookupString
#include "Rux/Manifest.h"         // for Manifest, Package, ParsePackageSpec
#include "Rux/Package.h"          // for PackageType, ScaffoldPackage
#include "Rux/Platform/Defines.h" // for RUX_OS_WINDOWS
#include "Rux/Platform/Types.h"   // for Platform

#include <algorithm>    // for sort
#include <cstdio>       // for stderr, size_t
#include <filesystem>   // for path, operator/, current_path, directory_iterator, exists
#include <optional>     // for optional
#include <print>        // for print
#include <span>         // for span
#include <string>       // for basic_string, char_traits, string, operator==, to_string
#include <string_view>  // for basic_string_view, operator==, string_view
#include <system_error> // for error_code
#include <utility>      // for get
#include <vector>       // for vector

/*
 * This is separate from the other ifdef because otherwise clang-format attempts
 * to change the order, which makes MSVC cry.
 */

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
#else
    #include <fcntl.h>    // for O_RDONLY, open
    #include <sys/wait.h> // for waitpid
    #include <unistd.h>   // for _exit, close, dup2, execv, fork
#endif

using namespace Rux;
using namespace Platform;
using namespace Misc;

int Cli::RunAdd(std::span<std::string_view const> args, GlobalOptions const &opts) {
    std::string_view spec;
    std::string_view pathArg;

    for (std::size_t i = 0; i < args.size(); ++i) {
        std::string_view arg = args[i];
        if (arg == "-h" or arg == "--help") {
            PrintHelpFor("add");
            return 0;
        }
        if (arg == "--path") {
            if (++i >= args.size()) {
                std::print(stderr, "error: '--path' requires an argument\n");
                return 1;
            }
            pathArg = args[i];
        }
        else if (!args[i].starts_with('-') and spec.empty()) {
            spec = args[i];
        }
        else {
            PrintUnknownOption(args[i], "add");
            return 1;
        }
    }

    if (spec.empty()) {
        std::print(stderr, "error: missing package name\n\n");
        PrintHelpFor("add");
        return 1;
    }

    auto manifestPath = RequireManifest();
    if (!manifestPath) {
        return 1;
    }

    auto manifest = LoadManifest(*manifestPath);
    if (!manifest) {
        return 1;
    }

    auto [pkgName, pkgVersion] = ParsePackageSpec(spec);

    if (!pathArg.empty()) {
        bool const changed = manifest->AddPathDependency(pkgName, std::string(pathArg));
        if (!manifest->Save(*manifestPath)) {
            std::print(stderr, "error: failed to write '{}'\n", manifestPath->string());
            return 1;
        }
        if (!opts.quiet) {
            std::print("{} {} @ path '{}'\n", changed ? "Added" : "Up-to-date", pkgName, pathArg);
        }
        return 0;
    }

    if (!opts.quiet) {
        std::print("     Fetching registry...\n");
    }

    auto const jsonOpt = FetchUrl(std::string(kRegistryUrl));
    if (!jsonOpt) {
        std::print(stderr, "error: failed to fetch package registry\n");
        return 1;
    }

    if (JsonLookupString(*jsonOpt, pkgName).empty()) {
        std::print(stderr, "error: package '{}' not found in registry\n", pkgName);
        return 1;
    }

    bool const changed = manifest->AddDependency(pkgName, pkgVersion);
    if (!manifest->Save(*manifestPath)) {
        std::print(stderr, "error: failed to write '{}'\n", manifestPath->string());
        return 1;
    }

    if (!opts.quiet) {
        std::string const ver = pkgVersion.empty() ? "latest" : pkgVersion;
        std::print("{} {} @ {}\n", changed ? "Added" : "Up-to-date", pkgName, ver);
    }
    return 0;
}

int Cli::RunRemove(std::span<std::string_view const> args, GlobalOptions const &opts) {
    std::string_view name;

    for (auto const &arg : args) {
        if (arg == "-h" or arg == "--help") {
            PrintHelpFor("remove");
            return 0;
        }
        if (!arg.starts_with('-') and name.empty()) {
            name = arg;
        }
        else {
            PrintUnknownOption(arg, "remove");
            return 1;
        }
    }

    if (name.empty()) {
        std::print(stderr, "error: missing package name\n\n");
        PrintHelpFor("remove");
        return 1;
    }

    auto const manifestPath = RequireManifest();
    if (!manifestPath) {
        return 1;
    }

    auto manifest = LoadManifest(*manifestPath);
    if (!manifest) {
        return 1;
    }

    std::string const pkgName{name};
    if (!manifest->RemoveDependency(pkgName)) {
        std::print(stderr, "error: package '{}' is not a dependency\n", pkgName);
        return 1;
    }

    if (!manifest->Save(*manifestPath)) {
        std::print(stderr, "error: failed to write '{}'\n", manifestPath->string());
        return 1;
    }

    if (!opts.quiet) {
        std::print("     Removed {}\n", pkgName);
    }
    return 0;
}

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
        RequireManifest();
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
            ResolveBuildOutputDir(pkgDir, *pkgManifest, isRelease ? "Release" : "Debug");
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

int Cli::RunInit(std::span<std::string_view const> args, GlobalOptions const &opts) {
    bool bin = false, lib = false;
    for (auto const &arg : args) {
        if (arg == "--bin") {
            bin = true;
        }
        else if (arg == "--lib") {
            lib = true;
        }
        else if (arg == "-h" or arg == "--help") {
            PrintHelpFor("init");
            return 0;
        }
        else {
            PrintUnknownOption(arg, "init");
            return 1;
        }
    }

    auto const type = (lib and !bin) ? PackageType::SharedLibrary : PackageType::Executable;
    auto const name = std::filesystem::current_path().filename().string();

    if (!opts.quiet) {
        std::print("  Initializing {} package '{}'\n",
                   type == PackageType::Executable ? "binary" : "library", name);
    }

    if (!ScaffoldPackage(std::filesystem::current_path(), name, type, true)) {
        return 1;
    }

    if (!opts.quiet) {
        std::print("    Initialized package '{}'\n", name);
    }
    return 0;
}
