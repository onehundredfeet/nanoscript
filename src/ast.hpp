#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// ── Node kind tag — avoids dynamic_cast / RTTI dependency on LLVM ─────────
enum class NodeKind {
    IntLiteral,
    Variable,
    BinaryOp,
    Assignment,
    If,
    Out,
    Program,
};

// ── Base ──────────────────────────────────────────────────────────────────
struct ASTNode {
    NodeKind kind;
    int      line = 0;
    int      col  = 0;
    virtual ~ASTNode() = default;
protected:
    explicit ASTNode(NodeKind k) : kind(k) {}
};

// ── Expression nodes ──────────────────────────────────────────────────────
struct ExprNode : ASTNode {
protected:
    explicit ExprNode(NodeKind k) : ASTNode(k) {}
};

struct IntLiteralNode : ExprNode {
    int64_t value;
    IntLiteralNode(int64_t v, int ln, int cl)
        : ExprNode(NodeKind::IntLiteral), value(v) { line = ln; col = cl; }
};

struct VariableNode : ExprNode {
    std::string name;
    VariableNode(std::string n, int ln, int cl)
        : ExprNode(NodeKind::Variable), name(std::move(n)) { line = ln; col = cl; }
};

struct BinaryOpNode : ExprNode {
    std::string                opStr; // "+", "-", "*", "/", "==", "!=", "<", "<=", ">", ">="
    std::unique_ptr<ExprNode>  left;
    std::unique_ptr<ExprNode>  right;
    BinaryOpNode(std::string op,
                 std::unique_ptr<ExprNode> l,
                 std::unique_ptr<ExprNode> r,
                 int ln, int cl)
        : ExprNode(NodeKind::BinaryOp),
          opStr(std::move(op)), left(std::move(l)), right(std::move(r))
    { line = ln; col = cl; }
};

// ── Statement nodes ───────────────────────────────────────────────────────
struct StmtNode : ASTNode {
protected:
    explicit StmtNode(NodeKind k) : ASTNode(k) {}
};

struct AssignmentNode : StmtNode {
    std::string               varName;
    std::unique_ptr<ExprNode> value;
    AssignmentNode(std::string n, std::unique_ptr<ExprNode> v, int ln, int cl)
        : StmtNode(NodeKind::Assignment), varName(std::move(n)), value(std::move(v))
    { line = ln; col = cl; }
};

struct IfNode : StmtNode {
    std::unique_ptr<ExprNode>              condition;
    std::vector<std::unique_ptr<StmtNode>> body;
    IfNode(std::unique_ptr<ExprNode> cond,
           std::vector<std::unique_ptr<StmtNode>> b,
           int ln, int cl)
        : StmtNode(NodeKind::If), condition(std::move(cond)), body(std::move(b))
    { line = ln; col = cl; }
};

struct OutNode : StmtNode {
    std::unique_ptr<ExprNode> expr;
    OutNode(std::unique_ptr<ExprNode> e, int ln, int cl)
        : StmtNode(NodeKind::Out), expr(std::move(e))
    { line = ln; col = cl; }
};

// ── Program root ──────────────────────────────────────────────────────────
struct ProgramNode : ASTNode {
    std::vector<std::unique_ptr<StmtNode>> statements;
    ProgramNode() : ASTNode(NodeKind::Program) {}
};
