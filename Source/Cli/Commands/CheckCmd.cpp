// Copyright (c) Rux contributors.
// SPDX-License-Identifier: MIT

#include "Rux/Ast.h"
#include "Rux/Cli/CliInternals.h"
#include "Rux/Manifest.h"
#include "Rux/Sema.h"
#include "Rux/SourceLoader.h"

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_set>

namespace Rux {

namespace {
struct JsonDiagnostic {
    std::string file;
    int line = 0;
    int column = 0;
    std::string severity;
    std::string message;
};

struct PendingPackage {
    std::string name;
    std::filesystem::path root;
    Manifest manifest;
};

struct ImportCollector {
    std::vector<std::string> &imports;
    std::string_view target;

    void collect(Decl const &decl) {
        if (auto const *ud = dynamic_cast<UseDecl const *>(&decl)) {
            if (Misc::DeclMatchesTarget(*ud, target) and !ud->path.empty()) {
                imports.push_back(ud->path.front());
            }
            return;
        }

        if (auto const *mod = dynamic_cast<ModuleDecl const *>(&decl)) {
            for (auto const &item : mod->items) {
                if (item) {
                    collect(*item);
                }
            }
        }
    }
};

struct DependencyQueue {
    std::string_view pkgName;
    Manifest const &ownerManifest;
    std::filesystem::path const &ownerRoot;
    std::unordered_set<std::string> &queuedPackageNames;
    std::vector<PendingPackage> &pendingPackages;
    std::string_view targetName;
};

using DiagnosticEmitter =
    std::function<void(std::string_view, int, int, std::string_view, std::string_view)>;

[[nodiscard]] auto JsonEscape(std::string_view s) -> std::string {
    std::string out;
    out.reserve(s.size() + (s.size() / 10) + 16);

    for (char const ch : s) {
        switch (auto const u_ch = static_cast<unsigned char>(ch)) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (u_ch < 0x20) {
                out += std::format("\\u{:04x}", u_ch);
            }
            else {
                out += ch;
            }
            break;
        }
    }
    return out;
}

[[nodiscard]] auto EnqueueDependency(DependencyQueue const &queue,
                                     DiagnosticEmitter const &EmitDiag) -> bool {
    if (queue.queuedPackageNames.contains(std::string{queue.pkgName})) {
        return true;
    }

    auto const deps = queue.ownerManifest.EffectiveDependencies(std::string{queue.targetName});
    auto const it = std::ranges::find(deps, queue.pkgName, &Dependency::name);

    if (it == deps.end()) {
        EmitDiag("", 0, 0, "error",
                 std::format("package '{}' is not listed in [Dependencies]", queue.pkgName));
        return false;
    }

    auto const &targetDep = *it;
    std::filesystem::path depRoot;

    if (targetDep.path.empty()) {
        depRoot = Misc::RegistryPackagesDir() / Misc::DependencyPackageName(targetDep);
        if (!std::filesystem::exists(depRoot)) {
            EmitDiag("", 0, 0, "error",
                     std::format("package '{}' is not installed — run 'rux install'",
                                 Misc::DependencyPackageName(targetDep)));
            return false;
        }
    }
    else {
        depRoot = (queue.ownerRoot / targetDep.path).lexically_normal();
        if (auto const rel = depRoot.lexically_relative(queue.ownerRoot);
            !rel.empty() and *rel.begin() == "..") {
            EmitDiag("", 0, 0, "error",
                     std::format("package '{}' contains an invalid path escaping root bounds",
                                 queue.pkgName));
            return false;
        }
    }

    auto depManifest = Manifest::Load(depRoot / "Rux.toml");
    if (!depManifest) {
        EmitDiag("", 0, 0, "error",
                 std::format("dependency package '{}' was not found at '{}'", queue.pkgName,
                             depRoot.string()));
        return false;
    }

    queue.queuedPackageNames.emplace(queue.pkgName);
    queue.pendingPackages.emplace_back(targetDep.name, std::move(depRoot), std::move(*depManifest));
    return true;
}

[[nodiscard]] int HandleJsonOutput(bool const hadErrors,
                                   std::span<JsonDiagnostic const> jsonDiags) {
    std::println("{{");
    std::println("  \"success\": {},", !hadErrors);
    std::println("  \"diagnostics\": [");

    for (std::size_t i = 0; i < jsonDiags.size(); ++i) {
        auto const &[file, line, column, severity, message] = jsonDiags[i];
        std::print("    "
                   "{{\"file\":\"{}\",\"line\":{},\"column\":{},\"severity\":\"{}\",\"message\":\"{"
                   "}\"}}{}\n",
                   JsonEscape(file), line, column, JsonEscape(severity), JsonEscape(message),
                   (i + 1 < jsonDiags.size()) ? "," : "");
    }

    std::println("  ]");
    std::println("}}");
    return hadErrors ? 1 : 0;
}

[[nodiscard]] auto HandleErrors(std::span<ParseResult const> parseResults,
                                std::span<ParseResult const> depParseResults,
                                std::span<std::string const> loadedPackages,
                                std::span<std::string const> loadedModuleNames,
                                Manifest const &manifest, std::string const &targetName,
                                DiagnosticEmitter const &EmitDiag) -> bool {
    std::vector<Module const *> userModules;
    userModules.reserve(parseResults.size());
    for (auto const &[module, _] : parseResults) {
        userModules.push_back(&module);
    }

    std::vector<DepPackage> depPackages;
    std::unordered_map<std::string_view, std::size_t> pkgIdx;

    for (std::size_t i = 0; i < loadedPackages.size(); ++i) {
        std::string_view const pkgName = loadedPackages[i];
        auto [it, inserted] = pkgIdx.emplace(pkgName, depPackages.size());
        if (inserted) {
            depPackages.push_back({std::string(pkgName), {}});
        }
        depPackages[it->second].modules.emplace_back(loadedModuleNames[i],
                                                     &depParseResults[i].module);
    }

    Sema sema(std::move(userModules), std::move(depPackages), manifest.package.name,
              std::string(Misc::TargetOsName(targetName)));
    auto semaResult = sema.Analyze();

    bool hadErrors = semaResult.HasErrors();
    for (auto const &[severity, sourceName, location, message] : semaResult.diagnostics) {
        auto const &loc = location;
        std::string_view sev = severity == SemaDiagnostic::Severity::Error ? "error" : "warning";
        EmitDiag(sourceName, static_cast<int>(loc.line), static_cast<int>(loc.column), sev,
                 message);
        if (severity == SemaDiagnostic::Severity::Error) {
            hadErrors = true;
        }
    }

    return !hadErrors;
}

[[nodiscard]] auto ProcessPendingIndex(
    GlobalOptions const &opts, bool jsonOutput, std::vector<PendingPackage> &pendingPackages,
    std::string const &targetName, std::vector<std::string> &imports, ImportCollector &collector,
    std::vector<ParseResult> &depParseResults, std::vector<std::string> &loadedPackages,
    std::vector<std::string> &loadedModuleNames,
    std::unordered_set<std::string> &queuedPackageNames, DiagnosticEmitter const &EmitDiag)
    -> bool {
    for (std::size_t pendingIndex = 0; pendingIndex < pendingPackages.size(); ++pendingIndex) {
        std::string currentPkgName = pendingPackages[pendingIndex].name;
        std::filesystem::path currentPkgRoot = pendingPackages[pendingIndex].root;
        Manifest currentManifest = pendingPackages[pendingIndex].manifest;

        if (opts.verbose and !jsonOutput) {
            std::println(" Loading package {} from {}", currentPkgName, currentPkgRoot.string());
        }

        auto depLoadResult = SourceLoader::Load(currentPkgRoot);
        if (!depLoadResult) {
            return false;
        }

        for (auto const &error : depLoadResult->errors) {
            if (jsonOutput) {
                EmitDiag("", 0, 0, "error", error);
            }
            else {
                std::print(stderr, "{}", error);
            }
        }

        if (!depLoadResult->errors.empty()) {
            return false;
        }

        std::vector<ParseResult> packageParseResults;
        packageParseResults.reserve(depLoadResult->files.size());
        bool parseErrors = false;

        for (auto const &[path, source] : depLoadResult->files) {
            Lexer depLexer(source, path.string());
            auto depLex = depLexer.Tokenize();

            for (auto const &[severity, location, message] : depLex.diagnostics) {
                std::string_view sev =
                    severity == LexerDiagnostic::Severity::Error ? "error" : "warning";
                EmitDiag(path.string(), static_cast<int>(location.line),
                         static_cast<int>(location.column), sev, message);
                if (severity == LexerDiagnostic::Severity::Error) {
                    parseErrors = true;
                }
            }
            if (depLex.HasErrors()) {
                parseErrors = true;
                break;
            }

            Parser depParser(std::move(depLex.tokens), path.string());
            auto depParse = depParser.Parse();

            for (auto const &[severity, location, message] : depParse.diagnostics) {
                std::string_view sev =
                    severity == ParserDiagnostic::Severity::Error ? "error" : "warning";
                EmitDiag(path.string(), static_cast<int>(location.line),
                         static_cast<int>(location.column), sev, message);
                if (severity == ParserDiagnostic::Severity::Error) {
                    parseErrors = true;
                }
            }
            if (depParse.HasErrors()) {
                parseErrors = true;
                break;
            }

            Misc::PruneModuleForTarget(depParse.module, targetName);
            packageParseResults.push_back(std::move(depParse));
        }

        if (parseErrors) {
            return false;
        }

        imports.clear();
        for (auto const &[module, _] : packageParseResults) {
            for (auto const &decl : module.items) {
                if (decl) {
                    collector.collect(*decl);
                }
            }
        }

        for (auto const &pkgName : imports) {
            if (pkgName == currentManifest.package.name or pkgName == currentPkgName) {
                continue;
            }

            DependencyQueue depQueue{.pkgName = pkgName,
                                     .ownerManifest = currentManifest,
                                     .ownerRoot = currentPkgRoot,
                                     .queuedPackageNames = queuedPackageNames,
                                     .pendingPackages = pendingPackages,
                                     .targetName = targetName};

            if (!EnqueueDependency(depQueue, EmitDiag)) {
                return false;
            }
        }

        for (auto &depParse : packageParseResults) {
            loadedModuleNames.push_back(depParse.module.name);
            depParseResults.push_back(std::move(depParse));
            loadedPackages.push_back(pendingPackages[pendingIndex].name);
        }
    }
    return true;
}
} // namespace

int Cli::RunCheck(std::span<std::string_view const> args, GlobalOptions const &opts) {
    bool jsonOutput = false;
    std::string_view target;

    for (auto it = args.begin(); it != args.end(); ++it) {
        if (*it == "-q" or *it == "--quiet" or *it == "-v" or *it == "--verbose") {
            continue;
        }
        if (*it == "--json") {
            jsonOutput = true;
            continue;
        }
        if (*it == "--target" and std::next(it) != args.end()) {
            target = *++it;
            continue;
        }
        if (*it == "-h" or *it == "--help") {
            PrintHelpFor("check");
            return 0;
        }
        PrintUnknownOption(*it, "check");
        return 1;
    }

    std::vector<JsonDiagnostic> jsonDiags;
    bool hasFatalError = false;

    auto EmitDiag = [&](std::string_view file, int line, int column, std::string_view severity,
                        std::string_view message) {
        if (jsonOutput) {
            jsonDiags.emplace_back(std::string{file}, line, column, std::string{severity},
                                   std::string{message});
        }
        else {
            if (file.empty()) {
                std::println(stderr, "error: {}", message);
            }
            else {
                std::println(stderr, "{}:{}:{}: {}: {}", file, line, column, severity, message);
            }
        }
    };

    auto EmitFatal = [&](std::string_view const message) {
        EmitDiag("", 0, 0, "error", message);
        hasFatalError = true;
    };

    auto const manifestPath = Misc::RequireManifest();
    if (!manifestPath) {
        if (jsonOutput) {
            EmitFatal("could not find 'Rux.toml' in current directory or any parent directory");
        }
        return 1;
    }

    auto const manifest = Misc::LoadManifest(*manifestPath);
    if (!manifest) {
        if (jsonOutput) {
            EmitFatal("failed to parse 'Rux.toml'");
        }
        return 1;
    }

    std::string targetName = target.empty() ? Misc::HostTargetTriple() : std::string(target);
    if (!Misc::IsSupportedTargetTriple(targetName)) {
        if (jsonOutput) {
            EmitFatal(std::format("unsupported target '{}'", targetName));
        }
        else {
            std::print(stderr,
                       "error: unsupported target '{}'; supported targets are linux-x64, "
                       "windows-x64, macos-x64, macos-aarch64, freebsd-x64, openbsd-x64, "
                       "netbsd-x64, dragonfly-x64, illumos-x64\n",
                       targetName);
        }
        return 1;
    }

    if (std::string const hostTarget = Misc::HostTargetTriple();
        hostTarget != "unknown" and targetName != hostTarget) {
        constexpr std::string_view err =
            "cross-target build from '{}' to '{}' is not supported yet";
        if (jsonOutput) {
            EmitFatal(std::format(err, hostTarget, targetName));
        }
        else {
            std::println(stderr, "error: {}", std::format(err, hostTarget, targetName));
        }
        return 1;
    }

    if (!opts.quiet and !jsonOutput) {
        std::println("Checking {} v{} [{}]", manifest->package.name, manifest->package.version,
                     manifestPath->parent_path().string());
    }

    auto loadResult = SourceLoader::Load(manifestPath->parent_path());
    if (!loadResult) {
        if (jsonOutput) {
            EmitFatal("failed to load source files");
        }
        return 1;
    }

    for (auto const &err : loadResult->errors) {
        if (jsonOutput) {
            EmitDiag("", 0, 0, "error", err);
            hasFatalError = true;
        }
        else {
            std::print(stderr, "{}", err);
        }
    }

    bool fileErrors = false;
    std::vector<LexerResult> lexResults;
    lexResults.reserve(loadResult->files.size());

    for (auto const &[path, source] : loadResult->files) {
        if (opts.verbose and !jsonOutput) {
            std::println("    Lexing {}", path.string());
        }

        Lexer lexer(source, path.string());
        auto lexResult = lexer.Tokenize();

        for (auto const &[severity, location, message] : lexResult.diagnostics) {
            std::string_view sev =
                severity == LexerDiagnostic::Severity::Error ? "error" : "warning";
            EmitDiag(path.string(), static_cast<int>(location.line),
                     static_cast<int>(location.column), sev, message);
            if (severity == LexerDiagnostic::Severity::Error) {
                fileErrors = true;
            }
        }
        lexResults.push_back(std::move(lexResult));
    }

    std::vector<ParseResult> parseResults;
    parseResults.reserve(loadResult->files.size());

    for (std::size_t fileIndex = 0; fileIndex < loadResult->files.size(); ++fileIndex) {
        auto const &[path, source] = loadResult->files[fileIndex];

        if (opts.verbose and !jsonOutput) {
            std::println("    Parsing {}", path.string());
        }

        auto &lexResult = lexResults[fileIndex];
        if (lexResult.HasErrors()) {
            continue;
        }

        Parser parser(std::move(lexResult.tokens), path.string());
        auto parseResult = parser.Parse();

        for (auto const &[severity, location, message] : parseResult.diagnostics) {
            std::string_view sev =
                severity == ParserDiagnostic::Severity::Error ? "error" : "warning";
            EmitDiag(path.string(), static_cast<int>(location.line),
                     static_cast<int>(location.column), sev, message);
            if (severity == ParserDiagnostic::Severity::Error) {
                fileErrors = true;
            }
        }

        if (!parseResult.HasErrors()) {
            Misc::PruneModuleForTarget(parseResult.module, targetName);
            parseResults.push_back(std::move(parseResult));
        }
    }

    hasFatalError |= fileErrors;

    std::vector<ParseResult> depParseResults;
    std::vector<std::string> loadedPackages;
    std::vector<std::string> loadedModuleNames;
    std::vector<PendingPackage> pendingPackages;
    std::unordered_set<std::string> queuedPackageNames;
    std::vector<std::string> imports;

    ImportCollector collector{imports, targetName};

    for (auto const &[module, _] : parseResults) {
        imports.clear();
        for (auto const &decl : module.items) {
            if (decl) {
                collector.collect(*decl);
            }
        }

        for (auto const &pkgName : imports) {
            if (pkgName == manifest->package.name) {
                continue;
            }

            DependencyQueue depQueue{.pkgName = pkgName,
                                     .ownerManifest = *manifest,
                                     .ownerRoot = manifestPath->parent_path(),
                                     .queuedPackageNames = queuedPackageNames,
                                     .pendingPackages = pendingPackages,
                                     .targetName = targetName};

            if (!EnqueueDependency(depQueue, EmitDiag)) {
                hasFatalError = true;
                break;
            }
        }
    }

    if (!hasFatalError and !ProcessPendingIndex(opts, jsonOutput, pendingPackages, targetName,
                                                imports, collector, depParseResults, loadedPackages,
                                                loadedModuleNames, queuedPackageNames, EmitDiag)) {
        hasFatalError = true;
    }

    if (!hasFatalError and !HandleErrors(parseResults, depParseResults, loadedPackages,
                                         loadedModuleNames, *manifest, targetName, EmitDiag)) {
        hasFatalError = true;
    }

    if (jsonOutput) {
        return HandleJsonOutput(hasFatalError, jsonDiags);
    }

    return hasFatalError ? 1 : 0;
}
} // namespace Rux
