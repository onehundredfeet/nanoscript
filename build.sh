#!/usr/bin/env bash
# ── NanoScript one-shot build & run script ──────────────────────────────────
set -euo pipefail

NANO_FILE="${1:-examples/hello.nano}"
OUTPUT_LL="output.ll"
OUTPUT_BIN="nano_binary"
BUILD_DIR="build"
CLANG="/opt/homebrew/opt/llvm/bin/clang"   # Homebrew clang (Apple Silicon)

echo "╔══════════════════════════════════════╗"
echo "║  Building NanoScript compiler        ║"
echo "╚══════════════════════════════════════╝"
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -Wno-dev
cmake --build "$BUILD_DIR" -j"$(sysctl -n hw.logicalcpu)"

echo ""
echo "╔══════════════════════════════════════╗"
echo "║  Compiling $NANO_FILE"
echo "╚══════════════════════════════════════╝"
"./${BUILD_DIR}/nanoscript" "$NANO_FILE" "$OUTPUT_LL"

echo ""
echo "╔══════════════════════════════════════╗"
echo "║  Linking with clang -g               ║"
echo "╚══════════════════════════════════════╝"
# Two-step: .ll → .o → binary so dsymutil can locate the object file
"$CLANG" -g -c "$OUTPUT_LL" -o output.o
"$CLANG" -g output.o -o "$OUTPUT_BIN"
dsymutil "$OUTPUT_BIN"   # extract DWARF into nano_binary.dSYM for CodeLLDB

echo ""
echo "╔══════════════════════════════════════╗"
echo "║  Running $OUTPUT_BIN                 ║"
echo "╚══════════════════════════════════════╝"
"./$OUTPUT_BIN"
