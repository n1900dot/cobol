#ifndef PARSER_H
#define PARSER_H

#include "token.h"
#include "ast.h"
#include "pic_descriptor.h"
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <stdexcept>


class Parser
{
    std::vector<Token> tokens;
    size_t pos = 0;

    const Token &peek() const { return tokens[pos]; }
    const Token &advance() { return tokens[pos++]; }
    bool check(TokenType t) const { return peek().type == t; }
bool consume(TokenType t);
bool expect(TokenType t, const std::string &msg);
void skipNewlines();

std::string parsePictureClause();

std::unique_ptr<ConditionNode> parseCondition();

std::vector<std::unique_ptr<ASTNode>> parseBlock(const std::vector<TokenType> &terminators);

std::unique_ptr<ExprNode> parseExpression();

std::unique_ptr<ExprNode> parseTerm();

std::unique_ptr<ExprNode> parseFactor();

public:
    explicit Parser(std::vector<Token> toks) : tokens(std::move(toks)) {}

std::unique_ptr<ProgramNode> parse();

std::unique_ptr<FileControlNode> parseFileControlEntry();

std::unique_ptr<FileDescriptionNode> parseFileDescription();

std::unique_ptr<DataItemNode> parseDataItem();

std::unique_ptr<ASTNode> parseStatement();

std::unique_ptr<ASTNode> parseOpen();

std::unique_ptr<ASTNode> parseClose();

std::unique_ptr<ASTNode> parseRead();

std::unique_ptr<ASTNode> parseStart();

std::unique_ptr<ASTNode> parseWrite();

std::unique_ptr<ASTNode> parseMove();

std::unique_ptr<ASTNode> parseAdd();

std::unique_ptr<ASTNode> parseMultiply();

std::unique_ptr<ASTNode> parseSubtract();

std::unique_ptr<ASTNode> parseDivide();

std::unique_ptr<ASTNode> parseCompute();

std::unique_ptr<ASTNode> parseDisplay();

std::unique_ptr<ASTNode> parsePrint();

std::unique_ptr<ASTNode> parseStop();

std::unique_ptr<ASTNode> parseIf();

std::unique_ptr<ASTNode> parsePerform();

std::unique_ptr<ASTNode> parseGoTo();

    std::unique_ptr<ASTNode> parseInspect();

    std::unique_ptr<ASTNode> parseAccept();

    std::unique_ptr<ASTNode> parseEvaluate();

    std::unique_ptr<ASTNode> parseString();

    std::unique_ptr<ASTNode> parseUnstring();

    // Table support
    std::unique_ptr<ASTNode> parseSearch();
    std::unique_ptr<ASTNode> parseSet();

    // Subscript support
    std::string readVariable();

};
#endif // PARSER_H
