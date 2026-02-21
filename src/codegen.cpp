#include "codegen.hpp"

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>

#include <stdexcept>

// ── Constructor ────────────────────────────────────────────────────────────

Codegen::Codegen(const std::string& sourceFile, const std::string& sourceDir,
                 BuildConfig config, bool wasm)
    : builder_(context_), config_(config), wasm_(wasm)
{
    setupModule();

    // Debug info: present for Debug and Development regardless of target
    if (config_ != BuildConfig::Shipping)
        setupDebugInfo(sourceFile, sourceDir);

    declarePrintf();

    int64Ty_ = llvm::Type::getInt64Ty(context_);
    int32Ty_ = llvm::Type::getInt32Ty(context_);
    ptrTy_   = llvm::PointerType::getUnqual(context_);   // opaque ptr

    if (diBuilder_)
        diInt64Ty_ = diBuilder_->createBasicType("int64", 64, llvm::dwarf::DW_ATE_signed);
}

// ── Module / target setup ─────────────────────────────────────────────────

void Codegen::setupModule() {
    module_ = std::make_unique<llvm::Module>("nanoscript", context_);

    if (wasm_) {
        // wasm32-wasi — used for all configs when --wasm is passed
        module_->setTargetTriple(llvm::Triple("wasm32-unknown-wasi"));
        module_->setDataLayout(
            "e-m:e-p:32:32-p10:8:8-p20:8:8-i64:64-i128:128-n32:64-S128-ni:1:10:20");
        if (config_ != BuildConfig::Shipping) {
            module_->addModuleFlag(llvm::Module::Warning, "Dwarf Version",      5);
            module_->addModuleFlag(llvm::Module::Warning, "Debug Info Version",
                                   llvm::DEBUG_METADATA_VERSION);
        }
    } else {
        // Native — use the host triple so clang doesn't warn about a mismatch
        module_->setTargetTriple(llvm::Triple(llvm::sys::getDefaultTargetTriple()));
        module_->setDataLayout(
            "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-n32:64-S128-Fn32");
        module_->addModuleFlag(llvm::Module::Max, "PIC Level", 2);
        if (config_ != BuildConfig::Shipping) {
            module_->addModuleFlag(llvm::Module::Warning, "Dwarf Version",      5);
            module_->addModuleFlag(llvm::Module::Warning, "Debug Info Version",
                                   llvm::DEBUG_METADATA_VERSION);
        }
    }
}

// ── DWARF debug-info setup ────────────────────────────────────────────────

void Codegen::setupDebugInfo(const std::string& sourceFile,
                              const std::string& sourceDir) {
    diBuilder_     = std::make_unique<llvm::DIBuilder>(*module_);
    diFile_        = diBuilder_->createFile(sourceFile, sourceDir);
    diCompileUnit_ = diBuilder_->createCompileUnit(
        llvm::dwarf::DW_LANG_C,
        diFile_,
        "NanoScript Compiler 1.0",
        /*isOptimized=*/config_ != BuildConfig::Debug,
        /*Flags=*/"",
        /*RuntimeVersion=*/0);
}

// ── printf declaration ────────────────────────────────────────────────────

void Codegen::declarePrintf() {
    // printf(ptr fmt, ...) -> i32
    auto* printfTy = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(context_),
        {llvm::PointerType::getUnqual(context_)},
        /*isVarArg=*/true);
    printfFn_ = llvm::Function::Create(
        printfTy, llvm::Function::ExternalLinkage, "printf", *module_);

    // Global constant: "%lld\n\0"  (6 bytes)
    auto* fmtData = llvm::ConstantDataArray::getString(context_, "%lld\n");
    fmtStr_ = new llvm::GlobalVariable(
        *module_,
        fmtData->getType(),
        /*isConstant=*/true,
        llvm::GlobalValue::PrivateLinkage,
        fmtData,
        ".fmt");
    fmtStr_->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
    fmtStr_->setAlignment(llvm::Align(1));
}

// ── main function scaffolding ─────────────────────────────────────────────

llvm::Function* Codegen::createMainFunction() {
    auto* mainTy = llvm::FunctionType::get(int32Ty_, /*isVarArg=*/false);
    auto* mainFn = llvm::Function::Create(
        mainTy, llvm::Function::ExternalLinkage, "main", *module_);

    if (wasm_) {
        // wasm32-wasi: crt1's _start calls __main_void, not main directly.
        // Emit the hidden alias clang would generate from C source so that
        // wasm-ld can resolve the undefined_weak:main reference in crt1.
        mainFn->setVisibility(llvm::GlobalValue::HiddenVisibility);
        auto* alias = llvm::GlobalAlias::create(
            mainTy, 0,
            llvm::GlobalValue::ExternalLinkage,
            "__main_void", mainFn, module_.get());
        alias->setVisibility(llvm::GlobalValue::HiddenVisibility);
    }

    if (diBuilder_) {
        auto* retDITy = diBuilder_->createBasicType("int", 32, llvm::dwarf::DW_ATE_signed);
        auto* subTy   = diBuilder_->createSubroutineType(
            diBuilder_->getOrCreateTypeArray({retDITy}));
        diMainFunc_ = diBuilder_->createFunction(
            diCompileUnit_,
            "main", "main",
            diFile_,
            /*LineNo=*/1,
            subTy,
            /*ScopeLine=*/1,
            llvm::DINode::FlagPrototyped,
            llvm::DISubprogram::SPFlagDefinition);
        mainFn->setSubprogram(diMainFunc_);
    }

    auto* entry = llvm::BasicBlock::Create(context_, "entry", mainFn);
    builder_.SetInsertPoint(entry);
    builder_.SetCurrentDebugLocation(llvm::DebugLoc());
    return mainFn;
}

// ── Alloca helper ─────────────────────────────────────────────────────────

llvm::AllocaInst* Codegen::createEntryAlloca(llvm::Function* fn,
                                              const std::string& name) {
    auto savedIP = builder_.saveIP();
    auto& entry  = fn->getEntryBlock();
    auto  it     = entry.begin();
    while (it != entry.end() && llvm::isa<llvm::AllocaInst>(*it))
        ++it;
    builder_.SetInsertPoint(&entry, it);
    auto* alloca = builder_.CreateAlloca(int64Ty_, nullptr, name);
    builder_.restoreIP(savedIP);
    return alloca;
}

// ── Debug location helper ─────────────────────────────────────────────────

void Codegen::setDebugLoc(int line, int col) {
    if (!diMainFunc_) return;
    builder_.SetCurrentDebugLocation(
        llvm::DILocation::get(context_,
                              static_cast<unsigned>(line),
                              static_cast<unsigned>(col),
                              diMainFunc_));
}

// ── Top-level generate ────────────────────────────────────────────────────

void Codegen::generate(const ProgramNode& program) {
    llvm::Function* mainFn = createMainFunction();

    for (const auto& stmt : program.statements)
        genStatement(*stmt, mainFn);

    setDebugLoc(1, 1);
    builder_.CreateRet(llvm::ConstantInt::get(int32Ty_, 0));

    if (diBuilder_) {
        diBuilder_->finalizeSubprogram(diMainFunc_);
        diBuilder_->finalize();
    }

    optimize();

    std::string errors;
    llvm::raw_string_ostream es(errors);
    if (llvm::verifyModule(*module_, &es)) {
        es.flush();
        throw std::runtime_error("LLVM module verification failed:\n" + errors);
    }
}

// ── Statement dispatch ────────────────────────────────────────────────────

void Codegen::genStatement(const StmtNode& stmt, llvm::Function* fn) {
    switch (stmt.kind) {
        case NodeKind::Assignment:
            genAssignment(static_cast<const AssignmentNode&>(stmt), fn);
            break;
        case NodeKind::If:
            genIf(static_cast<const IfNode&>(stmt), fn);
            break;
        case NodeKind::Out:
            genOut(static_cast<const OutNode&>(stmt));
            break;
        default:
            throw std::runtime_error("Unknown statement kind in codegen");
    }
}

// ── Assignment ────────────────────────────────────────────────────────────

void Codegen::genAssignment(const AssignmentNode& node, llvm::Function* fn) {
    setDebugLoc(node.line, node.col);

    if (variables_.find(node.varName) == variables_.end()) {
        auto* alloca = createEntryAlloca(fn, node.varName);
        variables_[node.varName] = alloca;

        if (diBuilder_) {
            auto* diVar = diBuilder_->createAutoVariable(
                diMainFunc_, node.varName, diFile_,
                static_cast<unsigned>(node.line), diInt64Ty_);
            auto* loc = llvm::DILocation::get(context_,
                                              static_cast<unsigned>(node.line),
                                              static_cast<unsigned>(node.col),
                                              diMainFunc_);
            diBuilder_->insertDeclare(
                alloca, diVar,
                diBuilder_->createExpression(),
                loc,
                builder_.GetInsertBlock());
        }
    }

    llvm::Value* val = genExpr(*node.value);
    setDebugLoc(node.line, node.col);
    builder_.CreateStore(val, variables_[node.varName]);
}

// ── If statement ──────────────────────────────────────────────────────────

void Codegen::genIf(const IfNode& node, llvm::Function* fn) {
    setDebugLoc(node.line, node.col);

    llvm::Value* cond = genExpr(*node.condition);

    if (cond->getType() == int64Ty_) {
        setDebugLoc(node.line, node.col);
        cond = builder_.CreateICmpNE(cond, llvm::ConstantInt::get(int64Ty_, 0), "ifcond");
    }

    auto* thenBB  = llvm::BasicBlock::Create(context_, "then",  fn);
    auto* mergeBB = llvm::BasicBlock::Create(context_, "merge", fn);

    builder_.CreateCondBr(cond, thenBB, mergeBB);

    builder_.SetInsertPoint(thenBB);
    for (const auto& s : node.body)
        genStatement(*s, fn);

    if (!builder_.GetInsertBlock()->getTerminator())
        builder_.CreateBr(mergeBB);

    builder_.SetInsertPoint(mergeBB);
}

// ── Out statement ─────────────────────────────────────────────────────────

void Codegen::genOut(const OutNode& node) {
    setDebugLoc(node.line, node.col);

    llvm::Value* val = genExpr(*node.expr);
    setDebugLoc(node.line, node.col);

    auto* zero   = llvm::ConstantInt::get(int32Ty_, 0);
    auto* fmtPtr = builder_.CreateInBoundsGEP(
        fmtStr_->getValueType(), fmtStr_, {zero, zero}, "fmtptr");

    builder_.CreateCall(printfFn_, {fmtPtr, val});
}

// ── Expression dispatch ───────────────────────────────────────────────────

llvm::Value* Codegen::genExpr(const ExprNode& expr) {
    switch (expr.kind) {
        case NodeKind::IntLiteral: {
            const auto& n = static_cast<const IntLiteralNode&>(expr);
            setDebugLoc(n.line, n.col);
            return llvm::ConstantInt::get(int64Ty_, n.value, /*isSigned=*/true);
        }
        case NodeKind::Variable: {
            const auto& n = static_cast<const VariableNode&>(expr);
            setDebugLoc(n.line, n.col);
            auto it = variables_.find(n.name);
            if (it == variables_.end())
                throw std::runtime_error(
                    "Undefined variable '" + n.name + "' at line " +
                    std::to_string(n.line));
            return builder_.CreateLoad(int64Ty_, it->second, n.name);
        }
        case NodeKind::BinaryOp: {
            const auto& n    = static_cast<const BinaryOpNode&>(expr);
            llvm::Value* lhs = genExpr(*n.left);
            llvm::Value* rhs = genExpr(*n.right);
            setDebugLoc(n.line, n.col);

            if (n.opStr == "+") return builder_.CreateAdd (lhs, rhs, "add");
            if (n.opStr == "-") return builder_.CreateSub (lhs, rhs, "sub");
            if (n.opStr == "*") return builder_.CreateMul (lhs, rhs, "mul");
            if (n.opStr == "/") return builder_.CreateSDiv(lhs, rhs, "div");

            llvm::Value* cmp = nullptr;
            if      (n.opStr == "==") cmp = builder_.CreateICmpEQ (lhs, rhs, "eq");
            else if (n.opStr == "!=") cmp = builder_.CreateICmpNE (lhs, rhs, "ne");
            else if (n.opStr == "<")  cmp = builder_.CreateICmpSLT(lhs, rhs, "lt");
            else if (n.opStr == ">")  cmp = builder_.CreateICmpSGT(lhs, rhs, "gt");
            else if (n.opStr == "<=") cmp = builder_.CreateICmpSLE(lhs, rhs, "le");
            else if (n.opStr == ">=") cmp = builder_.CreateICmpSGE(lhs, rhs, "ge");
            else throw std::runtime_error("Unknown operator: " + n.opStr);
            return builder_.CreateSExt(cmp, int64Ty_, "cmpext");
        }
        default:
            throw std::runtime_error("Unknown expression kind in codegen");
    }
}

// ── Optimisation pipeline ─────────────────────────────────────────────────

void Codegen::optimize() {
    if (config_ == BuildConfig::Debug) return;

    llvm::PassBuilder            pb;
    llvm::LoopAnalysisManager    lam;
    llvm::FunctionAnalysisManager fam;
    llvm::CGSCCAnalysisManager   cgam;
    llvm::ModuleAnalysisManager  mam;

    pb.registerModuleAnalyses(mam);
    pb.registerCGSCCAnalyses(cgam);
    pb.registerFunctionAnalyses(fam);
    pb.registerLoopAnalyses(lam);
    pb.crossRegisterProxies(lam, fam, cgam, mam);

    llvm::ModulePassManager mpm;
    if (config_ == BuildConfig::Development) {
        mpm = pb.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);
    } else {
        // Shipping: full LTO pipeline at O3 — whole-program optimisation
        // ExportSummary=nullptr: single-module build, no cross-module index needed
        mpm = pb.buildLTODefaultPipeline(llvm::OptimizationLevel::O3, nullptr);
    }
    mpm.run(*module_, mam);
}

// ── IR output ─────────────────────────────────────────────────────────────

void Codegen::writeIR(const std::string& outputPath) {
    std::error_code ec;
    llvm::raw_fd_ostream out(outputPath, ec, llvm::sys::fs::OF_Text);
    if (ec)
        throw std::runtime_error("Cannot open output file '" + outputPath +
                                 "': " + ec.message());
    module_->print(out, nullptr);
}
