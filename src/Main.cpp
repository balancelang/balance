#include "headers/Main.h"
#include "headers/Builder.h"
#include "headers/Builtins.h"
#include "headers/Visitor.h"

#include <experimental/filesystem>
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
Module *module;
IRBuilder<> *builder;
vector<BalanceType> types;
ScopeBlock *currentScope = nullptr;
bool verbose = true;

bool file_exist(string fileName)
{
    ifstream infile(fileName);
    return infile.good();
}

void initializeModule()
{
    context = new LLVMContext();
    module = new Module("main", *context);
    builder = new IRBuilder<>(*context);
    currentScope = nullptr;
}

Module *buildModuleFromStream(ANTLRInputStream stream)
{
    initializeModule();
    BalanceLexer lexer(&stream);
    CommonTokenStream tokens(&lexer);

    tokens.fill();

    BalanceParser parser(&tokens);
    tree::ParseTree *tree = parser.root();

    if (verbose)
    {
        cout << tree->toStringTree(&parser, true) << endl;
    }

    create_functions();
    create_types();

    FunctionType *funcType = FunctionType::get(builder->getInt32Ty(), false);
    Function *mainFunc = Function::Create(funcType, Function::ExternalLinkage, "main", module);
    BasicBlock *entry = BasicBlock::Create(*context, "entrypoint", mainFunc);

    // Set insert point to end of (start of) main block
    builder->SetInsertPoint(entry);
    currentScope = new ScopeBlock(entry, nullptr);
    BalanceVisitor visitor;

    // Visit entire tree
    visitor.visit(tree);

    // Return 0
    builder->CreateRet(ConstantInt::get(*context, APInt(32, 0)));
    return module;
}

tree::ParseTree *buildASTFromString(string program)
{
    ANTLRInputStream input(program);
    BalanceLexer lexer(&input);
    CommonTokenStream tokens(&lexer);

    tokens.fill();

    BalanceParser parser(&tokens);
    tree::ParseTree *tree = parser.root();

    if (verbose)
    {
        cout << tree->toStringTree(&parser, true) << endl;
    }
    return tree;
}

Module *buildModuleFromString(string program)
{
    ANTLRInputStream input(program);
    return buildModuleFromStream(input);
}

Module *buildModuleFromPath(string filePath)
{
    if (!file_exist(filePath))
    {
        cout << "Input file doesn't exist: " << filePath << endl;
        exit(1);
    }
    auto size = std::experimental::filesystem::file_size(filePath);
    cout << "File size: " << size << endl;

    ifstream inputStream;
    inputStream.open(filePath);


    ANTLRInputStream input(inputStream);
    return buildModuleFromStream(input);
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
        Module *mod = buildModuleFromPath(argv[1]);

        if (verbose)
        {
            module->print(llvm::errs(), nullptr);
        }
        writeModuleToFile(module);
    }

    return 0;
}
