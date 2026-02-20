#pragma once
#include "lexer.hpp"
#include "ast.hpp"
#include <memory>
#include <vector>

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    std::unique_ptr<ProgramNode> parse();

private:
    std::vector<Token> tokens_;
    size_t             pos_ = 0;

    const Token& peek(int offset = 0) const;
    const Token& advance();
    bool         check(TokenType type) const;
    bool         match(TokenType type);
    const Token& expect(TokenType type, const std::string& msg);

    std::unique_ptr<StmtNode> parseStatement();
    std::unique_ptr<StmtNode> parseAssignment();
    std::unique_ptr<StmtNode> parseIf();
    std::unique_ptr<StmtNode> parseOut();

    // Expression grammar (precedence climbing):
    //   expr       → comparison
    //   comparison → addSub (('==' | '!=' | '<' | '>' | '<=' | '>=') addSub)?
    //   addSub     → mulDiv (('+' | '-') mulDiv)*
    //   mulDiv     → primary (('*' | '/') primary)*
    //   primary    → INT_LITERAL | IDENT | '(' expr ')'
    std::unique_ptr<ExprNode> parseExpr();
    std::unique_ptr<ExprNode> parseComparison();
    std::unique_ptr<ExprNode> parseAddSub();
    std::unique_ptr<ExprNode> parseMulDiv();
    std::unique_ptr<ExprNode> parsePrimary();
};
