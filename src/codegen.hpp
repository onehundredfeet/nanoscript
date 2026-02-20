#pragma once
#include "ast.hpp"

#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>

#include <map>
#include <memory>
#include <string>

class Codegen {
public:
    /// sourceFile — basename of the .nano file (e.g. "hello.nano")
    /// sourceDir  — directory that contains it (e.g. "/home/user/examples")
    Codegen(const std::string& sourceFile, const std::string& sourceDir);

    void generate(const ProgramNode& program);
    void writeIR(const std::string& outputPath);

private:
    // ── LLVM core objects ─────────────────────────────────────────────────
    llvm::LLVMContext                context_;
    std::unique_ptr<llvm::Module>    module_;
    llvm::IRBuilder<>                builder_;

    // ── Cached LLVM types ─────────────────────────────────────────────────
    llvm::Type* int64Ty_ = nullptr;
    llvm::Type* int32Ty_ = nullptr;
    llvm::Type* ptrTy_   = nullptr; // opaque pointer (LLVM 17+)

    // ── Variable table: name → alloca ─────────────────────────────────────
    std::map<std::string, llvm::AllocaInst*> variables_;

    // ── printf machinery ──────────────────────────────────────────────────
    llvm::Function*       printfFn_ = nullptr;
    llvm::GlobalVariable* fmtStr_   = nullptr;

    // ── DWARF debug-info objects ──────────────────────────────────────────
    std::unique_ptr<llvm::DIBuilder> diBuilder_;
    llvm::DIFile*        diFile_        = nullptr;
    llvm::DICompileUnit* diCompileUnit_ = nullptr;
    llvm::DISubprogram*  diMainFunc_    = nullptr;
    llvm::DIType*        diInt64Ty_     = nullptr;

    // ── Private helpers ───────────────────────────────────────────────────
    void setupModule();
    void setupDebugInfo(const std::string& sourceFile, const std::string& sourceDir);
    void declarePrintf();
    llvm::Function* createMainFunction();

    /// Insert a new alloca in the entry block (before the first non-alloca).
    llvm::AllocaInst* createEntryAlloca(llvm::Function* fn, const std::string& name);

    /// Attach a debug location to every instruction that follows.
    void setDebugLoc(int line, int col);

    // ── Code generation ───────────────────────────────────────────────────
    void         genStatement (const StmtNode&      stmt, llvm::Function* fn);
    void         genAssignment(const AssignmentNode& node, llvm::Function* fn);
    void         genIf        (const IfNode&         node, llvm::Function* fn);
    void         genOut       (const OutNode&        node);
    llvm::Value* genExpr      (const ExprNode& expr);
};
