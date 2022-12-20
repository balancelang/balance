#include "LLVMTypeVisitor.h"

#include "../BalancePackage.h"
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

extern BalancePackage *currentPackage;

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

            BalanceType * btype = importedModule->getType(new BalanceTypeString(importString));
            if (btype != nullptr) {
                if (btype->isInterface()) {
                    // TODO: Handle
                } else {
                    createImportedClass(currentPackage->currentModule, (BalanceClass *) btype);
                }
                continue;
            }

            BalanceFunction *bfunction = importedModule->getFunction(importString);
            if (bfunction != nullptr) {
                createImportedFunction(currentPackage->currentModule, bfunction);
                continue;
            }

            // TODO: Handle
        }
    }

    return nullptr;
}

std::any LLVMTypeVisitor::visitClassDefinition(BalanceParser::ClassDefinitionContext *ctx) {
    string text = ctx->getText();
    string className = ctx->className->getText();

    BalanceClass *bclass = currentPackage->currentModule->classes[className];
    // If already finalized, skip
    if (bclass->finalized()) {
        return nullptr;
    }

    currentPackage->currentModule->currentClass = bclass;

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
    for (auto const &x : currentPackage->currentModule->currentClass->properties) {
        BalanceProperty *prop = x.second;
        if (!prop->finalized()) {
            // If a property type is not yet known
            currentPackage->currentModule->currentClass = nullptr;
            return nullptr;
        }
        propertyTypes.push_back(prop->type);
    }

    if (currentPackage->currentModule->currentClass->internalType == nullptr) {
        StructType *structType = StructType::create(*currentPackage->context, className);
        ArrayRef<Type *> propertyTypesRef(propertyTypes);
        structType->setBody(propertyTypesRef, false);
        currentPackage->currentModule->currentClass->internalType = structType;
        currentPackage->currentModule->currentClass->hasBody = true;
    }

    currentPackage->currentModule->currentClass->name->populateTypes();

    currentPackage->currentModule->currentClass = nullptr;
    return nullptr;
}

std::any LLVMTypeVisitor::visitFunctionSignature(BalanceParser::FunctionSignatureContext *ctx) {
    string functionName = ctx->IDENTIFIER()->getText();

    BalanceFunction *bfunction;
    if (currentPackage->currentModule->currentClass != nullptr) {
        bfunction = currentPackage->currentModule->currentClass->getMethod(functionName);
    } else if (currentPackage->currentModule->currentInterface != nullptr) {
        bfunction = currentPackage->currentModule->currentInterface->getMethod(functionName);
    } else {
        bfunction = currentPackage->currentModule->functions[functionName];
    }

    for (int i = 0; i < bfunction->parameters.size(); i++) {
        BalanceParameter *bparameter = bfunction->parameters[i];
        if (bparameter->type == nullptr) {
            Type *type = getBuiltinType(bparameter->balanceTypeString);
            if (type != nullptr) {
                bparameter->type = type;
            } else {
                BalanceType *btype = currentPackage->currentModule->getType(bparameter->balanceTypeString);
                if (btype == nullptr) {
                    throw std::runtime_error("Couldn't find type: " + bparameter->balanceTypeString->toString());
                }

                if (btype->internalType != nullptr) {
                    if (btype->isInterface()) {
                        BalanceType * fatPointerType = currentPackage->currentModule->getType(new BalanceTypeString("FatPointer"));
                        bparameter->type = fatPointerType->getReferencableType();
                        bparameter->balanceTypeString->isInterface = true;
                    } else {
                        bparameter->type = btype->getReferencableType();
                    }
                }
            }
        }
    }

    // If we have all types necessary to create function, do that.
    if (bfunction->hasAllTypes()) {
        vector<Type *> functionParameterTypes;
        vector<std::string> functionParameterNames;
        for (BalanceParameter *bparameter : bfunction->parameters) {
            // If struct-type, make pointer to element
            if (bparameter->type->isStructTy()) {
                functionParameterTypes.push_back(bparameter->type->getPointerTo());
            } else {
                functionParameterTypes.push_back(bparameter->type);
            }
            functionParameterNames.push_back(bparameter->name);
        }

        // Check if we are parsing a class method
        Function *function;
        if (currentPackage->currentModule->currentClass != nullptr) {
            std::string functionNameWithClass = currentPackage->currentModule->currentClass->name->toString() + "_" + functionName;
            ArrayRef<Type *> parametersReference(functionParameterTypes);
            // TODO: Make sure we have returnType before running all this? (when class methods can return class types)
            FunctionType *functionType = FunctionType::get(bfunction->returnType, parametersReference, false);
            function = Function::Create(functionType, Function::ExternalLinkage, functionNameWithClass, currentPackage->currentModule->module);

            bfunction->function = function;
        } else {
            ArrayRef<Type *> parametersReference(functionParameterTypes);
            FunctionType *functionType = FunctionType::get(bfunction->returnType, parametersReference, false);
            function = Function::Create(functionType, Function::ExternalLinkage, functionName, currentPackage->currentModule->module);
            bfunction->function = function;
        }
    }

    if (bfunction->returnType == nullptr) {
        Type * type = getBuiltinType(bfunction->returnTypeString);
        if (type != nullptr) {
            bfunction->returnType = type;
        } else {
            BalanceType * btype = currentPackage->currentModule->getType(bfunction->returnTypeString);
            if (btype == nullptr) {
                throw std::runtime_error("Failed to find type: " + bfunction->returnTypeString->toString());
            }
            if (btype->isInterface()) {
                BalanceType * fatPointerType = currentPackage->currentModule->getType(new BalanceTypeString("FatPointer"));
                bfunction->returnType = fatPointerType->getReferencableType();
                bfunction->returnTypeString->isInterface = true;
            } else {
                bfunction->returnType = btype->getReferencableType();
            }
        }
    }

    return nullptr;
}

std::any LLVMTypeVisitor::visitClassProperty(BalanceParser::ClassPropertyContext *ctx) {
    string text = ctx->getText();
    string typeString = ctx->type->getText();
    BalanceTypeString *btypeString = new BalanceTypeString(typeString); // TODO: Parse generics
    string name = ctx->name->getText();

    BalanceProperty *bprop = currentPackage->currentModule->currentClass->properties[name];
    // If already finalized, skip
    if (bprop->finalized()) {
        return nullptr;
    }

    Type *typeValue = getBuiltinType(btypeString);
    if (typeValue != nullptr) {
        bprop->type = typeValue;
    } else {
        BalanceType * btype = currentPackage->currentModule->getType(btypeString);
        if (btype == nullptr) {
            throw std::runtime_error("Failed to find type: " + btypeString->toString());
        }

        if (btype->isInterface()) {
            BalanceType * fatPointerType = currentPackage->currentModule->getType(new BalanceTypeString("FatPointer"));
            bprop->type = fatPointerType->getReferencableType();
            bprop->stringType->isInterface = true;
        } else {
            bprop->type = btype->getReferencableType();
        }
    }

    return nullptr;
}

std::any LLVMTypeVisitor::visitInterfaceDefinition(BalanceParser::InterfaceDefinitionContext *ctx) {
    std::string text = ctx->getText();
    string interfaceName = ctx->interfaceName->getText();

    BalanceTypeString *btypeString = new BalanceTypeString(interfaceName);
    BalanceInterface * binterface = (BalanceInterface *) currentPackage->currentModule->getType(btypeString);

    if (binterface->finalized()) {
        return nullptr;
    }

    currentPackage->currentModule->currentInterface = binterface;

    // Visit all interface functions
    for (auto const &x : ctx->interfaceElement()) {
        if (x->functionSignature()) {
            visit(x);
        }
    }

    currentPackage->currentModule->currentInterface = nullptr;

    return std::any();
}