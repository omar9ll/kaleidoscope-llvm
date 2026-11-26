#include "parser.hpp"
#include <cstdio>

ExprPtr logError(const char* msg) {
    std::fprintf(stderr, "error: %s\n", msg);
    return nullptr;
}
static std::unique_ptr<PrototypeAST> logErrorP(const char* msg) { logError(msg); return nullptr; }

int Parser::tokPrecedence() {
    int t = lex_.cur();
    if (t < 0 || t > 127) return -1;
    auto it = prec_.find(static_cast<char>(t));
    return it == prec_.end() ? -1 : it->second;
}

ExprPtr Parser::parseNumber() {
    auto r = std::make_unique<NumberExprAST>(lex_.number());
    lex_.next();
    return r;
}

ExprPtr Parser::parseParen() {
    lex_.next();  // eat '('
    auto v = parseExpression();
    if (!v) return nullptr;
    if (lex_.cur() != ')') return logError("expected ')'");
    lex_.next();
    return v;
}

ExprPtr Parser::parseIdentifier() {
    std::string name = lex_.identifier();
    lex_.next();
    if (lex_.cur() != '(') return std::make_unique<VariableExprAST>(name);  // plain variable

    lex_.next();  // eat '('
    std::vector<ExprPtr> args;
    if (lex_.cur() != ')') {
        while (true) {
            auto arg = parseExpression();
            if (!arg) return nullptr;
            args.push_back(std::move(arg));
            if (lex_.cur() == ')') break;
            if (lex_.cur() != ',') return logError("expected ')' or ',' in argument list");
            lex_.next();
        }
    }
    lex_.next();  // eat ')'
    return std::make_unique<CallExprAST>(name, std::move(args));
}

ExprPtr Parser::parseIf() {
    lex_.next();  // eat 'if'
    auto cond = parseExpression();
    if (!cond) return nullptr;
    if (lex_.cur() != tok_then) return logError("expected 'then'");
    lex_.next();
    auto then_ = parseExpression();
    if (!then_) return nullptr;
    if (lex_.cur() != tok_else) return logError("expected 'else'");
    lex_.next();
    auto else_ = parseExpression();
    if (!else_) return nullptr;
    return std::make_unique<IfExprAST>(std::move(cond), std::move(then_), std::move(else_));
}

ExprPtr Parser::parseFor() {
    lex_.next();  // eat 'for'
    if (lex_.cur() != tok_identifier) return logError("expected identifier after 'for'");
    std::string var = lex_.identifier();
    lex_.next();
    if (lex_.cur() != '=') return logError("expected '=' after for variable");
    lex_.next();
    auto start = parseExpression();
    if (!start) return nullptr;
    if (lex_.cur() != ',') return logError("expected ',' after for start value");
    lex_.next();
    auto end = parseExpression();
    if (!end) return nullptr;
    ExprPtr step;
    if (lex_.cur() == ',') {
        lex_.next();
        step = parseExpression();
        if (!step) return nullptr;
    }
    if (lex_.cur() != tok_in) return logError("expected 'in' after for");
    lex_.next();
    auto body = parseExpression();
    if (!body) return nullptr;
    return std::make_unique<ForExprAST>(var, std::move(start), std::move(end), std::move(step), std::move(body));
}

ExprPtr Parser::parsePrimary() {
    switch (lex_.cur()) {
        case tok_identifier: return parseIdentifier();
        case tok_number: return parseNumber();
        case '(': return parseParen();
        case tok_if: return parseIf();
        case tok_for: return parseFor();
        default: return logError("unknown token when expecting an expression");
    }
}

ExprPtr Parser::parseBinOpRHS(int exprPrec, ExprPtr lhs) {
    while (true) {
        int prec = tokPrecedence();
        if (prec < exprPrec) return lhs;
        int op = lex_.cur();
        lex_.next();
        auto rhs = parsePrimary();
        if (!rhs) return nullptr;
        if (prec < tokPrecedence()) {           // next operator binds tighter
            rhs = parseBinOpRHS(prec + 1, std::move(rhs));
            if (!rhs) return nullptr;
        }
        lhs = std::make_unique<BinaryExprAST>(static_cast<char>(op), std::move(lhs), std::move(rhs));
    }
}

ExprPtr Parser::parseExpression() {
    auto lhs = parsePrimary();
    if (!lhs) return nullptr;
    return parseBinOpRHS(0, std::move(lhs));
}

std::unique_ptr<PrototypeAST> Parser::parsePrototype() {
    if (lex_.cur() != tok_identifier) return logErrorP("expected function name in prototype");
    std::string name = lex_.identifier();
    lex_.next();
    if (lex_.cur() != '(') return logErrorP("expected '(' in prototype");
    std::vector<std::string> args;
    while (lex_.next() == tok_identifier) args.push_back(lex_.identifier());
    if (lex_.cur() != ')') return logErrorP("expected ')' in prototype");
    lex_.next();
    return std::make_unique<PrototypeAST>(name, std::move(args));
}

std::unique_ptr<FunctionAST> Parser::parseDefinition() {
    lex_.next();  // eat 'def'
    auto proto = parsePrototype();
    if (!proto) return nullptr;
    if (auto body = parseExpression()) return std::make_unique<FunctionAST>(std::move(proto), std::move(body));
    return nullptr;
}

std::unique_ptr<PrototypeAST> Parser::parseExtern() {
    lex_.next();  // eat 'extern'
    return parsePrototype();
}

std::unique_ptr<FunctionAST> Parser::parseTopLevelExpr() {
    if (auto e = parseExpression()) {
        auto proto = std::make_unique<PrototypeAST>("__anon_expr", std::vector<std::string>());
        return std::make_unique<FunctionAST>(std::move(proto), std::move(e));
    }
    return nullptr;
}
