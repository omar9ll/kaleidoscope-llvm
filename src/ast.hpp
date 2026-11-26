#pragma once
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace llvm { class Value; class Function; }
class CodeGen;

struct ExprAST {
    virtual ~ExprAST() = default;
    virtual llvm::Value* codegen(CodeGen& cg) = 0;
};
using ExprPtr = std::unique_ptr<ExprAST>;

struct NumberExprAST : ExprAST {
    double val;
    explicit NumberExprAST(double v) : val(v) {}
    llvm::Value* codegen(CodeGen& cg) override;
};

struct VariableExprAST : ExprAST {
    std::string name;
    explicit VariableExprAST(std::string n) : name(std::move(n)) {}
    llvm::Value* codegen(CodeGen& cg) override;
};

struct BinaryExprAST : ExprAST {
    char op;
    ExprPtr lhs, rhs;
    BinaryExprAST(char o, ExprPtr l, ExprPtr r) : op(o), lhs(std::move(l)), rhs(std::move(r)) {}
    llvm::Value* codegen(CodeGen& cg) override;
};

struct CallExprAST : ExprAST {
    std::string callee;
    std::vector<ExprPtr> args;
    CallExprAST(std::string c, std::vector<ExprPtr> a) : callee(std::move(c)), args(std::move(a)) {}
    llvm::Value* codegen(CodeGen& cg) override;
};

struct IfExprAST : ExprAST {
    ExprPtr cond, then_, else_;
    IfExprAST(ExprPtr c, ExprPtr t, ExprPtr e) : cond(std::move(c)), then_(std::move(t)), else_(std::move(e)) {}
    llvm::Value* codegen(CodeGen& cg) override;
};

// for i = start, end_cond, step in body   (step is optional, defaults to 1.0)
struct ForExprAST : ExprAST {
    std::string var;
    ExprPtr start, end, step, body;
    ForExprAST(std::string v, ExprPtr s, ExprPtr e, ExprPtr st, ExprPtr b)
        : var(std::move(v)), start(std::move(s)), end(std::move(e)), step(std::move(st)), body(std::move(b)) {}
    llvm::Value* codegen(CodeGen& cg) override;
};

struct PrototypeAST {
    std::string name;
    std::vector<std::string> args;
    PrototypeAST(std::string n, std::vector<std::string> a) : name(std::move(n)), args(std::move(a)) {}
    llvm::Function* codegen(CodeGen& cg);
};

struct FunctionAST {
    std::unique_ptr<PrototypeAST> proto;
    ExprPtr body;
    FunctionAST(std::unique_ptr<PrototypeAST> p, ExprPtr b) : proto(std::move(p)), body(std::move(b)) {}
    llvm::Function* codegen(CodeGen& cg);
};
