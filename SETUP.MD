# NanoScript — macOS Setup (Homebrew)

## Prerequisites

```bash
# 1. LLVM 21 (compiler + Wasm backend)
brew install llvm

# 2. Wasmtime (runtime for debug/development .wasm output)
brew install wasmtime

# 3. wasi-libc (C standard library for Wasm — required for 'out' / printf)
brew install wasi-libc

# 4. lld (provides wasm-ld, the Wasm linker)
brew install lld
```

### Post-install: link compiler-rt builtins into LLVM's resource directory

wasi-runtimes installs `libclang_rt.builtins-wasm32.a` in a non-standard path.
Create a symlink so that Homebrew clang can find it when linking Wasm binaries:

```bash
LLVM_VER=$(ls /opt/homebrew/Cellar/llvm/)
mkdir -p /opt/homebrew/Cellar/llvm/${LLVM_VER}/lib/clang/21/lib/wasm32-unknown-wasi
ln -sf /opt/homebrew/Cellar/wasi-runtimes/$(ls /opt/homebrew/Cellar/wasi-runtimes/)/share/wasi-runtimes/lib/wasi/libclang_rt.builtins-wasm32.a \
       /opt/homebrew/Cellar/llvm/${LLVM_VER}/lib/clang/21/lib/wasm32-unknown-wasi/libclang_rt.builtins.a
```

> **Note:** if you upgrade LLVM via Homebrew, re-run the symlink step with the new version number.

## Build the compiler

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The `nanoscript` binary is produced in `build/`.

## Usage

```bash
nanoscript <source.nano> [--config=debug|development|shipping] [output]
```

| Config | Optimisation | Debug info | Output | Run with |
|---|---|---|---|---|
| `debug` *(default)* | O0 | DWARF | `<stem>.wasm` | `wasmtime` |
| `development` | O2 | DWARF | `<stem>.wasm` | `wasmtime` |
| `shipping` | O3 | none | native binary | directly |

```bash
# Debug — hot-loadable Wasm, full debug info
build/nanoscript examples/hello.nano --config=debug
wasmtime hello.wasm

# Development — optimised Wasm, full debug info
build/nanoscript examples/hello.nano --config=development
wasmtime hello.wasm

# Shipping — native binary, maximum optimisation
build/nanoscript examples/hello.nano --config=shipping
./hello
```
