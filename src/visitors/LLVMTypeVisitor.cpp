#include "LLVMTypeVisitor.h"

#include "../Package.h"
#include "Visitor.h"
#include "BalanceLexer.h"
#include "BalanceParser.h"
#include "BalanceParserBaseVisitor.h"

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
            if (importedModule->classes.find(importString) != importedModule->classes.end()) {
                BalanceClass *bclass = importedModule->getClassFromBaseName(importString);
                createImportedClass(currentPackage->currentModule, bclass);
            } else if (importedModule->functions.find(importString) != importedModule->functions.end()) {
                BalanceFunction *bfunction = importedModule->getFunction(importString);
                createImportedFunction(currentPackage->currentModule, bfunction);
            } else if (importedModule->globals[importString] != nullptr) {
                // TODO: Handle
            }
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

    if (currentPackage->currentModule->currentClass->structType == nullptr) {
        StructType *structType = StructType::create(*currentPackage->context, className);
        currentPackage->currentModule->currentClass->structType = structType;
    }

    ArrayRef<Type *> propertyTypesRef(propertyTypes);
    currentPackage->currentModule->currentClass->structType->setBody(propertyTypesRef, false);
    currentPackage->currentModule->currentClass->hasBody = true;

    currentPackage->currentModule->currentClass->name->populateTypes();

    currentPackage->currentModule->currentClass = nullptr;
    return nullptr;
}

std::any LLVMTypeVisitor::visitFunctionDefinition(BalanceParser::FunctionDefinitionContext *ctx) {
    string functionName = ctx->functionSignature()->IDENTIFIER()->getText();

    BalanceFunction *bfunction;
    if (currentPackage->currentModule->currentClass != nullptr) {
        bfunction = currentPackage->currentModule->currentClass->methods[functionName];
    } else {
        bfunction = currentPackage->currentModule->functions[functionName];
    }

    for (BalanceParameter *bparameter : bfunction->parameters) {
        if (bparameter->type == nullptr) {
            Type *type = getBuiltinType(bparameter->balanceTypeString);
            if (type != nullptr) {
                bparameter->type = type;
            } else {
                BalanceClass *bclass = currentPackage->currentModule->getClass(bparameter->balanceTypeString);
                if (bclass == nullptr) {
                    BalanceImportedClass *ibclass = currentPackage->currentModule->getImportedClass(bparameter->balanceTypeString);
                    if (ibclass == nullptr) {
                        BalanceInterface * binterface = currentPackage->currentModule->getInterface(bparameter->balanceTypeString->base);
                        if (binterface == nullptr) {
                            // TODO: Imported interface
                            throw std::runtime_error("Couldn't find type: " + bparameter->balanceTypeString->toString());
                        } else {
                            bparameter->type = binterface->structType->getPointerTo();
                        }
                    } else {
                        if (ibclass->bclass->structType != nullptr) {
                            bparameter->type = ibclass->bclass->structType->getPointerTo();
                        }
                    }
                } else {
                    if (bclass->structType != nullptr) {
                        bparameter->type = bclass->structType->getPointerTo();
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

    // TODO: Do we want more checks below?
    if (bfunction->returnType == nullptr) {
        bfunction->returnType = getBuiltinType(bfunction->returnTypeString);
        if (bfunction->returnType == nullptr) {
            bfunction->returnType = currentPackage->currentModule->getClass(bfunction->returnTypeString)->structType->getPointerTo();
            if (bfunction->returnType == nullptr) {
                bfunction->returnType = currentPackage->currentModule->getImportedClass(bfunction->returnTypeString)->bclass->structType->getPointerTo();
            }
        }
    }

    return nullptr;
}

std::any LLVMTypeVisitor::visitClassProperty(BalanceParser::ClassPropertyContext *ctx) {
    string text = ctx->getText();
    string typeString = ctx->type->getText();
    BalanceTypeString * btypeString = new BalanceTypeString(typeString); // TODO: Parse generics
    string name = ctx->name->getText();

    BalanceProperty *bprop = currentPackage->currentModule->currentClass->properties[name];
    // If already finalized, skip
    if (bprop->finalized()) {
        return nullptr;
    }

    Type *typeValue = getBuiltinType(btypeString);
    if (typeValue == nullptr) {
        BalanceClass * bclass = currentPackage->currentModule->getClass(btypeString);
        if (bclass == nullptr) {
            BalanceImportedClass *ibclass = currentPackage->currentModule->getImportedClass(btypeString);
            if (ibclass == nullptr) {
                // TODO: Throw error
            } else {
                if (ibclass->bclass->finalized()) {
                    if (!bprop->finalized()) {
                        bprop->type = ibclass->bclass->structType->getPointerTo();
                    }
                }
            }
        } else {
            if (bclass->finalized()) {
                if (!bprop->finalized()) {
                    bprop->type = bclass->structType->getPointerTo();
                }
            }
        }
    } else {
        bprop->type = typeValue;
    }

    return nullptr;
}
