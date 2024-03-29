#include "LLVMTypeVisitor.h"

#include "../BalancePackage.h"
#include "BalanceLexer.h"
#include "BalanceParser.h"
#include "BalanceParserBaseVisitor.h"
#include "Visitor.h"
#include "../builtins/Lambda.h"

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
#include "../Builtins.h"

#include "assert.h"
#include <typeindex>
#include <typeinfo>

using namespace antlrcpptest;

extern BalancePackage *currentPackage;

// TODO: What's the difference between this and ForwardDeclarationVisitor::visitImportStatement ?
std::any LLVMTypeVisitor::visitImportStatement(BalanceParser::ImportStatementContext *ctx) {
    std::string text = ctx->getText();

    std::string importPath;
    if (ctx->IDENTIFIER()) {
        importPath = ctx->IDENTIFIER()->getText();
    } else if (ctx->IMPORT_PATH()) {
        importPath = ctx->IMPORT_PATH()->getText();
    }

    BalanceModule *importedModule = currentPackage->modules[importPath];

    for (BalanceParser::ImportDefinitionContext *parameter : ctx->importDefinitionList()->importDefinition()) {
        if (dynamic_cast<BalanceParser::UnnamedImportDefinitionContext *>(parameter)) {
            BalanceParser::UnnamedImportDefinitionContext *import = dynamic_cast<BalanceParser::UnnamedImportDefinitionContext *>(parameter);
            std::string importString = import->IDENTIFIER()->getText();

            BalanceType * btype = importedModule->getType(importString);
            if (btype != nullptr) {
                if (btype->isInterface) {
                    // TODO: Handle
                } else {
                    createImportedClass(currentPackage->currentModule, btype);
                }
                continue;
            }

            std::vector<BalanceFunction *> bfunctions = importedModule->getFunctionsByName(importString);
            for (BalanceFunction * bfunction : bfunctions) {
                createImportedFunction(currentPackage->currentModule, bfunction);
            }

            if (bfunctions.empty()) {
                // TODO: Handle
            }
        }
    }

    return nullptr;
}

std::any LLVMTypeVisitor::visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) {
    string text = ctx->getText();
    string className = ctx->className->getText();

    // For each known generic use of class, visit the class
    std::vector<BalanceType *> btypes = {};

    if (ctx->classGenerics()) {
        btypes = currentPackage->currentModule->getGenericVariants(className);
    } else {
        BalanceType * btype = currentPackage->currentModule->getType(className);
        btypes.push_back(btype);
    }

    for (BalanceType * btype : btypes) {
        currentPackage->currentModule->currentType = btype;

        // If already finalized, skip
        if (btype->finalized()) {
            continue;
        }

        // Visit all class properties
        for (auto const &x : ctx->classElement()) {
            if (x->classProperty()) {
                visit(x->classProperty());
            }
        }

        // Visit all class functions
        for (auto const &x : ctx->classElement()) {
            if (x->functionDefinition()) {
                visit(x);
            }
        }

        vector<Type *> propertyTypes;
        vector<BalanceProperty *> properties = currentPackage->currentModule->currentType->getProperties();
        for (BalanceProperty * bproperty : properties) {
            if (bproperty->balanceType->internalType == nullptr) {
                // If a property type is not yet known
                currentPackage->currentModule->currentType = nullptr;
                return nullptr;
            }
            propertyTypes.push_back(bproperty->balanceType->getReferencableType());
        }

        if (!currentPackage->currentModule->currentType->hasBody) {
            ArrayRef<Type *> propertyTypesRef(propertyTypes);
            if (currentPackage->currentModule->currentType->internalType == nullptr) {
                currentPackage->currentModule->currentType->internalType = llvm::StructType::create(*currentPackage->context, currentPackage->currentModule->currentType->name);
            }
            StructType * structType = (StructType *) currentPackage->currentModule->currentType->internalType;
            structType->setBody(propertyTypesRef, false);
            currentPackage->currentModule->currentType->hasBody = true;
        }

        currentPackage->currentModule->currentType = nullptr;
    }

    currentPackage->currentModule->currentType = nullptr;

    return nullptr;
}

std::any LLVMTypeVisitor::visitClassProperty(BalanceParser::ClassPropertyContext *ctx) {
    string text = ctx->getText();
    string name = ctx->variableTypeTuple()->name->getText();

    BalanceProperty *bprop = currentPackage->currentModule->currentType->properties[name];
    // If already finalized, skip
    if (bprop->balanceType->internalType != nullptr) {
        return nullptr;
    }

    return nullptr;
}

std::any LLVMTypeVisitor::visitInterfaceDefinition(BalanceParser::InterfaceDefinitionContext *ctx) {
    std::string text = ctx->getText();
    string interfaceName = ctx->interfaceName->getText();

    BalanceType * binterface = currentPackage->currentModule->getType(interfaceName);

    currentPackage->currentModule->currentType = binterface;

    if (binterface->internalType == nullptr) {
        BalanceType * fatPointerType = currentPackage->currentModule->getType("FatPointer");
        binterface->internalType = fatPointerType->getInternalType();
        binterface->hasBody = true;
    }

    // Visit all interface functions
    for (auto const &x : ctx->interfaceElement()) {
        if (x->functionSignature()) {
            visit(x);
        }
    }

    currentPackage->currentModule->currentType = nullptr;

    return std::any();
}

std::any LLVMTypeVisitor::visitFunctionSignature(BalanceParser::FunctionSignatureContext *ctx) {
    string functionName = ctx->IDENTIFIER()->getText();

    vector<BalanceType *> functionParameters;
    for (BalanceParser::VariableTypeTupleContext *parameter : ctx->parameterList()->variableTypeTuple()) {
        BalanceType * btype = nullptr;
        if (parameter->type) {
            std::string type = parameter->type->getText();
            BalanceType * btype = any_cast<BalanceType *>(visit(parameter->type));
            functionParameters.push_back(btype);
        } else {
            functionParameters.push_back(currentPackage->currentModule->getType("Any"));
        }
    }

    BalanceType *returnType;
    if (ctx->returnType()) {
        returnType = any_cast<BalanceType *>(visit(ctx->returnType()->balanceType()));
    } else {
        returnType = currentPackage->currentModule->getType("None");
    }

    BalanceFunction *bfunction;
    if (currentPackage->currentModule->currentType != nullptr) {
        functionParameters.insert(functionParameters.begin(), currentPackage->currentModule->currentType); // implicit 'self'

        if (functionName == currentPackage->currentModule->currentType->name) {
            // Constructor
            bfunction = currentPackage->currentModule->currentType->getConstructor(functionParameters);
        } else {
            // Class method
            bfunction = currentPackage->currentModule->currentType->getMethod(functionName, functionParameters);
        }
    } else if (currentPackage->currentModule->currentType != nullptr) {
        bfunction = currentPackage->currentModule->currentType->getMethod(functionName, functionParameters);
    } else {
        bfunction = currentPackage->currentModule->getFunction(functionName, functionParameters);
    }

    assert(bfunction != nullptr);

    if (bfunction->finalized()) {
        return nullptr;
    }

    // If we have all types necessary to create function, do that.
    bool hasAllTypes = true;
    if (bfunction->returnType->internalType == nullptr) {
        hasAllTypes = false;
    }

    for (BalanceParameter * bparameter : bfunction->parameters) {
        if (bparameter->balanceType->internalType == nullptr) {
            hasAllTypes = false;
        }
    }

    if (hasAllTypes) {
        vector<Type *> functionParameterTypes;
        vector<std::string> functionParameterNames;
        for (BalanceParameter *bparameter : bfunction->parameters) {
            functionParameterTypes.push_back(bparameter->balanceType->getReferencableType());
            functionParameterNames.push_back(bparameter->name);
        }

        Function * function = Function::Create(bfunction->getLlvmFunctionType(), Function::ExternalLinkage, bfunction->getFullyQualifiedFunctionName(), currentPackage->currentModule->module);
        bfunction->setLlvmFunction(function);
    }

    return nullptr;
}

std::any LLVMTypeVisitor::visitGenericType(BalanceParser::GenericTypeContext *ctx) {
    std::string base = ctx->IDENTIFIER()->getText();
    std::vector<BalanceType *> generics;

    for (BalanceParser::BalanceTypeContext *type: ctx->typeList()->balanceType()) {
        BalanceType * btype = any_cast<BalanceType *>(visit(type));
        generics.push_back(btype);
    }

    BalanceType * btype = currentPackage->currentModule->getType(base, generics);
    if (btype == nullptr) {
        btype = currentPackage->currentModule->createGenericType(base, generics);
    }

    if (!btype->finalized()) {
        if (btype->name == "Lambda") {
            LambdaBalanceType * lambdaType = (LambdaBalanceType *) currentPackage->getBuiltinType("Lambda");
            lambdaType->tryFinalizeGenericType(btype);
        }
    }

    return btype;
}

std::any LLVMTypeVisitor::visitSimpleType(BalanceParser::SimpleTypeContext *ctx) {
    std::string typeName = ctx->IDENTIFIER()->getText();

    if (currentPackage->currentModule->currentType != nullptr) {
        // Check if it is a class generic
        if (currentPackage->currentModule->currentType->genericsMapping.find(typeName) != currentPackage->currentModule->currentType->genericsMapping.end()) {
            return currentPackage->currentModule->currentType->genericsMapping[typeName];
        }
        for (BalanceType * genericType : currentPackage->currentModule->currentType->generics) {
            if (typeName == genericType->name) {
                return genericType;
            }
        }
    }
    return currentPackage->currentModule->getType(typeName);
}

std::any LLVMTypeVisitor::visitNoneType(BalanceParser::NoneTypeContext *ctx) {
    return currentPackage->currentModule->getType(ctx->NONE()->getText());
}

std::any LLVMTypeVisitor::visitLambdaType(BalanceParser::LambdaTypeContext *ctx) {
    std::string text = ctx->getText();
    std::vector<BalanceType *> generics;

    for (BalanceParser::BalanceTypeContext *type : ctx->typeList()->balanceType()) {
        BalanceType * btype = any_cast<BalanceType *>(visit(type));
        generics.push_back(btype);
    }

    BalanceType *returnType = any_cast<BalanceType *>(visit(ctx->balanceType()));
    generics.push_back(returnType);

    BalanceType * lambdaType = currentPackage->currentModule->getType("Lambda", generics);
    if (lambdaType == nullptr) {
        lambdaType = currentPackage->currentModule->createGenericType("Lambda", generics);
    }

    return lambdaType;
}

std::any LLVMTypeVisitor::visitLambdaExpression(BalanceParser::LambdaExpressionContext *ctx) {
    std::string text = ctx->getText();
    vector<BalanceType *> generics;

    for (BalanceParser::VariableTypeTupleContext *parameter : ctx->lambda()->parameterList()->variableTypeTuple()) {
        BalanceType * btype = nullptr;
        if (parameter->type) {
            std::string typeString = parameter->type->getText();
            btype = currentPackage->currentModule->getType(typeString);
        } else {
            btype = currentPackage->currentModule->getType("Any");
        }
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

    BalanceType * btype = currentPackage->currentModule->getType("Lambda", generics);
    if (btype == nullptr) {
        btype = currentPackage->currentModule->createGenericType("Lambda", generics);
    }

    if (!btype->finalized()) {
        LambdaBalanceType * lambdaType = (LambdaBalanceType *) currentPackage->getBuiltinType("Lambda");
        lambdaType->tryFinalizeGenericType(btype);
    }

    return btype;
}