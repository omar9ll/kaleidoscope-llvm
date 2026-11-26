#include "codegen.hpp"
#include "parser.hpp"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"

using namespace llvm;

static Value* logErrorV(const char* msg) { logError(msg); return nullptr; }

void CodeGen::newModule(const DataLayout& dl) {
    ctx = std::make_unique<LLVMContext>();
    mod = std::make_unique<Module>("kaleidoscope", *ctx);
    mod->setDataLayout(dl);
    builder = std::make_unique<IRBuilder<>>(*ctx);

    // Function-level optimization pipeline (new pass manager).
    fpm_ = std::make_unique<FunctionPassManager>();
    lam_ = std::make_unique<LoopAnalysisManager>();
    fam_ = std::make_unique<FunctionAnalysisManager>();
    cgam_ = std::make_unique<CGSCCAnalysisManager>();
    mam_ = std::make_unique<ModuleAnalysisManager>();
    pic_ = std::make_unique<PassInstrumentationCallbacks>();
    si_ = std::make_unique<StandardInstrumentations>(*ctx, /*DebugLogging*/ false);
    si_->registerCallbacks(*pic_, mam_.get());

    fpm_->addPass(InstCombinePass());   // peephole / algebraic simplification
    fpm_->addPass(ReassociatePass());   // reorder expressions to expose constants
    fpm_->addPass(GVNPass());           // eliminate common subexpressions
    fpm_->addPass(SimplifyCFGPass());   // clean up the control-flow graph

    PassBuilder pb;
    pb.registerModuleAnalyses(*mam_);
    pb.registerFunctionAnalyses(*fam_);
    pb.crossRegisterProxies(*lam_, *fam_, *cgam_, *mam_);
}

Function* CodeGen::getFunction(const std::string& name) {
    if (auto* f = mod->getFunction(name)) return f;
    // Defined in an earlier module: re-declare it here from its saved prototype.
    auto it = functionProtos.find(name);
    if (it != functionProtos.end()) return it->second->codegen(*this);
    return nullptr;
}

void CodeGen::runPasses(Function& f) {
    if (optimize) fpm_->run(f, *fam_);
}

Value* NumberExprAST::codegen(CodeGen& cg) { return ConstantFP::get(*cg.ctx, APFloat(val)); }

Value* VariableExprAST::codegen(CodeGen& cg) {
    Value* v = cg.namedValues[name];
    if (!v) return logErrorV(("unknown variable name '" + name + "'").c_str());
    return v;
}

Value* BinaryExprAST::codegen(CodeGen& cg) {
    Value* l = lhs->codegen(cg);
    Value* r = rhs->codegen(cg);
    if (!l || !r) return nullptr;
    auto& b = *cg.builder;
    switch (op) {
        case '+': return b.CreateFAdd(l, r, "addtmp");
        case '-': return b.CreateFSub(l, r, "subtmp");
        case '*': return b.CreateFMul(l, r, "multmp");
        case '<':
            l = b.CreateFCmpULT(l, r, "cmptmp");
            return b.CreateUIToFP(l, Type::getDoubleTy(*cg.ctx), "booltmp");  // bool -> 0.0 / 1.0
        default: return logErrorV("invalid binary operator");
    }
}

Value* CallExprAST::codegen(CodeGen& cg) {
    Function* f = cg.getFunction(callee);
    if (!f) return logErrorV(("unknown function '" + callee + "'").c_str());
    if (f->arg_size() != args.size()) return logErrorV("incorrect number of arguments passed");
    std::vector<Value*> argv;
    for (auto& a : args) {
        argv.push_back(a->codegen(cg));
        if (!argv.back()) return nullptr;
    }
    return cg.builder->CreateCall(f, argv, "calltmp");
}

Value* IfExprAST::codegen(CodeGen& cg) {
    Value* c = cond->codegen(cg);
    if (!c) return nullptr;
    auto& b = *cg.builder;
    c = b.CreateFCmpONE(c, ConstantFP::get(*cg.ctx, APFloat(0.0)), "ifcond");

    Function* fn = b.GetInsertBlock()->getParent();
    BasicBlock* thenBB = BasicBlock::Create(*cg.ctx, "then", fn);
    BasicBlock* elseBB = BasicBlock::Create(*cg.ctx, "else");
    BasicBlock* mergeBB = BasicBlock::Create(*cg.ctx, "ifcont");
    b.CreateCondBr(c, thenBB, elseBB);

    b.SetInsertPoint(thenBB);
    Value* thenV = then_->codegen(cg);
    if (!thenV) return nullptr;
    b.CreateBr(mergeBB);
    thenBB = b.GetInsertBlock();   // codegen of 'then' may have changed the current block

    fn->insert(fn->end(), elseBB);
    b.SetInsertPoint(elseBB);
    Value* elseV = else_->codegen(cg);
    if (!elseV) return nullptr;
    b.CreateBr(mergeBB);
    elseBB = b.GetInsertBlock();

    fn->insert(fn->end(), mergeBB);
    b.SetInsertPoint(mergeBB);
    PHINode* pn = b.CreatePHI(Type::getDoubleTy(*cg.ctx), 2, "iftmp");
    pn->addIncoming(thenV, thenBB);
    pn->addIncoming(elseV, elseBB);
    return pn;
}

// Loop variable is an SSA value carried by a PHI node in the loop header.
Value* ForExprAST::codegen(CodeGen& cg) {
    Value* startV = start->codegen(cg);
    if (!startV) return nullptr;
    auto& b = *cg.builder;
    Function* fn = b.GetInsertBlock()->getParent();
    BasicBlock* preheaderBB = b.GetInsertBlock();
    BasicBlock* loopBB = BasicBlock::Create(*cg.ctx, "loop", fn);
    b.CreateBr(loopBB);

    b.SetInsertPoint(loopBB);
    PHINode* var_ = b.CreatePHI(Type::getDoubleTy(*cg.ctx), 2, var);
    var_->addIncoming(startV, preheaderBB);

    Value* oldVal = cg.namedValues[var];   // shadow any outer variable with the same name
    cg.namedValues[var] = var_;

    if (!body->codegen(cg)) return nullptr;

    Value* stepV = step ? step->codegen(cg) : ConstantFP::get(*cg.ctx, APFloat(1.0));
    if (!stepV) return nullptr;
    Value* nextVar = b.CreateFAdd(var_, stepV, "nextvar");

    cg.namedValues[var] = nextVar;          // test the end condition on the updated value
    Value* endCond = end->codegen(cg);
    if (!endCond) return nullptr;
    endCond = b.CreateFCmpONE(endCond, ConstantFP::get(*cg.ctx, APFloat(0.0)), "loopcond");

    BasicBlock* loopEndBB = b.GetInsertBlock();
    BasicBlock* afterBB = BasicBlock::Create(*cg.ctx, "afterloop", fn);
    b.CreateCondBr(endCond, loopBB, afterBB);
    b.SetInsertPoint(afterBB);
    var_->addIncoming(nextVar, loopEndBB);

    if (oldVal) cg.namedValues[var] = oldVal; else cg.namedValues.erase(var);
    return Constant::getNullValue(Type::getDoubleTy(*cg.ctx));   // a for loop always returns 0.0
}

Function* PrototypeAST::codegen(CodeGen& cg) {
    std::vector<Type*> doubles(args.size(), Type::getDoubleTy(*cg.ctx));
    FunctionType* ft = FunctionType::get(Type::getDoubleTy(*cg.ctx), doubles, false);
    Function* f = Function::Create(ft, Function::ExternalLinkage, name, cg.mod.get());
    unsigned i = 0;
    for (auto& arg : f->args()) arg.setName(args[i++]);
    return f;
}

Function* FunctionAST::codegen(CodeGen& cg) {
    auto& p = *proto;
    cg.functionProtos[p.name] = std::make_unique<PrototypeAST>(p.name, p.args);
    Function* f = cg.getFunction(p.name);
    if (!f) return nullptr;

    BasicBlock* bb = BasicBlock::Create(*cg.ctx, "entry", f);
    cg.builder->SetInsertPoint(bb);
    cg.namedValues.clear();
    for (auto& arg : f->args()) cg.namedValues[std::string(arg.getName())] = &arg;

    if (Value* ret = body->codegen(cg)) {
        cg.builder->CreateRet(ret);
        verifyFunction(*f, &errs());
        cg.runPasses(*f);
        return f;
    }
    f->eraseFromParent();
    return nullptr;
}
