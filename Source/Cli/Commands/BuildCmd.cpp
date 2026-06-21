// Copyright (c) Rux contributors.
// SPDX-License-Identifier: MIT


#include "Rux/Asm.h"              // for Asm
#include "Rux/Ast.h"              // for Decl, UseDecl, Module, ModuleDecl
#include "Rux/Cli/Cli.h"          // for GlobalOptions, Cli
#include "Rux/Cli/CliInternals.h" // for BuildStats, ElapsedMs, CountLines, CountTokens, Depend...
#include "Rux/Hir.h"              // for Hir, HirPackage
#include "Rux/Lexer.h"            // for LexerResult, LexerDiagnostic, Lexer
#include "Rux/Linker.h"           // for LinkerError, Linker
#include "Rux/Lir.h"              // for Lir, LirPackage
#include "Rux/Manifest.h"         // for Manifest, Package, Dependency
#include "Rux/Parser.h"           // for ParseResult, ParserDiagnostic, Parser
#include "Rux/Platform/Host.h"    // for HostOS
#include "Rux/Platform/Types.h"   // for OS
#include "Rux/Rcu.h"              // for Rcu, RcuFile
#include "Rux/Sema.h"             // for DepPackage, SemaDiagnostic, Sema, SemaResult
#include "Rux/SourceLoader.h"     // for SourceFile, SourceLoadResult, SourceLoader
#include "Rux/Token.h"            // for Token, SourceLocation

#include <chrono>        // for steady_clock
#include <cstdio>        // for stderr, size_t
#include <filesystem>    // for path, operator/, create_directories, relative, exists
#include <memory>        // for unique_ptr
#include <optional>      // for optional
#include <print>         // for print
#include <span>          // for span
#include <string>        // for basic_string, char_traits, hash, string, operator==
#include <string_view>   // for basic_string_view, operator==, string_view
#include <system_error>  // for error_code
#include <unordered_map> // for unordered_map
#include <unordered_set> // for unordered_set
#include <utility>       // for move, get
#include <vector>        // for vector

namespace Rux {
int Cli::RunBuild(std::span<std::string_view const> args, GlobalOptions const &opts) {
    auto const t0 = std::chrono::steady_clock::now();
    bool isRelease = false;
    bool isDebug = false;
    std::string_view profile;
    std::string_view target;
    bool dumpTokens = false;
    bool dumpAst = false;
    bool dumpSema = false;
    bool dumpHir = false;
    bool dumpLir = false;
    bool dumpAsm = false;
    bool dumpRcu = false;
    bool showStats = false;
    for (std::size_t i = 0; i < args.size(); ++i) {
        std::string_view arg = args[i];
        if (arg == "--release") {
            isRelease = true;
            continue;
        }
        if (arg == "--debug") {
            isDebug = true;
            continue;
        }
        if (arg == "-q" or arg == "--quiet") {
            continue;
        }
        if (arg == "-v" or arg == "--verbose") {
            continue;
        }
        if (arg == "--stats") {
            showStats = true;
            continue;
        }
        if (arg == "--dump-tokens") {
            dumpTokens = true;
            continue;
        }
        if (arg == "--dump-ast") {
            dumpAst = true;
            continue;
        }
        if (arg == "--dump-sema") {
            dumpSema = true;
            continue;
        }
        if (arg == "--dump-hir") {
            dumpHir = true;
            continue;
        }
        if (arg == "--dump-lir") {
            dumpLir = true;
            continue;
        }
        if (arg == "--dump-asm") {
            dumpAsm = true;
            continue;
        }
        if (arg == "--dump-rcu") {
            dumpRcu = true;
            continue;
        }
        if (arg == "--profile" and i + 1 < args.size()) {
            profile = args[++i];
            continue;
        }
        if (arg == "--target" and i + 1 < args.size()) {
            target = args[++i];
            continue;
        }
        if (arg == "-h" or arg == "--help") {
            PrintHelpFor("build");
            return 0;
        }
        PrintUnknownOption(arg, "build");
        return 1;
    }

    (void)isDebug; // Stop -Wunused-but-set-variable

    auto manifestPath = Misc::RequireManifest();
    if (!manifestPath) {
        return 1;
    }

    auto manifest = Misc::LoadManifest(*manifestPath);
    if (!manifest) {
        return 1;
    }

    std::string targetName = target.empty() ? Misc::HostTargetTriple() : std::string(target);
    if (!Misc::IsSupportedTargetTriple(targetName)) {
        std::print(stderr,
                   "error: unsupported target '{}'; supported targets are "
                   "linux-x64, windows-x64, macos-x64, macos-arm64, "
                   "freebsd-x64, openbsd-x64, netbsd-x64, illumos-x64\n",
                   targetName);
        return 1;
    }
    if (std::string const hostTarget = Misc::HostTargetTriple();
        hostTarget != "unknown" and targetName != hostTarget) {
        // Target selection is currently used for source/dependency choice.
        // Linking foreign executable formats is kept explicit until the
        // backends support it end-to-end.
        std::print(stderr,
                   "error: cross-target build from '{}' to '{}' is not "
                   "supported yet\n",
                   hostTarget, targetName);
        return 1;
    }

    std::string_view profileName = isRelease ? "Release" : "Debug";
    if (!profile.empty()) {
        profileName = profile;
    }

    if (!opts.quiet and !showStats) {
        std::print("Compiling {} v{} [{}]\n", manifest->package.name, manifest->package.version,
                   manifestPath->parent_path().string());
    }

    // ── Lex ───────────────────────────────────────────────────────────────
    Misc::BuildStats stats;
    auto loadResult = SourceLoader::Load(manifestPath->parent_path());
    if (!loadResult) {
        return 1;
    }

    stats.localFiles = loadResult->files.size();
    for (auto const &[_, source] : loadResult->files) {
        stats.localLines += Misc::CountLines(source);
        stats.localSourceSize += source.size();
    }

    for (auto const &err : loadResult->errors) {
        std::print(stderr, "{}", err);
    }

    bool lexErrors = false;
    std::vector<LexerResult> lexResults;
    lexResults.reserve(loadResult->files.size());
    auto const localLexingStart = std::chrono::steady_clock::now();
    for (auto const &file : loadResult->files) {
        if (opts.verbose) {
            std::print("     Lexing {}\n", file.path.string());
        }

        Lexer lexer(file.source, file.path.string());
        auto lexResult = lexer.Tokenize();
        stats.localTokens += Misc::CountTokens(lexResult);

        for (auto const &[severity, location, message] : lexResult.diagnostics) {
            auto const &loc = location;
            char const *sev = severity == LexerDiagnostic::Severity::Error ? "error" : "warning";
            std::print(stderr, "{}:{}:{}: {}: {}\n", file.path.string(), loc.line, loc.column, sev,
                       message);
        }
        if (lexResult.HasErrors()) {
            lexErrors = true;
        }

        if (dumpTokens) {
            auto tempDir = manifestPath->parent_path() / "Temp" / "Tokens";
            std::filesystem::create_directories(tempDir);
            auto rel = std::filesystem::relative(file.path, manifestPath->parent_path() / "Src");
            auto tokPath = tempDir / rel;
            tokPath.replace_extension(".tokens");
            Lexer::DumpTokens(lexResult, tokPath);
        }
        lexResults.push_back(std::move(lexResult));
    }
    auto const localLexingEnd = std::chrono::steady_clock::now();
    stats.lexing += Misc::ElapsedMs(localLexingStart, localLexingEnd);
    if (lexErrors) {
        return 1;
    }

    // Parse
    bool parseErrors = false;
    std::vector<ParseResult> parseResults;
    parseResults.reserve(loadResult->files.size());

    auto const localParsingStart = std::chrono::steady_clock::now();
    for (std::size_t fileIndex = 0; fileIndex < loadResult->files.size(); ++fileIndex) {
        auto const &file = loadResult->files[fileIndex];
        if (opts.verbose) {
            std::print("    Parsing {}\n", file.path.string());
        }

        auto &lexResult = lexResults[fileIndex];
        if (lexResult.HasErrors()) {
            continue;
        }

        Parser parser(std::move(lexResult.tokens), file.path.string());
        auto parseResult = parser.Parse();

        for (auto const &diag : parseResult.diagnostics) {
            auto const &loc = diag.location;
            char const *sev =
                diag.severity == ParserDiagnostic::Severity::Error ? "error" : "warning";
            std::print(stderr, "{}:{}:{}: {}: {}\n", file.path.string(), loc.line, loc.column, sev,
                       diag.message);
        }
        if (parseResult.HasErrors()) {
            parseErrors = true;
            continue;
        }
        Misc::PruneModuleForTarget(parseResult.module, targetName);

        if (dumpAst) {
            auto tempDir = manifestPath->parent_path() / "Temp" / "Ast";
            std::filesystem::create_directories(tempDir);
            auto rel = std::filesystem::relative(file.path, manifestPath->parent_path() / "Src");
            auto astPath = (tempDir / rel).replace_extension(".ast");
            Parser::DumpAst(parseResult, astPath);
        }
        parseResults.push_back(std::move(parseResult));
    }
    stats.parsing += Misc::ElapsedMs(localParsingStart);
    if (parseErrors) {
        return 1;
    }

    // Load dependency packages referenced by import declarations

    std::vector<ParseResult> depParseResults;
    std::vector<std::string> loadedPackages;    // parallel: package name per entry
    std::vector<std::string> loadedModuleNames; // parallel: source name per entry
    {
        struct PendingPackage {
            std::string name;
            std::filesystem::path root;
            Manifest manifest;
        };

        std::vector<PendingPackage> pendingPackages;
        std::unordered_set<std::string> queuedPackageNames;

        auto enqueueDependency = [&](std::string const &pkgName, Manifest const &ownerManifest,
                                     std::filesystem::path const &ownerRoot) -> bool {
            if (queuedPackageNames.contains(pkgName)) {
                return true;
            }

            // Resolve imports through the target view of the owning
            // manifest. This is what maps Platform to Linux on linux-x64.
            auto const deps = ownerManifest.EffectiveDependencies(targetName);
            Dependency const *dep = nullptr;
            for (auto const &d : deps) {
                if (d.name == pkgName) {
                    dep = &d;
                    break;
                }
            }

            if (!dep) {
                std::print(stderr, "error: package '{}' is not listed in [Dependencies]\n",
                           pkgName);
                return false;
            }
            std::filesystem::path depRoot;
            if (dep->path.empty()) {
                depRoot = Misc::RegistryPackagesDir() / Misc::DependencyPackageName(*dep);
                if (!std::filesystem::exists(depRoot)) {
                    std::print(stderr,
                               "error: package '{}' is not installed — run "
                               "'rux install'\n",
                               Misc::DependencyPackageName(*dep));
                    return false;
                }
            }
            else {
                depRoot = (ownerRoot / dep->path).lexically_normal();
            }
            auto depManifest = Manifest::Load(depRoot / "Rux.toml");
            if (!depManifest) {
                std::print(stderr, "error: dependency package '{}' was not found at '{}'\n",
                           pkgName, depRoot.string());
                return false;
            }

            queuedPackageNames.insert(pkgName);
            // Keep the import name as the package namespace loaded into
            // Sema, even when the files came from another package name.
            pendingPackages.push_back({dep->name, depRoot, std::move(*depManifest)});
            return true;
        };

        std::vector<std::string> imports;
        auto collectImports = [&](this auto &&self, Decl const &decl) -> void {
            if (auto const *ud = dynamic_cast<UseDecl const *>(&decl)) {
                if (!Misc::DeclMatchesTarget(*ud, targetName)) {
                    return;
                }
                if (!ud->path.empty()) {
                    imports.push_back(ud->path[0]);
                }
                return;
            }
            if (auto const *mod = dynamic_cast<ModuleDecl const *>(&decl)) {
                for (auto const &item : mod->items) {
                    if (item) {
                        self(*item);
                    }
                }
            }
        };

        for (auto const &pr : parseResults) {
            imports.clear();
            for (auto const &decl : pr.module.items) {
                if (decl) {
                    collectImports(*decl);
                }
            }
            for (auto const &pkgName : imports) {
                if (pkgName == manifest->package.name) {
                    continue;
                }
                if (!enqueueDependency(pkgName, *manifest, manifestPath->parent_path())) {
                    return 1;
                }
            }
        }

        for (std::size_t pendingIndex = 0; pendingIndex < pendingPackages.size(); ++pendingIndex) {
            std::filesystem::path const pendingRoot = pendingPackages[pendingIndex].root;
            Manifest const pendingManifest = pendingPackages[pendingIndex].manifest;
            std::string const packageName = pendingPackages[pendingIndex].name;

            if (opts.verbose) {
                std::print("  Loading package {} from {}\n", packageName, pendingRoot.string());
            }

            auto depLoadResult = SourceLoader::Load(pendingRoot);
            if (!depLoadResult) {
                return 1;
            }
            stats.dependencyFiles += depLoadResult->files.size();
            for (auto const &depFile : depLoadResult->files) {
                stats.dependencyLines += Rux::Misc::CountLines(depFile.source);
                stats.dependencySourceSize += depFile.source.size();
            }

            for (auto const &error : depLoadResult->errors) {
                std::print(stderr, "{}", error);
            }
            if (!depLoadResult->errors.empty()) {
                return 1;
            }

            std::vector<ParseResult> packageParseResults;
            packageParseResults.reserve(depLoadResult->files.size());

            for (auto const &depFile : depLoadResult->files) {
                auto const depLexingStart = std::chrono::steady_clock::now();
                Lexer depLexer(depFile.source, depFile.path.string());
                auto depLex = depLexer.Tokenize();
                auto const depLexingEnd = std::chrono::steady_clock::now();
                stats.lexing += Misc::ElapsedMs(depLexingStart, depLexingEnd);
                stats.dependencyTokens += Misc::CountTokens(depLex);
                for (auto const &diag : depLex.diagnostics) {
                    char const *sev =
                        diag.severity == LexerDiagnostic::Severity::Error ? "error" : "warning";
                    std::print(stderr, "{}:{}:{}: {}: {}\n", depFile.path.string(),
                               diag.location.line, diag.location.column, sev, diag.message);
                }
                if (depLex.HasErrors()) {
                    return 1;
                }

                auto const depParsingStart = std::chrono::steady_clock::now();
                Parser depParser(std::move(depLex.tokens), depFile.path.string());
                auto depParse = depParser.Parse();
                stats.parsing += Misc::ElapsedMs(depParsingStart);
                for (auto const &diag : depParse.diagnostics) {
                    char const *sev =
                        diag.severity == ParserDiagnostic::Severity::Error ? "error" : "warning";
                    std::print(stderr, "{}:{}:{}: {}: {}\n", depFile.path.string(),
                               diag.location.line, diag.location.column, sev, diag.message);
                }
                if (depParse.HasErrors()) {
                    return 1;
                }
                Misc::PruneModuleForTarget(depParse.module, targetName);

                packageParseResults.push_back(std::move(depParse));
            }

            imports.clear();
            for (auto const &pr : packageParseResults) {
                for (auto const &decl : pr.module.items) {
                    if (decl) {
                        collectImports(*decl);
                    }
                }
            }
            for (auto const &pkgName : imports) {
                if (pkgName == pendingManifest.package.name or pkgName == packageName) {
                    continue;
                }
                if (!enqueueDependency(pkgName, pendingManifest, pendingRoot)) {
                    return 1;
                }
            }

            for (auto &depParse : packageParseResults) {
                loadedModuleNames.push_back(depParse.module.name);
                depParseResults.push_back(std::move(depParse));
                loadedPackages.push_back(packageName);
            }
        }
    }
    // Semantic analysis

    auto const semanticStart = std::chrono::steady_clock::now();
    if (opts.verbose) {
        std::print("  Analyzing {}\n", manifest->package.name);
    }

    std::vector<Module const *> userModules;
    userModules.reserve(parseResults.size());
    for (auto const &pr : parseResults) {
        userModules.push_back(&pr.module);
    }

    // Build per-package dep info so Sema can isolate imported package symbols.
    std::vector<DepPackage> depPackages;
    {
        std::unordered_map<std::string, std::size_t> pkgIdx;
        for (std::size_t i = 0; i < depParseResults.size(); ++i) {
            std::string const &pkgName = loadedPackages[i];
            auto [it, inserted] = pkgIdx.emplace(pkgName, depPackages.size());
            if (inserted) {
                depPackages.push_back({pkgName, {}});
            }
            depPackages[it->second].modules.push_back(
                {loadedModuleNames[i], &depParseResults[i].module});
        }
    }

    Sema sema(std::move(userModules), std::move(depPackages), manifest->package.name,
              std::string(Misc::TargetOsName(targetName)));
    auto semaResult = sema.Analyze();

    for (auto const &diag : semaResult.diagnostics) {
        auto const &loc = diag.location;
        char const *sev = diag.severity == SemaDiagnostic::Severity::Error ? "error" : "warning";
        std::print(stderr, "{}:{}:{}: {}: {}\n", diag.sourceName, loc.line, loc.column, sev,
                   diag.message);
    }
    if (dumpSema) {
        auto semaDir = manifestPath->parent_path() / "Temp" / "Sema";
        std::filesystem::create_directories(semaDir);
        Sema::DumpResult(semaResult, semaDir / "sema.txt");
    }
    if (semaResult.HasErrors()) {
        return 1;
    }
    stats.semantic = Misc::ElapsedMs(semanticStart);

    // HIR

    auto const hirStart = std::chrono::steady_clock::now();
    if (opts.verbose) {
        std::print("  Lowering {}\n", manifest->package.name);
    }

    std::vector<Module const *> hirModules;
    hirModules.reserve(depParseResults.size() + parseResults.size());
    for (auto const &pr : depParseResults) {
        hirModules.push_back(&pr.module);
    }
    for (auto const &pr : parseResults) {
        hirModules.push_back(&pr.module);
    }

    Hir hir(hirModules);
    auto hirPackage = hir.Generate();

    if (dumpHir) {
        auto hirDir = manifestPath->parent_path() / "Temp" / "Hir";
        std::filesystem::create_directories(hirDir);
        Hir::Dump(hirPackage, hirDir / "hir.txt");
    }
    stats.hir = Misc::ElapsedMs(hirStart);

    // LIR

    auto const lirStart = std::chrono::steady_clock::now();
    if (opts.verbose) {
        std::print("  Emitting LIR for {}\n", manifest->package.name);
    }

    Lir lir(std::move(hirPackage));
    auto lirPackage = lir.Generate();

    if (dumpLir) {
        auto lirDir = manifestPath->parent_path() / "Temp" / "Lir";
        std::filesystem::create_directories(lirDir);
        Lir::Dump(lirPackage, lirDir / "lir.txt");
    }
    stats.lir = Misc::ElapsedMs(lirStart);

    // Assembly dump (optional)

    auto const codegenStart = std::chrono::steady_clock::now();
    if (dumpAsm) {
        if (opts.verbose) {
            std::print("  Emitting assembly for {}\n", manifest->package.name);
        }
        auto asmDir = manifestPath->parent_path() / "Temp" / "Asm";
        std::filesystem::create_directories(asmDir);
        Asm::Emit(lirPackage, asmDir / "out.asm");
    }

    // RCU object generation

    if (opts.verbose) {
        std::print("  Emitting RCU objects for {}\n", manifest->package.name);
    }

    Rcu rcu(lirPackage, std::string(manifest->package.name));
    auto rcuFiles = rcu.Generate();

    if (dumpRcu) {
        auto objDir = manifestPath->parent_path() / "Temp" / "Obj";
        auto dumpDir = manifestPath->parent_path() / "Temp" / "Rcu";
        std::filesystem::create_directories(objDir);
        std::filesystem::create_directories(dumpDir);

        for (auto const &rcuFile : rcuFiles) {
            std::filesystem::path stem = rcuFile.sourcePath.empty()
                                           ? std::filesystem::path("out")
                                           : std::filesystem::path(rcuFile.sourcePath).stem();
            Rcu::Emit(rcuFile, objDir / (stem.string() + ".rcu"));
            Rcu::Dump(rcuFile, dumpDir / (stem.string() + ".rcu.txt"));
        }
    }
    stats.codegen = Misc::ElapsedMs(codegenStart);

    // Link

    auto const linkingStart = std::chrono::steady_clock::now();
    if (opts.verbose) {
        std::print("   Linking {}\n", manifest->package.name);
    }

    auto const root = manifestPath->parent_path();
    auto const binDir = Misc::ResolveBuildOutputDir(root, *manifest, profileName);
    bool const buildDll = (manifest->package.type == "Dll" or manifest->package.type == "dll");
    std::string outputName = manifest->package.name;
    if constexpr (Platform::HostOS == Platform::OS::Windows) {
        outputName += buildDll ? ".dll" : ".exe";
    }
    auto const exePath = binDir / outputName;

    Linker linker(std::move(rcuFiles), std::string(manifest->package.name), {root}, buildDll);
    if (!linker.Link(exePath)) {
        for (auto const &err : linker.Errors()) {
            std::print(stderr, "error: {}\n", err.message);
        }
        return 1;
    }
    stats.linking = Misc::ElapsedMs(linkingStart);

    // Done

    auto const buildEnd = std::chrono::steady_clock::now();
    stats.total = Misc::ElapsedMs(t0, buildEnd);
    stats.totalSeconds = Misc::ElapsedSeconds(t0, buildEnd);
    std::error_code sizeError;
    stats.executableSize = std::filesystem::file_size(exePath, sizeError);
    if (sizeError) {
        stats.executableSize = 0;
    }
    stats.peakMemoryBytes = Misc::PeakMemoryBytes();

    if (!opts.quiet and showStats) {
        PrintBuildStats(exePath, profileName, stats);
        return 0;
    }

    if (!opts.quiet) {
        PrintBuildSummary(exePath, profileName, stats);
    }
    return 0;
}
} // namespace Rux
