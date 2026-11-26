#pragma once
#include "ast.hpp"
#include "lexer.hpp"
#include <map>

// Recursive-descent parser with operator-precedence parsing for binary expressions.
class Parser {
public:
    explicit Parser(Lexer& lex) : lex_(lex) {}

    std::unique_ptr<FunctionAST> parseDefinition();
    std::unique_ptr<PrototypeAST> parseExtern();
    std::unique_ptr<FunctionAST> parseTopLevelExpr();

private:
    Lexer& lex_;
    std::map<char, int> prec_{{'<', 10}, {'+', 20}, {'-', 20}, {'*', 40}};

    int tokPrecedence();
    ExprPtr parseExpression();
    ExprPtr parsePrimary();
    ExprPtr parseBinOpRHS(int exprPrec, ExprPtr lhs);
    ExprPtr parseNumber();
    ExprPtr parseParen();
    ExprPtr parseIdentifier();
    ExprPtr parseIf();
    ExprPtr parseFor();
    std::unique_ptr<PrototypeAST> parsePrototype();
};

ExprPtr logError(const char* msg);
