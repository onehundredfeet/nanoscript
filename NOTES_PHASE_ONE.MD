# Phase 1 & 2 Implementation Notes

## LLVM Version Reality Check

`brew install llvm` installs **LLVM 21** (21.1.8 at time of writing), not 17 or 18 as many tutorials assume. Two breaking API changes hit immediately:

1. **`Module::setTargetTriple` no longer accepts a string literal.** It now requires an explicit `llvm::Triple` object:
   ```cpp
   // BROKEN in LLVM 21:
   module_->setTargetTriple("arm64-apple-macosx13.0.0");

   // CORRECT:
   #include <llvm/TargetParser/Triple.h>
   module_->setTargetTriple(llvm::Triple("arm64-apple-macosx13.0.0"));
   ```

2. **`project()` must list both `C` and `CXX`.** LLVM's own `FindLibEdit.cmake` (pulled in by `find_package(LLVM)`) calls `check_include_file`, which requires the C compiler to be enabled. A CXX-only project fails at configure time with a cryptic "unknown extension .c" error.
   ```cmake
   project(NanoScript C CXX)   # C is required by LLVM's FindLibEdit.cmake
   ```

---

## CMake: Use the Imported `LLVM` Target, Not Components

On a Homebrew install, LLVM ships as a **single shared dylib** (`libLLVM-21.dylib`). The CMake integration exports it as an imported target named `LLVM`. Linking against it is simpler and more reliable than using `llvm_map_components_to_libnames`:

```cmake
# Simpler and correct for Homebrew:
target_link_libraries(nanoscript PRIVATE LLVM)

# Avoid this pattern — component resolution works but produces a long
# list that may miss transitive deps or pick the wrong link order:
# llvm_map_components_to_libnames(LLVM_LIBS core support analysis)
```

---

## Target Triple vs. Data Layout

We hardcode both instead of querying them at runtime via `TargetRegistry`. This means we don't need to initialize the AArch64 backend (no `InitializeAllTargets()` call), which keeps the binary small and avoids linking in machine-code-generation machinery we don't need — we only emit `.ll` text.

The correct data layout for `arm64-apple-macosx` was obtained by asking clang:
```bash
echo 'void f(){}' | clang -target arm64-apple-macosx13.0.0 -S -emit-llvm -x c - -o - | grep datalayout
```
Result: `"e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-n32:64-S128-Fn32"`

The triple version (`macosx13.0.0`) doesn't need to match the host OS. Clang overrides it with the actual SDK version at link time with a harmless `-Woverride-module` warning.

---

## DWARF Module Flags Are Not Optional

`DIBuilder::finalize()` does **not** automatically emit all required module flags. Without explicit flags, clang produces `"ignoring debug info with an invalid version (0)"` and strips the DWARF entirely. The minimum set needed:

```cpp
module_->addModuleFlag(llvm::Module::Warning, "Dwarf Version",     5);
module_->addModuleFlag(llvm::Module::Warning, "Debug Info Version", llvm::DEBUG_METADATA_VERSION); // == 3
module_->addModuleFlag(llvm::Module::Max,     "PIC Level",          2);
```

DWARF 5 is what clang 21 defaults to. Using 4 also works but 5 gives better variable tracking in LLDB.

---

## RTTI: Avoid `dynamic_cast` on AST Nodes

LLVM is compiled with `-fno-rtti`. While you *can* use `dynamic_cast` on your own types in code that links against LLVM, the cleaner and safer approach is a `NodeKind` enum on the base class, dispatched with a `switch` and `static_cast`:

```cpp
// In ast.hpp — no RTTI needed:
enum class NodeKind { IntLiteral, Variable, BinaryOp, Assignment, If, Out, Program };
struct ASTNode { NodeKind kind; ... };

// In codegen.cpp — safe, no dynamic_cast:
switch (expr.kind) {
    case NodeKind::IntLiteral:
        return genIntLiteral(static_cast<const IntLiteralNode&>(expr));
    ...
}
```

This is the same pattern LLVM itself uses internally (`llvm::isa<>`, `llvm::cast<>`).

---

## Alloca Placement Strategy

All `AllocaInst`s must live in the **entry block** of the function, not at the point of first assignment. This is required for LLVM's `mem2reg` pass (which promotes allocas to SSA registers) and for correct debugger behavior. The implementation scans forward past existing allocas to insert each new one in order:

```cpp
auto& entry = fn->getEntryBlock();
auto  it    = entry.begin();
while (it != entry.end() && llvm::isa<llvm::AllocaInst>(*it)) ++it;
builder_.SetInsertPoint(&entry, it);   // insert after last alloca
auto* alloca = builder_.CreateAlloca(int64Ty_, nullptr, name);
builder_.restoreIP(savedIP);           // return to original insert point
```

---

## `printf` With Opaque Pointers (LLVM 17+)

In LLVM 17+, all pointer types are opaque (`ptr`). The format string GEP must pass the **element type** explicitly:

```cpp
// Declare printf: (ptr, ...) -> i32
auto* printfTy = llvm::FunctionType::get(int32Ty_,
    {llvm::PointerType::getUnqual(context_)}, /*isVarArg=*/true);

// GEP into the format string global — element type is the array type
auto* fmtPtr = builder_.CreateInBoundsGEP(
    fmtStr_->getValueType(), fmtStr_, {zero, zero}, "fmtptr");
```

`PointerType::getUnqual(Type*)` (element-type version) is deprecated in LLVM 21. Use `PointerType::getUnqual(LLVMContext&)` instead.

---

## Two-Step Link Required for a Valid dSYM

Compiling `.ll` directly to a binary in one step (`clang -g output.ll -o binary`) leaves DWARF orphaned — `dsymutil` cannot find the intermediate `.o` file and produces an empty `.dSYM`. The correct pipeline:

```bash
clang -g -c output.ll -o output.o   # compile: DWARF stays in the .o
clang -g output.o -o nano_binary     # link
dsymutil nano_binary                 # extract DWARF → nano_binary.dSYM
```

CodeLLDB reads the `.dSYM` bundle, so this step is required for VS Code breakpoints to resolve to `.nano` source lines.

---

## `#dbg_declare` vs. `llvm.dbg.declare`

In LLVM 19+, the old `llvm.dbg.declare` intrinsic is replaced by a new non-instruction `#dbg_declare` record. `DIBuilder::insertDeclare` now returns `DbgInstPtr` (a `PointerUnion<Instruction*, DbgRecord*>`). The call site is unchanged — just ignore the return value. The generated `.ll` will contain `#dbg_declare(...)` syntax instead of `call void @llvm.dbg.declare(...)`, which is correct for LLVM 21.

---

## What Was Collapsed (Phases 1–4 merged)

The spec described four sequential phases, but phases 1–4 were implemented together in a single session because:

- Phase 3 (debug metadata) cannot be bolted on after Phase 2 — the `DISubprogram` must be attached to the function *before* any instructions are emitted, and `SetCurrentDebugLocation` must be called at the start of codegen, not retrofitted.
- Phase 4 tooling (`main.cpp`, `build.sh`, `.vscode/`) is trivial once the compiler core works and has no architectural implications.

The implementation boundary that *does* matter for future phases: the `Codegen` class owns both the `IRBuilder` and the `DIBuilder`, which is intentional. Separating them would require threading the debug builder through every code-gen call.
