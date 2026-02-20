#include "parser.hpp"
#include <stdexcept>

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

const Token& Parser::peek(int offset) const {
    size_t idx = pos_ + static_cast<size_t>(offset);
    // Return the EOF token if we're past the end
    return idx < tokens_.size() ? tokens_[idx] : tokens_.back();
}

const Token& Parser::advance() {
    if (pos_ < tokens_.size()) ++pos_;
    return tokens_[pos_ - 1];
}

bool Parser::check(TokenType type) const {
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) { advance(); return true; }
    return false;
}

const Token& Parser::expect(TokenType type, const std::string& msg) {
    if (!check(type))
        throw std::runtime_error(
            "Parse error at line " + std::to_string(peek().line) +
            ", col " + std::to_string(peek().col) +
            ": " + msg + " (got '" + peek().value + "')");
    return advance();
}

// ── Top-level ─────────────────────────────────────────────────────────────

std::unique_ptr<ProgramNode> Parser::parse() {
    auto prog = std::make_unique<ProgramNode>();
    while (!check(TokenType::EOF_TOKEN))
        prog->statements.push_back(parseStatement());
    return prog;
}

// ── Statements ────────────────────────────────────────────────────────────

std::unique_ptr<StmtNode> Parser::parseStatement() {
    if (check(TokenType::IF))         return parseIf();
    if (check(TokenType::OUT))        return parseOut();
    if (check(TokenType::IDENTIFIER)) return parseAssignment();
    throw std::runtime_error(
        "Parse error at line " + std::to_string(peek().line) +
        ": unexpected token '" + peek().value + "'");
}

std::unique_ptr<StmtNode> Parser::parseAssignment() {
    const Token& id = expect(TokenType::IDENTIFIER, "Expected identifier");
    int ln = id.line, cl = id.col;
    expect(TokenType::ASSIGN, "Expected '=' after identifier");
    auto val = parseExpr();
    expect(TokenType::SEMICOLON, "Expected ';' after expression");
    return std::make_unique<AssignmentNode>(id.value, std::move(val), ln, cl);
}

std::unique_ptr<StmtNode> Parser::parseIf() {
    const Token& tok = expect(TokenType::IF, "Expected 'if'");
    int ln = tok.line, cl = tok.col;
    expect(TokenType::LPAREN, "Expected '(' after 'if'");
    auto cond = parseExpr();
    expect(TokenType::RPAREN, "Expected ')' after condition");
    expect(TokenType::LBRACE, "Expected '{' to open if-body");

    std::vector<std::unique_ptr<StmtNode>> body;
    while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN))
        body.push_back(parseStatement());

    expect(TokenType::RBRACE, "Expected '}' to close if-body");
    return std::make_unique<IfNode>(std::move(cond), std::move(body), ln, cl);
}

std::unique_ptr<StmtNode> Parser::parseOut() {
    const Token& tok = expect(TokenType::OUT, "Expected 'out'");
    int ln = tok.line, cl = tok.col;
    auto expr = parseExpr();
    expect(TokenType::SEMICOLON, "Expected ';' after out-expression");
    return std::make_unique<OutNode>(std::move(expr), ln, cl);
}

// ── Expressions ───────────────────────────────────────────────────────────

std::unique_ptr<ExprNode> Parser::parseExpr() {
    return parseComparison();
}

std::unique_ptr<ExprNode> Parser::parseComparison() {
    auto left = parseAddSub();
    while (check(TokenType::EQ)  || check(TokenType::NEQ) ||
           check(TokenType::LT)  || check(TokenType::GT)  ||
           check(TokenType::LEQ) || check(TokenType::GEQ)) {
        const Token& op = advance();
        int ln = op.line, cl = op.col;
        auto right = parseAddSub();
        left = std::make_unique<BinaryOpNode>(op.value, std::move(left), std::move(right), ln, cl);
    }
    return left;
}

std::unique_ptr<ExprNode> Parser::parseAddSub() {
    auto left = parseMulDiv();
    while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
        const Token& op = advance();
        int ln = op.line, cl = op.col;
        auto right = parseMulDiv();
        left = std::make_unique<BinaryOpNode>(op.value, std::move(left), std::move(right), ln, cl);
    }
    return left;
}

std::unique_ptr<ExprNode> Parser::parseMulDiv() {
    auto left = parsePrimary();
    while (check(TokenType::STAR) || check(TokenType::SLASH)) {
        const Token& op = advance();
        int ln = op.line, cl = op.col;
        auto right = parsePrimary();
        left = std::make_unique<BinaryOpNode>(op.value, std::move(left), std::move(right), ln, cl);
    }
    return left;
}

std::unique_ptr<ExprNode> Parser::parsePrimary() {
    if (check(TokenType::INT_LITERAL)) {
        const Token& tok = advance();
        return std::make_unique<IntLiteralNode>(std::stoll(tok.value), tok.line, tok.col);
    }
    if (check(TokenType::IDENTIFIER)) {
        const Token& tok = advance();
        return std::make_unique<VariableNode>(tok.value, tok.line, tok.col);
    }
    if (check(TokenType::LPAREN)) {
        advance(); // consume '('
        auto expr = parseExpr();
        expect(TokenType::RPAREN, "Expected ')' to close expression");
        return expr;
    }
    throw std::runtime_error(
        "Parse error at line " + std::to_string(peek().line) +
        ": expected expression, got '" + peek().value + "'");
}
