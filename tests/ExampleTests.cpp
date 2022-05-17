#include "CompileTests.h"
#include "../src/headers/Main.h"
#include "../src/headers/Builder.h"
#include "ASTTests.h"

#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>

#include <iostream>
#include <cstdio>
#include <fstream>
#include "clang/Driver/Driver.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Driver/Compilation.h"
#include "clang/CodeGen/CodeGenAction.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/TextDiagnosticBuffer.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Value.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "antlr4-runtime.h"

#include "BalanceLexer.h"
#include "BalanceParser.h"
#include "BalanceParserBaseVisitor.h"

#include <typeinfo>
#include <typeindex>

using namespace antlrcpptest;
using namespace llvm;
using namespace antlr4;
using namespace std;

// TODO: Consolidate this and the one in CompileTests
string execExample(const char* cmd) {
    array<char, 128> buffer;
    string result;
    unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

void examplesClassTest() {
    Module * module = buildModuleFromPath("../examples/class.bl");
    writeModuleToFile(module);
    string result = execExample("./main");
    assertEqual("25\n", result, "Class test");
}

void examplesHelloWorldTest() {
    Module * module = buildModuleFromPath("../examples/helloWorld.bl");
    writeModuleToFile(module);
    string result = execExample("./main");
    assertEqual("Hello world!\n", result, "Hello world");
}

void examplesVariablesTest() {
    Module * module = buildModuleFromPath("../examples/variables.bl");
    writeModuleToFile(module);
    string result = execExample("./main");
    assertEqual("1\n24\n25.66\nLorem ipsum\nThis is a multiline string\nthat started on the previous line\nand is now on the 3rd line\n", result, "Variables");
}

void examplesFunctionsTest() {
    Module * module = buildModuleFromPath("../examples/functions.bl");
    writeModuleToFile(module);
    string result = execExample("./main");
    assertEqual("80\nBecause this returns 'None', we can ommit '-> None'\n", result, "Functions");
}

void runExamplesTestSuite() {
    puts("RUNNING EXAMPLES TESTS");
    examplesClassTest();
    examplesHelloWorldTest();
    examplesVariablesTest();
    examplesFunctionsTest();
}
