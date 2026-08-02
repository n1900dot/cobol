#ifndef LEXER_H
#define LEXER_H

#include "token.h"
#include <vector>
#include <string>

class Lexer
{
private:
    std::string source;
    size_t pos = 0;
    int line = 1;
    int col = 1;
    std::vector<Token> tokens;

    char peek() const;
    char advance();
    void skipWhitespace();
    void skipComment();
    TokenType checkKeyword(const std::string &word);

public:
    explicit Lexer(std::string src);
    std::vector<Token> tokenize();
};

#endif
