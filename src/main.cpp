#include "codegen.hpp"
#include "parser.hpp"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>

using namespace llvm;
using namespace llvm::orc;

static ExitOnError ExitOnErr;

int main(int argc, char** argv) {
    bool emitIR = false, optimize = true;
    const char* path = nullptr;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--emit-ir")) emitIR = true;
        else if (!std::strcmp(argv[i], "--no-opt")) optimize = false;
        else path = argv[i];
    }

    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();

    auto jit = ExitOnErr(LLJITBuilder().create());
    // Let JIT-compiled code call the built-ins in runtime.cpp (putchard, printd).
    jit->getMainJITDylib().addGenerator(ExitOnErr(
        DynamicLibrarySearchGenerator::GetForCurrentProcess(jit->getDataLayout().getGlobalPrefix())));

    std::ifstream file;
    if (path) {
        file.open(path);
        if (!file) { std::cerr << "kaleido: cannot open " << path << "\n"; return 2; }
    }
    std::istream& in = path ? static_cast<std::istream&>(file) : std::cin;

    Lexer lex(in);
    Parser parser(lex);
    CodeGen cg;
    cg.optimize = optimize;
    cg.newModule(jit->getDataLayout());

    auto addModule = [&](ResourceTrackerSP rt = nullptr) {
        auto tsm = ThreadSafeModule(std::move(cg.mod), std::move(cg.ctx));
        ExitOnErr(rt ? jit->addIRModule(rt, std::move(tsm)) : jit->addIRModule(std::move(tsm)));
        cg.newModule(jit->getDataLayout());
    };

    if (!path) std::cerr << "ready> ";
    lex.next();
    int errors = 0;
    while (lex.cur() != tok_eof) {
        switch (lex.cur()) {
            case ';': lex.next(); continue;
            case tok_def: {
                if (auto fn = parser.parseDefinition()) {
                    if (auto* ir = fn->codegen(cg)) {
                        if (emitIR) ir->print(outs());
                        addModule();                      // functions stay in the JIT
                    } else errors++;
                } else { errors++; lex.next(); }
                break;
            }
            case tok_extern: {
                if (auto proto = parser.parseExtern()) {
                    if (auto* ir = proto->codegen(cg)) {
                        if (emitIR) ir->print(outs());
                        cg.functionProtos[proto->name] = std::move(proto);
                    }
                } else { errors++; lex.next(); }
                break;
            }
            default: {
                if (auto fn = parser.parseTopLevelExpr()) {
                    if (auto* ir = fn->codegen(cg)) {
                        if (emitIR) ir->print(outs());
                        // Compile the expression in its own module, run it, then free it.
                        auto rt = jit->getMainJITDylib().createResourceTracker();
                        addModule(rt);
                        auto sym = ExitOnErr(jit->lookup("__anon_expr"));
                        auto* fp = sym.toPtr<double (*)()>();
                        double result = fp();
                        std::fflush(stdout);              // flush output from putchard/printd first
                        if (!emitIR) std::cout << "=> " << result << std::endl;
                        ExitOnErr(rt->remove());
                    } else errors++;
                } else { errors++; lex.next(); }
                break;
            }
        }
        if (!path) std::cerr << "ready> ";
    }
    return errors ? 1 : 0;
}
