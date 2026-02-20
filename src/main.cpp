#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "codegen.hpp"
#include "lexer.hpp"
#include "parser.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: nanoscript <source.nano> [output.ll]\n";
        return 1;
    }

    const std::string inputFile  = argv[1];
    const std::string outputFile = argc >= 3 ? argv[2] : "output.ll";

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
        // 1. Lex
        Lexer lexer(source, inputFile);
        auto  tokens = lexer.tokenize();

        // 2. Parse
        Parser parser(std::move(tokens));
        auto   ast = parser.parse();

        // 3. Code-gen with debug info
        // Use absolute paths so LLDB can locate the source without a sourceMap.
        std::filesystem::path p = std::filesystem::absolute(inputFile);
        const std::string srcFile = p.filename().string();
        const std::string srcDir  = p.parent_path().string();

        Codegen cg(srcFile, srcDir);
        cg.generate(*ast);
        cg.writeIR(outputFile);

        std::cout << "Compiled '" << inputFile << "' → '" << outputFile << "'\n";
        std::cout << "Link:  clang -g " << outputFile << " -o nano_binary\n";
        std::cout << "Run:   ./nano_binary\n";

    } catch (const std::exception& e) {
        std::cerr << "Compilation error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
