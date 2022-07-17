#include "CompileTests.h"
#include "../src/Package.h"
#include "../src/Main.h"
#include "../src/visitors/Visitor.h"
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

string exec(const char* cmd) {
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

string run(string program) {
    BalancePackage * package = new BalancePackage("", "");
    bool success = package->executeString(program);
    return exec("./program");
}

void createPrintInt() {
    string program = "var a = 555\nprint(a)";
    string result = run(program);
    assertEqual("555\n", result, program);
}

void createPrintBool() {
    string program = "var a = true\nprint(a)";
    string result = run(program);
    assertEqual("true\n", result, program);
}

void createPrintDouble() {
    string program = "var a = 5.5\nprint(a)";
    string result = run(program);
    assertEqual("5.5\n", result, program);
}

void createPrintString() {
    string program = "var a = \"HELLO\"\nprint(a)";
    string result = run(program);
    assertEqual("HELLO\n", result, program);
}

void createPrintList() {
    string program = "var a = [1,2,3,4]\nprint(a)";
    string result = run(program);
    assertEqual("[1, 2, 3, 4]\n", result, program);
}

void createFunctionAndInvoke() {
    string program = "def test(Int a) -> None {\nprint(a)\n}\ntest(55)";
    string result = run(program);
    assertEqual("55\n", result, program);
}

void createLambdaAndInvoke() {
    string program = "var lamb = (Int a) -> None {\nprint(a)\n}\nlamb(55)";
    string result = run(program);
    assertEqual("55\n", result, program);
}

void createReassignment() {
    string program = "var a = 555\na=222\nprint(a)";
    string result = run(program);
    assertEqual("222\n", result, program);
}

void operatorsTwoOperandsTest() {
    string program = "print(46 + 2)";
    string result = run(program);
    assertEqual("48\n", result, program);

    program = "print(46 - 2)";
    result = run(program);
    assertEqual("44\n", result, program);

    program = "print(8 * 2)";
    result = run(program);
    assertEqual("16\n", result, program);

    program = "print(8 / 2)";
    result = run(program);
    assertEqual("4\n", result, program);
}

void operatorsThreeOperandsTest() {
    string program = "print(46 + 2 - 2)";
    string result = run(program);
    assertEqual("46\n", result, program);

    program = "print(46 - 2 * 2)";
    result = run(program);
    assertEqual("42\n", result, program);

    program = "print(8 * 2 + 6)";
    result = run(program);
    assertEqual("22\n", result, program);

    program = "print(6 + 8 * 2)";
    result = run(program);
    assertEqual("22\n", result, program);

    program = "print(8 / 2)";
    result = run(program);
    assertEqual("4\n", result, program);
}


    // func with no arguments and no return
    // func with no arguments and return
    // func with 1 argument and no return
    // func with 1 argument and return
    // func with 2 arguments and return
    // ^-- same for lambda
    // func with lambda as parameter

    // 5.5 + 5 (cast int to float)
    // 5 + 5.5
    // 5.5 - 5
    // 5 - 5.5
    // 2.2 + true
    // 2.2 - true
    // 2.2 + false
    // 2.2 - false



void runCompileTestSuite() {
    puts("RUNNING COMPILE TESTS");

    createPrintInt();
    createPrintBool();
    createPrintDouble();
    createPrintString();
    // createPrintList();
    createFunctionAndInvoke();
    createLambdaAndInvoke();
    createReassignment();
    operatorsTwoOperandsTest();
    operatorsThreeOperandsTest();
}


