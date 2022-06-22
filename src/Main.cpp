#include "headers/Main.h"
#include "headers/Builtins.h"
#include "headers/Visitor.h"
#include "headers/Package.h"
#include "headers/Utilities.h"

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

#include "../tests/ASTTests.h"
#include "../tests/CompileTests.h"
#include "../tests/ExampleTests.h"
#include "config.h"

#include "BalanceLexer.h"
#include "BalanceParser.h"
#include "BalanceParserBaseVisitor.h"

#include <typeinfo>
#include <typeindex>

using namespace antlrcpptest;
using namespace llvm;
using namespace antlr4;
using namespace std;

LLVMContext *context;
IRBuilder<> *builder;
vector<BalanceType> types;
BalancePackage *currentPackage = nullptr;
BalanceModule *currentModule = nullptr;
bool verbose = false;

void initializeModule(BalanceModule * bmodule)
{
    context = new LLVMContext();
    builder = new IRBuilder<>(*context);
    bmodule->module = new Module(bmodule->path, *context);
    currentModule = bmodule;

    // Initialize module root scope
    FunctionType *funcType = FunctionType::get(builder->getInt32Ty(), false);
    Function *rootFunc = Function::Create(funcType, Function::ExternalLinkage, bmodule->isEntrypoint ? "main" : "root", bmodule->module);
    BasicBlock *entry = BasicBlock::Create(*context, "entrypoint", rootFunc);
    builder->SetInsertPoint(entry);
    bmodule->rootScope = new ScopeBlock(entry, nullptr);
    bmodule->currentScope = bmodule->rootScope;
}

void printVersion()
{
    cout << "Balance version: " << BALANCE_VERSION << endl;
}

void printHelp()
{
    cout << "Run one of the following:" << endl;
    cout << "'./balance --help' to display this" << endl;
    cout << "'./balance --version' to display version" << endl;
    cout << "'./balance --test' to run test suite (currently embedded in executable)" << endl;
    cout << "'./balance --verbose' to have verbose output" << endl;
}

void createNewProject() {
    if (file_exist("package.json"))
    {
        cout << "This directory already contains a package.json" << endl;
        exit(1);
    }

    std::ofstream  dst("package.json",   std::ios::binary);

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

int main(int argc, char **argv)
{
    bool isTest = false;
    bool isHelp = false;
    bool isPrintVersion = false;

    if (argc == 1)
    {
        printVersion();
        printHelp();
        return 0;
    }

    // TODO: Consider something like https://github.com/CLIUtils/CLI11 to parse arguments
    char *arg1 = argv[1];
    if (strcmp(arg1, "new") == 0) {
        createNewProject();
        return 0;
    } else if (strcmp(arg1, "run") == 0) {
        std::string entryPoint;
        if (argc > 2) {
            entryPoint = argv[2];
        }

        if (!file_exist("package.json")) {
            cout << "Found no package.json in current directory, exiting.." << endl;
            return 1;
        }

        // TODO: One day we might allow executing from a different directory
        currentPackage = new BalancePackage("package.json", entryPoint);
        bool success = currentPackage->execute();
        return !success;
    } else {
        for (int i = 0; i < argc; i++)
        {
            char *arg = argv[i];
            if (strcmp(arg, "--test") == 0)
            {
                isTest = true;
            }
            else if (strcmp(arg, "--version") == 0)
            {
                isPrintVersion = true;
            }
            else if (strcmp(arg, "--help") == 0)
            {
                isHelp = true;
            }
            else if (strcmp(arg, "--verbose") == 0)
            {
                verbose = true;
            }
        }

        if (isPrintVersion)
        {
            printVersion();
        }
        else if (isHelp)
        {
            printHelp();
        }
        else if (isTest)
        {
            runASTTestSuite();
            runCompileTestSuite();
            runExamplesTestSuite();
        }
        else
        {
            // Module *mod = buildModuleFromPath(argv[1]);

            // if (verbose)
            // {
            //     mod->print(llvm::errs(), nullptr);
            // }
            // // TODO: Get filename from argv[1] path
            // writeModuleToFile("main", { mod });
        }
    }

    return 0;
}
