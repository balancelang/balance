#include "CompileTests.h"
#include "../src/Main.h"
#include "../src/BalancePackage.h"
#include "ASTTests.h"
#include "TestHelpers.h"

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

string runExample(string filePath) {
    std::cout << "Running example: " << filePath << std::endl;
    ifstream inputStream;
    inputStream.open(filePath);
    stringstream strStream;
    strStream << inputStream.rdbuf();

    BalancePackage * package = new BalancePackage("", "", false);
    bool success = package->executeString(strStream.str());
    return executeExecutable("./program");
}

void examplesClassTest() {
    string result = runExample("../examples/class.bl");
    assertEqual("25\n123\n", result, "Class test");
}

void examplesHelloWorldTest() {
    string result = runExample("../examples/helloWorld.bl");
    assertEqual("Hello world!\n", result, "Hello world");
}

void examplesVariablesTest() {
    // TODO: printing a string should print without quotes, but array of strings should include quotes?
    string result = runExample("../examples/variables.bl");
    assertEqual("true\n24\n25.66\nLorem ipsum\nThis is a multiline string\nthat started on the previous line\nand is now on the 3rd line\n[1, 2, 3]\n[true, false, true]\n[1.1, 2.2, 3.3]\n[a, b, c]\n", result, "Variables");
}

void examplesFunctionsTest() {
    string result = runExample("../examples/functions.bl");
    assertEqual("80\nBecause this returns 'None', we can ommit ': None'\nHere, x implicitly has type 'Any' (x: Any)\n", result, "Functions");
}

void examplesWhileTest() {
    string result = runExample("../examples/while.bl");
    assertEqual("0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n", result, "While-loop");
}

void examplesForTest() {
    string result = runExample("../examples/for.bl");
    assertEqual("0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n0\n10\n20\n30\n40\n50\n60\n70\n80\n90\n0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n", result, "For-loop");
    // \n0\n1\n2\n3\n4\n
}

void examplesLambdasTest() {
    string result = runExample("../examples/lambdas.bl");
    assertEqual("5\nThis implicitly returns None\n5\nHello\n7\n", result, "Lambdas");
}

void examplesFilesTest() {
    string result = runExample("../examples/files.bl");
    assertEqual("0123456789\n", result, "Files");
}

void examplesInterfacesTest() {
    string result = runExample("../examples/interfaces.bl");
    assertEqual("5\n7\n", result, "Interfaces");
}

void examplesInheritanceTest() {
    string result = runExample("../examples/inheritance.bl");
    assertEqual("25\n5\n", result, "Inheritance");
}

void runExamplesTestSuite() {
    puts("RUNNING EXAMPLES TESTS");
    examplesClassTest();
    examplesHelloWorldTest();
    examplesVariablesTest();
    examplesFunctionsTest();
    examplesWhileTest();
    examplesForTest();
    examplesLambdasTest();
    examplesFilesTest();
    examplesInterfacesTest();
    examplesInheritanceTest();
}
