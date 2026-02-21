# NanoScript

> **Proof of concept — complete, no further updates planned.**
> NanoScript exists solely to demonstrate a minimal end-to-end compiler toolchain — compiler, debugger integration, syntax highlighting, and code completion — in as few moving parts as possible. The implementation is finished and this repository will not receive further development. It is not intended for production use.

---

## What it is

NanoScript is a toy imperative language whose entire feature set fits on a napkin:

- **One data type** — 64-bit integers
- **Implicit variables** — declared on first assignment (`x = 10;`)
- **Arithmetic** — `+`, `-`, `*`, `/`
- **Comparisons** — `==`, `!=`, `<`, `<=`, `>`, `>=`
- **Conditionals** — `if (expr) { ... }` (no `else`)
- **Output** — `out expr;` prints an integer followed by a newline
- **No** loops, functions, strings, arrays, or modules

The language exists to give the toolchain something concrete to operate on. It is deliberately small so every layer of the stack can be read and understood in an afternoon.

## What the toolchain demonstrates

| Layer | Implementation |
|---|---|
| Lexer | Hand-written in C++ (`src/lexer.cpp`) |
| Recursive-descent parser | C++ (`src/parser.cpp`) |
| AST with source locations | C++ (`src/ast.hpp`) |
| LLVM IR code generation | C++ via LLVM C++ API (`src/codegen.cpp`) |
| DWARF debug metadata | `llvm::DIBuilder` — line/column mapped to every instruction |
| Native binary | Homebrew clang links the emitted `.ll` |
| WebAssembly binary | `wasm32-wasi` target + wasi-libc, runs under Wasmtime |
| Debugger | VS Code + CodeLLDB; native and wasm (via Wasmtime JIT-DWARF) |
| Syntax highlighting | VS Code TextMate grammar (`nano-language-support/`) |
| Code completion | VS Code language extension (`nano-language-support/`) |

## Build configurations

The compiler accepts orthogonal `--config` and `--wasm` flags, giving six combinations:

```
nanoscript <source.nano> [--config=debug|development|shipping] [--wasm] [output]
```

| Config | Optimisation | Debug info |
|---|---|---|
| `debug` (default) | O0 | Full DWARF |
| `development` | O2 | Full DWARF |
| `shipping` | O3 + LTO | None |

Omit `--wasm` to produce a native binary; add it to produce a `.wasm` file runnable with Wasmtime.

## Quick start

**Prerequisites (macOS / Apple Silicon)**

```bash
brew install llvm cmake wasmtime wasi-libc wasi-runtimes
```

See [SETUP.MD](SETUP.MD) for the full toolchain setup including the compiler-rt symlink required for wasm builds.

**Build the compiler**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.logicalcpu)
```

**Compile and run a NanoScript program**

```bash
# Native binary
./build/nanoscript examples/hello.nano
./hello

# WebAssembly
./build/nanoscript examples/hello.nano --wasm
wasmtime hello.wasm
```

**Clean build artifacts**

```bash
./clean.sh
```

## Debugging in VS Code

Install [CodeLLDB](https://marketplace.visualstudio.com/items?itemName=vadimcn.vscode-lldb) and the bundled `nano-language-support` extension, then use the pre-configured launch profiles:

- **Debug (native binary)** — standard LLDB attach, full breakpoint and variable support
- **Debug (Wasmtime / JIT)** — LLDB reads Wasmtime's JIT-registered DWARF via the GDB JIT interface; breakpoints and source stepping work, variable inspection has known limitations at the wasm JIT boundary

## Project structure

```
src/
  lexer.hpp / lexer.cpp       Tokeniser
  parser.hpp / parser.cpp     Recursive-descent parser
  ast.hpp                     AST node definitions
  codegen.hpp / codegen.cpp   LLVM IR + DWARF emission
  main.cpp                    CLI driver
nano-language-support/        VS Code extension (syntax + completion)
examples/                     Sample .nano programs
build.sh                      Quick build-and-run script
clean.sh                      Remove all build artifacts
CMakeLists.txt
```

## Limitations (by design)

This is a proof of concept. Among the things it intentionally omits:

- No type system beyond `int64`
- No `else`, loops, functions, closures, or modules
- No standard library
- No error recovery in the parser (first error aborts)
- No package manager (design notes in progress)
- Toolchain paths are hardcoded to Homebrew on Apple Silicon

## License

See [LICENSE](LICENSE).
