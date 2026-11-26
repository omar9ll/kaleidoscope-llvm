#pragma once
#include "ast.hpp"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include <map>
#include <memory>

// Holds the LLVM state for one module plus the optimization pipeline.
class CodeGen {
public:
    std::unique_ptr<llvm::LLVMContext> ctx;
    std::unique_ptr<llvm::Module> mod;
    std::unique_ptr<llvm::IRBuilder<>> builder;
    std::map<std::string, llvm::Value*> namedValues;                     // variables in scope
    std::map<std::string, std::unique_ptr<PrototypeAST>> functionProtos;  // every known signature
    bool optimize = true;

    void newModule(const llvm::DataLayout& dl);   // fresh context/module for the next top-level item
    llvm::Function* getFunction(const std::string& name);
    void runPasses(llvm::Function& f);

private:
    std::unique_ptr<llvm::FunctionPassManager> fpm_;
    std::unique_ptr<llvm::LoopAnalysisManager> lam_;
    std::unique_ptr<llvm::FunctionAnalysisManager> fam_;
    std::unique_ptr<llvm::CGSCCAnalysisManager> cgam_;
    std::unique_ptr<llvm::ModuleAnalysisManager> mam_;
    std::unique_ptr<llvm::PassInstrumentationCallbacks> pic_;
    std::unique_ptr<llvm::StandardInstrumentations> si_;
};
