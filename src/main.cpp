#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "codegen.hpp"
#include "lexer.hpp"
#include "parser.hpp"

static constexpr const char* LLVM_CLANG  =
    "/opt/homebrew/opt/llvm/bin/clang";
static constexpr const char* WASI_SYSROOT =
    "/opt/homebrew/opt/wasi-libc/share/wasi-sysroot";

static void printUsage() {
    std::cerr <<
        "Usage: nanoscript <source.nano> [options] [output]\n"
        "\n"
        "  --config=debug        O0  + DWARF debug info  [default]\n"
        "  --config=development  O2  + DWARF debug info\n"
        "  --config=shipping     O3 (full LTO) + no debug info\n"
        "\n"
        "  --wasm                Emit a .wasm binary (default: native binary)\n"
        "\n"
        "  output  Path for the produced artifact.\n"
        "          Defaults to <stem>.wasm (--wasm) or <stem> (native).\n";
}

static std::string defaultOutput(const std::string& inputFile, bool wasm) {
    std::filesystem::path p(inputFile);
    std::string stem = p.stem().string();
    return wasm ? (stem + ".wasm") : stem;
}

static int linkArtifact(const std::string& tmpLL,
                        const std::string& outputFile,
                        BuildConfig config,
                        bool wasm) {
    std::string cmd;
    if (wasm) {
        const std::string dbg = (config != BuildConfig::Shipping) ? " -g" : "";
        cmd = std::string(LLVM_CLANG)
            + " --target=wasm32-wasi"
            + " --sysroot=" + WASI_SYSROOT
            + dbg
            + " " + tmpLL + " -o " + outputFile;
    } else {
        // -Wno-override-module suppresses the cosmetic "overriding module triple"
        // warning that clang emits when it normalises darwin→macosx in the triple.
        const std::string dbg = (config != BuildConfig::Shipping) ? " -g" : " -O3";
        cmd = std::string(LLVM_CLANG) + dbg
            + " -Wno-override-module"
            + " " + tmpLL + " -o " + outputFile;
    }

    const int rc = std::system(cmd.c_str());
    std::filesystem::remove(tmpLL);
    return rc;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    // ── Argument parsing ───────────────────────────────────────────────────
    std::string inputFile;
    std::string outputArg;
    BuildConfig config = BuildConfig::Debug;
    bool        wasm   = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--config=", 0) == 0) {
            std::string val = arg.substr(9);
            if      (val == "debug")       config = BuildConfig::Debug;
            else if (val == "development") config = BuildConfig::Development;
            else if (val == "shipping")    config = BuildConfig::Shipping;
            else {
                std::cerr << "Unknown config '" << val
                          << "'. Expected debug, development, or shipping.\n";
                return 1;
            }
        } else if (arg == "--wasm") {
            wasm = true;
        } else if (inputFile.empty()) {
            inputFile = arg;
        } else {
            outputArg = arg;
        }
    }

    if (inputFile.empty()) {
        printUsage();
        return 1;
    }

    const std::string outputFile = outputArg.empty()
        ? defaultOutput(inputFile, wasm)
        : outputArg;

    // ── Read source ────────────────────────────────────────────────────────
    std::ifstream file(inputFile);
    if (!file) {
        std::cerr << "Error: cannot open '" << inputFile << "'\n";
        return 1;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    const std::string source = ss.str();

    // ── Compile ────────────────────────────────────────────────────────────
    try {
        Lexer lexer(source, inputFile);
        auto  tokens = lexer.tokenize();

        Parser parser(std::move(tokens));
        auto   ast = parser.parse();

        // Absolute paths so LLDB/Wasmtime can locate the source file
        std::filesystem::path p = std::filesystem::absolute(inputFile);
        const std::string srcFile = p.filename().string();
        const std::string srcDir  = p.parent_path().string();

        Codegen cg(srcFile, srcDir, config, wasm);
        cg.generate(*ast);

        const std::string tmpLL = outputFile + ".tmp.ll";
        cg.writeIR(tmpLL);

        const int rc = linkArtifact(tmpLL, outputFile, config, wasm);
        if (rc != 0) {
            std::cerr << "Link step failed (exit " << rc << ")\n";
            return 1;
        }

        // Summary line
        const char* cfgStr  = config == BuildConfig::Debug      ? "debug / O0 / DWARF"
                            : config == BuildConfig::Development ? "development / O2 / DWARF"
                                                                 : "shipping / O3+LTO";
        const char* fmtStr  = wasm ? "wasm" : "native";
        std::cout << "Compiled '" << inputFile << "' → '" << outputFile
                  << "' [" << cfgStr << " / " << fmtStr << "]\n";
        std::cout << "Run:   "
                  << (wasm ? "wasmtime " : "./") << outputFile << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Compilation error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
