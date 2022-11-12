#include "StructureVisitor.h"

#include "../Package.h"
#include "../models/BalanceTypeString.h"
#include "BalanceLexer.h"
#include "BalanceParser.h"
#include "BalanceParserBaseVisitor.h"
#include "Visitor.h"

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

#include "config.h"

#include "BalanceLexer.h"
#include "BalanceParser.h"
#include "BalanceParserBaseVisitor.h"

#include <typeindex>
#include <typeinfo>

using namespace antlrcpptest;
using namespace std;

extern BalancePackage *currentPackage;

std::any StructureVisitor::visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) {
    string className = ctx->className->getText();
    string text = ctx->getText();

    // TODO: check for duplicate className

    BalanceClass *bclass = new BalanceClass(new BalanceTypeString(className));
    currentPackage->currentModule->currentClass = bclass;
    currentPackage->currentModule->classes[className] = bclass;

    // TODO: Test that this happens when type-checking instead, since then methods and interfaces exist
    // // Parse extend/implements
    // if (ctx->classExtendsImplements()) {
    //     visit(ctx->classExtendsImplements());
    // }

    // Visit all class properties
    for (auto const &x : ctx->classElement()) {
        if (x->classProperty()) {
            visit(x);
        }
    }

    // Visit all class functions
    for (auto const &x : ctx->classElement()) {
        if (x->functionDefinition()) {
            visit(x);
        }
    }

    currentPackage->currentModule->currentClass = nullptr;
    return nullptr;
}

std::any StructureVisitor::visitSimpleType(BalanceParser::SimpleTypeContext *ctx) {
    if (ctx->IDENTIFIER()) {
        return new BalanceTypeString(ctx->IDENTIFIER()->getText());
    } else {
        return new BalanceTypeString(ctx->NONE()->getText());
    }
}

std::any StructureVisitor::visitGenericType(BalanceParser::GenericTypeContext *ctx) {
    std::string base = ctx->base->getText();
    std::vector<BalanceTypeString *> generics;

    for (BalanceParser::BalanceTypeContext *type : ctx->typeList()->balanceType()) {
        BalanceTypeString *typeString = any_cast<BalanceTypeString *>(visit(type));
        generics.push_back(typeString);
    }
    return new BalanceTypeString(base, generics);
}

std::any StructureVisitor::visitFunctionDefinition(BalanceParser::FunctionDefinitionContext *ctx) {
    string functionName = ctx->functionSignature()->IDENTIFIER()->getText();

    if (currentPackage->currentModule->currentClass != nullptr) {
        if (currentPackage->currentModule->currentClass->methods[functionName] != nullptr) {
            currentPackage->currentModule->addTypeError(ctx, "Duplicate class method name, method already exist: " + functionName);
            return std::any();
        }
    } else {
        // TODO: Should we allow overriding functions in new scopes?
        if (currentPackage->currentModule->functions[functionName] != nullptr) {
            currentPackage->currentModule->addTypeError(ctx, "Duplicate function name, function already exist: " + functionName);
            return std::any();
        }
    }

    vector<BalanceParameter *> parameters;

    // Add implicit "this" argument to class methods
    if (currentPackage->currentModule->currentClass != nullptr) {
        BalanceParameter *thisParameter = new BalanceParameter(currentPackage->currentModule->currentClass->name, "this");
        parameters.push_back(thisParameter);
    }

    for (BalanceParser::ParameterContext *parameter : ctx->functionSignature()->parameterList()->parameter()) {
        string parameterName = parameter->identifier->getText();
        BalanceTypeString *typeString = any_cast<BalanceTypeString *>(visit(parameter->balanceType()));
        parameters.push_back(new BalanceParameter(typeString, parameterName));
    }

    // If we don't have a return type, assume none
    BalanceTypeString *returnType;
    if (ctx->functionSignature()->returnType()) {
        returnType = any_cast<BalanceTypeString *>(visit(ctx->functionSignature()->returnType()->balanceType()));
    } else {
        returnType = new BalanceTypeString("None");
    }

    // Check if we are parsing a class method
    if (currentPackage->currentModule->currentClass != nullptr) {
        currentPackage->currentModule->currentClass->methods[functionName] = new BalanceFunction(functionName, parameters, returnType);
    } else {
        currentPackage->currentModule->functions[functionName] = new BalanceFunction(functionName, parameters, returnType);
    }

    return nullptr;
}

std::any StructureVisitor::visitClassProperty(BalanceParser::ClassPropertyContext *ctx) {
    string text = ctx->getText();
    BalanceTypeString *typeString = new BalanceTypeString(ctx->type->getText());
    string name = ctx->name->getText();

    int count = currentPackage->currentModule->currentClass->properties.size();
    currentPackage->currentModule->currentClass->properties[name] = new BalanceProperty(name, typeString, count);
    return nullptr;
}

std::any StructureVisitor::visitInterfaceDefinition(BalanceParser::InterfaceDefinitionContext *ctx) {
    string interfaceName = ctx->interfaceName->getText();
    string text = ctx->getText();

    // TODO: check for duplicate interfaceName

    BalanceInterface *binterface = new BalanceInterface(new BalanceTypeString(interfaceName));
    binterface->structType = llvm::StructType::create(*currentPackage->context, interfaceName);
    currentPackage->currentModule->currentInterface = binterface;
    currentPackage->currentModule->interfaces[interfaceName] = binterface;

    // Visit all class functions
    for (auto const &x : ctx->interfaceElement()) {
        if (x->functionSignature()) {
            visit(x);
        }
    }

    currentPackage->currentModule->currentInterface = nullptr;
    return nullptr;
}

std::any StructureVisitor::visitFunctionSignature(BalanceParser::FunctionSignatureContext *ctx) {
    if (currentPackage->currentModule->currentInterface != nullptr) {
        string functionName = ctx->IDENTIFIER()->getText();

        if (currentPackage->currentModule->currentInterface->methods[functionName] != nullptr) {
            currentPackage->currentModule->addTypeError(ctx, "Duplicate interface method name, method already exist: " + functionName);
            return std::any();
        }

        vector<BalanceParameter *> parameters;

        // Add implicit "this" argument to class methods
        BalanceParameter *thisParameter = new BalanceParameter(currentPackage->currentModule->currentInterface->name, "this");
        parameters.push_back(thisParameter);

        for (BalanceParser::ParameterContext *parameter : ctx->parameterList()->parameter()) {
            string parameterName = parameter->identifier->getText();
            BalanceTypeString *typeString = any_cast<BalanceTypeString *>(visit(parameter->balanceType()));
            parameters.push_back(new BalanceParameter(typeString, parameterName));
        }

        // If we don't have a return type, assume none
        BalanceTypeString *returnType;
        if (ctx->returnType()) {
            returnType = any_cast<BalanceTypeString *>(visit(ctx->returnType()->balanceType()));
        } else {
            returnType = new BalanceTypeString("None");
        }

        currentPackage->currentModule->currentInterface->methods[functionName] = new BalanceFunction(functionName, parameters, returnType);
    }
    return nullptr;
}