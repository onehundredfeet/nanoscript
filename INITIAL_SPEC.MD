Project Specification: The NanoScript Compiler
1. Goal
Build a compiler for "NanoScript," a minimalist imperative language. The compiler must translate .nano source code into LLVM IR using the LLVM C++ API. The output must be compatible with Clang and include DWARF debug metadata so it can be debuggable in VS Code using CodeLLDB on macOS.

2. Language Features
Data Types: 64-bit integers only (int64_t).

Variables: Implicitly declared on first assignment (e.g., x = 10;).

Arithmetic: +, -, *, /.

Conditionals: Simple if statements (no else required).

I/O: An out keyword that prints an integer followed by a newline.

Syntax: Statements end with ;. No functions or modules.

3. Technical Requirements
Implementation Language: C++ (Standard 17 or higher).

Build System: CMake.

Back-end: LLVM (Latest stable version available via brew install llvm).

Debug Info: Must use llvm::DIBuilder to generate:

DICompileUnit

DISubprogram (for a generated main function)

DILocation (attached to every generated LLVM instruction for line-mapping).

4. Phase-by-Phase Implementation Plan
Phase 1: Lexer & Parser
Create a simple Recursive Descent Parser.

Define an Abstract Syntax Tree (AST) where every node stores its LineNumber and ColumnNumber.

Phase 2: LLVM Code Generation
Implement a Codegen visitor class.

Generate a main function automatically as the entry point.

Use a std::map<std::string, llvm::AllocaInst*> to track variable memory locations.

Phase 3: Debug Metadata (Crucial)
Initialize llvm::DIBuilder.

Wrap all generated instructions with Builder.SetCurrentDebugLocation.

Ensure the DIFile points to the actual .nano file being compiled.

Phase 4: Integration & Tooling
Write a main.cpp that accepts a filename, runs the compiler, and outputs a .ll (LLVM Assembly) file.

Provide a shell script to automate the final build:
clang -g output.ll -o nano_binary

5. First Prompt to the LLM
"I want to build the NanoScript compiler defined in the plan above. Let's start with Phase 1 and 2. Please provide the C++ code for the AST nodes and a simple Parser that can handle variable assignments and the 'out' keyword. Ensure every AST node captures line numbers for future debug support."

Pro-Tips for success:
The "Out" Function: Since NanoScript doesn't have a standard library, tell the LLM to have the compiler automatically link to printf or a small C helper function for the out command.

M1 Architecture: Remind the LLM you are on Apple Silicon. It should ensure the LLVM Data Layout and Target Triple are set to arm64-apple-macosx.

Library Paths: Since you’ve had linker issues before, ask the LLM to generate a CMakeLists.txt that specifically uses find_package(LLVM) to locate the Homebrew-installed LLVM paths.