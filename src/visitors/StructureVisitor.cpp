#include "StructureVisitor.h"

#include "../BalancePackage.h"
#include "../models/BalanceType.h"
#include "../builtins/Array.h"
#include "../builtins/Lambda.h"
#include "../Utilities.h"
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

    // Check for duplicate className
    BalanceType * btype = currentPackage->currentModule->getType(className);
    currentPackage->currentModule->currentType = btype;

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

    currentPackage->currentModule->currentType = nullptr;
    return nullptr;
}

std::any StructureVisitor::visitSimpleType(BalanceParser::SimpleTypeContext *ctx) {
    if (ctx->IDENTIFIER()) {
        return currentPackage->currentModule->getType(ctx->IDENTIFIER()->getText());
    } else {
        return currentPackage->currentModule->getType(ctx->NONE()->getText());
    }
}

std::any StructureVisitor::visitLambdaType(BalanceParser::LambdaTypeContext *ctx) {
    std::vector<BalanceType *> generics;

    for (BalanceParser::BalanceTypeContext *type : ctx->typeList()->balanceType()) {
        BalanceType * btype = any_cast<BalanceType *>(visit(type));
        generics.push_back(btype);
    }

    BalanceType *returnType = any_cast<BalanceType *>(visit(ctx->balanceType()));
    generics.push_back(returnType);

    BalanceType * lambdaType = currentPackage->currentModule->getType("Lambda", generics);
    if (lambdaType == nullptr) {
        lambdaType = createType__Lambda(generics);
        createImportedClass(currentPackage->currentModule, lambdaType);
    }

    return lambdaType;
}

std::any StructureVisitor::visitLambdaExpression(BalanceParser::LambdaExpressionContext *ctx) {
    std::string text = ctx->getText();
    vector<BalanceType *> generics;

    for (BalanceParser::ParameterContext *parameter : ctx->lambda()->parameterList()->parameter()) {
        std::string typeString = parameter->type->getText();
        BalanceType * btype = currentPackage->currentModule->getType(typeString);
        generics.push_back(btype);
    }

    // If we don't have a return type, assume none
    BalanceType * returnType;
    if (ctx->lambda()->returnType()) {
        std::string functionReturnTypeString = ctx->lambda()->returnType()->balanceType()->getText();
        returnType = currentPackage->currentModule->getType(functionReturnTypeString);
    } else {
        returnType = currentPackage->currentModule->getType("None");
    }
    generics.push_back(returnType);
    BalanceType * lambdaType = currentPackage->currentModule->getType("Lambda", generics);
    if (lambdaType == nullptr) {
        lambdaType = createType__Lambda(generics);
        createImportedClass(currentPackage->currentModule, lambdaType);
    }
    return lambdaType;
}

std::any StructureVisitor::visitArrayLiteral(BalanceParser::ArrayLiteralContext *ctx) {
    std::string text = ctx->getText();
    BalanceType * firstValue;
    for (BalanceParser::ExpressionContext *expression : ctx->listElements()->expression()) {
        firstValue = any_cast<BalanceType *>(visit(expression));
        break;
    }

    BalanceType *arrayType = currentPackage->currentModule->getType("Array", { firstValue });
    if (arrayType == nullptr) {
        // Create type
        arrayType = createType__Array(firstValue);
        createImportedClass(currentPackage->currentModule, arrayType);
    }
    return arrayType;
}

std::any StructureVisitor::visitGenericType(BalanceParser::GenericTypeContext *ctx) {
    std::string base = ctx->base->getText();
    std::vector<BalanceType *> generics;

    for (BalanceParser::BalanceTypeContext *type : ctx->typeList()->balanceType()) {
        BalanceType * btype = any_cast<BalanceType *>(visit(type));
        generics.push_back(btype);
    }

    BalanceType * lambdaType = currentPackage->currentModule->getType("Lambda", generics);
    if (lambdaType == nullptr) {
        lambdaType = createType__Lambda(generics);
        createImportedClass(currentPackage->currentModule, lambdaType);
    }
    return lambdaType;
}

std::any StructureVisitor::visitClassProperty(BalanceParser::ClassPropertyContext *ctx) {
    string text = ctx->getText();
    BalanceType * btype = currentPackage->currentModule->getType(ctx->type->getText());
    string name = ctx->name->getText();

    // Check for duplicate property name
    if (currentPackage->currentModule->currentType->properties.find(name) != currentPackage->currentModule->currentType->properties.end()) {
        currentPackage->currentModule->addTypeError(ctx, "Duplicate property name, property already exist: " + name);
        throw StructureVisitorException();
    }

    int count = currentPackage->currentModule->currentType->properties.size();
    currentPackage->currentModule->currentType->properties[name] = new BalanceProperty(name, btype, count);
    return nullptr;
}

std::any StructureVisitor::visitInterfaceDefinition(BalanceParser::InterfaceDefinitionContext *ctx) {
    string interfaceName = ctx->interfaceName->getText();
    string text = ctx->getText();

    BalanceType *binterface = currentPackage->currentModule->getType(interfaceName);
    currentPackage->currentModule->currentType = binterface;

    // Visit all interface functions
    for (auto const &x : ctx->interfaceElement()) {
        if (x->functionSignature()) {
            visit(x);
        }
    }

    currentPackage->currentModule->currentType = nullptr;
    return nullptr;
}

std::any StructureVisitor::visitFunctionSignature(BalanceParser::FunctionSignatureContext *ctx) {
    string functionName = ctx->IDENTIFIER()->getText();

    if (currentPackage->currentModule->currentType != nullptr) {
        if (currentPackage->currentModule->currentType->getMethod(functionName) != nullptr) {
            if (currentPackage->currentModule->currentType->isInterface) {
                currentPackage->currentModule->addTypeError(ctx, "Duplicate interface method name, method already exist: " + functionName);
            } else {
                currentPackage->currentModule->addTypeError(ctx, "Duplicate class method name, method already exist: " + functionName);
            }
            throw StructureVisitorException();
        }
    } else {
        // TODO: Should we allow overriding functions in new scopes?
        if (currentPackage->currentModule->functions[functionName] != nullptr) {
            currentPackage->currentModule->addTypeError(ctx, "Duplicate function name, function already exist: " + functionName);
            throw StructureVisitorException();
        }
    }

    vector<BalanceParameter *> parameters;

    // Add implicit "this" argument to class methods
    if (currentPackage->currentModule->currentType != nullptr) {
        BalanceParameter *thisParameter = new BalanceParameter(currentPackage->currentModule->currentType, "this");
        parameters.push_back(thisParameter);
    }

    for (BalanceParser::ParameterContext *parameter : ctx->parameterList()->parameter()) {
        string parameterName = parameter->identifier->getText();
        BalanceType * btype = any_cast<BalanceType *>(visit(parameter->balanceType()));

        // Check for duplicate parameter name
        for (auto param : parameters) {
            if (param->name == parameterName) {
                currentPackage->currentModule->addTypeError(ctx, "Duplicate parameter name: " + parameterName);
                throw StructureVisitorException();
            }
        }

        parameters.push_back(new BalanceParameter(btype, parameterName));
    }

    // If we don't have a return type, assume none
    BalanceType *returnType;
    if (ctx->returnType()) {
        returnType = any_cast<BalanceType *>(visit(ctx->returnType()->balanceType()));
    } else {
        returnType = currentPackage->currentModule->getType("None");
    }

    // Check if we are parsing a class method
    if (currentPackage->currentModule->currentType != nullptr) {
        currentPackage->currentModule->currentType->addMethod(functionName, new BalanceFunction(functionName, parameters, returnType));
    } else {
        currentPackage->currentModule->functions[functionName] = new BalanceFunction(functionName, parameters, returnType);
    }

    return nullptr;
}

std::any StructureVisitor::visitNumericLiteral(BalanceParser::NumericLiteralContext *ctx) {
    return currentPackage->currentModule->getType("Int");
}

std::any StructureVisitor::visitStringLiteral(BalanceParser::StringLiteralContext *ctx) {
    return currentPackage->currentModule->getType("String");
}

std::any StructureVisitor::visitBooleanLiteral(BalanceParser::BooleanLiteralContext *ctx) {
    return currentPackage->currentModule->getType("Bool");
}

std::any StructureVisitor::visitDoubleLiteral(BalanceParser::DoubleLiteralContext *ctx) {
    return currentPackage->currentModule->getType("Double");
}

std::any StructureVisitor::visitNoneLiteral(BalanceParser::NoneLiteralContext *ctx) {
    return currentPackage->currentModule->getType("None");
}
