#include "Main.h"
#include "Builtins.h"
#include "BalancePackage.h"
#include "Utilities.h"
#include "visitors/Visitor.h"
#include "language-server/LanguageServer.h"

#include "antlr4-runtime.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/CodeGen/CodeGenAction.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/TextDiagnosticBuffer.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/IR/Verifier.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include <cstdio>
#include <fstream>
#include <iostream>

#include "../tests/ASTTests.h"
#include "../tests/CompileTests.h"
#include "../tests/ExampleTests.h"
#include "../tests/TypeTests.h"
#include "config.h"

#include "BalanceLexer.h"
#include "BalanceParser.h"
#include "BalanceParserBaseVisitor.h"

#include <typeindex>
#include <typeinfo>

using namespace antlrcpptest;
using namespace llvm;
using namespace antlr4;
using namespace std;

BalancePackage *currentPackage = nullptr;

void printVersion() {
    cout << "Balance version: " << BALANCE_VERSION << endl;
}

void printHelp() {
    cout << "Run one of the following:" << endl;
    cout << "'./balance --help' to display this" << endl;
    cout << "'./balance --version' to display version" << endl;
    cout << "'./balance --test' to run test suite (currently embedded in executable)" << endl;
    cout << "'./balance --verbose' to have verbose output" << endl;
}

void createNewProject() {
    if (fileExist("package.json")) {
        cout << "This directory already contains a package.json" << endl;
        exit(1);
    }

    std::ofstream dst("package.json", std::ios::binary);

    dst << ""
           "{"
           "\"name\": \"\",\n"
           "\"version\": \"0.0.0\",\n"
           "\"entrypoints\": {\n"
           "\"default\": \"[REPLACE WITH DEFAULT PROGRAM]\"\n"
           "},\n"
           "\"dependencies\": {\n"
           "}\n"
           "}\n";

    cout << "Created new project." << endl;
}

int main(int argc, char **argv) {
    bool isTest = false;
    bool isHelp = false;
    bool isPrintVersion = false;
    bool isRunLanguageServer = false;
    bool languageServerTcp = false;
    bool verboseLogging = false;
    bool isAnalyzeOnly = false;
    std::string entryPoint;

    std::vector<std::string> arguments;
    for (int i = 0; i < argc; i++) {
        arguments.push_back(argv[i]);
    }

    if (argc == 1) {
        printVersion();
        printHelp();
        return 0;
    }

    // i = 1, skip balance executable
    for (int i = 1; i < arguments.size(); i++) {
        std::string argument = arguments[i];
        if (argument == "--test") {
            isTest = true;
        } else if (argument == "--version") {
            isPrintVersion = true;
        } else if (argument == "--help") {
            isHelp = true;
        } else if (argument == "--verbose") {
            verboseLogging = true;
        } else if (argument == "--language-server") {
            isRunLanguageServer = true;
        } else if (argument == "--language-server-tcp") {
            languageServerTcp = true;
        } else if (argument == "--analyze-only") {
            isAnalyzeOnly = true;
        } else {
            if (argument != "new" && argument != "run") {
                entryPoint = argument;
            }
        }
    }

    // TODO: Consider something like https://github.com/CLIUtils/CLI11 to parse arguments
    char *arg1 = argv[1];
    if (strcmp(arg1, "new") == 0) {
        createNewProject();
        return 0;
    } else if (strcmp(arg1, "run") == 0) {
        if (!fileExist("package.json")) {
            cout << "Found no package.json in current directory, exiting.." << endl;
            return 1;
        }

        // TODO: One day we might allow executing from a different directory
        currentPackage = new BalancePackage("package.json", entryPoint, verboseLogging);
        currentPackage->isAnalyzeOnly = isAnalyzeOnly;
        bool success = currentPackage->execute();
        return !success;
    } else {
        if (isPrintVersion) {
            printVersion();
        } else if (isHelp) {
            printHelp();
        } else if (isTest) {
            runASTTestSuite();
            runCompileTestSuite();
            runExamplesTestSuite();
            runTypesTestSuite();
        } else if (isRunLanguageServer) {
            runLanguageServer(languageServerTcp);
        } else {
            currentPackage = new BalancePackage("", entryPoint, verboseLogging);
            currentPackage->isAnalyzeOnly = isAnalyzeOnly;
            return currentPackage->execute(true) ? 0 : 1;
        }
    }

    return 0;
}
