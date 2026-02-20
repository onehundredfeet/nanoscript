#pragma once
#include <string>
#include <vector>

enum class TokenType {
    // Literals
    INT_LITERAL,
    // Identifiers / keywords
    IDENTIFIER,
    IF,
    OUT,
    // Operators
    ASSIGN,   // =
    PLUS,     // +
    MINUS,    // -
    STAR,     // *
    SLASH,    // /
    EQ,       // ==
    NEQ,      // !=
    LT,       // <
    GT,       // >
    LEQ,      // <=
    GEQ,      // >=
    // Delimiters
    SEMICOLON,
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    // End of input
    EOF_TOKEN,
};

struct Token {
    TokenType   type;
    std::string value;
    int         line;
    int         col;
};

class Lexer {
public:
    Lexer(const std::string& source, const std::string& filename);
    std::vector<Token> tokenize();

private:
    const std::string& source_;
    std::string        filename_;
    size_t             pos_  = 0;
    int                line_ = 1;
    int                col_  = 1;

    char        peek(int offset = 0) const;
    char        advance();
    void        skipWhitespaceAndComments();
    Token       lexNumber();
    Token       lexIdentifierOrKeyword();
};
