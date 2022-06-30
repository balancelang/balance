#include "../headers/StructureVisitor.h"

#include "../headers/Visitor.h"
#include "../headers/Package.h"
#include "BalanceParserBaseVisitor.h"
#include "BalanceLexer.h"
#include "BalanceParser.h"

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

#include "config.h"

#include "BalanceLexer.h"
#include "BalanceParser.h"
#include "BalanceParserBaseVisitor.h"

#include <typeinfo>
#include <typeindex>

using namespace antlrcpptest;
using namespace std;

extern BalancePackage *currentPackage;

std::any StructureVisitor::visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) {
    string className = ctx->className->getText();
    string text = ctx->getText();

    BalanceClass * bclass = new BalanceClass(className);
    currentPackage->currentModule->currentClass = bclass;
    currentPackage->currentModule->classes[className] = bclass;

    // Visit all class properties
    for (auto const &x : ctx->classElement())
    {
        if (x->classProperty())
        {
            visit(x);
        }
    }

    // Visit all class functions
    for (auto const &x : ctx->classElement())
    {
        if (x->functionDefinition())
        {
            visit(x);
        }
    }

    currentPackage->currentModule->currentClass = nullptr;
    return nullptr;
}

std::any StructureVisitor::visitFunctionDefinition(BalanceParser::FunctionDefinitionContext *ctx) {
    string functionName = ctx->IDENTIFIER()->getText();
    vector<BalanceParameter *> parameters;
    for (BalanceParser::ParameterContext *parameter : ctx->parameterList()->parameter())
    {
        string parameterName = parameter->identifier->getText();
        string typeString = parameter->type->getText();
        parameters.push_back(new BalanceParameter(typeString, parameterName));
    }

    // If we don't have a return type, assume none
    string returnType;
    if (ctx->returnType())
    {
        returnType = ctx->returnType()->IDENTIFIER()->getText();
    }
    else
    {
        returnType = "None";
    }

    // Check if we are parsing a class method
    if (currentPackage->currentModule->currentClass != nullptr)
    {
        currentPackage->currentModule->currentClass->methods[functionName] = new BalanceFunction(functionName, parameters, returnType);
    }
    else
    {
        currentPackage->currentModule->functions[functionName] = new BalanceFunction(functionName, parameters, returnType);
    }

    return nullptr;
}

std::any StructureVisitor::visitClassProperty(BalanceParser::ClassPropertyContext *ctx) {
    string text = ctx->getText();
    string typeString = ctx->type->getText();
    string name = ctx->name->getText();

    int count = currentPackage->currentModule->currentClass->properties.size();
    currentPackage->currentModule->currentClass->properties[name] = new BalanceProperty(name, typeString, count);
    return nullptr;
}