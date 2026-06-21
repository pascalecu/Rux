// Copyright (c) Rux contributors.
// SPDX-License-Identifier: MIT

#include "Rux/Asm.h"
#include "Rux/Cli/Cli.h"
#include "Rux/Cli/CliInternals.h"
#include "Rux/Manifest.h"
#include "Rux/Platform/Defines.h"
#include "Rux/Platform/Host.h"
#include "Rux/Platform/Types.h"

#include <filesystem>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#if RUX_OS_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#else
    #include <sys/wait.h>
    #include <unistd.h>
#endif

namespace Rux {
int Cli::RunRun(std::span<std::string_view const> args, GlobalOptions const &opts) {
    bool isRelease = false;
    std::vector<std::string_view> runArgs;
    bool passThroughMode = false;
    for (auto arg : args) {
        if (passThroughMode) {
            runArgs.push_back(arg);
            continue;
        }
        if (arg == "--") {
            passThroughMode = true;
            continue;
        }
        if (arg == "--release") {
            isRelease = true;
            continue;
        }
        if (arg == "-h" or arg == "--help") {
            PrintHelpFor("run");
            return 0;
        }
        PrintUnknownOption(arg, "run");
        return 1;
    }
    auto manifestPath = Misc::RequireManifest();
    if (!manifestPath) {
        return 1;
    }
    auto manifest = Misc::LoadManifest(*manifestPath);
    if (!manifest) {
        return 1;
    }
    // Build first
    GlobalOptions buildOpts = opts;
    if (!opts.verbose) {
        buildOpts.quiet = true;
    }

    std::vector<std::string_view> buildArgs;
    if (isRelease) {
        buildArgs.emplace_back("--release");
    }
    if (buildOpts.quiet) {
        buildArgs.emplace_back("--quiet");
    }
    if (buildOpts.verbose) {
        buildArgs.emplace_back("--verbose");
    }
    int rc = RunBuild(buildArgs, buildOpts);
    if (rc != 0) {
        return rc;
    }
    std::string_view profileName = isRelease ? "Release" : "Debug";
    auto root = manifestPath->parent_path();
    auto binDir = Misc::ResolveBuildOutputDir(root, *manifest, profileName);
    bool const runDll = (manifest->package.type == "Dll" or manifest->package.type == "dll");
    if (runDll) {
        std::print(stderr, "error: cannot run a DLL package directly\n");
        return 1;
    }
    std::string exeName = manifest->package.name;

    if constexpr (Platform::HostOS == Platform::OS::Windows) {
        exeName.append(".exe");
    }

    auto exePath = binDir / exeName;
    if (!std::filesystem::exists(exePath)) {
        std::print(stderr, "error: executable not found: '{}'\n", exePath.string());
        return 1;
    }
    if (opts.verbose and !opts.quiet) {
        std::print("     Running `{}`\n", exePath.string());
    }
#if RUX_OS_WINDOWS
    std::string cmdLine = "\"" + exePath.string() + "\"";
    for (auto const &a : runArgs) {
        cmdLine += " \"";
        cmdLine += std::string(a);
        cmdLine += '"';
    }
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.dwFlags = STARTF_USESTDHANDLES;
    if (!CreateProcessA(nullptr, cmdLine.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si,
                        &pi)) {
        std::print(stderr, "error: failed to launch '{}' (code {})\n", exePath.string(),
                   GetLastError());
        return 1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(exitCode);
#else
    std::vector<std::string> argStrings;
    argStrings.push_back(exePath.string());
    for (auto const &a : runArgs) {
        argStrings.emplace_back(a);
    }

    std::vector<char *> argv;
    for (auto &s : argStrings) {
        argv.push_back(s.data());
    }

    argv.push_back(nullptr);

    pid_t pid = fork();
    if (pid < 0) {
        std::print(stderr, "error: fork failed\n");
        return 1;
    }

    if (pid == 0) {
        execv(exePath.c_str(), argv.data());
        std::print(stderr, "error: failed to launch '{}'\n", exePath.string());
        _exit(127);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
#endif
}
} // namespace Rux
