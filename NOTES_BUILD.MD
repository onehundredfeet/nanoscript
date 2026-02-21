# NanoScript Build Configuration Notes

## Three Configurations

| | Debug | Development | Shipping |
|---|---|---|---|
| **LLVM opt** | O0 — none | O2 — major passes | O3 — full |
| **Debug info** | Full DWARF | Full DWARF | Stripped |
| **Output** | `.wasm` | `.wasm` | Native binary |
| **Execution** | Wasmtime JIT | Wasmtime JIT | Direct OS load |
| **Hot-load** | Yes | Yes | No |

## Pipeline

Lexer → Parser → AST → Codegen → LLVM opt passes → output

The first four stages are identical across all configurations. The only branches are:

1. Whether `DIBuilder` runs (Debug + Dev only)
2. Which `OptimizationLevel` is passed to the LLVM pass pipeline
3. Whether the output targets Wasm (Debug/Dev) or native via clang (Ship)

## Why Wasm for Debug and Development

Wasmtime JIT provides hot-loading — a running application can swap in a newly compiled `.wasm` module at a safe point without restarting. This is the fast iteration path for both game client and game server during development.

The same `.nano` source produces the same logical program in all three configurations. Wasm is a development vehicle, not a shipping artifact.

## Why Native for Shipping

- 100% native performance — no runtime overhead from a Wasm layer
- No Wasmtime dependency ships in the product
- LLVM produces the same quality binary it would for any C++ application
- Harder to inspect/reverse than Wasm bytecode

## O2 vs O3

O2 hits all structurally important passes (mem2reg, GVN, inlining, LICM, constant folding) and compiles significantly faster than O3. The O3 additions — auto-vectorization and aggressive loop unrolling — are expensive to compute and only matter for hot numeric loops. For NanoScript's current int64-only arithmetic, O2 captures the meaningful wins. O3 is reserved for shipping where compile time is not a constraint.

`mem2reg` in particular is high-impact for NanoScript: all variables are emitted as stack allocas, and mem2reg promotes them to SSA registers, enabling most subsequent optimization passes to fire.

## Optimization Before Wasm Emission

LLVM optimization is applied to the IR before the Wasm backend runs, not after. This is deliberate — Cranelift (Wasmtime's JIT compiler) works from Wasm bytecode and cannot recover information lost during lowering. Running LLVM passes first means Cranelift starts from already-clean code and its own passes compound on top.
