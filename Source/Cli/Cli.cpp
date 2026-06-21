// Copyright (c) Rux contributors.
// SPDX-License-Identifier: MIT

#include "Rux/Cli/Cli.h"

#include "Rux/Cli/CliInternals.h" // for Cli::ParseGlobalOptions, Misc
#include "Rux/Platform/Defines.h" // for RUX_OS_WINDOWS
#include "Rux/Platform/Types.h"   // for Platform

#include <cstddef>     // for size_t
#include <string>      // for char_traits
#include <string_view> // for basic_string_view, operator==, string_view
#include <vector>      // for vector

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
    #include <winhttp.h>
#else
    #include <sys/resource.h>
    #include <sys/wait.h>
    #include <unistd.h>
#endif

using namespace Rux;
using namespace Platform;
using namespace Misc;

namespace {

std::unordered_map<std::string_view, Cli::CommandFn> const kCommands = {
    {"help", Cli::RunHelp},
    {"version", [](auto, auto const &opts) { return Cli::RunVersion(opts); }},
    {"build", Cli::RunBuild},
    {"clean", Cli::RunClean},
    {"doc", Cli::RunDoc},
    {"fmt", Cli::RunFmt},
    {"init", Cli::RunInit},
    {"install", Cli::RunInstall},
    {"uninstall", Cli::RunUninstall},
    {"list", Cli::RunList},
    {"new", Cli::RunNew},
    {"add", Cli::RunAdd},
    {"remove", Cli::RunRemove},
    {"run", Cli::RunRun},
    {"test", Cli::RunTest},
    {"update", Cli::RunUpdate},
    {"info", Cli::RunInfo},
    {"check", Cli::RunCheck},
};

bool IsGlobalFlag(std::string_view const arg) {
    return arg == "-h" or arg == "--help" or arg == "-V" or arg == "--version" or arg == "-q" or
           arg == "--quiet" or arg == "-v" or arg == "--verbose" or arg == "--color" or
           arg.starts_with("--color=");
}

} // namespace

Cli::Cli(int const argc, char *argv[])
    : args(argv, argc) {
}

int Cli::Run() const {
    std::vector<std::string_view> sv;
    sv.reserve(args.size());

    for (auto *a : args.subspan(1)) {
        sv.emplace_back(a);
    }

    if (sv.empty()) {
        PrintHelp();
        return 0;
    }

    std::vector<std::string_view> globals;
    std::vector<std::string_view> cmdArgs;
    std::string_view command;
    bool foundCommand = false;

    for (std::size_t i = 0; i < sv.size(); ++i) {
        auto arg = sv[i];

        if (!foundCommand) {
            if (arg == "-h" or arg == "--help") {
                return PrintHelp(), 0;
            }
            if (arg == "-V" or arg == "--version") {
                return PrintVersion(), 0;
            }

            if (IsGlobalFlag(arg)) {
                globals.push_back(arg);

                if (arg == "--color" and i + 1 < sv.size()) {
                    globals.push_back(sv[++i]);
                }
                continue;
            }

            command = arg;
            foundCommand = true;
        }
        else {
            cmdArgs.push_back(arg);
        }
    }

    if (!foundCommand) {
        PrintHelp();
        return 0;
    }

    std::vector<std::string_view> allArgs;
    allArgs.reserve(globals.size() + cmdArgs.size());
    allArgs.insert(allArgs.end(), globals.begin(), globals.end());
    allArgs.insert(allArgs.end(), cmdArgs.begin(), cmdArgs.end());

    GlobalOptions const opts = ParseGlobalOptions(allArgs);

    std::span<std::string_view const> const rest(cmdArgs);

    auto const it = kCommands.find(command);
    if (it == kCommands.end()) {
        PrintUnknownCommand(command);
        return 1;
    }

    return it->second(rest, opts);
}
