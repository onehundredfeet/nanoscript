#include "lexer.hpp"
#include <cctype>
#include <stdexcept>

Lexer::Lexer(const std::string& source, const std::string& filename)
    : source_(source), filename_(filename) {}

char Lexer::peek(int offset) const {
    size_t idx = pos_ + static_cast<size_t>(offset);
    return idx < source_.size() ? source_[idx] : '\0';
}

char Lexer::advance() {
    char c = source_[pos_++];
    if (c == '\n') { ++line_; col_ = 1; }
    else           { ++col_;            }
    return c;
}

void Lexer::skipWhitespaceAndComments() {
    while (pos_ < source_.size()) {
        char c = peek();
        if (std::isspace(static_cast<unsigned char>(c))) {
            advance();
        } else if (c == '/' && peek(1) == '/') {
            // Single-line comment — consume until newline
            while (pos_ < source_.size() && peek() != '\n') advance();
        } else {
            break;
        }
    }
}

Token Lexer::lexNumber() {
    int startLine = line_, startCol = col_;
    std::string num;
    while (pos_ < source_.size() && std::isdigit(static_cast<unsigned char>(peek())))
        num += advance();
    return {TokenType::INT_LITERAL, num, startLine, startCol};
}

Token Lexer::lexIdentifierOrKeyword() {
    int startLine = line_, startCol = col_;
    std::string ident;
    while (pos_ < source_.size() &&
           (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_'))
        ident += advance();

    TokenType type = TokenType::IDENTIFIER;
    if (ident == "if")  type = TokenType::IF;
    if (ident == "out") type = TokenType::OUT;
    return {type, ident, startLine, startCol};
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (true) {
        skipWhitespaceAndComments();

        if (pos_ >= source_.size()) {
            tokens.push_back({TokenType::EOF_TOKEN, "", line_, col_});
            break;
        }

        int  startLine = line_;
        int  startCol  = col_;
        char c         = peek();

        if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back(lexNumber());
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            tokens.push_back(lexIdentifierOrKeyword());
            continue;
        }

        advance(); // consume the single character
        switch (c) {
            case '=':
                if (peek() == '=') { advance(); tokens.push_back({TokenType::EQ,   "==", startLine, startCol}); }
                else               {             tokens.push_back({TokenType::ASSIGN,"=", startLine, startCol}); }
                break;
            case '!':
                if (peek() == '=') { advance(); tokens.push_back({TokenType::NEQ,  "!=", startLine, startCol}); }
                else throw std::runtime_error("Unexpected '!' at line " + std::to_string(startLine));
                break;
            case '<':
                if (peek() == '=') { advance(); tokens.push_back({TokenType::LEQ,  "<=", startLine, startCol}); }
                else               {             tokens.push_back({TokenType::LT,   "<",  startLine, startCol}); }
                break;
            case '>':
                if (peek() == '=') { advance(); tokens.push_back({TokenType::GEQ,  ">=", startLine, startCol}); }
                else               {             tokens.push_back({TokenType::GT,   ">",  startLine, startCol}); }
                break;
            case '+': tokens.push_back({TokenType::PLUS,      "+", startLine, startCol}); break;
            case '-': tokens.push_back({TokenType::MINUS,     "-", startLine, startCol}); break;
            case '*': tokens.push_back({TokenType::STAR,      "*", startLine, startCol}); break;
            case '/': tokens.push_back({TokenType::SLASH,     "/", startLine, startCol}); break;
            case ';': tokens.push_back({TokenType::SEMICOLON, ";", startLine, startCol}); break;
            case '(': tokens.push_back({TokenType::LPAREN,    "(", startLine, startCol}); break;
            case ')': tokens.push_back({TokenType::RPAREN,    ")", startLine, startCol}); break;
            case '{': tokens.push_back({TokenType::LBRACE,    "{", startLine, startCol}); break;
            case '}': tokens.push_back({TokenType::RBRACE,    "}", startLine, startCol}); break;
            default:
                throw std::runtime_error(
                    std::string("Unexpected character '") + c +
                    "' at line " + std::to_string(startLine));
        }
    }

    return tokens;
}
