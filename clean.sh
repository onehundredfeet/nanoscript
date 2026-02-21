#!/usr/bin/env bash
# ── NanoScript clean script ──────────────────────────────────────────────────
set -euo pipefail

echo "Cleaning NanoScript build artifacts..."

# CMake build directory
rm -rf build/

# Compiled nano artifacts (native binaries + wasm)
rm -f nano_binary nano_binary.wasm
rm -f hello hello.wasm
rm -f hello_debug hello_debug.wasm
rm -f hello_dev hello_dev.wasm
rm -f output output.wasm

# Old intermediate files from build.sh
rm -f output.ll output.o

# dSYM debug bundles (macOS)
rm -rf ./*.dSYM

# Temp .ll files emitted by the compiler (*.tmp.ll)
rm -f ./*.tmp.ll

# Any stray .ll or .o at repo root
rm -f ./*.ll ./*.o

echo "Done."
